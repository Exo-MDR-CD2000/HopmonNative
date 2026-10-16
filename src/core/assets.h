// Game data access. Every asset comes from the user's own copy of Hopmon.exe
// (and the Music*.mid files shipped next to it); nothing is bundled with the port.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "pe_resources.h"

struct Image {
    int width = 0, height = 0;
    int bpp = 0;                    // bit depth of the source DIB
    std::vector<uint32_t> pixels;   // top-down, 0xAARRGGBB, alpha = 0xFF
    // Native (mods): a PNG with real transparency uses its alpha instead of a colour key, and a
    // replacement image is laid out at the original's size (so an HD image just looks sharper).
    bool hasAlpha = false;
    int logicalWidth = 0, logicalHeight = 0;  // 0 = width / height
    int LogicalWidth() const { return logicalWidth ? logicalWidth : width; }
    int LogicalHeight() const { return logicalHeight ? logicalHeight : height; }
};

namespace Assets {

// Locate and open Hopmon.exe. `explicitPath` may be empty.
// Returns false and fills `error` if no acceptable executable is found.
bool Init(const std::string& explicitPath, bool allowUnverified, std::string* error);
void Shutdown();

const std::string& GameDir();             // folder containing Hopmon.exe (with trailing separator)
const std::string& ExePath();
bool IsVerified();                        // hash matched a known release

const std::vector<uint8_t>* Bitmap(uint32_t id);   // raw DIB (BITMAPINFOHEADER + palette + bits)
const std::vector<uint8_t>* RCData(uint32_t id);
const std::vector<uint8_t>* Wave(uint32_t id);
std::string RCDataCString(uint32_t id);

// Images, sounds and models check enabled mods first (see core/mods.h), then Hopmon.exe.
bool LoadBitmapImage(uint32_t id, Image* out);
bool LoadOriginalBitmapImage(uint32_t id, Image* out);  // never from a mod (asset export)
bool DecodeDIB(const uint8_t* data, size_t size, Image* out);
// PNG / BMP / JPEG / TGA file into an Image (any size).
bool DecodeImageFile(const std::string& path, Image* out);
bool DecodeImageMemory(const uint8_t* data, size_t size, Image* out);

// A mod's replacement for sound resource `id`, decoded to float PCM (interleaved).
bool LoadModSound(uint32_t id, std::vector<float>* pcm, int* channels, int* rate);

bool ReadFile(const std::string& path, std::vector<uint8_t>* out);
std::string ResolveGameFile(const std::string& name);  // GameDir()+name

}  // namespace Assets
