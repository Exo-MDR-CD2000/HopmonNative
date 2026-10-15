#include "d3dapp.h"
#include "interp.h"
#include "d3dtextr.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../gfx/device.h"
#include "../input/input.h"
#include "../audio/midi.h"
#include "../ui/options_menu.h"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <sstream>

static float g_tickAlpha = 0.f;
static uint64_t g_timeBase = 0;

DWORD AppTimeMs() { return (DWORD)(SDL_GetTicks() - g_timeBase + 1000); }
float AppTickAlpha() { return g_tickAlpha; }

HRESULT BackBuffer::Blt(const RECT* dst, Surface* src, const RECT* srcRect, bool keySrc) {
    return GetDevice().BltToBackBuffer(dst, src, srcRect, keySrc);
}
HRESULT BackBuffer::ColorFill(const RECT* dst, uint32_t rgb) { return GetDevice().ColorFill(dst, rgb); }

CD3DApplication::CD3DApplication() : menu_(std::make_unique<OptionsMenu>(this)) {}
CD3DApplication::~CD3DApplication() {
    menu_.reset();  // owns a GL texture: free it while the context still exists
    D3DTextr_FreeAll();
    GetDevice().Shutdown();
    if (glContext_) SDL_GL_DestroyContext((SDL_GLContext)glContext_);
    if (window_) SDL_DestroyWindow(window_);
}

Device* CD3DApplication::GetD3DDevice() { return &GetDevice(); }
int CD3DApplication::DisplayFPS() const { return GetConfig().showFps ? 1 : 0; }

bool CD3DApplication::OptionsOpen() const { return menu_ && menu_->IsOpen(); }

void CD3DApplication::ApplyVSync() { SDL_GL_SetSwapInterval(GetConfig().vsync ? 1 : 0); }

// Monitor selected in the config (0 = primary).
static SDL_DisplayID ConfigDisplay() {
    int idx = GetConfig().displayIndex;
    SDL_DisplayID d = SDL_GetPrimaryDisplay();
    if (idx > 0) {
        int n = 0;
        SDL_DisplayID* ids = SDL_GetDisplays(&n);
        if (ids && idx <= n) d = ids[idx - 1];
        SDL_free(ids);
    }
    return d;
}

// Borderless mode on Windows is a plain borderless window rather than SDL's "desktop fullscreen":
// GPU drivers promote an OpenGL window that exactly covers a monitor to exclusive fullscreen,
// which makes Alt+Tab flicker, black out or minimize the game. One extra pixel of height keeps
// the window from matching the monitor exactly (that row is off-screen). Elsewhere SDL's
// desktop fullscreen already behaves as borderless.
static bool UseBorderlessWindow() {
#if defined(_WIN32)
    return true;
#else
    return SDL_getenv("HOPMON_BORDERLESS_WINDOW") != nullptr;  // for testing the Windows path
#endif
}

void CD3DApplication::ApplyDisplaySettings() {
    const Config& cfg = GetConfig();
    SDL_DisplayID target = ConfigDisplay();
    // leave fullscreen first so size and monitor changes take effect cleanly
    SDL_SetWindowFullscreen(window_, false);
    SDL_SyncWindow(window_);

    if (cfg.fullscreen == FullscreenMode::Windowed) {
        SDL_SetWindowBordered(window_, true);
        SDL_SetWindowResizable(window_, true);
        SDL_SetWindowSize(window_, cfg.windowWidth, cfg.windowHeight);
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED_DISPLAY(target), SDL_WINDOWPOS_CENTERED_DISPLAY(target));
        SDL_SyncWindow(window_);
        Log("Display: window %dx%d, monitor %d", cfg.windowWidth, cfg.windowHeight, cfg.displayIndex);
    } else if (cfg.fullscreen == FullscreenMode::Borderless) {
        SDL_Rect b{0, 0, cfg.windowWidth, cfg.windowHeight};
        SDL_GetDisplayBounds(target, &b);
        if (UseBorderlessWindow()) {
            SDL_SetWindowResizable(window_, false);
            SDL_SetWindowBordered(window_, false);
            SDL_SetWindowPosition(window_, b.x, b.y);
            SDL_SetWindowSize(window_, b.w, b.h + 1);
            SDL_SyncWindow(window_);
            SDL_RaiseWindow(window_);
        } else {
            SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED_DISPLAY(target), SDL_WINDOWPOS_CENTERED_DISPLAY(target));
            SDL_SyncWindow(window_);
            SDL_SetWindowFullscreenMode(window_, nullptr);
            SDL_SetWindowFullscreen(window_, true);
            SDL_SyncWindow(window_);
        }
        Log("Display: borderless %dx%d, monitor %d", b.w, b.h, cfg.displayIndex);
    } else {
        // exclusive: desktop resolution unless a specific mode was chosen
        SDL_SetWindowBordered(window_, true);
        SDL_SetWindowResizable(window_, true);
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED_DISPLAY(target), SDL_WINDOWPOS_CENTERED_DISPLAY(target));
        SDL_SyncWindow(window_);
        SDL_DisplayMode mode{};
        bool have = false;
        if (cfg.fullscreenWidth > 0 && cfg.fullscreenHeight > 0)
            have = SDL_GetClosestFullscreenDisplayMode(target, cfg.fullscreenWidth, cfg.fullscreenHeight, 0.f, true, &mode);
        if (!have) {
            if (const SDL_DisplayMode* desk = SDL_GetDesktopDisplayMode(target)) {
                mode = *desk;
                have = true;
            }
        }
        SDL_SetWindowFullscreenMode(window_, have ? &mode : nullptr);
        SDL_SetWindowFullscreen(window_, true);
        SDL_SyncWindow(window_);
        Log("Display: exclusive %dx%d @ %.0f Hz, monitor %d", mode.w, mode.h, mode.refresh_rate, cfg.displayIndex);
    }
}

