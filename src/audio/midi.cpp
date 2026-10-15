#include "midi.h"
#include "audio.h"
#include "../core/assets.h"
#include "../core/log.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>
#include <condition_variable>

#define TSF_IMPLEMENTATION
#include "../../third_party/tsf.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {

struct Event {
    double time;       // seconds from start
    uint8_t status, d1, d2;
    std::vector<uint8_t> sysex;
};

struct Song {
    std::vector<Event> events;
    double length = 0;
};

uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
uint16_t be16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }

bool ParseSMF(const std::vector<uint8_t>& d, Song* song) {
    if (d.size() < 14 || memcmp(d.data(), "MThd", 4) != 0) return false;
    uint32_t hlen = be32(&d[4]);
    uint16_t ntrk = be16(&d[10]);
    uint16_t div = be16(&d[12]);
    if (div & 0x8000) return false;  // SMPTE timing not used by these files
    struct Raw { uint64_t tick; int order; uint8_t st, a, b; std::vector<uint8_t> sx; uint32_t tempo; bool isTempo; };
    std::vector<Raw> raw;
    size_t p = 8 + hlen;
    int order = 0;
    for (int t = 0; t < ntrk && p + 8 <= d.size(); t++) {
        if (memcmp(&d[p], "MTrk", 4) != 0) break;
        uint32_t len = be32(&d[p + 4]);
        size_t q = p + 8, end = std::min(d.size(), q + len);
        uint64_t tick = 0;
        uint8_t running = 0;
        auto vlq = [&](uint32_t* out) {
            uint32_t v = 0;
            for (int i = 0; i < 4 && q < end; i++) {
                uint8_t c = d[q++];
                v = (v << 7) | (c & 0x7F);
                if (!(c & 0x80)) break;
            }
            *out = v;
        };
        while (q < end) {
            uint32_t delta;
            vlq(&delta);
            tick += delta;
            if (q >= end) break;
            uint8_t st = d[q];
            if (st & 0x80) q++;
            else st = running;
            if (st == 0xFF) {
                if (q >= end) break;
                uint8_t type = d[q++];
                uint32_t l;
                vlq(&l);
                if (type == 0x51 && l == 3 && q + 3 <= end) {
                    Raw r{tick, order++, 0, 0, 0, {}, (uint32_t)(d[q] << 16 | d[q + 1] << 8 | d[q + 2]), true};
                    raw.push_back(r);
                }
                q += l;
                if (type == 0x2F) break;
            } else if (st == 0xF0 || st == 0xF7) {
                uint32_t l;
                vlq(&l);
                Raw r{tick, order++, st, 0, 0, {}, 0, false};
                if (st == 0xF0) r.sx.push_back(0xF0);
                for (uint32_t i = 0; i < l && q + i < end; i++) r.sx.push_back(d[q + i]);
                raw.push_back(r);
                q += l;
            } else {
                running = st;
                uint8_t hi = st & 0xF0;
                int n = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
                Raw r{tick, order++, st, 0, 0, {}, 0, false};
                if (q < end) r.a = d[q];
                if (n == 2 && q + 1 < end) r.b = d[q + 1];
                q += n;
                raw.push_back(r);
            }
        }
        p = 8 + p + len;
    }
    std::stable_sort(raw.begin(), raw.end(), [](const Raw& a, const Raw& b) { return a.tick < b.tick; });
    double tempo = 500000.0, sec = 0;
    uint64_t lastTick = 0;
    for (auto& r : raw) {
        sec += (double)(r.tick - lastTick) * tempo / 1e6 / div;
        lastTick = r.tick;
        if (r.isTempo) {
            tempo = r.tempo;
            continue;
        }
        Event e{sec, r.st, r.a, r.b, std::move(r.sx)};
        song->events.push_back(std::move(e));
    }
    song->length = sec;
    return true;
}

// ---------------------------------------------------------------------------
std::mutex g_mutex;
Song g_song;
bool g_loaded = false;
bool g_playing = false;
double g_pos = 0;        // seconds
size_t g_next = 0;
std::atomic<bool> g_finished{false};
float g_volume = 1.f;

