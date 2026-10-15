#include "saves.h"
#include "config.h"
#include "log.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Saves {
namespace {

std::string g_dir;

const char* const kAppFolder = "HopmonNative";
const char* const kFileName = "progress.ini";
const char* const kBackupName = "progress.bak";
const char* const kPointerName = "save_location.txt";  // in the per-user folder: last save dir used

bool EndsWithSep(const std::string& s) { return !s.empty() && (s.back() == '/' || s.back() == '\\'); }
std::string WithSep(std::string s) {
    if (!EndsWithSep(s)) s += '/';
    return s;
}

bool IsAbsolute(const std::string& p) {
    if (p.empty()) return false;
    if (p[0] == '/' || p[0] == '\\') return true;
    return p.size() >= 2 && p[1] == ':';
}

// %NAME% (any system) and a leading ~ (home folder) are expanded, so save_path can say
// %USERPROFILE%\Documents\Hopmon or ~/Games/Hopmon.
std::string ExpandVars(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '%') {
            size_t end = in.find('%', i + 1);
            if (end != std::string::npos && end > i + 1) {
                std::string name = in.substr(i + 1, end - i - 1);
                const char* v = SDL_getenv(name.c_str());
                if (v) {
                    out += v;
                    i = end;
                    continue;
                }
            }
        }
        out += in[i];
    }
    if (!out.empty() && out[0] == '~' && (out.size() == 1 || out[1] == '/' || out[1] == '\\')) {
        const char* home = SDL_getenv("HOME");
        if (!home) home = SDL_getenv("USERPROFILE");
        if (home) out = std::string(home) + out.substr(1);
    }
    return out;
}

// The standard per-user data folder for this program (created if missing), or "" if unavailable.
std::string UserFolder() {
    char* p = SDL_GetPrefPath("", kAppFolder);
    std::string s = p ? p : "";
    SDL_free(p);
    return s;
}

std::string BaseFolder() {
    const char* b = SDL_GetBasePath();
    return b ? b : "./";
}

bool FileExists(const std::string& path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

bool DirExists(const std::string& path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

std::string PackFolder(const std::string& dir, const std::string& packId) {
    return packId == "original" ? dir + "original/" : dir + "packs/" + packId + "/";
}

bool ReadText(const std::string& path, std::string* out) {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) return false;
    out->assign((const char*)data, size);
    SDL_free(data);
    return true;
}

bool WriteText(const std::string& path, const std::string& text) {
    return SDL_SaveFile(path.c_str(), text.data(), text.size());
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Parses a save file. A file without save_version is treated as damaged.
bool Parse(const std::string& text, PackSave* out) {
    PackSave s;
    s.saveVersion = 0;
    std::string section;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() + 1 : nl + 1;
        size_t c = line.find(';');
        if (c != std::string::npos) line = line.substr(0, c);
        line = Trim(line);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            s.Set(section, "", "");  // keep empty sections too
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        if (section.empty()) {
            if (k == "save_version") s.saveVersion = std::atoi(v.c_str());
            else if (k == "pack") s.pack = v;
            else if (k == "pack_version") s.packVersion = v;
        } else {
            s.Set(section, k, v);
        }
    }
    if (s.saveVersion <= 0) return false;
    *out = s;
    return true;
}

std::string Serialize(const PackSave& s) {
    std::string t;
    t += "; Hopmon Native saved progress. Back this folder up to keep your progress.\n";
    t += "; Editing is possible but not needed: the cheats in the options menu can unlock levels\n";
    t += "; without changing this file.\n";
    t += "save_version = " + std::to_string(PackSave::kVersion) + "\n";
    t += "pack = " + s.pack + "\n";
    t += "pack_version = " + s.packVersion + "\n";
    for (auto& sec : s.sections) {
        t += "\n[" + sec.first + "]\n";
        if (sec.first == "progress" && s.pack == "original")
            t += "; unlocked: the highest level you can pick on level select (1 to 45)\n";
        for (auto& kv : sec.second)
            if (!kv.first.empty()) t += kv.first + " = " + kv.second + "\n";
    }
    return t;
}

bool HasAnySave(const std::string& dir) {
    if (FileExists(dir + "original/" + kFileName) || FileExists(dir + "original/" + kBackupName)) return true;
    bool any = false;
    if (DirExists(dir + "packs")) {
        SDL_EnumerateDirectory(
            (dir + "packs").c_str(),
            [](void* ud, const char*, const char*) -> SDL_EnumerationResult {
                *(bool*)ud = true;
                return SDL_ENUM_SUCCESS;
            },
            &any);
    }
    return any;
}

// Copies a folder tree (used when save_path changes). Existing files at the destination are kept.
struct CopyCtx {
    std::string from, to;
    int files = 0;
    bool ok = true;
};
SDL_EnumerationResult SDLCALL CopyEntry(void* ud, const char* dirname, const char* fname) {
    CopyCtx* c = (CopyCtx*)ud;
    std::string src = std::string(dirname) + fname;
    std::string rel = src.substr(c->from.size());
    std::string dst = c->to + rel;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(src.c_str(), &info)) return SDL_ENUM_CONTINUE;
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        SDL_CreateDirectory(dst.c_str());
        SDL_EnumerateDirectory((src + "/").c_str(), CopyEntry, c);
    } else if (info.type == SDL_PATHTYPE_FILE && !FileExists(dst)) {
        if (SDL_CopyFile(src.c_str(), dst.c_str())) c->files++;
        else c->ok = false;
    }
    return SDL_ENUM_CONTINUE;
}