void CD3DApplication::ToggleFullscreen() {
    Config& cfg = GetConfig();
    cfg.fullscreen = cfg.fullscreen == FullscreenMode::Windowed ? FullscreenMode::Borderless : FullscreenMode::Windowed;
    ApplyDisplaySettings();
}

HRESULT CD3DApplication::Create(const std::vector<std::string>& args) {
    Config& cfg = GetConfig();
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == "--auto" && i + 1 < args.size()) {
            std::stringstream ss(args[++i]);
            std::string item;
            while (std::getline(ss, item, ';')) {
                size_t c = item.find(':');
                if (c == std::string::npos) continue;
                AutoStep s;
                s.t = (DWORD)atoi(item.substr(0, c).c_str());
                std::string act = item.substr(c + 1);
                size_t eq = act.find('=');
                s.action = act.substr(0, eq);
                s.arg = eq == std::string::npos ? "" : act.substr(eq + 1);
                s.done = false;
                auto_.push_back(s);
            }
        }
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    window_ = SDL_CreateWindow(m_strWindowTitle, cfg.windowWidth, cfg.windowHeight,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!window_) {
        Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return E_FAIL;
    }
    SDL_SetWindowMinimumSize(window_, 320, 240);
    if (cfg.displayIndex > 0) {
        int n = 0;
        SDL_DisplayID* ids = SDL_GetDisplays(&n);
        if (ids && cfg.displayIndex <= n) {
            SDL_DisplayID d = ids[cfg.displayIndex - 1];
            SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED_DISPLAY(d), SDL_WINDOWPOS_CENTERED_DISPLAY(d));
        } else {
            Log("Config: display %d not found (%d connected), using the primary monitor", cfg.displayIndex, n);
        }
        SDL_free(ids);
    }
    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) {
        Log("OpenGL 3.3 context creation failed: %s", SDL_GetError());
        return E_FAIL;
    }
    SDL_GL_MakeCurrent(window_, (SDL_GLContext)glContext_);
    SDL_GL_SetSwapInterval(cfg.vsync ? 1 : 0);
    if (!GetDevice().Init(window_)) return E_FAIL;
    if (cfg.fullscreen != FullscreenMode::Windowed) ApplyDisplaySettings();
    SDL_ShowWindow(window_);
    m_bActive = true;

    g_timeBase = SDL_GetTicks();
    HRESULT hr = OneTimeSceneInit();
    if (FAILED(hr)) return hr;
    hr = InitDeviceObjects();
    if (FAILED(hr)) return hr;
    m_bReady = true;
    m_dwLastFrameTime = AppTimeMs();
    autoStart_ = AppTimeMs();
    return S_OK;
}

void CD3DApplication::Pause(bool pause) {
    (void)pause;  // time base handling is implicit: Render3DEnvironment clamps catch-up to 20 ticks
}

