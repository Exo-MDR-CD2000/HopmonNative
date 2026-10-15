#include "music_stream.h"
#include "audio.h"
#include "../core/assets.h"
#include "../core/config.h"
#include "../core/log.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "../../third_party/dr_mp3.h"
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_OGG
#include "../../third_party/dr_flac.h"

namespace {

enum class Kind { None, MP3, FLAC };

struct Track {
    Kind kind = Kind::None;
    std::string path;
    void* fileData = nullptr;  // whole compressed file (SDL_LoadFile); decoders read from it
    size_t fileSize = 0;
    drmp3 mp3{};
    std::vector<drmp3_seek_point> seekPoints;
    drflac* flac = nullptr;
    int channels = 0;
    int rate = 0;
    uint64_t total = 0;      // PCM frames (0 = unknown)
    uint64_t loopStart = 0;  // frames
    uint64_t loopEnd = 0;    // exclusive; 0 = end of file
    uint64_t cursor = 0;     // next frame the decoder will return
    SDL_AudioStream* conv = nullptr;  // file format -> stereo float at the output rate
    int loadMs = 0;
};

std::mutex g_mutex;
std::unique_ptr<Track> g_track;  // heap-allocated: the decoders keep pointers into themselves
bool g_open = false;
bool g_playing = false;
std::vector<float> g_decode, g_out;

// ------------------------------------------------------------------ loop tags
// Accepted (case-insensitive): LOOPSTART / LOOP_START, LOOPLENGTH / LOOP_LENGTH, LOOPEND / LOOP_END,
// values in sample frames - the convention used by RPG Maker and most game-music rips.
struct LoopTags {
    int64_t start = -1, length = -1, end = -1;
    void Set(std::string key, const std::string& value) {
        for (auto& c : key) c = (char)std::toupper((unsigned char)c);
        key.erase(std::remove(key.begin(), key.end(), '_'), key.end());
        char* e = nullptr;
        long long v = strtoll(value.c_str(), &e, 10);
        if (e == value.c_str() || v < 0) return;
        if (key == "LOOPSTART") start = v;
        else if (key == "LOOPLENGTH") length = v;
        else if (key == "LOOPEND") end = v;
    }
};

void OnFlacMeta(void* user, drflac_metadata* m) {
    if (m->type != DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) return;
    auto* tags = (LoopTags*)user;
    drflac_vorbis_comment_iterator it;
    drflac_init_vorbis_comment_iterator(&it, m->data.vorbis_comment.commentCount, m->data.vorbis_comment.pComments);
    drflac_uint32 len;
    while (const char* c = drflac_next_vorbis_comment(&it, &len)) {
        std::string s(c, len);
        size_t eq = s.find('=');
        if (eq != std::string::npos) tags->Set(s.substr(0, eq), s.substr(eq + 1));
    }
}

// ID3v2 text-with-description frames: TXXX <encoding> <description> 0 <value>
std::string Id3Text(const uint8_t* p, size_t n, int enc) {
    std::string s;
    if (enc == 1 || enc == 2) {  // UTF-16 (with or without BOM): the tags we need are ASCII
        size_t i = 0;
        bool be = enc == 2;
        if (n >= 2 && p[0] == 0xFE && p[1] == 0xFF) { be = true; i = 2; }
        else if (n >= 2 && p[0] == 0xFF && p[1] == 0xFE) { be = false; i = 2; }
        for (; i + 1 < n; i += 2) {
            unsigned c = be ? (p[i] << 8 | p[i + 1]) : (p[i] | p[i + 1] << 8);
            if (c == 0) break;
            s += c < 128 ? (char)c : '?';
        }
    } else {
        for (size_t i = 0; i < n && p[i]; i++) s += (char)p[i];
    }
    return s;
}

void OnMp3Meta(void* user, const drmp3_metadata* m) {
    if (m->type != DRMP3_METADATA_TYPE_ID3V2 || m->rawDataSize < 10) return;
    auto* tags = (LoopTags*)user;
    const uint8_t* d = (const uint8_t*)m->pRawData;
    int ver = d[3];
    size_t size = m->rawDataSize, pos = 10;
    if (d[5] & 0x40 && pos + 4 <= size) {  // extended header: skip it
        size_t ext = ver >= 4 ? ((d[10] & 0x7f) << 21 | (d[11] & 0x7f) << 14 | (d[12] & 0x7f) << 7 | (d[13] & 0x7f))
                              : ((size_t)d[10] << 24 | d[11] << 16 | d[12] << 8 | d[13]) + 4;
        pos += ext;
    }
    while (ver >= 3 && pos + 10 <= size) {
        const uint8_t* f = d + pos;
        if (f[0] == 0) break;  // padding
        size_t fs = ver >= 4 ? ((f[4] & 0x7f) << 21 | (f[5] & 0x7f) << 14 | (f[6] & 0x7f) << 7 | (f[7] & 0x7f))
                             : ((size_t)f[4] << 24 | f[5] << 16 | f[6] << 8 | f[7]);
        if (fs == 0 || pos + 10 + fs > size) break;
        if (memcmp(f, "TXXX", 4) == 0 && fs > 1) {
            const uint8_t* b = f + 10;
            int enc = b[0];
            size_t i = 1, unit = (enc == 1 || enc == 2) ? 2 : 1;
            // description ends at a 0 (or 0 0 for UTF-16, aligned)
            while (i + unit <= fs && !(b[i] == 0 && (unit == 1 || b[i + 1] == 0))) i += unit;
            std::string key = Id3Text(b + 1, i - 1, enc);
            i += unit;
            if (i < fs) tags->Set(key, Id3Text(b + i, fs - i, enc));
        }
        pos += 10 + fs;
    }
}

// ------------------------------------------------------------------ decoding
void FreeTrack(std::unique_ptr<Track>& p) {
    if (!p) return;
    Track& t = *p;
    if (t.conv) SDL_DestroyAudioStream(t.conv);
    if (t.kind == Kind::MP3) drmp3_uninit(&t.mp3);
    if (t.flac) drflac_close(t.flac);
    if (t.fileData) SDL_free(t.fileData);
    p.reset();
}

uint64_t ReadFrames(Track& t, float* out, uint64_t n) {
    if (t.kind == Kind::MP3) return drmp3_read_pcm_frames_f32(&t.mp3, n, out);
    if (t.kind == Kind::FLAC) return drflac_read_pcm_frames_f32(t.flac, n, out);
    return 0;
}

bool SeekFrame(Track& t, uint64_t f) {
    bool ok = t.kind == Kind::MP3 ? drmp3_seek_to_pcm_frame(&t.mp3, f) : drflac_seek_to_pcm_frame(t.flac, f);
    if (ok) t.cursor = f;
    return ok;
}

// Decode up to `want` frames into the converter, wrapping at the loop end. Returns false if
// the file cannot produce audio any more (corrupt data).
bool Feed(Track& t, int want) {
    const uint64_t end = t.loopEnd ? t.loopEnd : UINT64_MAX;
    int fed = 0, emptyReads = 0;
    while (fed < want) {
        uint64_t n = (uint64_t)std::min(want - fed, 4096);
        if (end != UINT64_MAX) n = std::min<uint64_t>(n, end > t.cursor ? end - t.cursor : 0);
        uint64_t got = 0;
        if (n > 0) {
            g_decode.resize((size_t)n * t.channels);
            got = ReadFrames(t, g_decode.data(), n);
            t.cursor += got;
        }
        if (got > 0) {
            SDL_PutAudioStreamData(t.conv, g_decode.data(), (int)(got * t.channels * sizeof(float)));
            fed += (int)got;
            emptyReads = 0;
        }
        if (got < n || n == 0 || t.cursor >= end) {
            // reached the loop end (or the end of the file): continue from the loop start
            if (++emptyReads > 2 || !SeekFrame(t, t.loopStart)) return false;
        }
    }
    return true;
}

void Render(float* out, int frames, int outRate) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_open || !g_playing || !g_track) return;
    Track& t = *g_track;
    (void)outRate;
    const int bytes = frames * 2 * (int)sizeof(float);
    // the converter holds a little decoded audio; top it up until it can fill this block
    int guard = 0;
    while (SDL_GetAudioStreamAvailable(t.conv) < bytes && guard++ < 16) {
        int need = (bytes - SDL_GetAudioStreamAvailable(t.conv)) / (2 * (int)sizeof(float));
        int srcFrames = (int)((int64_t)need * t.rate / Audio::OutputRate()) + 64;
        if (!Feed(t, srcFrames)) {
            Log("Music: playback of %s stopped: the file could not be decoded any further", t.path.c_str());
            g_playing = false;
            return;
        }
    }
    g_out.resize((size_t)frames * 2);
    int got = SDL_GetAudioStreamData(t.conv, g_out.data(), bytes);
    int n = got > 0 ? got / (int)sizeof(float) : 0;
    for (int i = 0; i < n; i++) out[i] += g_out[(size_t)i];
}