bool SameFolder(std::string a, std::string b) {
    for (auto* s : {&a, &b}) {
        for (char& ch : *s) {
            if (ch == '\\') ch = '/';
#if defined(_WIN32)
            ch = (char)SDL_tolower((unsigned char)ch);
#endif
        }
        *s = WithSep(*s);
    }
    return a == b;
}

}  // namespace

// ------------------------------------------------------------------ PackSave
std::string PackSave::Get(const std::string& section, const std::string& key, const std::string& def) const {
    for (auto& sec : sections)
        if (sec.first == section)
            for (auto& kv : sec.second)
                if (kv.first == key) return kv.second;
    return def;
}

int PackSave::GetInt(const std::string& section, const std::string& key, int def) const {
    std::string v = Get(section, key);
    if (v.empty()) return def;
    char* end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    return (end && *end == 0) ? (int)n : def;
}

void PackSave::Set(const std::string& section, const std::string& key, const std::string& value) {
    for (auto& sec : sections) {
        if (sec.first != section) continue;
        if (key.empty()) return;
        for (auto& kv : sec.second)
            if (kv.first == key) {
                kv.second = value;
                return;
            }
        sec.second.push_back({key, value});
        return;
    }
    sections.push_back({section, {}});
    if (!key.empty()) sections.back().second.push_back({key, value});
}

// ------------------------------------------------------------------ folder
void Init() {
    const Config& cfg = GetConfig();
    std::string user = UserFolder();
    std::string dir;
    std::string setting = Trim(cfg.savePath);
    if (setting.empty()) {
        dir = user.empty() ? BaseFolder() + "saves/" : user + "saves/";
    } else {
        std::string p = ExpandVars(setting);
        dir = WithSep(IsAbsolute(p) ? p : BaseFolder() + p);
    }
    g_dir = dir;
    if (!SDL_CreateDirectory(g_dir.c_str())) Log("Saves: could not create %s (%s)", g_dir.c_str(), SDL_GetError());

    // save_path changed since the last run and the new folder has nothing yet: copy the saves over.
    // The old copies are left where they were.
    if (!user.empty()) {
        std::string pointer = user + kPointerName, last;
        if (ReadText(pointer, &last)) {
            last = Trim(last);
            if (!last.empty() && !SameFolder(last, g_dir) && HasAnySave(WithSep(last))) {
                if (HasAnySave(g_dir)) {
                    Log("Saves: save folder changed from %s; the new folder already has saves, so nothing was "
                        "copied", last.c_str());
                } else {
                    CopyCtx c{WithSep(last), g_dir};
                    SDL_EnumerateDirectory(c.from.c_str(), CopyEntry, &c);
                    Log("Saves: save folder changed; copied %d file(s) from %s%s", c.files, c.from.c_str(),
                        c.ok ? "" : " (some files could not be copied)");
                }
            }
        }
        if (!WriteText(pointer, g_dir + "\n")) Log("Saves: could not write %s", pointer.c_str());
    }
    Log("Saves: %s", g_dir.c_str());
}

const std::string& Dir() { return g_dir; }

bool IsValidPackId(const std::string& id) {
    if (id.empty() || id.size() > 64 || id[0] == '.') return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    return true;
}

