#include "mods.h"
#include "assets.h"
#include "config.h"
#include "log.h"
#include "version.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include "../../third_party/miniz.h"

namespace Mods {
namespace {

// ------------------------------------------------------------------ name tables
// Original resource ids -> readable names. Listed in MODDING.md; never renumber or rename,
// mods depend on them.
const std::vector<std::pair<uint32_t, const char*>> kImages = {
    {0x9c, "floor_castle"},       {0xfb, "floor_forest"},        {0xfc, "floor_toy"},
    {0xc1, "map_background"},     {0xa3, "shadow"},              {0xa1, "player_fire"},
    {0xa0, "fire_hit"},           {0xa2, "explosion"},           {0xa9, "cannon_shot"},
    {0xb6, "ufo_shot"},           {0xaa, "smoke_purple"},        {0xb7, "smoke_green"},
    {0xdc, "steam"},              {0x14c, "logo_background"},    {0x13c, "saito_logo"},
    {0xdb, "title_logo"},         {0xf8, "text_copyright"},      {0xf7, "text_unregistered"},
    {0xcd, "menu_start"},         {0xca, "menu_options"},        {0xc9, "menu_exit"},
    {0xcc, "menu_select_level"},  {0xc8, "menu_back"},           {0xd0, "menu_arrow_right"},
    {0xd1, "menu_arrow_left"},    {0xce, "level_arrow_right"},   {0xcf, "level_arrow_left"},
    {0x140, "level_lights"},      {0xb9, "hud_level"},           {0xbc, "hud_crystals"},
    {0xba, "digits"},             {0xbd, "hud_fire_ready"},      {0xc0, "hud_fire_lights"},
    {0xbf, "hud_fps"},            {0xbb, "digits_small"},        {0xc2, "fade_right"},
    {0xc3, "fade_left"},          {0x141, "register_info"},      {0x142, "screenshot_1"},
    {0x143, "screenshot_2"},      {0x144, "ending_congratulations"}, {0x145, "ending_staff"},
    {0xf6, "unused_banner"},
};
const std::vector<std::pair<uint32_t, const char*>> kModels = {
    {0x94, "hopmon"},        {0x95, "hopmon_2"},       {0x96, "hopmon_3"},     {0xab, "crystal"},
    {0x9d, "ghost"},         {0x149, "cube"},          {0xa4, "cube_2"},       {0xaf, "jellyfish"},
    {0xae, "jellyfish_2"},   {0xad, "ufo"},            {0x14a, "sesame"},      {0x14b, "sesame_2"},
    {0xd2, "sesame_3"},      {0xd4, "sesame_4"},       {0xa5, "needle_ball"},  {0xb3, "needle_ball_fast"},
    {0xb2, "needle_box"},    {0xb1, "needle_box_drop"}, {0xb4, "gate"},        {0xa6, "cannon_up"},
    {0xac, "cannon"},        {0xb0, "volcano"},        {0xa7, "pole"},         {0xa8, "tree"},
    {0x13b, "short_pole"},   {0xc7, "unused_model"},
};
const std::vector<std::pair<uint32_t, const char*>> kSounds = {
    {0xff, "hop"},              {0xee, "fire"},               {0xfe, "fire_ready"},
    {0xea, "fire_hit"},         {0xeb, "enemy_explode"},      {0xe8, "crystal_pickup"},
    {0xe9, "crystal_delivered"}, {0xf0, "level_complete"},    {0xef, "player_hit"},
    {0xf1, "player_vanish"},    {0xf3, "cannon"},             {0xf4, "cannon_up"},
    {0xf5, "needle_box"},       {0x13e, "steam"},             {0x13f, "ufo_fire"},
    {0xec, "menu_move"},        {0xed, "menu_select"},        {0x13d, "logo"},
};
// The original level order: castle, forest and toy world, 15 levels each.
const uint16_t kWorldLevels[3][15] = {
    {0x11c, 0x115, 0x116, 0x117, 0x118, 0x119, 0x11a, 0x11b, 0x114, 0x109, 0x10a, 0x10b, 0x10c, 0x10d, 0x10e},
    {0x11d, 0x11e, 0x11f, 0x120, 0x121, 0x122, 0x123, 0x124, 0x125, 0x126, 0x127, 0x128, 0x129, 0x12a, 0x12b},
    {0x12c, 0x12d, 0x12e, 0x12f, 0x130, 0x131, 0x132, 0x133, 0x134, 0x135, 0x136, 0x137, 0x138, 0x139, 0x13a},
};
const char* const kLookNames[3] = {"castle", "forest", "toy"};
const uint16_t kLookFloors[3] = {0x9c, 0xfb, 0xfc};
std::vector<std::pair<uint32_t, const char*>> g_levelNames;
std::vector<std::string> g_levelNameStore;

const char* Lookup(const std::vector<std::pair<uint32_t, const char*>>& t, uint32_t id) {
    for (auto& e : t)
        if (e.first == id) return e.second;
    return "";
}

// ------------------------------------------------------------------ state
std::vector<ModInfo> g_mods;
std::vector<Pack> g_packs;
size_t g_activePack = 0;
std::string g_activePackId = "original";   // the pack in play
std::string g_chosenPackId = "original";   // the pack picked in the menu (in play after the next Init)
std::string g_modsFolder;
std::vector<std::string> g_dynamic;   // id 0x8000 + i -> file path
// per mod: lower-case relative path -> actual relative path
std::map<std::string, std::map<std::string, std::string>> g_files;
bool g_initDone = false;

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
bool IsDir(const std::string& p) {
    SDL_PathInfo i;
    return SDL_GetPathInfo(p.c_str(), &i) && i.type == SDL_PATHTYPE_DIRECTORY;
}
bool IsFile(const std::string& p, SDL_PathInfo* info = nullptr) {
    SDL_PathInfo i;
    bool ok = SDL_GetPathInfo(p.c_str(), &i) && i.type == SDL_PATHTYPE_FILE;
    if (ok && info) *info = i;
    return ok;
}
bool ReadText(const std::string& path, std::string* out) {
    size_t n = 0;
    void* d = SDL_LoadFile(path.c_str(), &n);
    if (!d) return false;
    out->assign((const char*)d, n);
    SDL_free(d);
    return true;
}
std::vector<std::string> ListDir(const std::string& dir) {
    std::vector<std::string> names;
    SDL_EnumerateDirectory(
        dir.c_str(),
        [](void* ud, const char*, const char* name) -> SDL_EnumerationResult {
            ((std::vector<std::string>*)ud)->push_back(name);
            return SDL_ENUM_CONTINUE;
        },
        &names);
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return Lower(a) < Lower(b); });
    return names;
}
std::string UserFolder() {
    char* p = SDL_GetPrefPath("", "HopmonNative");
    std::string s = p ? p : "";
    SDL_free(p);
    return s;
}

