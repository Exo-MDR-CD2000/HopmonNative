#include "assets.h"
#include "mods.h"
#include "sha256.h"
#include "log.h"
#include "../../third_party/stb_image.h"
#define STB_VORBIS_HEADER_ONLY
#include "../../third_party/stb_vorbis.c"
#include "../../third_party/dr_flac.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace {
PEResources g_res;
std::string g_gameDir, g_exePath;
bool g_verified = false;

// Known retail/freeware builds of Hopmon.exe (SHA-256).
struct KnownBuild { const char* sha256; const char* label; };
const KnownBuild kKnownBuilds[] = {
    {"6f4acb933e1692d97cc18819204c605287f72a71b9f62e1210ff8908be85930b", "Hopmon (2022 rebuild, freeware)"},
};

bool FileExists(const std::string& p) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(p.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

std::string DirOf(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? std::string("./") : p.substr(0, s + 1);
}

uint32_t rd32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
}  // namespace

namespace Assets {

bool ReadFile(const std::string& path, std::vector<uint8_t>* out) {
    // SDL takes UTF-8 paths on every system (fopen can't open non-ASCII paths on Windows)
    size_t n = 0;
    void* d = SDL_LoadFile(path.c_str(), &n);
    if (!d) return false;
    out->assign((const uint8_t*)d, (const uint8_t*)d + n);
    SDL_free(d);
    return true;
}

bool Init(const std::string& explicitPath, bool allowUnverified, std::string* error) {
    std::vector<std::string> candidates;
    if (!explicitPath.empty()) {
        candidates.push_back(explicitPath);
        candidates.push_back(explicitPath + "/Hopmon.exe");
    }
    const char* base = SDL_GetBasePath();
    std::string baseDir = base ? base : "./";
    for (const char* name : {"Hopmon.exe", "hopmon.exe", "HOPMON.EXE"}) {
        candidates.push_back(baseDir + name);
        candidates.push_back(baseDir + "../" + name);
        candidates.push_back(std::string("./") + name);
    }
    std::string found;
    for (auto& c : candidates)
        if (FileExists(c)) { found = c; break; }
    if (found.empty()) {
        if (error)
            *error = "Hopmon.exe was not found.\n\nPlace this program in the same folder as your original "
                     "Hopmon.exe, or pass --game \"path\\to\\Hopmon.exe\".";
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!ReadFile(found, &bytes)) {
        if (error) *error = "Could not read " + found;
        return false;
    }
    std::string hash = Sha256Hex(bytes.data(), bytes.size());
    g_verified = false;
    for (auto& k : kKnownBuilds)
        if (hash == k.sha256) {
            g_verified = true;
            Log("Found %s at %s", k.label, found.c_str());
        }
    if (!g_verified) {
        if (!allowUnverified) {
            if (error)
                *error = "The file " + found + " is not a recognised Hopmon release (SHA-256 " + hash +
                         ").\n\nThis port only runs with an original copy of Hopmon. If you are sure this "
                         "is a genuine version, start with --allow-unverified.";
            return false;
        }
        Log("WARNING: unrecognised Hopmon.exe (sha256 %s); continuing because --allow-unverified was given", hash.c_str());
    }
    std::string err;
    if (!g_res.LoadFromMemory(std::move(bytes), &err)) {
        if (error) *error = "Could not read resources from " + found + ": " + err;
        return false;
    }
    if (!g_res.Find(RT_RCDATA_, 148) || !g_res.Find(RT_BITMAP_, 156) || !g_res.Find("WAVE", 232)) {
        if (error) *error = found + " does not contain the Hopmon game data.";
        return false;
    }
    g_exePath = found;
    g_gameDir = DirOf(found);
    Log("Loaded %zu resources", g_res.Count());
    return true;
}

void Shutdown() { g_res = PEResources(); }

const std::string& GameDir() { return g_gameDir; }
const std::string& ExePath() { return g_exePath; }
bool IsVerified() { return g_verified; }
const std::vector<uint8_t>* Bitmap(uint32_t id) { return g_res.Find(RT_BITMAP_, id); }
const std::vector<uint8_t>* RCData(uint32_t id) { return g_res.Find(RT_RCDATA_, id); }
const std::vector<uint8_t>* Wave(uint32_t id) { return g_res.Find("WAVE", id); }
std::string RCDataCString(uint32_t id) { return g_res.FindCString(RT_RCDATA_, id); }
std::string ResolveGameFile(const std::string& name) { return g_gameDir + name; }

bool DecodeDIB(const uint8_t* d, size_t size, Image* out) {
    if (size < 40) return false;
    uint32_t hdr = rd32(d);
    int32_t w = (int32_t)rd32(d + 4), h = (int32_t)rd32(d + 8);
    uint16_t bpp = rd16(d + 14);
    uint32_t comp = rd32(d + 16);
    uint32_t clrUsed = rd32(d + 32);
    if (comp != 0 && comp != 3) return false;  // BI_RGB / BI_BITFIELDS only
    bool topDown = h < 0;
    if (h < 0) h = -h;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return false;
    uint32_t palCount = bpp <= 8 ? (clrUsed ? clrUsed : (1u << bpp)) : 0;
    const uint8_t* pal = d + hdr;
    size_t bitsOff = hdr + palCount * 4 + (comp == 3 ? 12 : 0);
    size_t stride = ((size_t)w * bpp + 31) / 32 * 4;
    if (bitsOff + stride * h > size) return false;
    out->width = w;
    out->height = h;
    out->bpp = bpp;
    out->pixels.assign((size_t)w * h, 0xFF000000u);
    for (int y = 0; y < h; y++) {
        const uint8_t* row = d + bitsOff + stride * (topDown ? y : (h - 1 - y));
        uint32_t* dst = &out->pixels[(size_t)y * w];
        for (int x = 0; x < w; x++) {
            uint32_t idx = 0, r = 0, g = 0, b = 0;
            switch (bpp) {
                case 1: idx = (row[x >> 3] >> (7 - (x & 7))) & 1; break;
                case 4: idx = (row[x >> 1] >> ((x & 1) ? 0 : 4)) & 15; break;
                case 8: idx = row[x]; break;
                case 16: {
                    uint16_t v = rd16(row + x * 2);
                    r = ((v >> 10) & 31) * 255 / 31; g = ((v >> 5) & 31) * 255 / 31; b = (v & 31) * 255 / 31;
                    break;
                }
                case 24: b = row[x * 3]; g = row[x * 3 + 1]; r = row[x * 3 + 2]; break;
                case 32: b = row[x * 4]; g = row[x * 4 + 1]; r = row[x * 4 + 2]; break;
                default: return false;
            }
            if (bpp <= 8) {
                if (idx < palCount) { b = pal[idx * 4]; g = pal[idx * 4 + 1]; r = pal[idx * 4 + 2]; }
            }
            dst[x] = 0xFF000000u | r << 16 | g << 8 | b;
        }
    }
    return true;
}

bool LoadOriginalBitmapImage(uint32_t id, Image* out) {
    const auto* b = Bitmap(id);
    if (!b) return false;
    return DecodeDIB(b->data(), b->size(), out);
}

bool DecodeImageMemory(const uint8_t* data, size_t size, Image* out) {
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!px) return false;
    out->width = w;
    out->height = h;
    out->bpp = 32;
    out->pixels.resize((size_t)w * h);
    out->hasAlpha = false;
    for (size_t i = 0; i < out->pixels.size(); i++) {
        const unsigned char* p = px + i * 4;
        out->pixels[i] = (uint32_t)p[3] << 24 | (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
        if (p[3] != 0xFF) out->hasAlpha = true;
    }
    stbi_image_free(px);
    return true;
}

bool DecodeImageFile(const std::string& path, Image* out) {
    std::vector<uint8_t> bytes;
    if (!ReadFile(path, &bytes)) return false;
    return DecodeImageMemory(bytes.data(), bytes.size(), out);
}

bool LoadBitmapImage(uint32_t id, Image* out) {
    if (Mods::IsDynamic(id)) {
        std::string path = Mods::DynamicPath(id);
        if (DecodeImageFile(path, out)) return true;
        Log("Mods: could not read image %s", path.c_str());
        return false;
    }
    const Mods::ModInfo* from = nullptr;
    std::string path = Mods::FindFile("textures", Mods::ImageName(id), {"png", "bmp", "tga", "jpg"}, &from);
    if (!path.empty()) {
        Image img;
        Image orig;
        bool haveOrig = LoadOriginalBitmapImage(id, &orig);
        if (DecodeImageFile(path, &img)) {
            if (haveOrig) {
                img.logicalWidth = orig.width;   // laid out at the original's size
                img.logicalHeight = orig.height;
            }
            Log("Mods: texture %s from %s (%dx%d)", Mods::ImageName(id), from ? from->id.c_str() : "?", img.width, img.height);
            *out = std::move(img);
            return true;
        }
        Log("Mods: could not read %s (not a PNG, BMP, TGA or JPEG?); using the original", path.c_str());
        if (haveOrig) {
            *out = std::move(orig);
            return true;
        }
    }
    return LoadOriginalBitmapImage(id, out);
}

// ------------------------------------------------------------------ mod sounds
namespace {
// WAV: PCM 8/16/24/32-bit or 32-bit float, any channel count (mixed down beyond stereo).
bool DecodeWav(const std::vector<uint8_t>& b, std::vector<float>* pcm, int* channels, int* rate) {
    if (b.size() < 12 || memcmp(b.data(), "RIFF", 4) || memcmp(b.data() + 8, "WAVE", 4)) return false;
    size_t p = 12;
    int fmt = 0, ch = 0, bits = 0;
    uint32_t sr = 0;
    const uint8_t* data = nullptr;
    size_t dataSize = 0;
    while (p + 8 <= b.size()) {
        uint32_t sz = rd32(&b[p + 4]);
        if (!memcmp(&b[p], "fmt ", 4) && p + 8 + 16 <= b.size()) {
            fmt = rd16(&b[p + 8]);
            ch = rd16(&b[p + 10]);
            sr = rd32(&b[p + 12]);
            bits = rd16(&b[p + 22]);
            if (fmt == 0xFFFE && sz >= 26 && p + 8 + 26 <= b.size()) fmt = rd16(&b[p + 32]);  // extensible
        } else if (!memcmp(&b[p], "data", 4)) {
            data = &b[p + 8];
            dataSize = std::min<size_t>(sz, b.size() - (p + 8));
        }
        p += 8 + ((size_t)sz + 1) / 2 * 2;
    }
    if (!data || ch <= 0 || !sr) return false;
    int bytes = bits / 8;
    if (!((fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && bits == 32))) return false;
    size_t frames = dataSize / ((size_t)bytes * ch);
    pcm->resize(frames * ch);
    for (size_t i = 0; i < frames * ch; i++) {
        const uint8_t* s = data + i * bytes;
        float v = 0;
        if (fmt == 3) {
            memcpy(&v, s, 4);
        } else if (bits == 8) {
            v = (s[0] - 128) / 128.f;
        } else if (bits == 16) {
            v = (int16_t)rd16(s) / 32768.f;
        } else if (bits == 24) {
            int32_t x = (int32_t)((uint32_t)s[0] << 8 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 24) >> 8;
            v = x / 8388608.f;
        } else {
            v = (int32_t)rd32(s) / 2147483648.f;
        }
        (*pcm)[i] = v;
    }
    *channels = ch;
    *rate = (int)sr;
    return true;
}
}  // namespace

bool LoadModSound(uint32_t id, std::vector<float>* pcm, int* channels, int* rate) {
    const Mods::ModInfo* from = nullptr;
    std::string path = Mods::FindFile("sounds", Mods::SoundName(id), {"wav", "ogg", "flac"}, &from);
    if (path.empty()) return false;
    std::vector<uint8_t> bytes;
    if (!ReadFile(path, &bytes)) return false;
    std::string ext = path.substr(path.find_last_of('.') + 1);
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    bool ok = false;
    if (ext == "wav") {
        ok = DecodeWav(bytes, pcm, channels, rate);
    } else if (ext == "ogg") {
        int ch = 0, sr = 0;
        short* out = nullptr;
        int frames = stb_vorbis_decode_memory(bytes.data(), (int)bytes.size(), &ch, &sr, &out);
        if (frames > 0 && out) {
            pcm->resize((size_t)frames * ch);
            for (size_t i = 0; i < pcm->size(); i++) (*pcm)[i] = out[i] / 32768.f;
            *channels = ch;
            *rate = sr;
            ok = true;
        }
        free(out);
    } else if (ext == "flac") {
        unsigned int ch = 0, sr = 0;
        drflac_uint64 frames = 0;
        float* out = drflac_open_memory_and_read_pcm_frames_f32(bytes.data(), bytes.size(), &ch, &sr, &frames, nullptr);
        if (out) {
            pcm->assign(out, out + frames * ch);
            *channels = (int)ch;
            *rate = (int)sr;
            ok = true;
            drflac_free(out, nullptr);
        }
    }
    if (ok && *channels > 2) {  // mix down to stereo
        int ch = *channels;
        size_t frames = pcm->size() / ch;
        std::vector<float> st(frames * 2);
        for (size_t f = 0; f < frames; f++) {
            st[f * 2] = (*pcm)[f * ch];
            st[f * 2 + 1] = (*pcm)[f * ch + 1];
        }
        pcm->swap(st);
        *channels = 2;
    }
    if (!ok) Log("Mods: could not decode %s; using the original sound", path.c_str());
    else if (pcm->empty()) ok = false;
    else Log("Mods: sound %s from %s", Mods::SoundName(id), from ? from->id.c_str() : "?");
    return ok;
}

}  // namespace Assets
