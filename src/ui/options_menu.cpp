#include "options_menu.h"
#include "../audio/audio.h"
#include "../audio/midi.h"
#include "../core/assets.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../framework/d3dapp.h"
#include "../framework/d3dtextr.h"
#include "../gfx/device.h"
#include "../input/input.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

namespace {

const UIColor kWhite{1, 1, 1, 1};
const UIColor kGrey{0.55f, 0.55f, 0.62f, 1};
const UIColor kGold{1.0f, 0.82f, 0.25f, 1};
const UIColor kValue{0.62f, 0.9f, 1.0f, 1};
const UIColor kSelValue{1.0f, 0.95f, 0.55f, 1};
const UIColor kPanel{0.05f, 0.04f, 0.16f, 0.92f};
const UIColor kBorder{0.45f, 0.6f, 1.0f, 1};
const UIColor kSelBar{0.22f, 0.3f, 0.75f, 0.75f};
const UIColor kDim{0, 0, 0, 0.5f};

std::string OnOff(bool b) { return b ? "On" : "Off"; }

std::string Fit(const std::string& s, size_t n) {
    if (s.size() <= n) return s;
    return s.substr(0, n - 2) + "..";
}

// Cycle through a list of values: returns the index after moving `d` from the closest match.
template <class T>
int CycleIndex(const std::vector<T>& list, const T& cur, int d) {
    int idx = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (list[i] == cur) idx = (int)i;
    int n = (int)list.size();
    return ((idx + d) % n + n) % n;
}

int NearestIndex(const std::vector<float>& list, float v) {
    int best = 0;
    for (size_t i = 0; i < list.size(); i++)
        if (std::fabs(list[i] - v) < std::fabs(list[best] - v)) best = (int)i;
    return best;
}

float StepFloat(const std::vector<float>& list, float cur, int d) {
    int i = NearestIndex(list, cur) + d;
    i = std::clamp(i, 0, (int)list.size() - 1);
    return list[(size_t)i];
}

std::string Pct(int pct) { return std::to_string(pct) + "%"; }

struct KeyAction { const char* label; int dik; };
const KeyAction kKeyActions[] = {
    {"Move up", DIK_UP},          {"Move down", DIK_DOWN},   {"Move left", DIK_LEFT},
    {"Move right", DIK_RIGHT},    {"Fire", DIK_Z},           {"Camera modifier", DIK_X},
    {"Menu confirm", DIK_RETURN}, {"Back / give up", DIK_ESCAPE}, {"Pause", DIK_P},
};

std::string BaseName(const std::string& p) {
    size_t s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

}  // namespace

OptionsMenu::OptionsMenu(CD3DApplication* app) : app_(app) {}

// ------------------------------------------------------------------ open / close / save
void OptionsMenu::Open() {
    if (open_) return;
    open_ = true;
    mode_ = Mode::Normal;
    stack_.clear();
    stack_.push_back(MainPage());
    Input::SetBlocked(true);
    ResetStick();  // stick movements made while the menu was closed never reached it
    message_.clear();
    Sound(true);
}

void OptionsMenu::Close() {
    if (!open_) return;
    if (mode_ == Mode::ConfirmDisplay) RevertDisplay();
    DiscardPending(false);
    open_ = false;
    mode_ = Mode::Normal;
    ResetStick();
    stack_.clear();
    Input::SetBlocked(false);
}

void OptionsMenu::Save() {
    if (!GetConfig().Save()) Log("Options: could not save %s", GetConfig().path.c_str());
}

// Every setting back to its default, applied at once and saved. Kept: the path to Hopmon.exe
// (needed to find the game data), the save folder, the ini location, and saved progress
// (stored separately).
void OptionsMenu::ResetAllSettings() {
    Config& c = GetConfig();
    const Config old = c;
    Config fresh;
    fresh.path = old.path;
    fresh.version = old.version;
    fresh.gamePath = old.gamePath;
    fresh.savePath = old.savePath;  // resetting must not move where progress is saved
    c = fresh;
    pendingActive_ = false;
    // apply everything that is not read live every frame
    if (old.fullscreen != c.fullscreen || old.windowWidth != c.windowWidth || old.windowHeight != c.windowHeight ||
        old.displayIndex != c.displayIndex || old.fullscreenWidth != c.fullscreenWidth ||
        old.fullscreenHeight != c.fullscreenHeight)
        app_->ApplyDisplaySettings();
    app_->ApplyVSync();
    GetDevice().OnSettingsChanged();  // antialiasing, render scale, aspect
    if (old.enhancedShadows != c.enhancedShadows) D3DTextr_InvalidateTexture(0xa3);
    Audio::SetMasterVolumes(c.masterVolume / 100.f, c.sfxVolume / 100.f, c.musicVolume / 100.f);
    if (old.soundFont != c.soundFont) Midi::SwitchSynth(c.soundFont);
    Midi::SetVolume(c.masterVolume / 100.f * c.musicVolume / 100.f);
    bool musicFilesChanged = false;
    for (int i = 0; i < 4; i++) musicFilesChanged |= old.musicFiles[i] != c.musicFiles[i];
    if (old.musicSource != c.musicSource || musicFilesChanged) app_->ReloadMusic();
    Input::ReloadBindings();
    Save();
    // rebuild the menu so every page shows the new values, staying on the main page
    int cursor = stack_.empty() ? 0 : stack_.front().cursor;
    stack_.clear();
    stack_.push_back(MainPage());
    stack_.back().cursor = cursor;
    SetMessage("All settings reset to defaults.");
    Log("Options: all settings reset to defaults");
}

void OptionsMenu::SetMessage(const std::string& m, DWORD ms) {
    message_ = m;
    messageUntil_ = now_ + ms;
}

// ------------------------------------------------------------------ display changes
void OptionsMenu::BeginDisplayChange() {
    const Config& c = GetConfig();
    if (mode_ != Mode::ConfirmDisplay)
        snapshot_ = {(int)c.fullscreen, c.windowWidth, c.windowHeight, c.fullscreenWidth, c.fullscreenHeight,
                     c.displayIndex};
}

void OptionsMenu::ApplyDisplay() {
    app_->ApplyDisplaySettings();
    Save();
    mode_ = Mode::ConfirmDisplay;
    confirmDeadline_ = now_ + 10000;
    confirmChoice_ = 1;
}

void OptionsMenu::RevertDisplay() {
    Config& c = GetConfig();
    c.fullscreen = (FullscreenMode)snapshot_.fullscreen;
    c.windowWidth = snapshot_.w;
    c.windowHeight = snapshot_.h;
    c.fullscreenWidth = snapshot_.fw;
    c.fullscreenHeight = snapshot_.fh;
    c.displayIndex = snapshot_.display;
    app_->ApplyDisplaySettings();
    Save();
    mode_ = Mode::Normal;
    SetMessage("Display settings reverted.");
}

static SDL_DisplayID DisplayIdFor(int index) {
    SDL_DisplayID d = SDL_GetPrimaryDisplay();
    if (index > 0) {
        int n = 0;
        SDL_DisplayID* ids = SDL_GetDisplays(&n);
        if (ids && index <= n) d = ids[index - 1];
        SDL_free(ids);
    }
    return d;
}

// "16:9", "4:3", "21:9" ... for a resolution
static std::string AspectName(int w, int h) {
    if (w <= 0 || h <= 0) return "";
    double r = (double)w / h;
    struct A { double r; const char* name; double tol; };
    static const A known[] = {{5.0 / 4, "5:4", 0.01},   {4.0 / 3, "4:3", 0.01},  {3.0 / 2, "3:2", 0.01},
                              {16.0 / 10, "16:10", 0.01}, {16.0 / 9, "16:9", 0.01}, {21.0 / 9, "21:9", 0.06},
                              {32.0 / 9, "32:9", 0.02}};
    for (const A& a : known)
        if (std::fabs(r - a.r) <= a.tol) return a.name;
    int x = w, y = h;
    while (y) { int t = x % y; x = y; y = t; }
    return std::to_string(w / x) + ":" + std::to_string(h / x);
}

static std::string SizeName(int w, int h) {
    return std::to_string(w) + "x" + std::to_string(h) + " " + AspectName(w, h);
}

std::vector<std::pair<int, int>> OptionsMenu::ResolutionList(FullscreenMode mode, int display) const {
    const Config& c = GetConfig();
    std::vector<std::pair<int, int>> out;
    SDL_DisplayID disp = DisplayIdFor(display);
    auto byArea = [](auto& a, auto& b) {
        return a.first * a.second < b.first * b.second || (a.first * a.second == b.first * b.second && a.first < b.first);
    };
    if (mode == FullscreenMode::Exclusive) {
        int n = 0;
        SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(disp, &n);
        for (int i = 0; i < n; i++) {
            std::pair<int, int> r{modes[i]->w, modes[i]->h};
            if (std::find(out.begin(), out.end(), r) == out.end()) out.push_back(r);
        }
        SDL_free(modes);
        if (c.fullscreenWidth > 0 && std::find(out.begin(), out.end(), std::pair<int, int>{c.fullscreenWidth, c.fullscreenHeight}) == out.end())
            out.push_back({c.fullscreenWidth, c.fullscreenHeight});
        std::sort(out.begin(), out.end(), byArea);
        out.push_back({0, 0});  // "Desktop": the monitor's own resolution, the largest choice
    } else {
        SDL_Rect usable{0, 0, 1920, 1080};
        SDL_GetDisplayUsableBounds(disp, &usable);
        static const int common[][2] = {{640, 480},   {800, 600},   {1024, 768},  {1280, 720},  {1280, 960},
                                        {1366, 768},  {1440, 1080}, {1600, 900},  {1600, 1200}, {1920, 1080},
                                        {1920, 1440}, {2560, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160}};
        for (auto& r : common)
            if (r[0] <= usable.w && r[1] <= usable.h) out.push_back({r[0], r[1]});
        std::pair<int, int> cur{c.windowWidth, c.windowHeight};
        if (std::find(out.begin(), out.end(), cur) == out.end()) out.push_back(cur);
        std::sort(out.begin(), out.end(), byArea);
    }
    return out;
}

// ------------------------------------------------------------------ display settings
OptionsMenu::DisplayCfg OptionsMenu::CurrentDisplay() const {
    const Config& c = GetConfig();
    return {c.fullscreen, c.fullscreenWidth > 0 ? c.fullscreenWidth : 0, c.fullscreenWidth > 0 ? c.fullscreenHeight : 0,
            c.displayIndex};
}

OptionsMenu::DisplayCfg OptionsMenu::ShownDisplay() const { return pendingActive_ ? pending_ : CurrentDisplay(); }

void OptionsMenu::ProposeDisplay(const DisplayCfg& d) {
    if (d.mode != FullscreenMode::Exclusive) {
        // Window / Borderless can't leave the screen unusable: apply straight away, no confirmation
        Config& c = GetConfig();
        pendingActive_ = false;
        c.fullscreen = d.mode;
        c.displayIndex = d.display;
        app_->ApplyDisplaySettings();
        Save();
        return;
    }
    // ends in exclusive fullscreen: hold it until Enter
    pending_ = d;
    pendingActive_ = !(d == CurrentDisplay());
    if (pendingActive_) SetMessage("Press Enter to apply.", 4000);
}

void OptionsMenu::ApplyPending() {
    if (!pendingActive_) return;
    BeginDisplayChange();
    Config& c = GetConfig();
    c.fullscreen = pending_.mode;
    c.fullscreenWidth = pending_.fw;
    c.fullscreenHeight = pending_.fh;
    c.displayIndex = pending_.display;
    pendingActive_ = false;
    ApplyDisplay();  // asks "Keep these display settings?"
}

void OptionsMenu::DiscardPending(bool tell) {
    if (!pendingActive_) return;
    pendingActive_ = false;
    if (tell) SetMessage("Display change not applied.");
}

std::string OptionsMenu::DisplayHelp(const std::string& base) const {
    if (pendingActive_) return "Not applied yet (*). Press Enter to switch; you'll be asked to keep it.";
    return base;
}

// ------------------------------------------------------------------ synth list
std::vector<std::string> OptionsMenu::SynthList() const {
    std::vector<std::string> out{""};  // automatic
#if defined(_WIN32)
    out.push_back("system");
#endif
    std::vector<std::string> dirs;
    if (const char* b = SDL_GetBasePath()) dirs.push_back(b);
    std::string gameDir = Assets::GameDir();
    if (!gameDir.empty() && std::find(dirs.begin(), dirs.end(), gameDir) == dirs.end()) dirs.push_back(gameDir);
    for (size_t d = 0; d < dirs.size(); d++) {
        int n = 0;
        char** files = SDL_GlobDirectory(dirs[d].c_str(), "*.sf2", SDL_GLOB_CASEINSENSITIVE, &n);
        for (int i = 0; i < n; i++) {
            // files next to the program are stored by name (portable); others with their full path
            std::string v = d == 0 ? std::string(files[i]) : dirs[d] + files[i];
            if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
        }
        SDL_free(files);
    }
    const std::string& cur = GetConfig().soundFont;
    if (std::find(out.begin(), out.end(), cur) == out.end()) out.push_back(cur);
    return out;
}

std::string OptionsMenu::KeyNamesFor(int dik) const {
    const Config& c = GetConfig();
    std::vector<std::string> names;
    auto it = c.keyBinds.find(dik);
    if (it != c.keyBinds.end()) {
        names = it->second;
    } else {
        std::vector<int> scs;
        Input::DefaultScancodesForDik(dik, &scs);
        for (int sc : scs) names.push_back(SDL_GetScancodeName((SDL_Scancode)sc));
    }
    std::string s;
    for (auto& n : names) s += (s.empty() ? "" : ", ") + n;
    return s.empty() ? "(none)" : s;
}

std::string OptionsMenu::PadNamesFor(int button) const {
    const Config& c = GetConfig();
    auto it = c.padBinds.find(button);
    if (it == c.padBinds.end()) return button == 0 ? "A / bottom" : button == 1 ? "B / right" : "(default)";
    std::string s;
    for (auto& n : it->second) s += (s.empty() ? "" : ", ") + n;
    return s.empty() ? "(none)" : s;
}

// ------------------------------------------------------------------ pages
std::string OptionsMenu::LostLine() const {
    return app_->InLevel() ? "Progress in the current level will be lost." : "";
}

OptionsMenu::Page OptionsMenu::MainPage() {
    Page p;
    // On the title screen (where "Main menu" has nowhere to go) this is an options menu, not a pause.
    const bool onTitle = !app_->CanReturnToMainMenu();
    p.title = onTitle ? "OPTIONS" : "PAUSED";
    if (onTitle)
        p.items.push_back({"Back", "Close this menu (F10 / Start).", nullptr, nullptr, [this] { Close(); }, nullptr});
    else
        p.items.push_back({"Resume game", "Close this menu and continue (F10 / Start).", nullptr, nullptr,
                           [this] { Close(); }, nullptr});
    p.items.push_back({"Video", "Window, resolution, frame rate, widescreen and HUD.", nullptr, nullptr,
                       [this] { PushPage(VideoPage()); }, nullptr});
    p.items.push_back({"Graphics", "Antialiasing, sharpness and visual upgrades.", nullptr, nullptr,
                       [this] { PushPage(GraphicsPage()); }, nullptr});
    p.items.push_back({"Audio", "Volume levels and the music synthesizer.", nullptr, nullptr,
                       [this] { PushPage(AudioPage()); }, nullptr});
    p.items.push_back({"Controls", "Keyboard and gamepad bindings, stick dead zone.", nullptr, nullptr,
                       [this] { PushPage(ControlsPage()); }, nullptr});
    p.items.push_back({"Cheats", "Game modifiers: unlock all levels, infinite fire, no crystal slowdown.", nullptr,
                       nullptr, [this] { PushPage(CheatsPage()); }, nullptr});
    p.items.push_back({"Reset all settings",
                       "Restore every setting to its default. Saved progress, the game path and the save folder are kept.", nullptr,
                       nullptr,
                       [this] {
                           AskConfirm("Reset all settings?", "Video, graphics, audio, controls and cheats go back to defaults.",
                                      [this] { ResetAllSettings(); });
                       },
                       nullptr});
    if (!onTitle)
        p.items.push_back({"Main menu", "Leave the current screen and return to the title screen.", nullptr, nullptr,
                           [this] {
                               AskConfirm("Return to the main menu?", LostLine(), [this] {
                                   Close();
                                   app_->ReturnToMainMenu();
                               });
                           },
                           [this] { return app_->CanReturnToMainMenu(); }});
    p.items.push_back({"Quit game", "Close Hopmon. Settings are already saved.", nullptr, nullptr,
                       [this] {
                           AskConfirm("Quit the game?", LostLine(), [this] {
                               Close();
                               app_->RequestQuit();
                           });
                       },
                       nullptr});
    return p;
}

OptionsMenu::Page OptionsMenu::VideoPage() {
    Page p;
    p.title = "OPTIONS > VIDEO";
    auto& v = p.items;
    auto modeName = [](FullscreenMode m) {
        switch (m) {
            case FullscreenMode::Windowed: return std::string("Window");
            case FullscreenMode::Borderless: return std::string("Borderless");
            default: return std::string("Exclusive");
        }
    };
    auto star = [this] { return pendingActive_ ? std::string(" *") : std::string(); };
    {
        Item it{"Display mode", "", [this, modeName, star] { return modeName(ShownDisplay().mode) + star(); },
                [this](int d) {
                    DisplayCfg p = ShownDisplay();
                    p.mode = (FullscreenMode)((((int)p.mode + d) % 3 + 3) % 3);
                    ProposeDisplay(p);
                },
                [this] {
                    if (pendingActive_) { ApplyPending(); return; }
                    DisplayCfg p = ShownDisplay();
                    p.mode = (FullscreenMode)(((int)p.mode + 1) % 3);
                    ProposeDisplay(p);
                },
                nullptr};
        it.helpFn = [this] {
            return DisplayHelp("Window and Borderless switch at once. Exclusive: choose, then press Enter.");
        };
        v.push_back(it);
    }
    {
        Item it{"Resolution", "",
                [this, star] {
                    DisplayCfg p = ShownDisplay();
                    const Config& c = GetConfig();
                    if (p.mode == FullscreenMode::Borderless) return std::string("Desktop");
                    if (p.mode == FullscreenMode::Exclusive)
                        return (p.fw <= 0 ? std::string("Desktop") : SizeName(p.fw, p.fh)) + star();
                    return SizeName(c.windowWidth, c.windowHeight);
                },
                [this](int d) {
                    DisplayCfg p = ShownDisplay();
                    Config& c = GetConfig();
                    auto list = ResolutionList(p.mode, p.display);
                    if (p.mode == FullscreenMode::Exclusive) {
                        int i = CycleIndex(list, std::pair<int, int>{p.fw, p.fh}, d);
                        p.fw = list[(size_t)i].first;
                        p.fh = list[(size_t)i].second;
                        ProposeDisplay(p);
                    } else {  // window size: resize right away
                        int i = std::clamp(CycleIndex(list, std::pair<int, int>{c.windowWidth, c.windowHeight}, 0) + d, 0,
                                           (int)list.size() - 1);
                        c.windowWidth = list[(size_t)i].first;
                        c.windowHeight = list[(size_t)i].second;
                        app_->ApplyDisplaySettings();
                        Save();
                    }
                },
                [this] { ApplyPending(); },
                [this] { return ShownDisplay().mode != FullscreenMode::Borderless; }};
        it.helpFn = [this] {
            DisplayCfg p = ShownDisplay();
            const Config& c = GetConfig();
            const SDL_DisplayMode* desk = SDL_GetDesktopDisplayMode(DisplayIdFor(p.display));
            std::string deskNote;
            if (desk) {
                int w = p.mode == FullscreenMode::Exclusive ? p.fw : c.windowWidth;
                int h = p.mode == FullscreenMode::Exclusive ? p.fh : c.windowHeight;
                if (p.mode == FullscreenMode::Exclusive && w <= 0)
                    deskNote = "Desktop = your monitor's own resolution, " + std::to_string(desk->w) + "x" +
                               std::to_string(desk->h) + ".";
                else if (w == desk->w && h == desk->h)
                    deskNote = "This is your desktop resolution.";
            }
            std::string base = p.mode == FullscreenMode::Exclusive
                                   ? "Screen mode for exclusive fullscreen; press Enter to apply. " + deskNote
                                   : "Window size; changes at once. " + deskNote;
            return DisplayHelp(base);
        };
        v.push_back(it);
    }
    {
        Item it{"Monitor", "",
                [this, star] {
                    int idx = ShownDisplay().display;
                    std::string s;
                    if (idx == 0) {
                        s = "Primary";
                    } else {
                        int n = 0;
                        SDL_DisplayID* ids = SDL_GetDisplays(&n);
                        s = std::to_string(idx);
                        if (ids && idx <= n) {
                            const char* name = SDL_GetDisplayName(ids[idx - 1]);
                            if (name) s += ": " + std::string(name);
                        }
                        SDL_free(ids);
                    }
                    return Fit(s, 13) + star();
                },
                [this](int d) {
                    int n = 0;
                    SDL_DisplayID* ids = SDL_GetDisplays(&n);
                    SDL_free(ids);
                    if (n < 1) n = 1;
                    DisplayCfg p = ShownDisplay();
                    p.display = ((p.display + d) % (n + 1) + (n + 1)) % (n + 1);
                    ProposeDisplay(p);
                },
                [this] { ApplyPending(); }, nullptr};
        it.helpFn = [this] { return DisplayHelp("Which display the game uses."); };
        v.push_back(it);
    }
    v.push_back({"VSync", "Sync to the monitor refresh rate (prevents tearing).",
                 [] { return OnOff(GetConfig().vsync); },
                 [this](int) {
                     GetConfig().vsync = !GetConfig().vsync;
                     app_->ApplyVSync();
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Smooth motion", "Draw in-between frames so motion is fluid at any refresh rate.",
                 [] { return OnOff(GetConfig().interpolation); },
                 [this](int) {
                     GetConfig().interpolation = !GetConfig().interpolation;
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Frame limit", "Maximum frames per second (needs Smooth motion; without it the cap is 100).",
                 [] {
                     int f = GetConfig().maxFps;
                     return f == 0 ? std::string("Unlimited") : std::to_string(f);
                 },
                 [this](int d) {
                     static const std::vector<int> l{0, 30, 60, 75, 90, 120, 144, 165, 240, 360};
                     GetConfig().maxFps = l[(size_t)CycleIndex(l, GetConfig().maxFps, d)];
                     Save();
                 },
                 nullptr, [] { return GetConfig().interpolation; }});
    v.push_back({"Aspect", "4:3 as original, stretched, or widescreen (wider 3D view).",
                 [] {
                     switch (GetConfig().aspect) {
                         case AspectMode::Original4x3: return std::string("4:3");
                         case AspectMode::Stretch: return std::string("Stretch");
                         default: return std::string("Widescreen");
                     }
                 },
                 [this](int d) {
                     Config& c = GetConfig();
                     c.aspect = (AspectMode)((((int)c.aspect + d) % 3 + 3) % 3);
                     GetDevice().OnSettingsChanged();
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"HUD position", "Widescreen only: HUD at the screen edges or in the 4:3 area.",
                 [] { return std::string(GetConfig().hudLayout ? "Edges" : "Centered"); },
                 [this](int) {
                     GetConfig().hudLayout = GetConfig().hudLayout ? 0 : 1;
                     Save();
                 },
                 nullptr, [] { return GetConfig().aspect == AspectMode::Widescreen; }});
    v.push_back({"HUD max width", "How far the edge HUD may spread on ultrawide screens.",
                 [] {
                     float a = GetConfig().hudMaxAspect;
                     if (a <= 0) return std::string("Unlimited");
                     if (std::fabs(a - 1.78f) < 0.05f) return std::string("16:9");
                     if (std::fabs(a - 1.6f) < 0.05f) return std::string("16:10");
                     if (std::fabs(a - 2.4f) < 0.05f) return std::string("21:9");
                     if (std::fabs(a - 3.56f) < 0.05f) return std::string("32:9");
                     char b[16];
                     snprintf(b, sizeof b, "%.2f", a);
                     return std::string(b);
                 },
                 [this](int d) {
                     static const std::vector<float> l{1.6f, 1.78f, 2.4f, 3.56f, 0.f};
                     int i = NearestIndex(l, GetConfig().hudMaxAspect <= 0 ? 0.f : GetConfig().hudMaxAspect);
                     int n = (int)l.size();
                     GetConfig().hudMaxAspect = l[(size_t)(((i + d) % n + n) % n)];
                     Save();
                 },
                 nullptr,
                 [] { return GetConfig().aspect == AspectMode::Widescreen && GetConfig().hudLayout == 1; }});
    v.push_back({"HUD size", "Size of the in-game HUD. Each part grows from its corner. 100% = original.",
                 [] { return std::to_string((int)std::lround(GetConfig().hudScale * 100)) + "%"; },
                 [this](int d) {
                     Config& c = GetConfig();
                     c.hudScale = std::clamp(std::round((c.hudScale + d * 0.1f) * 10.f) / 10.f, 0.5f, 2.0f);
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Field of view", "Camera zoom in levels (menus and the ending keep the original view). 100% = original.",
                 [] { return std::to_string((int)std::lround(GetConfig().fovScale * 100)) + "%"; },
                 [this](int d) {
                     Config& c = GetConfig();
                     c.fovScale = std::clamp(std::round((c.fovScale + d * 0.05f) * 20.f) / 20.f, 0.5f, 2.0f);
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Show FPS", "The original frames-per-second counter.",
                 [] { return OnOff(GetConfig().showFps); },
                 [this](int) {
                     GetConfig().showFps = !GetConfig().showFps;
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Back", "", nullptr, nullptr, [this] { Back(false); }, nullptr});
    return p;
}

OptionsMenu::Page OptionsMenu::GraphicsPage() {
    Page p;
    p.title = "OPTIONS > GRAPHICS";
    auto& v = p.items;
    v.push_back({"Antialiasing", "Smooths polygon edges (MSAA). Lowered automatically if unsupported.",
                 [] {
                     int m = GetConfig().msaa;
                     return m < 2 ? std::string("Off") : std::to_string(m) + "x";
                 },
                 [this](int d) {
                     static const std::vector<int> l{0, 2, 4, 8, 16};
                     GetConfig().msaa = l[(size_t)CycleIndex(l, GetConfig().msaa, d)];
                     GetDevice().OnSettingsChanged();
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Render scale", "Renders the 3D scene larger (sharper) or smaller (faster) than the screen.",
                 [] { return std::to_string((int)std::lround(GetConfig().renderScale * 100)) + "%"; },
                 [this](int d) {
                     static const std::vector<float> l{0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 3.0f, 4.0f};
                     GetConfig().renderScale = StepFloat(l, GetConfig().renderScale, d);
                     GetDevice().OnSettingsChanged();
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Texture filtering", "Anisotropic filtering: keeps floors sharp at an angle.",
                 [] {
                     int a = GetConfig().anisotropy;
                     return a <= 1 ? std::string("Off") : std::to_string(a) + "x";
                 },
                 [this](int d) {
                     static const std::vector<int> l{1, 2, 4, 8, 16};
                     GetConfig().anisotropy = l[(size_t)CycleIndex(l, GetConfig().anisotropy, d)];
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Smooth textures", "Bilinear filtering on textures the original drew pixelated.",
                 [] { return OnOff(GetConfig().smoothTextures); },
                 [this](int) {
                     GetConfig().smoothTextures = !GetConfig().smoothTextures;
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Smooth 2D images", "Smooth scaling of menus, logo and HUD (off = sharp pixels).",
                 [] { return OnOff(GetConfig().smoothSprites); },
                 [this](int) {
                     GetConfig().smoothSprites = !GetConfig().smoothSprites;
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Per-pixel lighting", "The original lights calculated per pixel for smoother shading.",
                 [] { return OnOff(GetConfig().perPixelLighting); },
                 [this](int) {
                     GetConfig().perPixelLighting = !GetConfig().perPixelLighting;
                     Save();
                 },
                 nullptr, nullptr});
    v.push_back({"Soft shadows", "Soft-edged blob shadows under characters and objects.",
                 [] { return OnOff(GetConfig().enhancedShadows); },
                 [this](int) {
                     GetConfig().enhancedShadows = !GetConfig().enhancedShadows;
                     D3DTextr_InvalidateTexture(0xa3);
                     Save();
                 },
                 nullptr, nullptr});
    {
        Item it{"Brightness", "Adjusts how bright the picture looks. 100% = original.",
                [] { return Pct(GetConfig().brightness); },
                [this](int d) {
                    GetConfig().brightness = std::clamp(GetConfig().brightness + d * 5, 50, 150);
                    Save();
                },
                [] {}, nullptr};
        it.bar = [] { return GetConfig().brightness - 50; };  // 50..150 % shown as an empty..full bar
        v.push_back(it);
    }
    v.push_back({"Back", "", nullptr, nullptr, [this] { Back(false); }, nullptr});
    return p;
}

OptionsMenu::Page OptionsMenu::AudioPage() {
    Page p;
    p.title = "OPTIONS > AUDIO";
    auto& v = p.items;
    auto applyVolumes = [] {
        const Config& c = GetConfig();
        Audio::SetMasterVolumes(c.masterVolume / 100.f, c.sfxVolume / 100.f, c.musicVolume / 100.f);
        Midi::SetVolume(c.masterVolume / 100.f * c.musicVolume / 100.f);
    };
    struct Vol { const char* label; const char* help; int Config::*field; int (*bar)(); };
    static const Vol vols[] = {
        {"Master volume", "Overall volume of sound effects and music.", &Config::masterVolume,
         [] { return GetConfig().masterVolume; }},
        {"Sound effects", "Volume of sound effects.", &Config::sfxVolume, [] { return GetConfig().sfxVolume; }},
        {"Music", "Volume of the music.", &Config::musicVolume, [] { return GetConfig().musicVolume; }}};
    for (const Vol& vol : vols) {
        int Config::*f = vol.field;
        Item it{vol.label, vol.help, [f] { return Pct(GetConfig().*f); },
                [this, f, applyVolumes](int d) {
                    GetConfig().*f = std::clamp(GetConfig().*f + d * 5, 0, 100);
                    applyVolumes();
                    Save();
                },
                [] {}, nullptr};
        it.bar = vol.bar;
        v.push_back(it);
    }
    v.push_back({"Music source",
                 "Auto: MP3/FLAC from the music folder replace the matching MIDI track. MIDI: original music only.",
                 [] { return std::string(GetConfig().musicSource == 1 ? "MIDI only" : "Auto"); },
                 [this](int) {
                     Config& c = GetConfig();
                     c.musicSource = c.musicSource == 1 ? 0 : 1;
                     app_->ReloadMusic();
                     Save();
                     const char* src = app_->MusicSource();
                     SetMessage(*src ? std::string("Now playing: ") + src : std::string("Music source changed."));
                 },
                 nullptr, nullptr});
    v.push_back({"Music synth",
                 "Automatic uses soundfont.sf2/hopmon.sf2 if present, else the Windows synth. Or pick a .sf2 file.",
                 [] {
                     const std::string& s = GetConfig().soundFont;
                     if (s.empty()) return std::string("Automatic");
                     if (s == "system") return std::string("Windows synth");
                     return Fit(BaseName(s), 13);
                 },
                 [this, applyVolumes](int d) {
                     auto list = SynthList();
                     Config& c = GetConfig();
                     c.soundFont = list[(size_t)CycleIndex(list, c.soundFont, d)];
                     Midi::SwitchSynth(c.soundFont);
                     applyVolumes();
                     Save();
                     SetMessage(std::string("Music now plays through: ") + Midi::BackendName());
                 },
                 nullptr, nullptr});
    v.push_back({"Back", "", nullptr, nullptr, [this] { Back(false); }, nullptr});
    return p;
}

OptionsMenu::Page OptionsMenu::ControlsPage() {
    Page p;
    p.title = "OPTIONS > CONTROLS";
    auto& v = p.items;
    v.push_back({"Keyboard", "Change which keys move, fire, rotate the camera and so on.", nullptr, nullptr,
                 [this] { PushPage(KeysPage()); }, nullptr});
    v.push_back({"Gamepad buttons", "Change the fire and camera buttons. Movement is the stick and d-pad.",
                 nullptr, nullptr, [this] { PushPage(PadPage()); }, nullptr});
    v.push_back({"Stick dead zone", "How far the analog stick must move before it counts.",
                 [] { return std::to_string(GetConfig().gamepadDeadzone) + "%"; },
                 [this](int d) {
                     GetConfig().gamepadDeadzone = std::clamp(GetConfig().gamepadDeadzone + d * 5, 0, 90);
                     Save();
                 },
                 [] {}, nullptr});
    v.push_back({"Reset controls", "Restore the original keys and buttons and the default dead zone.", nullptr,
                 nullptr,
                 [this] {
                     Config& c = GetConfig();
                     c.keyBinds.clear();
                     c.padBinds.clear();
                     c.gamepadDeadzone = 25;
                     Input::ReloadBindings();
                     Save();
                     SetMessage("Controls reset to defaults.");
                 },
                 nullptr});
    v.push_back({"Back", "", nullptr, nullptr, [this] { Back(false); }, nullptr});
    return p;
}

OptionsMenu::Page OptionsMenu::CheatsPage() {
    Page p;
    p.title = "OPTIONS > CHEATS";
    struct Cheat { const char* label; const char* help; bool Config::*field; };
    static const Cheat cheats[] = {
        {"Unlock all levels",
         "Every level can be picked on level select. Your saved progress is not changed.",
         &Config::unlockAllLevels},
        {"Infinite fire energy",
         "The fire gauge stays full: fire on every press (still max 3 shots on screen).",
         &Config::infiniteFireEnergy},
        {"No crystal slowdown", "Carrying crystals no longer slows Hopmon's hops.", &Config::noCrystalSlowdown},
    };
    for (const Cheat& ch : cheats) {
        bool Config::*f = ch.field;
        Item it{ch.label, ch.help, [f] { return OnOff(GetConfig().*f); },
                [this, f](int) {
                    GetConfig().*f = !(GetConfig().*f);
                    Save();
                },
                nullptr, nullptr};
        if (f == &Config::unlockAllLevels) {
            // only has a visible effect on level select, so it can't be changed during a level
            it.enabled = [this] { return !app_->InLevel(); };
            it.focusWhenDisabled = true;
            std::string help = ch.help;
            it.helpFn = [this, help] {
                return app_->InLevel() ? std::string("Change this from OPTIONS on the title screen.") : help;
            };
        }
        p.items.push_back(it);
    }
    p.items.push_back({"Back", "All cheats are off by default; with them off the game plays like the original.",
                       nullptr, nullptr, [this] { Back(false); }, nullptr});
    return p;
}

OptionsMenu::Page OptionsMenu::KeysPage() {
    Page p;
    p.title = "OPTIONS > KEYBOARD";
    for (const KeyAction& a : kKeyActions) {
        int dik = a.dik;
        std::string label = a.label;
        p.items.push_back({label, "Enter: press a new key.  Backspace: restore the default.",
                           [this, dik] { return Fit(KeyNamesFor(dik), 13); }, nullptr,
                           [this, dik, label] {
                               mode_ = Mode::CaptureKey;
                               captureDik_ = dik;
                               captureLabel_ = label;
                           },
                           nullptr});
    }
    p.items.push_back({"Back", "Shift also fires and Ctrl is also the camera modifier, as in the original.", nullptr,
                       nullptr, [this] { Back(false); }, nullptr});
    return p;
}

OptionsMenu::Page OptionsMenu::PadPage() {
    Page p;
    p.title = "OPTIONS > GAMEPAD";
    struct B { const char* label; int button; };
    static const B buttons[] = {{"Fire / confirm", 0}, {"Camera / confirm", 1}};
    for (const B& b : buttons) {
        int btn = b.button;
        std::string label = b.label;
        p.items.push_back({label, "Enter: press a gamepad button.  Backspace: restore the default.",
                           [this, btn] { return Fit(PadNamesFor(btn), 13); }, nullptr,
                           [this, btn, label] {
                               mode_ = Mode::CaptureButton;
                               captureButton_ = btn;
                               captureLabel_ = label;
                           },
                           nullptr});
    }
    p.items.push_back({"Back", "", nullptr, nullptr, [this] { Back(false); }, nullptr});
    return p;
}

// ------------------------------------------------------------------ navigation
void OptionsMenu::PushPage(Page p) { stack_.push_back(std::move(p)); }

void OptionsMenu::ResetStick() {
    stickDir_ = stickX_ = stickY_ = 0;
    stickPad_ = 0;
}

void OptionsMenu::Sound(bool confirm) {
    app_->PlayUISound(confirm ? CD3DApplication::UISound::Confirm : CD3DApplication::UISound::Move);
}

void OptionsMenu::AskConfirm(const std::string& title, const std::string& line, std::function<void()> yes) {
    askTitle_ = title;
    askLine_ = line;
    askYes_ = std::move(yes);
    confirmChoice_ = 1;  // "No" is the default
    mode_ = Mode::ConfirmAction;
}

void OptionsMenu::Back(bool sound) {
    if (sound) Sound(false);
    DiscardPending(true);
    if (stack_.size() > 1) stack_.pop_back();
    else Close();
}

void OptionsMenu::Move(int d) {
    Page& p = Cur();
    int n = (int)p.items.size();
    int before = p.cursor;
    for (int tries = 0; tries < n; tries++) {
        p.cursor = ((p.cursor + d) % n + n) % n;
        const Item& it = p.items[(size_t)p.cursor];
        if (!it.enabled || it.enabled() || it.focusWhenDisabled) break;
    }
    if (p.cursor != before) Sound(false);
}

void OptionsMenu::Change(int d) {
    Item& it = Cur().items[(size_t)Cur().cursor];
    if (it.enabled && !it.enabled()) return;
    if (!it.change) return;
    std::string before = it.value ? it.value() : "";
    it.change(d);
    // play after the change, so a volume change is heard at the new level
    if (!it.value || it.value() != before) Sound(false);
}

void OptionsMenu::Activate() {
    Item& it = Cur().items[(size_t)Cur().cursor];
    if (it.enabled && !it.enabled()) return;
    if (it.activate) {
        Sound(true);
        it.activate();
    } else if (it.change) {
        Change(+1);
    }
}

// ------------------------------------------------------------------ input
bool OptionsMenu::HandleEvent(const SDL_Event& e) {
    bool isKey = e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP;
    bool isPad = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
                 e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION;
    if (!isKey && !isPad) return false;

    if (!open_) {
        bool request = (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_F10) ||
                       (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && e.gbutton.button == SDL_GAMEPAD_BUTTON_START);
        if (!request) return false;
        // during the intro, the ending, or while Hopmon isn't controllable the press is ignored
        if (app_->CanOpenOptions()) Open();
        return true;
    }

    // ---- capture modes
    if (mode_ == Mode::CaptureKey) {
        if (e.type != SDL_EVENT_KEY_DOWN || e.key.repeat) return true;
        SDL_Scancode sc = e.key.scancode;
        Config& c = GetConfig();
        if (sc == SDL_SCANCODE_ESCAPE && captureDik_ != DIK_ESCAPE) {
            mode_ = Mode::Normal;
        } else if (sc == SDL_SCANCODE_BACKSPACE || sc == SDL_SCANCODE_DELETE) {
            c.keyBinds.erase(captureDik_);
            Input::ReloadBindings();
            Save();
            mode_ = Mode::Normal;
            SetMessage(captureLabel_ + " restored to " + KeyNamesFor(captureDik_) + ".");
        } else if (sc == SDL_SCANCODE_F10 || sc == SDL_SCANCODE_F11 || sc == SDL_SCANCODE_F12) {
            SetMessage("F10, F11 and F12 are reserved.");
        } else {
            std::string name = SDL_GetScancodeName(sc);
            c.keyBinds[captureDik_] = {name};
            Input::ReloadBindings();
            Save();
            mode_ = Mode::Normal;
            // warn if that key also does something else
            std::string also;
            for (const KeyAction& a : kKeyActions) {
                if (a.dik == captureDik_) continue;
                std::string names = ", " + KeyNamesFor(a.dik) + ",";
                if (names.find(", " + name + ",") != std::string::npos) also = a.label;
            }
            if (!also.empty()) SetMessage(name + " is also used for: " + also + ".", 5000);
            else SetMessage(captureLabel_ + " set to " + name + ".");
        }
        return true;
    }
    if (mode_ == Mode::CaptureButton) {
        Config& c = GetConfig();
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            if (e.key.scancode == SDL_SCANCODE_ESCAPE) mode_ = Mode::Normal;
            if (e.key.scancode == SDL_SCANCODE_BACKSPACE || e.key.scancode == SDL_SCANCODE_DELETE) {
                c.padBinds.erase(captureButton_);
                Input::ReloadBindings();
                Save();
                mode_ = Mode::Normal;
                SetMessage(captureLabel_ + " restored to the default button.");
            }
        } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            SDL_GamepadButton b = (SDL_GamepadButton)e.gbutton.button;
            if (b == SDL_GAMEPAD_BUTTON_START) {
                mode_ = Mode::Normal;
            } else if (const char* name = SDL_GetGamepadStringForButton(b)) {
                c.padBinds[captureButton_] = {name};
                Input::ReloadBindings();
                Save();
                mode_ = Mode::Normal;
                SetMessage(captureLabel_ + " set to " + name + ".");
            }
        }
        return true;
    }

    // ---- translate to menu actions
    enum { None, Up, Down, Left, Right, Ok, Cancel, Exit } act = None;
    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.scancode) {
            case SDL_SCANCODE_UP: act = Up; break;
            case SDL_SCANCODE_DOWN: act = Down; break;
            case SDL_SCANCODE_LEFT: act = Left; break;
            case SDL_SCANCODE_RIGHT: act = Right; break;
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
            case SDL_SCANCODE_SPACE:
            case SDL_SCANCODE_Z: if (!e.key.repeat) act = Ok; break;
            case SDL_SCANCODE_ESCAPE:
            case SDL_SCANCODE_BACKSPACE:
            case SDL_SCANCODE_X: if (!e.key.repeat) act = Cancel; break;
            case SDL_SCANCODE_F10: if (!e.key.repeat) act = Exit; break;
            default: break;
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        switch (e.gbutton.button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP: act = Up; break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN: act = Down; break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: act = Left; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: act = Right; break;
            case SDL_GAMEPAD_BUTTON_SOUTH: act = Ok; break;
            case SDL_GAMEPAD_BUTTON_EAST: act = Cancel; break;
            case SDL_GAMEPAD_BUTTON_START: act = Exit; break;
            default: break;
        }
    } else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION &&
               (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX || e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)) {
        // Each axis is tracked on its own, with hysteresis: it counts as pushed past 50% and as
        // released again below 35%. Releasing the axis that drives the auto-repeat stops it.
        bool isY = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY;
        int& axisDir = isY ? stickY_ : stickX_;
        int v = e.gaxis.value;
        int nd = axisDir;
        if (v < -kStickPress) nd = -1;
        else if (v > kStickPress) nd = 1;
        else if (std::abs(v) < kStickRelease) nd = 0;
        if (nd != axisDir) {
            axisDir = nd;
            int scaled = isY ? nd : nd * 2;
            if (nd != 0) {
                stickDir_ = scaled;
                stickPad_ = e.gaxis.which;
                stickNext_ = now_ + 400;
                act = scaled == -1 ? Up : scaled == 1 ? Down : scaled == -2 ? Left : Right;
            } else if ((std::abs(stickDir_) == 1) == isY) {
                stickDir_ = 0;  // the axis driving the repeat was released
            }
        }
    }

    if (mode_ == Mode::ConfirmAction) {
        if (act == Left || act == Right || act == Up || act == Down) {
            confirmChoice_ ^= 1;
            Sound(false);
        }
        if (act == Ok) {
            Sound(true);
            mode_ = Mode::Normal;
            if (confirmChoice_ == 0 && askYes_) {
                auto yes = std::move(askYes_);
                askYes_ = nullptr;
                yes();
            }
        }
        if (act == Cancel) {
            Sound(false);
            mode_ = Mode::Normal;
        }
        if (act == Exit) Close();
        return true;
    }
    if (mode_ == Mode::ConfirmDisplay) {
        if (act == Left || act == Right || act == Up || act == Down) {
            confirmChoice_ ^= 1;
            Sound(false);
        }
        if (act == Ok) Sound(true);
        if (act == Ok) {
            if (confirmChoice_ == 0) {
                mode_ = Mode::Normal;
                SetMessage("Display settings kept.");
            } else {
                RevertDisplay();
            }
        }
        if (act == Cancel || act == Exit) RevertDisplay();
        return true;
    }

    switch (act) {
        case Up: Move(-1); break;
        case Down: Move(+1); break;
        case Left: Change(-1); break;
        case Right: Change(+1); break;
        case Ok: Activate(); break;
        case Cancel: Back(); break;
        case Exit:
            Sound(false);
            Close();
            break;
        default: break;
    }
    return true;
}

void OptionsMenu::Update(DWORD nowMs) {
    now_ = nowMs;
    if (!open_) return;
    if (mode_ == Mode::ConfirmDisplay && now_ >= confirmDeadline_) RevertDisplay();
    // analog stick auto-repeat. Before each repeat, re-read the stick directly so a lost
    // "released" event (or a disconnected controller) can never leave it scrolling.
    if (stickDir_ != 0) {
        SDL_Gamepad* pad = SDL_GetGamepadFromID(stickPad_);
        bool held = false;
        if (pad) {
            int v = SDL_GetGamepadAxis(pad, std::abs(stickDir_) == 1 ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_LEFTX);
            held = (stickDir_ < 0) ? v < -kStickRelease : v > kStickRelease;
        }
        if (!held) ResetStick();
    }
    if (stickDir_ != 0 && mode_ == Mode::Normal && now_ >= stickNext_) {
        stickNext_ = now_ + 120;
        if (stickDir_ == -1) Move(-1);
        else if (stickDir_ == 1) Move(+1);
        else Change(stickDir_ > 0 ? +1 : -1);
    }
}

// ------------------------------------------------------------------ drawing
void OptionsMenu::Render(Device* d) {
    if (!open_) return;
    ui_.Begin(d);
    ui_.Rect(ui_.Left(), 0, ui_.Right(), 480, kDim);
    const float x0 = 40, x1 = 600, y0 = 36, y1 = 444;
    ui_.Rect(x0 - 2, y0 - 2, x1 + 2, y1 + 2, kBorder);
    ui_.Rect(x0, y0, x1, y1, kPanel);

    Page& p = Cur();
    ui_.Text(x0 + 16, y0 + 14, p.title, 3, kGold);
    ui_.Rect(x0 + 16, y0 + 46, x1 - 16, y0 + 48, UIColor{kBorder.r, kBorder.g, kBorder.b, 0.6f});

    float y = y0 + 60;
    const float rowH = 22;
    // rows that fit above the help text; longer pages scroll to keep the cursor visible
    const int visible = (int)((y1 - 52 - y) / rowH);
    const int count = (int)p.items.size();
    if (count <= visible) p.scroll = 0;
    else {
        if (p.cursor < p.scroll) p.scroll = p.cursor;
        if (p.cursor >= p.scroll + visible) p.scroll = p.cursor - visible + 1;
        p.scroll = std::clamp(p.scroll, 0, count - visible);
        if (p.scroll > 0) ui_.Text(x1 - 28, y0 + 50, "^", 1, kGrey);
        if (p.scroll + visible < count) ui_.Text(x1 - 28, y + visible * rowH - 6, "v", 1, kGrey);
    }
    for (size_t i = (size_t)p.scroll; i < p.items.size() && (int)i < p.scroll + std::max(visible, 1); i++) {
        const Item& it = p.items[i];
        bool sel = (int)i == p.cursor;
        bool en = !it.enabled || it.enabled();
        if (sel) ui_.Rect(x0 + 8, y - 3, x1 - 8, y + 19, kSelBar);
        ui_.Text(x0 + 20, y, it.label, 2, en ? kWhite : kGrey);
        if (it.value) {
            std::string val = it.value();
            if (sel && en && it.change) val = "< " + val + " >";
            float w = ui_.TextWidth(val, 2);
            float vx = x1 - 20 - w;
            ui_.Text(vx, y, val, 2, !en ? kGrey : sel ? kSelValue : kValue);
            if (it.bar) {
                // level bar left of the number
                float bx1 = x1 - 20 - ui_.TextWidth("< 100% >", 2) - 8, bx0 = bx1 - 120;
                float f = std::clamp(it.bar(), 0, 100) / 100.f;
                ui_.Rect(bx0, y + 3, bx1, y + 13, UIColor{0.1f, 0.1f, 0.25f, 1});
                ui_.Rect(bx0, y + 3, bx0 + (bx1 - bx0) * f, y + 13, sel ? kSelValue : kValue);
            }
        } else if (it.activate && it.label != "Back" && it.label != "Resume game" && it.label != "Reset controls" &&
                   it.label != "Main menu" && it.label != "Quit game" && it.label != "Reset all settings") {
            ui_.Text(x1 - 20 - 16, y, ">", 2, sel ? kSelValue : kGrey);
        }
        y += rowH;
    }

    // help line for the selected item (wrapped to two lines), or a status message
    const Item& selItem = p.items[(size_t)p.cursor];
    std::string help = selItem.helpFn ? selItem.helpFn() : selItem.help;
    UIColor helpColor{0.75f, 0.78f, 0.9f, 1};
    if (!message_.empty() && now_ < messageUntil_) {
        help = message_;
        helpColor = kGold;
    }
    const size_t perLine = 66;
    std::string l1 = help, l2;
    if (help.size() > perLine) {
        size_t cut = help.rfind(' ', perLine);
        if (cut == std::string::npos) cut = perLine;
        l1 = help.substr(0, cut);
        l2 = help.substr(cut + 1);
    }
    ui_.Text(x0 + 16, y1 - 46, l1, 1, helpColor);
    if (!l2.empty()) ui_.Text(x0 + 16, y1 - 34, Fit(l2, perLine), 1, helpColor);
    ui_.Text(x0 + 16, y1 - 16, "Arrows/D-pad  Enter/A: select  Esc/B: back  F10/Start: close", 1,
             kGrey);

    // modal overlays
    auto box = [&](const std::string& title, const std::string& line2) {
        float bx0 = 60, bx1 = 580, by0 = 170, by1 = 300;
        ui_.Rect(bx0 - 2, by0 - 2, bx1 + 2, by1 + 2, kGold);
        ui_.Rect(bx0, by0, bx1, by1, UIColor{0.08f, 0.06f, 0.2f, 0.98f});
        ui_.Text(320 - ui_.TextWidth(title, 2) / 2, by0 + 22, title, 2, kWhite);
        ui_.Text(320 - ui_.TextWidth(line2, 1) / 2, by0 + 52, line2, 1, kValue);
    };
    if (mode_ == Mode::CaptureKey) {
        box("Press a key for", captureLabel_);
        std::string hint = captureDik_ == DIK_ESCAPE ? "Backspace: restore default" : "Esc: cancel   Backspace: restore default";
        ui_.Text(320 - ui_.TextWidth(hint, 1) / 2, 270, hint, 1, kGrey);
    } else if (mode_ == Mode::CaptureButton) {
        box("Press a gamepad button for", captureLabel_);
        std::string hint = "Start/Esc: cancel   Backspace: restore default";
        ui_.Text(320 - ui_.TextWidth(hint, 1) / 2, 270, hint, 1, kGrey);
    } else if (mode_ == Mode::ConfirmDisplay) {
        int secs = (int)((confirmDeadline_ > now_ ? confirmDeadline_ - now_ : 0) + 999) / 1000;
        box("Keep these display settings?", "Reverting in " + std::to_string(secs) + " seconds");
        std::string keep = confirmChoice_ == 0 ? "> Keep <" : "  Keep  ";
        std::string rev = confirmChoice_ == 1 ? "> Revert <" : "  Revert  ";
        ui_.Text(220 - ui_.TextWidth(keep, 2) / 2, 258, keep, 2, confirmChoice_ == 0 ? kSelValue : kGrey);
        ui_.Text(420 - ui_.TextWidth(rev, 2) / 2, 258, rev, 2, confirmChoice_ == 1 ? kSelValue : kGrey);
    } else if (mode_ == Mode::ConfirmAction) {
        box(askTitle_, askLine_);
        std::string yes = confirmChoice_ == 0 ? "> Yes <" : "  Yes  ";
        std::string no = confirmChoice_ == 1 ? "> No <" : "  No  ";
        ui_.Text(220 - ui_.TextWidth(yes, 2) / 2, 258, yes, 2, confirmChoice_ == 0 ? kSelValue : kGrey);
        ui_.Text(420 - ui_.TextWidth(no, 2) / 2, 258, no, 2, confirmChoice_ == 1 ? kSelValue : kGrey);
    }
}