// Dotted version compare: -1, 0, 1.
int CompareVersions(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i < a.size() || j < b.size()) {
        long x = 0, y = 0;
        while (i < a.size() && std::isdigit((unsigned char)a[i])) x = x * 10 + (a[i++] - '0');
        while (j < b.size() && std::isdigit((unsigned char)b[j])) y = y * 10 + (b[j++] - '0');
        if (x != y) return x < y ? -1 : 1;
        while (i < a.size() && !std::isdigit((unsigned char)a[i])) i++;
        while (j < b.size() && !std::isdigit((unsigned char)b[j])) j++;
    }
    return 0;
}

std::string SafeId(const std::string& s) {
    std::string out;
    for (char c : Lower(s)) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') out += c;
        else if (c == ' ') out += '-';
    }
    while (!out.empty() && out[0] == '.') out.erase(0, 1);
    if (out.size() > 64) out.resize(64);
    return out;
}

// ------------------------------------------------------------------ zip mods
// A zip is unpacked once into <user folder>/modcache/<name>/ and reused until the zip changes.
// Entries that would land outside that folder (absolute paths, "..") are refused.
const uint64_t kMaxUnpacked = 1024ull * 1024 * 1024;  // 1 GB per mod
bool SafeZipPath(const std::string& p) {
    if (p.empty() || p[0] == '/' || p[0] == '\\' || (p.size() > 1 && p[1] == ':')) return false;
    size_t start = 0;
    while (start <= p.size()) {
        size_t end = p.find_first_of("/\\", start);
        std::string part = p.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}
std::string UnpackZip(const std::string& zipPath, const std::string& name, std::string* problem) {
    std::string user = UserFolder();
    if (user.empty()) {
        *problem = "no folder to unpack zipped mods into";
        return "";
    }
    std::string dest = user + "modcache/" + SafeId(name) + "/";
    SDL_PathInfo zi;
    IsFile(zipPath, &zi);
    std::string stamp = zipPath + "\n" + std::to_string((unsigned long long)zi.size) + "\n" +
                        std::to_string((long long)zi.modify_time) + "\n";
    std::string old;
    if (ReadText(dest + ".unpacked", &old) && old == stamp) return dest;  // already unpacked
    // start clean: remove an older unpack of this zip
    std::function<void(const std::string&)> wipe = [&](const std::string& dir) {
        for (auto& n : ListDir(dir)) {
            std::string p = dir + n;
            if (IsDir(p)) wipe(p + "/");
            SDL_RemovePath(p.c_str());
        }
    };
    if (IsDir(dest)) wipe(dest);
    SDL_CreateDirectory(dest.c_str());

    size_t zsize = 0;
    void* zdata = SDL_LoadFile(zipPath.c_str(), &zsize);
    if (!zdata) {
        *problem = "the zip can't be read";
        return "";
    }
    mz_zip_archive zip;
    memset(&zip, 0, sizeof zip);
    if (!mz_zip_reader_init_mem(&zip, zdata, zsize, 0)) {
        SDL_free(zdata);
        *problem = "not a valid zip file";
        return "";
    }
    uint64_t total = 0;
    int files = 0;
    bool ok = true;
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip) && ok; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
        std::string p = st.m_filename;
        for (char& c : p)
            if (c == '\\') c = '/';
        if (!SafeZipPath(p)) {
            Log("Mods: %s: skipped unsafe path '%s'", zipPath.c_str(), st.m_filename);
            continue;
        }
        if (st.m_is_directory) {
            SDL_CreateDirectory((dest + p).c_str());
            continue;
        }
        total += st.m_uncomp_size;
        if (total > kMaxUnpacked) {
            *problem = "the zip unpacks to more than 1 GB";
            ok = false;
            break;
        }
        size_t slash = p.find_last_of('/');
        if (slash != std::string::npos) SDL_CreateDirectory((dest + p.substr(0, slash)).c_str());
        size_t outSize = 0;
        void* out = mz_zip_reader_extract_to_heap(&zip, i, &outSize, 0);
        if (!out) {
            *problem = "a file in the zip is damaged (" + p + ")";
            ok = false;
            break;
        }
        if (!SDL_SaveFile((dest + p).c_str(), out, outSize)) ok = false;
        mz_free(out);
        files++;
    }
    mz_zip_reader_end(&zip);
    SDL_free(zdata);
    if (!ok) {
        if (problem->empty()) *problem = "could not unpack the zip";
        return "";
    }
    SDL_SaveFile((dest + ".unpacked").c_str(), stamp.data(), stamp.size());
    Log("Mods: unpacked %s (%d file%s)", zipPath.c_str(), files, files == 1 ? "" : "s");
    return dest;
}

