#include "export_assets.h"
#include "assets.h"
#include "log.h"
#include "mods.h"
#include "../framework/d3dfile.h"
#include "../../third_party/miniz.h"
#include <SDL3/SDL.h>
#include <cstdio>

namespace ExportAssets {
namespace {

bool Write(const std::string& path, const void* data, size_t size) {
    if (SDL_SaveFile(path.c_str(), data, size)) return true;
    Log("Export: could not write %s (%s)", path.c_str(), SDL_GetError());
    return false;
}

bool WritePng(const std::string& path, const Image& img) {
    std::vector<uint8_t> rgba((size_t)img.width * img.height * 4);
    for (size_t i = 0; i < img.pixels.size(); i++) {
        uint32_t c = img.pixels[i];
        rgba[i * 4] = (uint8_t)(c >> 16);
        rgba[i * 4 + 1] = (uint8_t)(c >> 8);
        rgba[i * 4 + 2] = (uint8_t)c;
        rgba[i * 4 + 3] = (uint8_t)(c >> 24);
    }
    size_t len = 0;
    void* png = tdefl_write_image_to_png_file_in_memory_ex(rgba.data(), img.width, img.height, 4, &len, 6, MZ_FALSE);
    if (!png) return false;
    bool ok = Write(path, png, len);
    mz_free(png);
    return ok;
}

}  // namespace

int Run(const std::string& folder) {
    int files = 0;
    for (const char* sub : {"", "textures", "models", "sounds", "levels"}) SDL_CreateDirectory((folder + sub).c_str());

    for (auto& e : Mods::ImageNames()) {
        Image img;
        if (Assets::LoadOriginalBitmapImage(e.first, &img) && WritePng(folder + "textures/" + e.second + ".png", img)) files++;
    }
    for (auto& e : Mods::ModelNames()) {
        if (const auto* x = Assets::RCData(e.first)) {
            if (Write(folder + "models/" + e.second + ".x", x->data(), x->size())) files++;
            std::vector<uint8_t> glb;
            if (CD3DFile::ExportOriginalGlb((WORD)e.first, &glb) && Write(folder + "models/" + e.second + ".glb", glb.data(), glb.size()))
                files++;
        }
    }
    for (auto& e : Mods::SoundNames())
        if (const auto* w = Assets::Wave(e.first))
            if (Write(folder + "sounds/" + e.second + ".wav", w->data(), w->size())) files++;
    for (auto& e : Mods::LevelNames()) {
        std::string raw = Assets::RCDataCString(e.first);
        // the original resources end the grid with a line of '_'; keep just the grid
        std::string grid;
        size_t pos = 0;
        while (pos < raw.size()) {
            size_t nl = raw.find('\n', pos);
            std::string l = raw.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            pos = nl == std::string::npos ? raw.size() : nl + 1;
            if (!l.empty() && l.back() == '\r') l.pop_back();
            if (l.empty() || l[0] == '_') break;
            grid += l + "\n";
        }
        std::string text = "[level]\nname = " + std::string(e.second) + "\n\n[map]\n" + grid;
        if (Write(folder + "levels/" + e.second + ".txt", text.data(), text.size())) files++;
    }
    std::string readme =
        "These files were exported from YOUR copy of Hopmon (Hopmon.exe) by Hopmon Native.\n"
        "They belong to Saito Games: use them as a starting point for your own work, but don't\n"
        "share them or include them (or edited copies of them) in a mod.\n\n"
        "Each file has the name a mod uses to replace it. See MODDING.md for how mods work.\n"
        "  textures/  every image, as PNG (the original colour-key colours are kept as they are)\n"
        "  models/    every model, as the original .x and as .glb (opens in Blender)\n"
        "  sounds/    every sound effect, as WAV\n"
        "  levels/    all 45 levels as text, in the level file format\n"
        "The music is in the Music01.mid - Music04.mid files next to Hopmon.exe.\n";
    if (Write(folder + "README.txt", readme.data(), readme.size())) files++;
    Log("Export: wrote %d files to %s", files, folder.c_str());
    return files;
}

}  // namespace ExportAssets
