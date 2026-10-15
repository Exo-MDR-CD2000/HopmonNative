#include "audio.h"
#include "../core/log.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <mutex>
#include <vector>

namespace {
struct Buffer {
    bool used = false;
    std::vector<uint8_t> data;
    int rate = 22050;
    double pos = 0;
    bool playing = false;
    bool loop = false;
    float gain = 1.f;
};

SDL_AudioStream* g_stream = nullptr;
std::mutex g_mutex;
std::vector<Buffer> g_buffers;
std::function<void(float*, int, int)> g_music, g_streamMusic;
int g_rate = 44100;
float g_master = 1.f, g_sfx = 1.f, g_musicVol = 1.f;
std::vector<float> g_mix, g_musicBuf;

void SDLCALL Callback(void*, SDL_AudioStream* stream, int additional, int) {
    int frames = additional / (int)(sizeof(float) * 2);
    if (frames <= 0) return;
    g_mix.assign((size_t)frames * 2, 0.f);
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        for (auto& b : g_buffers) {
            if (!b.used || !b.playing || b.data.empty()) continue;
            double step = (double)b.rate / g_rate;
            float g = b.gain * g_sfx;
            size_t n = b.data.size();
            for (int i = 0; i < frames; i++) {
                size_t ip = (size_t)b.pos;
                if (ip >= n) {
                    if (b.loop) {
                        b.pos -= (double)n;
                        ip = (size_t)b.pos;
                    } else {
                        b.playing = false;
                        b.pos = 0;  // DirectSound leaves the play cursor at the end; game always rewinds first
                        break;
                    }
                }
                // linear interpolation between 8-bit samples
                size_t ip2 = ip + 1 < n ? ip + 1 : (b.loop ? 0 : ip);
                float f = (float)(b.pos - (double)ip);
                float s0 = (b.data[ip] - 128) / 128.f, s1 = (b.data[ip2] - 128) / 128.f;
                float s = (s0 + (s1 - s0) * f) * g;
                g_mix[i * 2] += s;
                g_mix[i * 2 + 1] += s;
                b.pos += step;
            }
        }
        if (g_music || g_streamMusic) {
            g_musicBuf.assign((size_t)frames * 2, 0.f);
            if (g_music) g_music(g_musicBuf.data(), frames, g_rate);
            if (g_streamMusic) g_streamMusic(g_musicBuf.data(), frames, g_rate);
            for (size_t i = 0; i < g_mix.size(); i++) g_mix[i] += g_musicBuf[i] * g_musicVol;
        }
    }
    for (auto& s : g_mix) {
        s *= g_master;
        if (s > 1.f) s = 1.f;
        if (s < -1.f) s = -1.f;
    }
    SDL_PutAudioStreamData(stream, g_mix.data(), frames * (int)sizeof(float) * 2);
}
}  // namespace

namespace Audio {

bool Init() {
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 44100};
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Callback, nullptr);
    if (!g_stream) {
        Log("Audio: could not open device: %s", SDL_GetError());
        return false;
    }
    g_rate = spec.freq;
    SDL_ResumeAudioStreamDevice(g_stream);
    Log("Audio: %d Hz stereo", g_rate);
    return true;
}

void Shutdown() {
    if (g_stream) SDL_DestroyAudioStream(g_stream);
    g_stream = nullptr;
    std::lock_guard<std::mutex> lk(g_mutex);
    g_buffers.clear();
    g_music = nullptr;
}

int CreateBuffer(const uint8_t* pcm, size_t length, int sampleRate) {
    std::lock_guard<std::mutex> lk(g_mutex);
    size_t i = 0;
    for (; i < g_buffers.size(); i++)
        if (!g_buffers[i].used) break;
    if (i == g_buffers.size()) g_buffers.emplace_back();
    Buffer& b = g_buffers[i];
    b = Buffer();
    b.used = true;
    b.data.assign(pcm, pcm + length);
    b.rate = sampleRate;
    return (int)i;
}

void DestroyBuffer(int h) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (h >= 0 && h < (int)g_buffers.size()) g_buffers[h] = Buffer();
}

void Play(int h, bool loop) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (h < 0 || h >= (int)g_buffers.size()) return;
    g_buffers[h].playing = true;
    g_buffers[h].loop = loop;
}

void Stop(int h) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (h >= 0 && h < (int)g_buffers.size()) g_buffers[h].playing = false;
}

void SetPosition(int h, size_t pos) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (h >= 0 && h < (int)g_buffers.size()) g_buffers[h].pos = (double)pos;
}

void SetVolume(int h, long mB) {
    if (mB > 0) mB = 0;
    if (mB < -10000) mB = -10000;
    float gain = mB <= -10000 ? 0.f : powf(10.f, (float)mB / 2000.f);
    std::lock_guard<std::mutex> lk(g_mutex);
    if (h >= 0 && h < (int)g_buffers.size()) g_buffers[h].gain = gain;
}

bool IsPlaying(int h) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return h >= 0 && h < (int)g_buffers.size() && g_buffers[h].playing;
}

void SetMusicRenderer(std::function<void(float*, int, int)> fn) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_music = std::move(fn);
}

void SetStreamRenderer(std::function<void(float*, int, int)> fn) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_streamMusic = std::move(fn);
}

int OutputRate() { return g_rate; }

void SetMasterVolumes(float master, float sfx, float music) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_master = master;
    g_sfx = sfx;
    g_musicVol = music;
}

}  // namespace Audio