// mod.ini at the top, or inside a single top-level folder (how most zips are made)
std::string FindModRoot(const std::string& dir) {
    if (IsFile(dir + "mod.ini")) return dir;
    std::string only;
    int count = 0;
    for (auto& n : ListDir(dir)) {
        if (n[0] == '.') continue;
        count++;
        if (IsDir(dir + n)) only = dir + n + "/";
    }
    if (count == 1 && !only.empty() && IsFile(only + "mod.ini")) return only;
    return "";
}

void IndexFiles(const std::string& root, const std::string& rel, std::map<std::string, std::string>& out, int depth = 0) {
    if (depth > 6 || out.size() > 20000) return;
    for (auto& n : ListDir(root + rel)) {
        std::string r = rel + n;
        if (IsDir(root + r)) IndexFiles(root, r + "/", out, depth + 1);
        else out[Lower(r)] = r;
    }
}

uint32_t AddDynamic(const std::string& path) {
    for (size_t i = 0; i < g_dynamic.size(); i++)
        if (g_dynamic[i] == path) return 0x8000 + (uint32_t)i;
    if (g_dynamic.size() >= 0x7000) return 0;
    g_dynamic.push_back(path);
    return 0x8000 + (uint32_t)(g_dynamic.size() - 1);
}

// ------------------------------------------------------------------ level files
// A level file is either a bare grid (the original format) or an ini header followed by a
// [map] line and the grid. The grid ends at the end of the file, an empty line, or a line
// starting with '_' (like the original resources).
std::string GridFromLevelFile(const std::string& text, Ini* header) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string l = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    size_t start = 0;
    for (size_t i = 0; i < lines.size(); i++)
        if (Lower(Trim(lines[i])) == "[map]") {
            std::string head;
            for (size_t k = 0; k < i; k++) head += lines[k] + "\n";
            if (header) ParseIni(head, header);
            start = i + 1;
            break;
        }
    std::string grid;
    bool started = false;
    for (size_t i = start; i < lines.size(); i++) {
        std::string l = lines[i];
        while (!l.empty() && (l.back() == ' ' || l.back() == '\t')) l.pop_back();
        if (l.empty()) {
            if (started) break;
            continue;
        }
        if (l[0] == '_') break;
        for (char& c : l)
            if (c == ' ' || c == '\t') c = '.';
        grid += l + "\n";
        started = true;
    }
    return grid;
}