// ------------------------------------------------------------------ load / store
bool Load(const std::string& packId, PackSave* out) {
    if (!IsValidPackId(packId)) return false;
    std::string folder = PackFolder(g_dir, packId);
    std::string main = folder + kFileName, backup = folder + kBackupName, text;
    bool mainExists = FileExists(main);
    if (mainExists && ReadText(main, &text) && Parse(text, out)) {
        if (out->pack.empty()) out->pack = packId;
        return true;
    }
    // No file = no progress. Saves are swapped in as one step, so the file is never missing
    // because of a crash; a missing file was removed on purpose (starting over), and the backup
    // must not bring it back.
    if (!mainExists) return false;
    if (FileExists(backup) && ReadText(backup, &text) && Parse(text, out)) {
        if (out->pack.empty()) out->pack = packId;
        // keep the damaged file for inspection, then put the backup back in its place
        SDL_RenamePath(main.c_str(), (folder + "progress.damaged").c_str());
        Log("Saves: %s was damaged; restored the previous save (damaged copy kept as progress.damaged)",
            main.c_str());
        SDL_CopyFile(backup.c_str(), main.c_str());
        return true;
    }
    {
        SDL_RenamePath(main.c_str(), (folder + "progress.damaged").c_str());
        Log("Saves: %s could not be read and there is no backup; kept it as progress.damaged", main.c_str());
    }
    return false;
}

bool Store(const PackSave& save) {
    if (!IsValidPackId(save.pack)) {
        Log("Saves: refusing to save invalid pack id '%s'", save.pack.c_str());
        return false;
    }
    std::string folder = PackFolder(g_dir, save.pack);
    if (!SDL_CreateDirectory(folder.c_str())) {
        Log("Saves: could not create %s (%s)", folder.c_str(), SDL_GetError());
        return false;
    }
    std::string main = folder + kFileName, temp = folder + "progress.tmp", backup = folder + kBackupName;
    std::string text = Serialize(save), current;
    // unchanged: nothing to write (this also keeps progress.bak as the previous, different version)
    if (ReadText(main, &current) && current == text) return true;
    if (!WriteText(temp, text)) {
        Log("Saves: could not write %s (%s)", temp.c_str(), SDL_GetError());
        return false;
    }
    // the file just written must read back intact before it replaces the current save
    std::string check;
    PackSave parsed;
    if (!ReadText(temp, &check) || check != text || !Parse(check, &parsed)) {
        Log("Saves: %s did not read back correctly; the previous save was kept", temp.c_str());
        SDL_RemovePath(temp.c_str());
        return false;
    }
    if (FileExists(main) && !SDL_CopyFile(main.c_str(), backup.c_str()))
        Log("Saves: could not update %s (%s)", backup.c_str(), SDL_GetError());
    if (!SDL_RenamePath(temp.c_str(), main.c_str())) {
        Log("Saves: could not replace %s (%s)", main.c_str(), SDL_GetError());
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ legacy
bool TakeLegacyLevelOnce(int* level) {
    std::string user = UserFolder();
    if (user.empty()) return false;
    std::string marker = user + "legacy_import_done";
    if (FileExists(marker)) return false;
    WriteText(marker, "The original game's progress was offered for import once; it is not imported again.\n");
    return ReadLegacyLevel(level);
}

bool ReadLegacyLevel(int* level) {
#if defined(_WIN32)
    HKEY k = nullptr;  // read-only: never creates the key
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SaitoGames\\Hopmon", 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return false;
    DWORD type = 0, value = 0, size = sizeof value;
    LONG r = RegQueryValueExA(k, "Level", nullptr, &type, (BYTE*)&value, &size);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || type != REG_DWORD) return false;
    *level = (int)value;
    return true;
#else
    // Older Linux builds kept it in <data home>/SaitoGames/HopmonNative/registry.ini. The path is
    // built by hand because SDL_GetPrefPath would create that folder.
    std::string home;
    if (const char* x = SDL_getenv("XDG_DATA_HOME"); x && *x) home = x;
    else if (const char* h = SDL_getenv("HOME")) home = std::string(h) + "/.local/share";
    else return false;
    std::string text;
    if (!ReadText(WithSep(home) + "SaitoGames/HopmonNative/registry.ini", &text)) return false;
    const std::string key = "Software\\SaitoGames\\Hopmon\\Level=";
    size_t p = text.find(key);
    if (p == std::string::npos) return false;
    *level = std::atoi(text.c_str() + p + key.size());
    return true;
#endif
}

}  // namespace Saves