enum class Backend { None, System, Soft } g_backend = Backend::None;
tsf* g_tsf = nullptr;

#if defined(_WIN32)
HMIDIOUT g_hmo = nullptr;
std::thread g_thread;
std::atomic<bool> g_quit{false};
#endif

// The Windows synth plays outside our mixer, so music/master volume is applied by scaling each
// channel's volume controller (CC7). midiOutSetVolume is not used: the MIDI mapper usually
// rejects it, and where it works it changes the synth's volume for every program.
#if defined(_WIN32)
uint8_t g_chanVol[16] = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};

uint8_t ScaledChannelVolume(uint8_t songValue) {
    // GM synths map CC7 to amplitude roughly as (cc/127)^2, so scale by sqrt(volume) to get a
    // linear amplitude change matching the sound-effect volume.
    float v = std::clamp(g_volume, 0.f, 1.f);
    int out = (int)(songValue * std::sqrt(v) + 0.5f);
    return (uint8_t)std::clamp(out, 0, 127);
}
#endif

void SendShort(uint8_t st, uint8_t a, uint8_t b) {
    if (g_backend == Backend::Soft && g_tsf) {
        int ch = st & 0x0F;
        switch (st & 0xF0) {
            case 0x80: tsf_channel_note_off(g_tsf, ch, a); break;
            case 0x90: tsf_channel_note_on(g_tsf, ch, a, b / 127.f); break;
            case 0xB0: tsf_channel_midi_control(g_tsf, ch, a, b); break;
            case 0xC0: tsf_channel_set_presetnumber(g_tsf, ch, a, ch == 9); break;
            case 0xE0: tsf_channel_set_pitchwheel(g_tsf, ch, a | (b << 7)); break;
        }
    }
#if defined(_WIN32)
    else if (g_backend == Backend::System && g_hmo) {
        if ((st & 0xF0) == 0xB0 && a == 7) {
            // Channel volume: remember the song's value and send it scaled by our volume.
            g_chanVol[st & 0x0F] = b;
            b = ScaledChannelVolume(b);
        }
        midiOutShortMsg(g_hmo, (DWORD)st | (DWORD)a << 8 | (DWORD)b << 16);
        if ((st & 0xF0) == 0xB0 && a == 121) {
            // Some synths reset channel volume on "reset all controllers": re-apply ours.
            uint8_t ch = st & 0x0F;
            midiOutShortMsg(g_hmo, (DWORD)(0xB0 | ch) | 7u << 8 | (DWORD)ScaledChannelVolume(g_chanVol[ch]) << 16);
        }
    }
#endif
}

void SendSysex(const std::vector<uint8_t>& sx) {
#if defined(_WIN32)
    if (g_backend == Backend::System && g_hmo && !sx.empty()) {
        std::vector<uint8_t> buf = sx;
        MIDIHDR h{};
        h.lpData = (LPSTR)buf.data();
        h.dwBufferLength = h.dwBytesRecorded = (DWORD)buf.size();
        if (midiOutPrepareHeader(g_hmo, &h, sizeof h) == MMSYSERR_NOERROR) {
            midiOutLongMsg(g_hmo, &h, sizeof h);
            for (int i = 0; i < 200 && !(h.dwFlags & MHDR_DONE); i++) SDL_Delay(1);
            midiOutUnprepareHeader(g_hmo, &h, sizeof h);
        }
        // A GM/GS/XG reset puts every channel back to volume 100 at full scale; re-apply ours.
        bool gmReset = sx.size() >= 5 && sx[1] == 0x7E && sx[3] == 0x09;
        bool gsReset = sx.size() >= 9 && sx[1] == 0x41 && sx[3] == 0x42 && sx[5] == 0x40 && sx[6] == 0x00 && sx[7] == 0x7F;
        bool xgReset = sx.size() >= 8 && sx[1] == 0x43 && sx[3] == 0x4C && sx[4] == 0x00 && sx[5] == 0x00 && sx[6] == 0x7E;
        if (gmReset || gsReset || xgReset) {
            for (int ch = 0; ch < 16; ch++) SendShort((uint8_t)(0xB0 | ch), 7, 100);
        }
    }
#else
    (void)sx;
#endif
}

