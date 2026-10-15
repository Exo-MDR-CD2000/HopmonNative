// Saved progress (native replacement for the original's registry storage).
//
// Each campaign ("pack") keeps its progress in its own text file:
//   <save dir>/original/progress.ini          the original 45 levels
//   <save dir>/packs/<pack id>/progress.ini   level packs (future modding framework)
// The save dir is %APPDATA%\HopmonNative\saves on Windows (the per-user data folder on other
// systems) unless [game] save_path in hopmon_native.ini says otherwise.
//
// Files are written to a temporary file first and then swapped in, keeping the previous version
// as progress.bak, so a crash or power cut during a save never leaves a broken file.
// Unknown sections and keys are kept when a file is rewritten, so newer versions (and mods) can
// add data without older versions destroying it.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace Saves {

// One pack's save file. Values are strings; sections and keys keep their order.
struct PackSave {
    static constexpr int kVersion = 1;  // save_version written by this build
    int saveVersion = kVersion;         // as read from the file
    std::string pack;                   // pack id ("original" for the base game)
    std::string packVersion;            // version of the pack the progress belongs to
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> sections;

    std::string Get(const std::string& section, const std::string& key, const std::string& def = "") const;
    int GetInt(const std::string& section, const std::string& key, int def) const;
    void Set(const std::string& section, const std::string& key, const std::string& value);
    void SetInt(const std::string& section, const std::string& key, int value) { Set(section, key, std::to_string(value)); }
};

// Resolves the save folder from the config, moves saves over if save_path changed since the
// last run, and logs where saves live. Call once at startup, after the config is loaded.
void Init();

// The resolved save folder, ending with a path separator.
const std::string& Dir();

// A pack id is lowercase letters, digits, '.', '_' and '-' (it becomes a folder name).
bool IsValidPackId(const std::string& id);

// Reads a pack's progress (falling back to its backup if the main file is damaged).
// Returns false when the pack has no saved progress yet.
bool Load(const std::string& packId, PackSave* out);

// Writes a pack's progress safely. Returns false (and logs why) on failure.
bool Store(const PackSave& save);

// The original game's progress as stored by older versions of Hopmon Native (and by Hopmon.exe
// itself on Windows): the registry value HKCU\Software\SaitoGames\Hopmon\Level, read without
// creating or changing anything. Returns false when there is none.
bool ReadLegacyLevel(int* level);

// ReadLegacyLevel, but only the first time it is ever asked for this user (a marker file in the
// per-user folder remembers it), so deleting your saves to start over doesn't bring it back.
bool TakeLegacyLevelOnce(int* level);

}  // namespace Saves