void BuildOriginalPack() {
    Pack p;
    p.id = "original";
    p.name = "Hopmon (original game)";
    p.author = "Saito Games";
    p.version = "1";
    for (int w = 0; w < 3; w++) {
        World world;
        world.name = kLookNames[w];
        world.look = w;
        world.floorTexture = kLookFloors[w];
        p.worlds.push_back(world);
        for (int l = 0; l < 15; l++) {
            p.levels.push_back(kWorldLevels[w][l]);
            p.levelWorld.push_back(w);
            p.levelNames.push_back(LevelName(kWorldLevels[w][l]));
        }
    }
    g_packs.push_back(p);
}

int LookIndex(const std::string& v) {
    std::string l = Lower(Trim(v));
    for (int i = 0; i < 3; i++)
        if (l == kLookNames[i]) return i;
    return -1;
}

bool BuildModPack(const ModInfo& m) {
    const Ini& ini = m.ini;
    Pack p;
    p.id = m.id;
    p.name = ini.Get("pack", "name", m.name);
    p.author = m.author;
    p.version = ini.Get("pack", "version", m.version.empty() ? "1" : m.version);
    auto& files = g_files[m.id];
    for (auto& sec : ini.sections) {
        std::string sname = Lower(sec.first);
        if (sname.rfind("world", 0) != 0) continue;
        World w;
        w.name = ini.Get(sec.first, "name", sec.first);
        std::string look = ini.Get(sec.first, "look", "castle");
        w.look = LookIndex(look);
        if (w.look < 0) {
            Log("Mods: %s [%s]: unknown look '%s' (castle, forest or toy); using castle", m.id.c_str(),
                sec.first.c_str(), look.c_str());
            w.look = 0;
        }
        w.floorTexture = kLookFloors[w.look];
        std::string floor = Trim(ini.Get(sec.first, "floor"));
        if (!floor.empty()) {
            bool found = false;
            for (auto& e : kImages)
                if (Lower(floor) == e.second) {
                    w.floorTexture = (uint16_t)e.first;
                    found = true;
                }
            if (!found) {
                for (const char* ext : {".png", ".bmp"}) {
                    std::string key = "textures/" + Lower(floor) + ext;
                    auto it = files.find(key);
                    if (it == files.end() && Lower(floor).find('.') != std::string::npos)
                        it = files.find("textures/" + Lower(floor));
                    if (it != files.end()) {
                        if (uint32_t id = AddDynamic(m.root + it->second)) w.floorTexture = (uint16_t)id;
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
                Log("Mods: %s [%s]: floor texture '%s' not found in textures/", m.id.c_str(), sec.first.c_str(),
                    floor.c_str());
        }
        std::string music = Trim(ini.Get(sec.first, "music"));
        if (!music.empty()) {
            std::string lm = Lower(music);
            if (lm.size() == 7 && lm.rfind("music0", 0) == 0 && lm[6] >= '1' && lm[6] <= '4') {
                w.musicTrack = lm[6] - '1';
            } else {
                auto it = files.find("music/" + lm);
                if (it != files.end()) w.musicFile = m.root + it->second;
                else
                    Log("Mods: %s [%s]: music '%s' not found (use Music01-Music04 or a file in music/)", m.id.c_str(),
                        sec.first.c_str(), music.c_str());
            }
        }
        int worldIndex = (int)p.worlds.size();
        p.worlds.push_back(w);
        std::string list = ini.Get(sec.first, "levels");
        size_t s = 0;
        while (s <= list.size()) {
            size_t c = list.find(',', s);
            std::string name = Trim(list.substr(s, c == std::string::npos ? std::string::npos : c - s));
            s = c == std::string::npos ? list.size() + 1 : c + 1;
            if (name.empty()) continue;
            std::string ln = Lower(name);
            auto it = files.find("levels/" + ln);
            if (it == files.end()) it = files.find("levels/" + ln + ".txt");
            if (it == files.end()) {
                Log("Mods: %s: level '%s' not found in levels/", m.id.c_str(), name.c_str());
                continue;
            }
            std::string text;
            ReadText(m.root + it->second, &text);
            Ini header;
            std::string grid = GridFromLevelFile(text, &header);
            std::string bad = CheckLevel(grid);
            if (!bad.empty()) {
                Log("Mods: %s: level '%s' skipped: %s", m.id.c_str(), name.c_str(), bad.c_str());
                continue;
            }
            uint32_t id = AddDynamic(m.root + it->second);
            if (!id) continue;
            p.levels.push_back((uint16_t)id);
            p.levelWorld.push_back(worldIndex);
            p.levelNames.push_back(header.Get("level", "name", name));
        }
    }
    if (p.levels.empty()) {
        Log("Mods: %s defines a level pack but has no playable levels", m.id.c_str());
        return false;
    }
    // worlds without playable levels stay in the list (levelWorld indexes it) but don't count
    std::vector<bool> used(p.worlds.size(), false);
    for (int w : p.levelWorld) used[(size_t)w] = true;
    size_t worlds = (size_t)std::count(used.begin(), used.end(), true);
    Log("Mods: level pack '%s' (%s): %zu level%s in %zu world%s", p.name.c_str(), p.id.c_str(), p.levels.size(),
        p.levels.size() == 1 ? "" : "s", worlds, worlds == 1 ? "" : "s");
    g_packs.push_back(p);
    return true;
}

}  // namespace

// ------------------------------------------------------------------ ini
std::string Ini::Get(const std::string& section, const std::string& key, const std::string& def) const {
    std::string s = Lower(section), k = Lower(key);
    for (auto& sec : sections)
        if (Lower(sec.first) == s)
            for (auto& kv : sec.second)
                if (kv.first == k) return kv.second;
    return def;
}

const std::vector<std::pair<std::string, std::string>>* Ini::Section(const std::string& name) const {
    for (auto& sec : sections)
        if (Lower(sec.first) == Lower(name)) return &sec.second;
    return nullptr;
}

bool ParseIni(const std::string& text, Ini* out) {
    out->sections.clear();
    out->sections.push_back({"", {}});
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            out->sections.push_back({Trim(line.substr(1, line.size() - 2)), {}});
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Lower(Trim(line.substr(0, eq))), v = line.substr(eq + 1);
        // a comment after a value needs a space before the ';' (so values may contain ';')
        size_t c = v.find(" ;");
        if (c != std::string::npos) v = v.substr(0, c);
        out->sections.back().second.push_back({k, Trim(v)});
    }
    return true;
}

// ------------------------------------------------------------------ names
const char* ImageName(uint32_t id) { return Lookup(kImages, id); }
const char* ModelName(uint32_t id) { return Lookup(kModels, id); }
const char* SoundName(uint32_t id) { return Lookup(kSounds, id); }
const char* LevelName(uint32_t id) {
    (void)LevelNames();
    return Lookup(g_levelNames, id);
}
const std::vector<std::pair<uint32_t, const char*>>& ImageNames() { return kImages; }
const std::vector<std::pair<uint32_t, const char*>>& ModelNames() { return kModels; }
const std::vector<std::pair<uint32_t, const char*>>& SoundNames() { return kSounds; }
const std::vector<std::pair<uint32_t, const char*>>& LevelNames() {
    if (g_levelNames.empty()) {
        g_levelNameStore.reserve(45);
        for (int w = 0; w < 3; w++)
            for (int l = 0; l < 15; l++) {
                char buf[32];
                snprintf(buf, sizeof buf, "%s-%02d", kLookNames[w], l + 1);
                g_levelNameStore.push_back(buf);
            }
        for (int w = 0; w < 3; w++)
            for (int l = 0; l < 15; l++) g_levelNames.push_back({kWorldLevels[w][l], g_levelNameStore[w * 15 + l].c_str()});
    }
    return g_levelNames;
}

// ------------------------------------------------------------------ discovery
namespace {
// Reads one installed mod (a folder or a .zip) found in mods/<folder>/.
ModInfo ReadMod(const std::string& folder, const std::string& entry) {
    std::string path = g_modsFolder + folder + "/" + entry;
    ModInfo m;
    m.folder = folder;
    std::string stem = entry;
    if (IsDir(path)) {
        m.source = path + "/";
        m.root = FindModRoot(m.source);
    } else {
        m.isZip = true;
        m.source = path;
        stem = entry.substr(0, entry.size() - 4);
        std::string unpacked = UnpackZip(path, folder + "-" + stem, &m.problem);
        if (!unpacked.empty()) m.root = FindModRoot(unpacked);
    }
    if (m.root.empty() && m.problem.empty()) m.problem = "no mod.ini found";
    if (!m.root.empty()) {
        std::string text;
        ReadText(m.root + "mod.ini", &text);
        ParseIni(text, &m.ini);
    }
    m.id = SafeId(m.ini.Get("mod", "id", stem));
    if (m.id.empty()) m.id = SafeId(stem);
    if (m.id.empty() || m.id == "original" || m.id == "pack") {
        m.problem = "invalid id '" + m.id + "'";
        m.id = SafeId(stem) + "-mod";
    }
    m.name = m.ini.Get("mod", "name", stem);
    m.author = m.ini.Get("mod", "author");
    m.version = m.ini.Get("mod", "version");
    m.description = m.ini.Get("mod", "description");
    m.license = m.ini.Get("mod", "license");
    m.requiresVersion = m.ini.Get("mod", "requires");
    std::string api = m.ini.Get("mod", "api");
    m.api = api.empty() ? kApiVersion : std::atoi(api.c_str());
    if (m.problem.empty() && m.api > kApiVersion)
        m.problem = "made for a newer Hopmon Native (mod API " + std::to_string(m.api) + ")";
    if (m.problem.empty() && !m.requiresVersion.empty() && CompareVersions(HOPMON_VERSION_STRING, m.requiresVersion) < 0)
        m.problem = "needs Hopmon Native " + m.requiresVersion + " or newer";
    // the kind comes from mod.ini, wherever the mod was put
    m.isPack = m.ini.Section("pack") != nullptr;
    if (m.isPack) m.name = m.ini.Get("pack", "name", m.name);
    if (!m.root.empty()) {
        if (m.isPack && folder != "levelpacks")
            Log("Mods: %s is a level pack; it belongs in mods/levelpacks/ (used as a level pack anyway)", m.source.c_str());
        else if (!m.isPack && folder != "addons")
            Log("Mods: %s has no [pack] section, so it's an add-on; it belongs in mods/addons/ (used as an add-on anyway)",
                m.source.c_str());
    }
    return m;
}
bool IsZipName(const std::string& entry) {
    std::string l = Lower(entry);
    return l.size() > 4 && l.substr(l.size() - 4) == ".zip";
}
}  // namespace

void CreateFolders() {
    if (g_modsFolder.empty()) {
        const char* base = SDL_GetBasePath();
        g_modsFolder = std::string(base ? base : "./") + "mods/";
    }
    SDL_CreateDirectory((g_modsFolder + "levelpacks").c_str());  // creates mods/ too
    SDL_CreateDirectory((g_modsFolder + "addons").c_str());
}

void Init() {
    g_mods.clear();
    g_files.clear();
    g_packs.clear();
    g_dynamic.clear();
    const char* base = SDL_GetBasePath();
    g_modsFolder = std::string(base ? base : "./") + "mods/";
    Config& cfg = GetConfig();

    std::vector<ModInfo> found;
    if (IsDir(g_modsFolder)) {
        for (const char* folder : {"levelpacks", "addons"}) {
            std::string dir = g_modsFolder + folder + "/";
            if (!IsDir(dir)) continue;
            for (auto& entry : ListDir(dir)) {
                if (entry[0] == '.') continue;
                if (!IsDir(dir + entry) && !IsZipName(entry)) continue;
                found.push_back(ReadMod(folder, entry));
            }
        }
        // anything else directly in mods/ isn't loaded: say where it should go
        for (auto& entry : ListDir(g_modsFolder)) {
            std::string l = Lower(entry);
            if (entry[0] == '.' || l == "levelpacks" || l == "addons") continue;
            if (IsDir(g_modsFolder + entry) || IsZipName(entry))
                Log("Mods: mods/%s isn't loaded: put level packs in mods/levelpacks/ and add-ons in mods/addons/",
                    entry.c_str());
        }
    }
    for (size_t i = 0; i < found.size(); i++)
        for (size_t k = 0; k < i; k++)
            if (found[k].id == found[i].id && found[i].problem.empty())
                found[i].problem = "same id '" + found[i].id + "' as " + found[k].source;
    for (auto& m : found) {
        if (m.root.empty()) continue;
        auto& idx = g_files[m.id + (m.problem.empty() ? "" : "\x01" + m.source)];  // broken duplicates get no files
        IndexFiles(m.root, "", idx);
        for (auto& f : idx) {
            const std::string& k = f.first;
            if (k.rfind("textures/", 0) == 0) m.hasTextures = true;
            else if (k.rfind("models/", 0) == 0) m.hasModels = true;
            else if (k.rfind("sounds/", 0) == 0) m.hasSounds = true;
            else if (k.rfind("music/", 0) == 0) m.hasMusic = true;
            else if (k.rfind("levels/", 0) == 0) m.hasLevels = true;
        }
    }

    // add-ons: load order (first wins) and on/off from [mods]; new ones go at the end, switched on.
    // Level packs follow, sorted by name.
    std::vector<ModInfo> ordered;
    for (auto& e : cfg.mods)
        for (auto& m : found)
            if (!m.isPack && m.id == e.first && m.source != "") {
                m.enabled = e.second;
                ordered.push_back(m);
                m.source = "";  // taken
            }
    for (auto& m : found)
        if (!m.isPack && m.source != "") ordered.push_back(m);
    std::vector<ModInfo> packs;
    for (auto& m : found)
        if (m.isPack) packs.push_back(m);
    std::stable_sort(packs.begin(), packs.end(), [](const ModInfo& a, const ModInfo& b) { return Lower(a.name) < Lower(b.name); });
    for (auto& m : packs) ordered.push_back(m);
    g_mods = std::move(ordered);

    BuildOriginalPack();
    for (auto& m : g_mods)
        if (m.isPack && m.problem.empty() && !m.root.empty() && !BuildModPack(m))
            m.problem = "it has no playable levels (the log says why)";
    for (auto& m : g_mods) {
        const char* kind = m.isPack ? "level pack" : "add-on";
        if (!m.problem.empty()) Log("Mods: %s %s (%s) can't be used: %s", kind, m.name.c_str(), m.source.c_str(), m.problem.c_str());
        else Log("Mods: %s %s%s%s by %s (%s)%s", kind, m.name.c_str(), m.version.empty() ? "" : " ", m.version.c_str(),
                 m.author.empty() ? "unknown" : m.author.c_str(), m.id.c_str(), (m.isPack || m.enabled) ? "" : " - off");
    }

    g_activePack = 0;
    for (size_t i = 0; i < g_packs.size(); i++)
        if (g_packs[i].id == cfg.modPack) g_activePack = i;
    if (g_packs[g_activePack].id != cfg.modPack && cfg.modPack != "original")
        Log("Mods: level pack '%s' is not available; playing the original game", cfg.modPack.c_str());
    g_activePackId = g_packs[g_activePack].id;
    g_chosenPackId = g_activePackId;
    StoreInConfig();
    g_initDone = true;
}

const std::vector<ModInfo>& Installed() { return g_mods; }

std::vector<const ModInfo*> Addons() {
    std::vector<const ModInfo*> out;
    for (auto& m : g_mods)
        if (!m.isPack) out.push_back(&m);
    return out;
}

std::vector<const ModInfo*> LevelPackMods() {
    std::vector<const ModInfo*> out;
    for (auto& m : g_mods)
        if (m.isPack) out.push_back(&m);
    return out;
}

std::vector<const ModInfo*> Active() {
    std::vector<const ModInfo*> out;
    // lowest priority first (FindFile searches from the end): the pack in play, then the add-ons
    // from the bottom of the list up, so the add-on at the top wins
    for (auto& m : g_mods)
        if (m.isPack && m.id == g_activePackId && m.problem.empty() && !m.root.empty()) out.push_back(&m);
    for (auto it = g_mods.rbegin(); it != g_mods.rend(); ++it)
        if (!it->isPack && it->enabled && it->problem.empty() && !it->root.empty()) out.push_back(&*it);
    return out;
}

const std::string& ModsFolder() { return g_modsFolder; }

void StoreInConfig() {
    Config& cfg = GetConfig();
    cfg.mods.clear();
    for (auto& m : g_mods)
        if (!m.isPack) cfg.mods.push_back({m.id, m.enabled});
    cfg.modPack = g_chosenPackId;
}

void SetEnabled(const std::string& id, bool on) {
    for (auto& m : g_mods)
        if (m.id == id && !m.isPack) m.enabled = on;
    StoreInConfig();
}

void Move(const std::string& id, int delta) {
    // add-ons come first in g_mods, so moving within them never crosses into the packs
    size_t addons = 0;
    while (addons < g_mods.size() && !g_mods[addons].isPack) addons++;
    for (size_t i = 0; i < addons; i++)
        if (g_mods[i].id == id) {
            long j = (long)i + delta;
            if (j < 0 || j >= (long)addons) return;
            std::swap(g_mods[i], g_mods[(size_t)j]);
            break;
        }
    StoreInConfig();
}

std::string FindFile(const std::string& subdir, const std::string& stem, const std::vector<std::string>& exts,
                     const ModInfo** from) {
    if (stem.empty()) return "";
    auto active = Active();
    for (auto it = active.rbegin(); it != active.rend(); ++it) {
        auto f = g_files.find((*it)->id);
        if (f == g_files.end()) continue;
        for (auto& ext : exts) {
            auto hit = f->second.find(Lower(subdir + "/" + stem + "." + ext));
            if (hit != f->second.end()) {
                if (from) *from = *it;
                return (*it)->root + hit->second;
            }
        }
    }
    return "";
}

bool IsDynamic(uint32_t id) { return id >= 0x8000 && id < 0x8000 + g_dynamic.size(); }
std::string DynamicPath(uint32_t id) { return IsDynamic(id) ? g_dynamic[id - 0x8000] : ""; }

// ------------------------------------------------------------------ levels
std::string CheckLevel(const std::string& grid) {
    if (grid.empty()) return "the map is empty";
    static const std::string allowed = ".-+#=*[]<>!123@$abcdemnopqrxyANSEWVP";
    int starts = 0, crystals = 0, warps = 0, enemies = 0, objects = 0, rows = 0;
    size_t width = 0, lineStart = 0;
    for (size_t i = 0; i <= grid.size(); i++) {
        if (i == grid.size() || grid[i] == '\n') {
            if (i > lineStart) {
                rows++;
                width = std::max(width, i - lineStart);
            }
            lineStart = i + 1;
            continue;
        }
        char c = grid[i];
        if (allowed.find(c) == std::string::npos) return std::string("unknown tile '") + c + "'";
        if (c == '@') starts++;
        else if (c == '$') crystals++;
        else if (c == '*') warps++;
        else if (std::strchr("abcdemnopqrxy", c)) enemies++;
        else if (std::strchr("ANSEWVP!", c)) objects++;
    }
    if (rows > 62 || width > 62) return "the map is larger than 62 x 62";
    if (starts != 1) return starts ? "more than one start '@'" : "no start '@'";
    if (!crystals) return "no crystals '$'";
    if (crystals > 30) return "more than 30 crystals";
    if (!warps) return "no warp tile '*'";
    if (enemies > 30) return "more than 30 enemies and gates";
    if (objects > 25) return "more than 25 objects (cannons, volcanoes, poles, steam vents)";
    return "";
}

std::string LevelText(uint32_t id) {
    if (IsDynamic(id)) {
        std::string text;
        if (!ReadText(DynamicPath(id), &text)) return "";
        return GridFromLevelFile(text, nullptr);
    }
    const char* name = LevelName(id);
    if (*name) {
        std::string path = FindFile("levels", name, {"txt"});
        if (!path.empty()) {
            std::string text;
            ReadText(path, &text);
            std::string grid = GridFromLevelFile(text, nullptr);
            std::string bad = CheckLevel(grid);
            if (bad.empty()) return grid;
            Log("Mods: %s ignored: %s", path.c_str(), bad.c_str());
        }
    }
    return Assets::RCDataCString(id);
}

// ------------------------------------------------------------------ packs
const std::vector<Pack>& Packs() {
    if (g_packs.empty()) BuildOriginalPack();
    return g_packs;
}
const Pack* FindPack(const std::string& id) {
    for (auto& p : Packs())
        if (p.id == id) return &p;
    return nullptr;
}
const Pack& ActivePack() { return Packs()[g_activePack < Packs().size() ? g_activePack : 0]; }
const std::string& ActivePackId() { return g_activePackId; }
const std::string& ChosenPackId() { return g_chosenPackId; }
void SetActivePack(const std::string& id) {
    g_chosenPackId = id;
    StoreInConfig();
}

}  // namespace Mods