void AllNotesOff() {
    for (int ch = 0; ch < 16; ch++) {
        SendShort((uint8_t)(0xB0 | ch), 123, 0);
        SendShort((uint8_t)(0xB0 | ch), 120, 0);
        SendShort((uint8_t)(0xB0 | ch), 64, 0);
    }
    if (g_backend == Backend::Soft && g_tsf) tsf_note_off_all(g_tsf);
}

void ResetControllers() {
    for (int ch = 0; ch < 16; ch++) {
        SendShort((uint8_t)(0xB0 | ch), 121, 0);
        SendShort((uint8_t)(0xE0 | ch), 0, 0x40);
    }
    if (g_backend == Backend::Soft && g_tsf) {
        for (int ch = 0; ch < 16; ch++) {
            tsf_channel_set_presetnumber(g_tsf, ch, 0, ch == 9);
            tsf_channel_set_volume(g_tsf, ch, 100.f / 127.f);
        }
    }
}

// Dispatch all events up to g_pos. Caller holds g_mutex.
void DispatchUpTo(double t) {
    while (g_next < g_song.events.size() && g_song.events[g_next].time <= t) {
        const Event& e = g_song.events[g_next++];
        if (!e.sysex.empty()) SendSysex(e.sysex);
        else SendShort(e.status, e.d1, e.d2);
    }
    if (g_next >= g_song.events.size() && t >= g_song.length) {
        g_playing = false;
        g_finished = true;
        AllNotesOff();
    }
}

void SoftRender(float* out, int frames, int rate) {
    // Called from the audio thread with Audio's lock held; we only take our own lock.
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_tsf) return;
    const int block = 64;
    int done = 0;
    while (done < frames) {
        int n = std::min(block, frames - done);
        if (g_playing) {
            DispatchUpTo(g_pos);
            g_pos += (double)n / rate;
        }
        tsf_render_float(g_tsf, out + done * 2, n, 1);
        done += n;
    }
}

#if defined(_WIN32)
void SystemThread() {
    timeBeginPeriod(1);
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t last = SDL_GetPerformanceCounter();
    while (!g_quit) {
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            uint64_t now = SDL_GetPerformanceCounter();
            double dt = (double)(now - last) / freq;
            last = now;
            if (g_playing) {
                g_pos += dt;
                DispatchUpTo(g_pos);
            }
        }
        SDL_Delay(1);
    }
    timeEndPeriod(1);
}
#endif

}  // namespace

