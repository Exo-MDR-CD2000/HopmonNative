// Mod framework (Tier 1: asset replacement, Tier 2: levels and level packs).
//
// A mod is a folder, or a .zip of one, described by a mod.ini. There are two kinds:
//   mods/levelpacks/<pack>/   a level pack: a campaign of its own levels ([pack] in mod.ini), with
//                             optional textures/models/sounds/music used only while it's played.
//                             One pack (or the original game) is played at a time.
//   mods/addons/<addon>/      an add-on: replacements that apply to whatever is played. Any number
//                             can be on at once, in a load order.
// Mods only ever add their creators' own files: anything a mod doesn't replace still comes from
// the player's Hopmon.exe. Zipped mods are unpacked once into a per-user cache.
//
// Game assets are addressed by readable names (floor_castle, crystal, crystal_pickup,
// castle-07...); the tables live in mods.cpp and are listed in MODDING.md. Replacements are
// looked up in the add-ons that are on, in load order (the first in the list wins), then in the
// level pack in play, then the original game: add-ons apply on top of a pack, as they do on top of
// the original game.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Mods {

constexpr int kApiVersion = 1;  // mod.ini "api" this build understands

// A parsed ini file: sections in order, keys in order (lower-cased keys, values trimmed).
struct Ini {
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> sections;
    std::string Get(const std::string& section, const std::string& key, const std::string& def = "") const;
    const std::vector<std::pair<std::string, std::string>>* Section(const std::string& name) const;
};
bool ParseIni(const std::string& text, Ini* out);

struct ModInfo {
    std::string id;           // folder-safe id from mod.ini (or the folder name)
    std::string name, author, version, description, license, requiresVersion;
    int api = kApiVersion;
    std::string root;         // folder holding mod.ini, with a trailing separator
    std::string source;       // the folder or .zip as installed in mods/
    bool isZip = false;
    bool isPack = false;      // a level pack ([pack] in mod.ini); otherwise an add-on
    std::string folder;       // where it was found: "levelpacks" or "addons"
    bool enabled = true;      // add-ons: on/off from [mods] in hopmon_native.ini
    std::string problem;      // why it can't be used ("" = fine)
    bool hasTextures = false, hasModels = false, hasSounds = false, hasMusic = false, hasLevels = false;
    Ini ini;
};

// A playable campaign: the original game (id "original") or a mod's level pack.
struct World {
    std::string name;
    int look = 0;                 // 0 castle, 1 forest, 2 toy: props, default floor and music
    uint16_t floorTexture = 0;    // texture resource id (an original or a mod image)
    std::string musicFile;        // a file to play instead of the look's track ("" = none)
    int musicTrack = -1;          // 0..3 = Music01..04 (-1 = the look's track)
};
struct Pack {
    std::string id, name, author, version;
    std::vector<World> worlds;
    std::vector<uint16_t> levels;      // level resource ids in play order
    std::vector<int> levelWorld;       // world index of each level
    std::vector<std::string> levelNames;
};

// Scans mods/ (unpacking zips as needed), reads every mod.ini, applies the [mods] settings and
// builds the level packs. Safe to call again to pick up changes (the options menu does).
void Init();

const std::vector<ModInfo>& Installed();          // every mod found, add-ons first (load order)
std::vector<const ModInfo*> Addons();              // installed add-ons, load order (first = wins)
std::vector<const ModInfo*> LevelPackMods();       // installed level packs, by name
std::vector<const ModInfo*> Active();              // searched for replacements, lowest priority first:
                                                   // the pack in play, then the add-ons that are on
const std::string& ModsFolder();                   // <game folder>/mods/
// Creates mods/, mods/levelpacks/ and mods/addons/ if they're missing.
void CreateFolders();

// Writes the current add-on settings and pack choice into the config (does not save it).
void StoreInConfig();
// Add-on changes made in the options menu (applied by Init when the menu closes).
void SetEnabled(const std::string& id, bool on);
void Move(const std::string& id, int delta);       // among add-ons: -1 = up (wins over more), +1 = down

// The newest enabled mod that has <subdir>/<stem>.<ext> (first matching extension in the list);
// returns the full path, or "" if none. *from receives the mod.
std::string FindFile(const std::string& subdir, const std::string& stem, const std::vector<std::string>& exts,
                     const ModInfo** from = nullptr);

// Readable names of the original game's resources ("" when the id has none).
const char* ImageName(uint32_t id);
const char* ModelName(uint32_t id);
const char* SoundName(uint32_t id);
const char* LevelName(uint32_t id);
// Every name, for the guide and the asset export: id -> name.
const std::vector<std::pair<uint32_t, const char*>>& ImageNames();
const std::vector<std::pair<uint32_t, const char*>>& ModelNames();
const std::vector<std::pair<uint32_t, const char*>>& SoundNames();
const std::vector<std::pair<uint32_t, const char*>>& LevelNames();

// Resources that only exist in mods (a pack's own floor texture or levels) get ids from 0x8000.
bool IsDynamic(uint32_t id);
std::string DynamicPath(uint32_t id);              // the file behind a dynamic id

// Level text for a level resource id (an original level, a mod's replacement of one, or a pack
// level), as map rows separated by '\n'. "" if unavailable.
std::string LevelText(uint32_t id);
// Checks a level grid; returns "" when it's playable, else what's wrong.
std::string CheckLevel(const std::string& grid);

// Level packs: index 0 is always the original game, then every usable pack.
const std::vector<Pack>& Packs();
const Pack* FindPack(const std::string& id);
const Pack& ActivePack();
const std::string& ActivePackId();                 // the pack in play
const std::string& ChosenPackId();                 // the pack picked in the menu
void SetActivePack(const std::string& id);         // picks a pack; it's in play after the next Init()

}  // namespace Mods
