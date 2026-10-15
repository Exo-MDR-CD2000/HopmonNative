// User settings for the enhanced features, stored in hopmon_native.ini.
// None of these change gameplay logic; the simulation always runs the original 10 ms tick.
#pragma once
#include <string>
#include <map>
#include <vector>

enum class AspectMode { Original4x3 = 0, Stretch = 1, Widescreen = 2 };
enum class FullscreenMode { Windowed = 0, Borderless = 1, Exclusive = 2 };

struct Config {
    // video
    int windowWidth = 1280;
    int windowHeight = 960;
    FullscreenMode fullscreen = FullscreenMode::Windowed;
    int fullscreenWidth = 0;       // exclusive fullscreen screen mode; 0 = desktop resolution
    int fullscreenHeight = 0;
    int displayIndex = 0;          // 0 = primary monitor, 1..n = monitors in system order
    bool vsync = true;
    int maxFps = 0;                // 0 = uncapped (or vsync limited)
    bool interpolation = true;     // smooth motion between 100 Hz logic ticks
    int msaa = 4;                  // 0, 2, 4, 8
    int anisotropy = 8;            // 1 = off
    bool smoothTextures = false;   // force linear filtering on 3D textures that used point sampling
    bool smoothSprites = false;    // bilinear scaling of 2D sprites
    bool perPixelLighting = false; // evaluate the original lighting per pixel instead of per vertex
    bool enhancedShadows = false;  // softer blob shadows
    int brightness = 100;          // 50..150 %, applied as a gamma curve to the final picture
    float renderScale = 1.0f;      // supersampling factor for the 3D scene
    AspectMode aspect = AspectMode::Original4x3;
    float fovScale = 1.0f;         // multiplies the original vertical FOV (widescreen keeps vertical FOV)
    bool showFps = false;
    int hudLayout = 1;             // widescreen only: 0 = original centred 4:3 HUD, 1 = HUD at screen edges
    float hudMaxAspect = 2.4f;     // edges layout spreads at most this wide (0 = no limit)
    float hudScale = 1.0f;         // in-game HUD size (0.5..2.0); each part grows from its screen corner

    // audio
    int masterVolume = 50;   // 0..100 (default 50)
    int sfxVolume = 50;
    int musicVolume = 50;
    std::string soundFont;
    int musicSource = 0;            // 0 = MP3/FLAC from the music folder when present, else MIDI; 1 = MIDI only
    std::string musicFiles[4];      // optional file name in the music folder for each track   // optional .sf2 for the built-in synth; otherwise the OS MIDI synth is used

    // input
    int gamepadDeadzone = 25;  // percent
    // Remapping: original DirectInput key code -> list of SDL scancode names that trigger it
    std::map<int, std::vector<std::string>> keyBinds;
    std::map<int, std::vector<std::string>> padBinds;  // joystick button index -> gamepad button names

    std::string gamePath;      // optional explicit path to Hopmon.exe
    std::string savePath;      // [game] save_path: empty = the per-user data folder (see core/saves.h)
    // [cheats] (off by default; also in the pause menu)
    bool unlockAllLevels = false;     // every level selectable; saved progress is left untouched
    bool infiniteFireEnergy = false;  // fire gauge always full: fire on every press (max 3 shots on screen)
    bool noCrystalSlowdown = false;   // carried crystals don't slow Hopmon's hops

    std::string path;
    int version = 0;                    // config_version found in the file (0 = older file)
    static constexpr int kCurrentVersion = 12;
    bool Load(const std::string& file);
    bool Save() const;
};

Config& GetConfig();