namespace Midi {

bool Init(const std::string& soundFontPath) {
    std::string sf = soundFontPath;
    if (sf == "system") {
        sf.clear();  // explicit choice of the operating system synthesizer: skip auto-detection
    } else if (!sf.empty()) {
        // relative paths are relative to the program's folder
        bool absolute = sf[0] == '/' || sf[0] == '\\' || (sf.size() > 1 && sf[1] == ':');
        if (!absolute) sf = std::string(SDL_GetBasePath() ? SDL_GetBasePath() : "") + sf;
    } else {
        for (const char* cand : {"soundfont.sf2", "hopmon.sf2"}) {
            std::string p1 = std::string(SDL_GetBasePath() ? SDL_GetBasePath() : "") + cand;
            FILE* f = fopen(p1.c_str(), "rb");
            if (f) { fclose(f); sf = p1; break; }
            std::string p2 = Assets::ResolveGameFile(cand);
            f = fopen(p2.c_str(), "rb");
            if (f) { fclose(f); sf = p2; break; }
        }
    }
    if (!sf.empty()) {
        g_tsf = tsf_load_filename(sf.c_str());
        if (g_tsf) {
            tsf_set_output(g_tsf, TSF_STEREO_INTERLEAVED, Audio::OutputRate(), 0.0f);
            tsf_set_max_voices(g_tsf, 256);
            g_backend = Backend::Soft;
            Audio::SetMusicRenderer(SoftRender);
            Log("MIDI: using built-in synthesizer with %s", sf.c_str());
            return true;
        }
        Log("MIDI: failed to load SoundFont %s", sf.c_str());
    }
#if defined(_WIN32)
    if (midiOutOpen(&g_hmo, MIDI_MAPPER, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
        g_backend = Backend::System;
        g_quit = false;
        g_thread = std::thread(SystemThread);
        Log("MIDI: using the Windows system synthesizer");
        return true;
    }
#endif
    Log("MIDI: no synthesizer available (place a General MIDI soundfont.sf2 next to the game to enable music)");
    g_backend = Backend::None;
    return false;
}

static void ShutdownBackend() {
#if defined(_WIN32)
    if (g_backend == Backend::System) {
        g_quit = true;
        if (g_thread.joinable()) g_thread.join();
        midiOutReset(g_hmo);
        midiOutClose(g_hmo);
        g_hmo = nullptr;
    }
#endif
    Audio::SetMusicRenderer(nullptr);
    if (g_tsf) tsf_close(g_tsf);
    g_tsf = nullptr;
    g_backend = Backend::None;
}

void Shutdown() {
    Close();
    ShutdownBackend();
}

bool SwitchSynth(const std::string& soundFontPath) {
    // Keep the song and its position, swap the synthesizer underneath, then replay the song's
    // controller/program state up to the current position so instruments and volumes match.
    Song song;
    bool loaded, playing;
    double pos;
    size_t next;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_playing) AllNotesOff();
        song = std::move(g_song);
        loaded = g_loaded;
        playing = g_playing;
        pos = g_pos;
        next = g_next;
        g_playing = false;
        g_loaded = false;
        g_song = Song();
    }
    ShutdownBackend();
    bool ok = Init(soundFontPath);
    std::lock_guard<std::mutex> lk(g_mutex);
    g_song = std::move(song);
    g_loaded = loaded;
    g_pos = pos;
    g_next = next;
    if (loaded) {
        ResetControllers();
        for (size_t i = 0; i < next && i < g_song.events.size(); i++) {
            const Event& e = g_song.events[i];
            if (!e.sysex.empty()) SendSysex(e.sysex);
            else if ((e.status & 0xF0) == 0xB0 || (e.status & 0xF0) == 0xC0 || (e.status & 0xF0) == 0xE0)
                SendShort(e.status, e.d1, e.d2);
        }
    }
    g_playing = playing;
    return ok;
}

bool Open(const std::string& path) {
    std::vector<uint8_t> bytes;
    Song song;
    bool ok = Assets::ReadFile(path, &bytes) && ParseSMF(bytes, &song);
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_playing) AllNotesOff();
    g_playing = false;
    g_finished = false;
    g_loaded = ok;
    g_song = ok ? std::move(song) : Song();
    g_pos = 0;
    g_next = 0;
    if (!ok) Log("MIDI: could not load %s", path.c_str());
    return ok;
}

void PlayFromStart() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_loaded) return;
    AllNotesOff();
    ResetControllers();
#if defined(_WIN32)
    for (auto& c : g_chanVol) c = 100;  // GM default until the song sets its own
#endif
    for (int ch = 0; ch < 16; ch++) SendShort((uint8_t)(0xB0 | ch), 7, 100);
    g_pos = 0;
    g_next = 0;
    g_finished = false;
    g_playing = true;
}

void Pause() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_playing) AllNotesOff();
    g_playing = false;
}

void Resume() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_loaded) return;
    if (g_next >= g_song.events.size()) {  // MCI: playing at end restarts nothing; behave like "from current"
        g_pos = 0;
        g_next = 0;
    }
    g_playing = true;
}

void Close() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_playing) AllNotesOff();
    g_playing = false;
    g_loaded = false;
    g_song = Song();
    g_pos = 0;
    g_next = 0;
}

bool PollFinished() { return g_finished.exchange(false); }

void SetVolume(float v) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_volume = v;
#if defined(_WIN32)
    if (g_backend == Backend::System && g_hmo)
        for (int ch = 0; ch < 16; ch++) SendShort((uint8_t)(0xB0 | ch), 7, g_chanVol[ch]);
#endif
}

const char* BackendName() {
    switch (g_backend) {
        case Backend::System: return "Windows MIDI synth";
        case Backend::Soft: return "SoundFont synth";
        default: return "unavailable";
    }
}

}  // namespace Midi
