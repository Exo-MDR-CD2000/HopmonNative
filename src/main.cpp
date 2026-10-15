// Entry point (replaces WinMain from XAppMain.cpp)
#include "core/assets.h"
#include "core/config.h"
#include "core/log.h"
#include "audio/audio.h"
#include "audio/midi.h"
#include "audio/music_stream.h"
#include "version.h"
#include "input/input.h"
#include "framework/d3dapp.h"
#include "framework/d3dfile.h"
#include "framework/d3dtextr.h"
#include "framework/d3dutil.h"
#include "gfx/device.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

#ifndef HOPMON_PLATFORM_TEST_ONLY
#include "game/XGame.h"
#endif

// Simple scene used to validate the platform layer (--platform-test)
class PlatformTestApp : public CD3DApplication {
public:
    PlatformTestApp() { m_strWindowTitle = "Hopmon platform test"; }
protected:
    HRESULT OneTimeSceneInit() override {
        mesh_.LoadFromResource(getenv("HOPMON_TEST_MESH") ? (WORD)strtol(getenv("HOPMON_TEST_MESH"), nullptr, 0) : 148);
        D3DTextr_CreateTextureFromResource(0x9c, 0, 0);
        int w, h;
        sprite_ = DDLoadBitmap(0xbf, &w, &h);
        if (sprite_) DDSetColorKey(sprite_, CLR_INVALID);
        return S_OK;
    }
    HRESULT InitDeviceObjects() override {
        Device* d = GetD3DDevice();
        d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, 0);
        d->SetRenderState(D3DRENDERSTATE_DITHERENABLE, 1);
        d->SetRenderState(D3DRENDERSTATE_LIGHTING, 1);
        d->SetRenderState(D3DRENDERSTATE_ZENABLE, 1);
        D3DMATRIX proj;
        D3DUtil_SetProjectionMatrix(proj, 0.7330383f, 0.75f, 10.f, 2000.f);
        d->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &proj);
        D3DLIGHT7 light;
        D3DUtil_InitLight(light, D3DLIGHT_DIRECTIONAL, 0.77f, 0.1f, 0.5f, -1.0f, 0.5f);
        d->SetLight(0, &light);
        d->LightEnable(0, 1);
        d->SetRenderState(D3DRENDERSTATE_AMBIENT, 0xff898989);
        mesh_.Scale(getenv("HOPMON_TEST_SCALE") ? (float)atof(getenv("HOPMON_TEST_SCALE")) : 60.f);
        return S_OK;
    }
    HRESULT FrameMove(DWORD n) override {
        angle_ += 0.01f * n;
        return S_OK;
    }
    HRESULT Render() override {
        Device* d = GetD3DDevice();
        d->Clear(0, nullptr, 3, 0x2d1d62, 1.f, 0);
        d->BeginScene();
        D3DMATRIX view;
        D3DVECTOR from(sinf(angle_) * 100.f, 60.f, -cosf(angle_) * 100.f), at(0, 0, 0), up(0, 1, 0);
        D3DUtil_SetViewMatrix(view, from, at, up);
        d->SetTransform(D3DTRANSFORMSTATE_VIEW, &view);
        D3DMATRIX world;
        D3DUtil_SetIdentityMatrix(world);
        d->SetTransform(D3DTRANSFORMSTATE_WORLD, &world);
        mesh_.Render(d);
        if (sprite_) {
            RECT dst{10, 10, 10 + sprite_->Width(), 10 + sprite_->Height()};
            RECT src{0, 0, sprite_->Width(), sprite_->Height()};
            GetBackBuffer()->Blt(&dst, sprite_, &src, true);
        }
        d->EndScene();
        return S_OK;
    }
private:
    CD3DFile mesh_;
    Surface* sprite_ = nullptr;
    float angle_ = 0;
};

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string basePath = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    LogInit((basePath + "hopmon_native.log").c_str());
    Log("Hopmon Native %s", HOPMON_VERSION_STRING);
    Config& cfg = GetConfig();
    if (!cfg.Load(basePath + "hopmon_native.ini")) {
        cfg.path = basePath + "hopmon_native.ini";
        cfg.Save();
    } else if (cfg.version < Config::kCurrentVersion) {
        cfg.Save();  // rewrite older files with the current comments, keeping their values
    }
    std::string gamePath = cfg.gamePath;
    bool allowUnverified = false, platformTest = false;
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "--game" && i + 1 < args.size()) gamePath = args[++i];
        else if (args[i] == "--allow-unverified") allowUnverified = true;
        else if (args[i] == "--platform-test") platformTest = true;
        else if (args[i] == "--windowed") cfg.fullscreen = FullscreenMode::Windowed;
        else if (args[i] == "--fullscreen") cfg.fullscreen = FullscreenMode::Borderless;
        else if (args[i] == "--size" && i + 2 < args.size()) {
            cfg.windowWidth = atoi(args[++i].c_str());
            cfg.windowHeight = atoi(args[++i].c_str());
        } else if (args[i] == "--set" && i + 1 < args.size()) {
            // --set section.key=value (temporary override, not saved)
            std::string kv = args[++i];
            size_t dot = kv.find('.'), eq = kv.find('=');
            if (dot != std::string::npos && eq != std::string::npos) {
                std::string tmp = basePath + "hopmon_native.override.ini";
                FILE* f = fopen(tmp.c_str(), "w");
                if (f) {
                    fprintf(f, "[%s]\n%s = %s\n", kv.substr(0, dot).c_str(), kv.substr(dot + 1, eq - dot - 1).c_str(),
                            kv.substr(eq + 1).c_str());
                    fclose(f);
                    std::string keep = cfg.path;
                    cfg.Load(tmp);
                    cfg.path = keep;
                    remove(tmp.c_str());
                }
            }
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        FatalError("Hopmon", "SDL initialisation failed: %s", SDL_GetError());
    }
    std::string err;
    if (!Assets::Init(gamePath, allowUnverified, &err)) FatalError("Hopmon", "%s", err.c_str());
    Audio::Init();
    Audio::SetMasterVolumes(cfg.masterVolume / 100.f, cfg.sfxVolume / 100.f, cfg.musicVolume / 100.f);
    Midi::Init(cfg.soundFont);
    MusicStream::Init();
    // The SoundFont synth is mixed through Audio (which applies master/music volume); the Windows
    // synth bypasses the mixer, so it gets the combined volume directly.
    Midi::SetVolume(cfg.masterVolume / 100.f * cfg.musicVolume / 100.f);
    Log("Audio: master %d%%, sfx %d%%, music %d%% (music: %s)", cfg.masterVolume, cfg.sfxVolume,
        cfg.musicVolume, Midi::BackendName());
    Input::Init();

    int res = 0;
    if (platformTest) {
        PlatformTestApp app;
        if (FAILED(app.Create(args))) FatalError("Hopmon", "Could not create the game window (OpenGL 3.3 is required).");
        res = app.Run();
    } else {
#ifndef HOPMON_PLATFORM_TEST_ONLY
        res = HopmonMain(args);
#endif
    }
    Input::Shutdown();
    MusicStream::Shutdown();
    Midi::Shutdown();
    Audio::Shutdown();
    SDL_Quit();
    return res;
}