void CD3DApplication::UpdateStats() {
    float t = AppTimeMs() * 0.001f;
    statsFrames_++;
    if (t - statsLastTime_ > 1.0f) {
        m_fFPS = statsFrames_ / (t - statsLastTime_);
        statsFrames_ = 0;
        statsLastTime_ = t;
    }
}

HRESULT CD3DApplication::Render3DEnvironment(DWORD now, bool forceRender) {
    if (now < m_dwLastFrameTime) m_dwLastFrameTime = now;
    menu_->Update(now);
    if (menu_->IsOpen()) {
        // game paused underneath: keep showing the last frame, don't accumulate catch-up time
        m_dwLastFrameTime = now;
        g_tickAlpha = 1.f;
        GetDevice().BeginFrame();
        HRESULT hr = Render();
        if (FAILED(hr)) return hr;
        menu_->Render(&GetDevice());
        UpdateStats();
        GetDevice().Present();
        SDL_GL_SwapWindow(window_);
        return S_OK;
    }
    DWORD elapsed = now - m_dwLastFrameTime;
    DWORD ticks = elapsed / 10;
    if (ticks == 0 && !forceRender) return S_OK;
    if (ticks > 0) {
        m_dwLastFrameTime = now - elapsed % 10;
        if (m_bFrameMoving || m_bSingleStep) {
            Interp::BeginFrameMove();
            HRESULT hr = FrameMove(ticks);
            Interp::EndFrameMove();
            if (FAILED(hr)) return hr;
            m_bSingleStep = false;
        }
    }
    g_tickAlpha = (float)(now - m_dwLastFrameTime) / 10.f;
    if (g_tickAlpha > 1.f) g_tickAlpha = 1.f;
    if (!m_bFrameMoving) g_tickAlpha = 1.f;
    GetDevice().BeginFrame();
    HRESULT hr = Render();
    if (FAILED(hr)) return hr;
    UpdateStats();
    GetDevice().Present();
    SDL_GL_SwapWindow(window_);
    return S_OK;
}

void CD3DApplication::HandleAutomation(DWORD now) {
    DWORD t = now - autoStart_;
    for (auto& s : auto_) {
        if (s.done || t < s.t) continue;
        s.done = true;
        if (s.action == "key" || s.action == "down" || s.action == "up") {
            SDL_Scancode sc = SDL_GetScancodeFromName(s.arg.c_str());
            int dik = Input::DikFromScancode(sc);
            if (s.action != "up") {
                Input::InjectKey(dik, true);
                OnKeyDown(dik);
            }
            if (s.action != "down") {
                // a "key" tap stays down for 50 ms so the 100 Hz polling sees it
                AutoStep rel{s.t + 50, "up", s.arg, false};
                if (s.action == "up") Input::InjectKey(dik, false);
                else auto_.push_back(rel);
            }
            return;  // vector may have grown
        } else if (s.action == "sdlkey") {
            // a real key press through the SDL event queue (reaches the options menu)
            SDL_Event ev{};
            ev.type = SDL_EVENT_KEY_DOWN;
            ev.key.scancode = SDL_GetScancodeFromName(s.arg.c_str());
            ev.key.down = true;
            ev.key.windowID = SDL_GetWindowID(window_);
            SDL_PushEvent(&ev);
            ev.type = SDL_EVENT_KEY_UP;
            ev.key.down = false;
            SDL_PushEvent(&ev);
        } else if (s.action == "focus") {
            // test-only: simulate the window losing (focus=0) or regaining (focus=1) focus
            SDL_Event ev{};
            ev.type = s.arg == "0" ? SDL_EVENT_WINDOW_FOCUS_LOST : SDL_EVENT_WINDOW_FOCUS_GAINED;
            ev.window.windowID = SDL_GetWindowID(window_);
            SDL_PushEvent(&ev);
        } else if (s.action == "vpad") {
            // test-only virtual controller: "vpad", "padaxis=<axis>:<value>", "padbtn=<button>:<0|1>"
            SDL_VirtualJoystickDesc desc;
            SDL_INIT_INTERFACE(&desc);
            desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
            desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
            desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
            desc.name = "Automation pad";
            SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
            autoPad_ = SDL_OpenJoystick(id);
            Log("auto: virtual pad %u %s", (unsigned)id, autoPad_ ? "attached" : SDL_GetError());
        } else if ((s.action == "padaxis" || s.action == "padbtn") && autoPad_) {
            size_t c = s.arg.find(':');
            int idx = atoi(s.arg.substr(0, c).c_str()), val = atoi(s.arg.substr(c + 1).c_str());
            if (s.action == "padaxis") SDL_SetJoystickVirtualAxis((SDL_Joystick*)autoPad_, idx, (Sint16)val);
            else SDL_SetJoystickVirtualButton((SDL_Joystick*)autoPad_, idx, val != 0);
        } else if (s.action == "shot") {
            GetDevice().SaveScreenshot(s.arg.c_str());
            Log("auto: screenshot %s", s.arg.c_str());
        } else if (s.action == "quit") {
            quit_ = true;
        } else {
            OnAutomation(s.action, s.arg);
        }
    }
}

