#include "assets.h"
#include "sha256.h"
#include "log.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>

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
    FILE* f = fopen(p.c_str(), "rb");
    if (f) { fclose(f); return true; }
    return false;
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
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out->resize(n > 0 ? (size_t)n : 0);
    bool ok = n <= 0 || fread(out->data(), 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    return ok;
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

bool LoadBitmapImage(uint32_t id, Image* out) {
    const auto* b = Bitmap(id);
    if (!b) return false;
    return DecodeDIB(b->data(), b->size(), out);
}

}  // namespace Assets