std::string Lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string Clock(double secs) {
    char b[32];
    int m = (int)(secs / 60);
    snprintf(b, sizeof b, "%d:%05.2f", m, secs - m * 60);
    return b;
}

bool Exists(const std::string& p) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(p.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

// "music" folders, matched case-insensitively: next to Hopmon.exe, then next to this program.
std::vector<std::string> MusicDirs() {
    std::vector<std::string> parents{Assets::GameDir()};
    if (const char* b = SDL_GetBasePath())
        if (std::find(parents.begin(), parents.end(), std::string(b)) == parents.end()) parents.push_back(b);
    std::vector<std::string> out;
    for (const auto& parent : parents) {
        if (parent.empty()) continue;
        int n = 0;
        char** names = SDL_GlobDirectory(parent.c_str(), "music", SDL_GLOB_CASEINSENSITIVE, &n);
        for (int i = 0; i < n; i++) {
            std::string d = parent + names[i];
            SDL_PathInfo info;
            if (SDL_GetPathInfo(d.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY) out.push_back(d + "/");
        }
        SDL_free(names);
    }
    return out;
}

}  // namespace

namespace MusicStream {

bool Init() {
    Audio::SetStreamRenderer(Render);
    return true;
}

void Shutdown() {
    Audio::SetStreamRenderer(nullptr);
    Close();
}

bool Open(const std::string& path) {
    Close();
    uint64_t t0 = SDL_GetTicks();
    // Everything slow (loading, scanning the MP3 for a seek table) happens here, outside the
    // lock, so the mixer and sound effects never wait on it.
    auto tp = std::make_unique<Track>();
    Track& t = *tp;
    t.path = path;
    t.fileData = SDL_LoadFile(path.c_str(), &t.fileSize);
    if (!t.fileData) {
        Log("Music: cannot read %s (%s)", path.c_str(), SDL_GetError());
        return false;
    }
    std::string ext = Lower(path.substr(path.find_last_of('.') + 1));
    const uint8_t* head = (const uint8_t*)t.fileData;
    bool isFlac = t.fileSize >= 4 && memcmp(head, "fLaC", 4) == 0;
    LoopTags tags;
    if (isFlac || ext == "flac") {
        t.flac = drflac_open_memory_with_metadata(t.fileData, t.fileSize, OnFlacMeta, &tags, nullptr);
        if (t.flac) {
            t.kind = Kind::FLAC;
            t.channels = (int)t.flac->channels;
            t.rate = (int)t.flac->sampleRate;
            t.total = t.flac->totalPCMFrameCount;
        }
    } else if (ext == "mp3") {
        if (drmp3_init_memory_with_metadata(&t.mp3, t.fileData, t.fileSize, OnMp3Meta, &tags, nullptr)) {
            t.kind = Kind::MP3;
            t.channels = (int)t.mp3.channels;
            t.rate = (int)t.mp3.sampleRate;
            // a seek table makes jumping back to a loop start instant
            drmp3_uint32 count = 256;
            t.seekPoints.resize(count);
            if (drmp3_calculate_seek_points(&t.mp3, &count, t.seekPoints.data()) && count > 0) {
                t.seekPoints.resize(count);
                drmp3_bind_seek_table(&t.mp3, count, t.seekPoints.data());
            } else {
                t.seekPoints.clear();
            }
            t.total = drmp3_get_pcm_frame_count(&t.mp3);
        }
    }
    if (t.kind == Kind::None || t.channels < 1 || t.rate < 1000) {
        Log("Music: %s is not a playable %s file", path.c_str(), ext == "flac" || isFlac ? "FLAC" : "MP3");
        FreeTrack(tp);
        return false;
    }
    // loop points from tags, checked against the file length
    if (tags.start >= 0) {
        uint64_t s = (uint64_t)tags.start, e = 0;
        if (tags.length > 0) e = s + (uint64_t)tags.length;
        else if (tags.end > 0) e = (uint64_t)tags.end;
        if (t.total && e > t.total) e = t.total;
        if (t.total && s >= t.total) s = 0;
        if (e && e <= s) e = 0;
        t.loopStart = s;
        t.loopEnd = e;
    }
    SDL_AudioSpec src{SDL_AUDIO_F32, t.channels, t.rate};
    SDL_AudioSpec dst{SDL_AUDIO_F32, 2, Audio::OutputRate()};
    t.conv = SDL_CreateAudioStream(&src, &dst);
    // make sure the start of the file really decodes
    std::vector<float> probe((size_t)1152 * t.channels);
    if (!t.conv || ReadFrames(t, probe.data(), 1152) == 0 || !SeekFrame(t, 0)) {
        Log("Music: %s could not be decoded", path.c_str());
        FreeTrack(tp);
        return false;
    }
    t.loadMs = (int)(SDL_GetTicks() - t0);
    std::unique_ptr<Track> old;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        old = std::move(g_track);
        g_track = std::move(tp);
        g_open = true;
        g_playing = false;
    }
    FreeTrack(old);
    return true;
}

void PlayFromStart() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_open || !g_track) return;
    SeekFrame(*g_track, 0);
    SDL_ClearAudioStream(g_track->conv);
    g_playing = true;
}