int CD3DApplication::Run() {
    const Config& cfg = GetConfig();
    uint64_t perfFreq = SDL_GetPerformanceFrequency();
    uint64_t lastPresent = SDL_GetPerformanceCounter();
    while (!quit_) {
        SDL_Event e;
        bool gotEvent = m_bActive ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 100);
        while (gotEvent) {
            Input::HandleEvent(e);
            switch (e.type) {
                case SDL_EVENT_QUIT: quit_ = true; break;
                case SDL_EVENT_WINDOW_FOCUS_GAINED: Input::SetFocus(true); OnActivateApp(true); break;
                case SDL_EVENT_WINDOW_FOCUS_LOST: Input::SetFocus(false); OnActivateApp(false); break;
                case SDL_EVENT_WINDOW_MINIMIZED: m_bActive = false; break;
                case SDL_EVENT_WINDOW_RESTORED:
                case SDL_EVENT_WINDOW_MAXIMIZED:
                case SDL_EVENT_WINDOW_SHOWN:
                    if (!m_bActive) m_dwLastFrameTime = AppTimeMs();
                    m_bActive = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (!e.key.repeat && ((e.key.scancode == SDL_SCANCODE_RETURN && (e.key.mod & SDL_KMOD_ALT)) ||
                                          e.key.scancode == SDL_SCANCODE_F11 || e.key.scancode == SDL_SCANCODE_F12)) {
                    } else if (menu_->HandleEvent(e)) {
                        break;
                    }
                    if (e.key.repeat) break;
                    if ((e.key.scancode == SDL_SCANCODE_RETURN && (e.key.mod & SDL_KMOD_ALT)) ||
                        e.key.scancode == SDL_SCANCODE_F11) {
                        ToggleFullscreen();
                        break;
                    }
                    if (e.key.scancode == SDL_SCANCODE_F12) {
                        char name[1024];
                        const char* base = SDL_GetBasePath();
                        snprintf(name, sizeof name, "%shopmon_%u.bmp", base ? base : "", (unsigned)AppTimeMs());
                        GetDevice().SaveScreenshot(name);
                        Log("Saved screenshot %s", name);
                        break;
                    }
                    {
                        std::vector<int> diks;
                        Input::DiksForScancode(e.key.scancode, &diks);
                        for (int d : diks) OnKeyDown(d);
                    }
                    break;
                default:
                    menu_->HandleEvent(e);  // gamepad: Start opens the menu; navigation while open
                    break;
            }
            gotEvent = SDL_PollEvent(&e);
        }
        if (quit_) break;
        if (Midi::PollFinished()) OnMidiNotifySuccessful();
        DWORD now = AppTimeMs();
        if (!auto_.empty()) HandleAutomation(now);
        if (!m_bActive || !m_bReady) continue;

        // frame pacing for the interpolated (decoupled) path; the options menu always redraws (max 60 fps)
        bool menuOpen = menu_->IsOpen();
        bool force = cfg.interpolation || menuOpen;
        int cap = menuOpen ? 60 : cfg.maxFps;
        if (force && cap > 0) {
            double minDt = 1.0 / cap;
            double dt = (double)(SDL_GetPerformanceCounter() - lastPresent) / perfFreq;
            if (dt < minDt) {
                SDL_DelayPrecise((uint64_t)((minDt - dt) * 1e9 * 0.9));
                continue;
            }
        }
        DWORD before = m_dwLastFrameTime;
        if (FAILED(Render3DEnvironment(now, force))) quit_ = true;
        if (!force && m_dwLastFrameTime == before) SDL_Delay(1);  // original only renders on tick boundaries
        else lastPresent = SDL_GetPerformanceCounter();
    }
    DeleteDeviceObjects();
    FinalCleanup();
    return 0;
}