void Pause() {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_playing = false;
}

void Resume() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_open) g_playing = true;
}

void Close() {
    std::unique_ptr<Track> old;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_playing = false;
        g_open = false;
        old = std::move(g_track);
    }
    FreeTrack(old);
}

bool IsOpen() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_open;
}

std::string Describe() {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (!g_open || !g_track) return "";
    const Track& t = *g_track;
    std::string s = t.kind == Kind::FLAC ? "FLAC" : "MP3";
    s += ", " + std::to_string(t.rate) + " Hz, " +
         (t.channels == 1 ? std::string("mono") : t.channels == 2 ? std::string("stereo")
                                                                   : std::to_string(t.channels) + " channels");
    if (t.total) s += ", " + Clock((double)t.total / t.rate);
    if (t.loopStart || t.loopEnd)
        s += ", loop " + Clock((double)t.loopStart / t.rate) + "-" +
             (t.loopEnd ? Clock((double)t.loopEnd / t.rate) : std::string("end"));
    s += ", loaded in " + std::to_string(t.loadMs) + " ms";
    return s;
}

std::string FindReplacement(int index, const std::string& midiName) {
    const Config& cfg = GetConfig();
    if (cfg.musicSource == 1) return "";  // MIDI only
    std::string chosen = index >= 0 && index < 4 ? cfg.musicFiles[index] : std::string();
    auto dirs = MusicDirs();
    if (!chosen.empty()) {
        // a file name inside the music folder (an absolute path also works)
        bool absolute = chosen[0] == '/' || chosen[0] == '\\' || (chosen.size() > 1 && chosen[1] == ':');
        if (absolute) {
            if (Exists(chosen)) return chosen;
        } else {
            for (const auto& d : dirs)
                if (Exists(d + chosen)) return d + chosen;
        }
    }
    std::string stem = Lower(midiName.substr(0, midiName.find_last_of('.')));
    if (!chosen.empty())
        Log("Music: music_%d = %s was not found in the music folder; looking for %s.flac / %s.mp3 instead",
            index + 1, chosen.c_str(), midiName.substr(0, midiName.find_last_of('.')).c_str(),
            midiName.substr(0, midiName.find_last_of('.')).c_str());
    for (const auto& d : dirs) {
        int n = 0;
        char** names = SDL_GlobDirectory(d.c_str(), "*", SDL_GLOB_CASEINSENSITIVE, &n);
        std::string flac, mp3;
        for (int i = 0; i < n; i++) {
            std::string l = Lower(names[i]);
            if (l == stem + ".flac") flac = d + names[i];
            else if (l == stem + ".mp3") mp3 = d + names[i];
        }
        SDL_free(names);
        if (!flac.empty()) return flac;  // FLAC preferred when both exist
        if (!mp3.empty()) return mp3;
    }
    return "";
}

}  // namespace MusicStream
