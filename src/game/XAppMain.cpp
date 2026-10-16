// Port of XAppMain.cpp / XRegister.cpp
#include "XGame.h"
#include "../core/saves.h"
#include "../core/mods.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../framework/d3dtextr.h"
#include "../framework/d3dfile.h"
#include "../framework/d3dutil.h"
#include "../gfx/device.h"
#include <SDL3/SDL.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ============================================================ XRegister.cpp
// The 2022 release ships with registration permanently unlocked.
struct JREGISTERINFO { int registered; char regKey[40]; };
static JREGISTERINFO* g_pRegInfo = nullptr;

void LoadRegisterInfo(const char*) {
    delete g_pRegInfo;
    g_pRegInfo = new JREGISTERINFO();
    memset(g_pRegInfo, 0, sizeof *g_pRegInfo);
    g_pRegInfo->registered = 1;  // _CheckRegistrationKey()
}
void SaveRegisterInfo(const char*) {}
void DeleteRegisterInfo() {
    delete g_pRegInfo;
    g_pRegInfo = nullptr;
}
int IsRegistered() { return 1; }

// ============================================================ Sticky keys
#if defined(_WIN32)
static STICKYKEYS g_prevStickyKeys = {sizeof(STICKYKEYS), 0};
static TOGGLEKEYS g_prevToggleKeys = {sizeof(TOGGLEKEYS), 0};
static FILTERKEYS g_prevFilterKeys = {sizeof(FILTERKEYS), 0};
static void DisableStickyKeys() {
    SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &g_prevStickyKeys, 0);
    SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_prevToggleKeys, 0);
    SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &g_prevFilterKeys, 0);
    STICKYKEYS sk = g_prevStickyKeys;
    if ((sk.dwFlags & SKF_STICKYKEYSON) == 0) {
        sk.dwFlags &= ~SKF_HOTKEYACTIVE;
        sk.dwFlags &= ~SKF_CONFIRMHOTKEY;
        SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &sk, 0);
    }
    TOGGLEKEYS tk = g_prevToggleKeys;
    if ((tk.dwFlags & TKF_TOGGLEKEYSON) == 0) {
        tk.dwFlags &= ~TKF_HOTKEYACTIVE;
        tk.dwFlags &= ~TKF_CONFIRMHOTKEY;
        SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tk, 0);
    }
    FILTERKEYS fk = g_prevFilterKeys;
    if ((fk.dwFlags & FKF_FILTERKEYSON) == 0) {
        fk.dwFlags &= ~FKF_HOTKEYACTIVE;
        fk.dwFlags &= ~FKF_CONFIRMHOTKEY;
        SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fk, 0);
    }
}
static void RestoreStickyKeys() {
    SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &g_prevStickyKeys, 0);
    SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &g_prevToggleKeys, 0);
    SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &g_prevFilterKeys, 0);
}
#else
static void DisableStickyKeys() {}
static void RestoreStickyKeys() {}
#endif

// ============================================================ XFPSDisplay
XFPSDisplay::XFPSDisplay(XApplication* a) : app(a) {
    textFPS = new JSprite(app->GetSurfaceList(), 0xbf, 1);
    numFPS = new JNumber(app->GetSurfaceList(), 0xbb, 10);
    numFPS->SetDisplayZero(1);
    numFPS->SetNumDigits(3);
}

XFPSDisplay::~XFPSDisplay() {
    // the original never freed these; harmless to do so
    delete textFPS;
    delete numFPS;
}

void XFPSDisplay::Render() {
    if (!app->DisplayFPS()) return;
    int posX = app->GetScreenWidth() - 0x5a + GetDevice().HudEdgeShift();
    int posY = 4;
    // hud_scale: grows from the top-right corner
    const float hs = GetConfig().hudScale;
    const float ax = (float)(app->GetScreenWidth() + GetDevice().HudEdgeShift());
    // at large HUD sizes the level/crystals line can reach this corner: drop below it instead
    if (ax + (posX - ax) * hs < GetDevice().HudTopLeftRight() + 6.f)
        posY = (int)std::ceil((GetDevice().HudTopLeftBottom() + 4.f) / hs);
    GetDevice().BeginHudScale(hs, ax, 0.f);
    textFPS->SetPosition(posX, posY);
    textFPS->Draw(app->GetBackBuffer());
    posX = posX + textFPS->GetWidth() + 4;
    numFPS->SetPosition(posX, posY);
    numFPS->SetNumber(app->GetFPS());
    numFPS->Draw(app->GetBackBuffer());
    GetDevice().EndHudScale();
}

// ============================================================ XApplication
XApplication::XApplication()
    : currentLevel(0), maxSelLevel(0), maxFreeLevel(9),
      maxLevel((int)Mods::ActivePack().levels.size() - 1),  // native: 0x2c for the original game
      tempList(new JStringList()), state(NON),
      dinput(nullptr), midiList(nullptr), soundList(nullptr), surfaceList(nullptr), processObj(nullptr),
      fpsDisplay(nullptr) {
    DisableStickyKeys();
    m_strWindowTitle = "Hopmon";
    m_bAppUseZBuffer = true;
    surfaceList = new JSurfaceList(this);
    LoadWinRegData();
}

XApplication::~XApplication() {
    SaveWinRegData();
    DeleteRegisterInfo();
    RestoreStickyKeys();
}

// Native addition: [cheats] unlock_all_levels widens what the level-select screen offers without
// touching maxSelLevel, so the saved progress stays the player's real progress.
int XApplication::SelectableLevel() const { return GetConfig().unlockAllLevels ? maxLevel : maxSelLevel; }

// Native: progress lives in <save dir>/original/progress.ini (see core/saves.h) instead of the
// registry key HKCU\Software\SaitoGames\Hopmon that the original used; each level pack keeps its
// own in <save dir>/packs/<id>/progress.ini. On the very first start with no saved progress, the
// original's registry value is imported once (read only, so the original Hopmon.exe keeps its
// own progress untouched).
void XApplication::LoadWinRegData() {
    LoadRegisterInfo("Software\\SaitoGames\\Hopmon");
    const Mods::Pack& pack = Mods::ActivePack();
    maxSelLevel = 0;
    Saves::PackSave save;
    if (Saves::Load(pack.id, &save)) {
        maxSelLevel = save.GetInt("progress", "unlocked", 1) - 1;
        if (save.packVersion != pack.version)
            Log("Saves: progress for '%s' was saved with version %s of the pack (now %s)", pack.id.c_str(),
                save.packVersion.c_str(), pack.version.c_str());
    } else if (pack.id == "original") {
        int legacy = 0;
        if (Saves::TakeLegacyLevelOnce(&legacy) && legacy >= 0 && legacy <= maxLevel) {
            maxSelLevel = legacy;
            Log("Saves: imported progress from the original game (level %d unlocked)", legacy + 1);
            SaveWinRegData();
        }
    }
    if (maxSelLevel < 0) maxSelLevel = 0;
    if (maxSelLevel > maxLevel) maxSelLevel = maxLevel;  // a pack may have become shorter
}

void XApplication::SaveWinRegData() {
    SaveRegisterInfo("Software\\SaitoGames\\Hopmon");
    const Mods::Pack& pack = Mods::ActivePack();
    Saves::PackSave save;
    if (!Saves::Load(pack.id, &save)) save = Saves::PackSave();
    save.pack = pack.id;
    save.packVersion = pack.version;
    save.SetInt("progress", "unlocked", maxSelLevel + 1);
    Saves::Store(save);
}

// ---- Native: level packs
int XApplication::GetCurrentWorld() const {
    const Mods::Pack& p = Mods::ActivePack();
    if (currentLevel >= 0 && currentLevel < (int)p.levelWorld.size()) return p.worlds[p.levelWorld[currentLevel]].look;
    return 0;
}

WORD XApplication::CurrentFloorTexture() const {
    const Mods::Pack& p = Mods::ActivePack();
    if (currentLevel >= 0 && currentLevel < (int)p.levelWorld.size()) return p.worlds[p.levelWorld[currentLevel]].floorTexture;
    return 0x9c;
}

void XApplication::PlayLevelMusic() {
    const Mods::Pack& p = Mods::ActivePack();
    int look = GetCurrentWorld();
    int track = look + 1;  // the original: Music02 castle, Music03 forest, Music04 toy
    if (currentLevel >= 0 && currentLevel < (int)p.levelWorld.size()) {
        const Mods::World& w = p.worlds[p.levelWorld[currentLevel]];
        if (!w.musicFile.empty()) track = midiList->TrackFor(w.musicFile);
        else if (w.musicTrack >= 0) track = w.musicTrack;
    }
    midiList->Play(track);
}

// Floor textures that only exist in a level pack get their texture objects here (the originals'
// are created in OneTimeSceneInit).
void XApplication::CreatePackTextures() {
    for (const Mods::World& w : Mods::ActivePack().worlds)
        if (w.floorTexture >= 0x8000) D3DTextr_CreateTextureFromResource(w.floorTexture, 0, 0);
}

void XApplication::ApplyModChanges() {
    SaveWinRegData();  // progress of the pack that was in play
    if (midiList) midiList->Stop();
    if (soundList) soundList->StopAll();
    Mods::Init();
    CD3DFile::ClearCache();
    D3DTextr_ReloadAll();
    CreatePackTextures();
    D3DTextr_RestoreAllTextures();
    surfaceList->RestoreAllSurfaces();
    if (soundList) soundList->ReloadAll();
    maxLevel = (int)Mods::ActivePack().levels.size() - 1;
    currentLevel = 0;
    LoadWinRegData();
    GetConfig().Save();
    Log("Mods: changes applied; level pack: %s (%d levels)", Mods::ActivePack().name.c_str(), maxLevel + 1);
    // the title screen starts again with the new assets (and the pack's progress)
    if (state == IN_PROCESS || state == NON) state = START_TITLE;
}

HRESULT XApplication::Create(const std::vector<std::string>& args) {
    // Device objects are created by CD3DApplication::Create -> OneTimeSceneInit/InitDeviceObjects.
    // The original created input/audio after the framework, which needs the window; same order here.
    HRESULT hr = CD3DApplication::Create(args);
    if (FAILED(hr)) return hr;

    dinput = new JDInput();
    if (!dinput->Create()) return S_OK;  // original: return 0 (S_OK) without finishing setup

    midiList = new JMidiList();
    midiList->Append("Music01.mid");
    midiList->Append("Music02.mid");
    midiList->Append("Music03.mid");
    midiList->Append("Music04.mid");

    soundList = new JSoundList();
    soundList->Create();
    soundList->CreateSound(0xf3, 4);
    soundList->CreateSound(0xf4, 4);
    soundList->CreateSound(0xf5, 4);
    soundList->CreateSound(0x13e, 4);
    soundList->CreateSound(0x13f, 2);
    soundList->CreateSound(0xeb, 1);
    soundList->CreateSound(0xea, 1);
    soundList->CreateSound(0xff, 1);
    soundList->CreateSound(0xee, 1);
    soundList->CreateSound(0xef, 1);
    soundList->CreateSound(0xf0, 1);
    soundList->CreateSound(0xf1, 1);
    soundList->CreateSound(0xe8, 1);
    soundList->CreateSound(0xe9, 1);
    soundList->CreateSound(0xfe, 1);
    soundList->CreateSound(0xec, 1);
    soundList->CreateSound(0xed, 1);
    soundList->CreateSound(0x13d, 1);

    surfaceList->CreateSurface(0xc3, 0);
    surfaceList->CreateSurface(0xc2, 0);
    surfaceList->CreateSurface(0xbf, 1);
    surfaceList->CreateSurface(0xbb, 1);

    fpsDisplay = new XFPSDisplay(this);
    state = START_LOGO;
    return S_OK;
}

void XApplication::OnActivateApp(bool active) {
    if (active) {
        if (midiList) midiList->Resume();
        if (dinput) {
            dinput->SetAcquireKeyboard(1);
            dinput->SetAcquireJoystick(1);
        }
    } else {
        if (midiList) midiList->Pause();
        if (dinput) {
            dinput->SetAcquireKeyboard(0);
            dinput->SetAcquireJoystick(0);
        }
    }
}

// Test automation only (--auto): "skiplevel" acts as if the current level had been completed.
void XApplication::OnAutomation(const std::string& action, const std::string& arg) {
    (void)arg;
    if (action == "flyup") {  // test: start the level-complete sequence as if all crystals were delivered
        if (auto* g = dynamic_cast<XGameObject*>(processObj)) g->pac->StartLevelCompleted();
    } else if (action == "skiplevel") {
        if (auto* g = dynamic_cast<XGameObject*>(processObj)) {
            Log("auto: skip level %d", currentLevel + 1);
            g->StartFadeOut(1);
        }
    }
}

// Pause-menu hooks (native addition). The menu uses the title screen's own cursor sounds.
void XApplication::PlayUISound(UISound s) {
    if (soundList) soundList->Play(s == UISound::Move ? 0xec : 0xed, 100, 0);
}

bool XApplication::CanReturnToMainMenu() const {
    // already there (or still on the logo, which leads there by itself)
    return !(state == IN_PROCESS && (dynamic_cast<XTitleObject*>(processObj) || dynamic_cast<XLogoObject*>(processObj)));
}

// Native addition: the options menu stays closed during non-interactive sequences. In a level it
// follows the game's own input rule: the player controls Hopmon only while he is waiting, moving
// or riding an arrow, so the level-complete flight, the death animation and the shutter between
// levels are all excluded, and the menu is back as soon as the next level accepts input.
// On the title screen and level select, F10 / Start do nothing: the title screen has its own
// OPTIONS entry (which opens the menu directly, without asking here).
bool XApplication::CanOpenOptions() const {
    if (state == START_LOGO || state == START_ENDING || state == START_TITLE || state == START_SELLEVEL) return false;
    if (dynamic_cast<XLogoObject*>(processObj) || dynamic_cast<XEndingObject*>(processObj)) return false;
    if (dynamic_cast<XTitleObject*>(processObj) || dynamic_cast<XSelectLevelObject*>(processObj)) return false;
    if (auto* g = dynamic_cast<XGameObject*>(processObj)) {
        if (state != IN_PROCESS || !g->pac) return false;
        XPac::STATE ps = g->pac->GetState();
        return ps == XPac::WAIT || ps == XPac::MOVE || ps == XPac::ON_ARROW;
    }
    return true;
}

bool XApplication::InLevel() const { return state == IN_PROCESS && dynamic_cast<XGameObject*>(processObj); }

void XApplication::ReturnToMainMenu() {
    // Same path the game takes between screens: the next FrameMove replaces the current screen
    // with a fresh title screen (which restarts the title music and opens with the shutter).
    if (CanReturnToMainMenu() && state != EXIT && state != NON) state = START_TITLE;
}

void XApplication::OnKeyDown(int dik) {
    // WM_KEYDOWN 'P' -> IDM_TOGGLESTART (pause/unpause the simulation)
    if (dik == DIK_P) m_bFrameMoving = !m_bFrameMoving;
}

void XApplication::OnMidiNotifySuccessful() {
    if (midiList) midiList->Replay();
}

HRESULT XApplication::OneTimeSceneInit() {
    D3DTextr_CreateTextureFromResource(0x9c, 0, 0);
    D3DTextr_CreateTextureFromResource(0xfb, 0, 0);
    D3DTextr_CreateTextureFromResource(0xfc, 0, 0);
    D3DTextr_CreateTextureFromResource(0xa0, 0, 0);
    D3DTextr_CreateTextureFromResource(0xa1, 0, 0);
    D3DTextr_CreateTextureFromResource(0xa9, 0, 0);
    D3DTextr_CreateTextureFromResource(0xaa, 0, 0);
    D3DTextr_CreateTextureFromResource(0xb6, 0, 0);
    D3DTextr_CreateTextureFromResource(0xb7, 0, 0);
    D3DTextr_CreateTextureFromResource(0xdc, 2, 0);  // steam puff: flags = D3DTEXTR_TRANSPARENTBLACK
    D3DTextr_CreateTextureFromResource(0xa2, 0, 0);
    D3DTextr_CreateTextureFromResource(0xa3, 0, 0);
    D3DTextr_CreateTextureFromResource(0xc1, 0, 0);
    D3DTextr_CreateTextureFromResource(0x14c, 0, 0);
    CreatePackTextures();  // native: a level pack's own floor textures
    return S_OK;
}

HRESULT XApplication::InitDeviceObjects() {
    surfaceList->RestoreAllSurfaces();
    D3DTextr_RestoreAllTextures();
    Device* d = GetD3DDevice();
    d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
    d->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE);
    d->SetRenderState(D3DRENDERSTATE_LIGHTING, TRUE);
    d->SetRenderState(D3DRENDERSTATE_ZENABLE, TRUE);
    D3DMATRIX projMatrix;
    D3DUtil_SetProjectionMatrix(projMatrix, 0.7330383f, 0.75f, 10.0f, 2000.0f);
    d->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projMatrix);
    D3DLIGHT7 light;
    D3DUtil_InitLight(light, D3DLIGHT_DIRECTIONAL, 0.77f, 0.1f, 0.5f, -1.0f, 0.5f);
    d->SetLight(0, &light);
    d->LightEnable(0, TRUE);
    d->SetRenderState(D3DRENDERSTATE_AMBIENT, 0xff898989);
    return S_OK;
}

HRESULT XApplication::DeleteDeviceObjects() {
    if (surfaceList) surfaceList->InvalidateAllSurfaces();
    D3DTextr_InvalidateAllTextures();
    return S_OK;
}

HRESULT XApplication::Render() {
    Device* d = GetD3DDevice();
    // Native: the fov_scale enhancement is for gameplay only (title, level select, ending use the original lens)
    d->SetGameplayView(state == IN_PROCESS && dynamic_cast<XGameObject*>(processObj) != nullptr);
    d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x2d1d62, 1.0f, 0);
    if (state == IN_PROCESS && SUCCEEDED(d->BeginScene())) {
        processObj->Render();
        fpsDisplay->Render();
        d->EndScene();
    }
    return S_OK;
}

HRESULT XApplication::FrameMove(DWORD frameCount) {
    if (frameCount > 20) frameCount = 20;
    if (state == IN_PROCESS) processObj->ProcessFrame(frameCount);
    auto replace = [&](XProcessObject* p) {
        delete processObj;
        processObj = nullptr;
        processObj = p;
        state = IN_PROCESS;
    };
    if (state == START_LOGO) {
        delete processObj;
        processObj = nullptr;
        replace(new XLogoObject(this));
    } else if (state == START_TITLE) {
        delete processObj;
        processObj = nullptr;
        replace(new XTitleObject(this));
    } else if (state == START_SELLEVEL) {
        delete processObj;
        processObj = nullptr;
        replace(new XSelectLevelObject(this));
    } else if (state == START_GAME) {
        delete processObj;
        processObj = nullptr;
        replace(new XGameObject(this));
    } else if (state == START_REGINFO) {
        delete processObj;
        processObj = nullptr;
        replace(new XRegisterInfoObject(this));
    } else if (state == START_ENDING) {
        delete processObj;
        processObj = nullptr;
        replace(new XEndingObject(this));
    } else if (state == EXIT) {
        RequestQuit();  // PostMessage(WM_COMMAND, IDM_EXIT)
        state = NON;
    }
    return S_OK;
}

HRESULT XApplication::FinalCleanup() {
    delete processObj;
    processObj = nullptr;
    CD3DFile::ClearCache();  // native: mod models own GL textures; free them while the context exists
    delete fpsDisplay;
    fpsDisplay = nullptr;
    delete surfaceList;
    surfaceList = nullptr;
    delete soundList;
    soundList = nullptr;
    delete midiList;
    midiList = nullptr;
    delete dinput;
    dinput = nullptr;
    delete tempList;
    tempList = nullptr;
    return S_OK;
}

HRESULT XApplication::RestoreSurfaces() {
    surfaceList->RestoreAllSurfaces();
    return S_OK;
}

// ============================================================ WinMain
int HopmonMain(const std::vector<std::string>& args) {
#if defined(_WIN32)
    HANDLE mutex = CreateMutexA(nullptr, TRUE, "JJsoft Hopmon Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Hopmon", "HOPMON is already running.", nullptr);
        return 0;
    }
#endif
    int res = 0;
    {
        XApplication d3dApp;
        if (FAILED(d3dApp.Create(args))) {
            FatalError("Hopmon", "Could not create the game window. OpenGL 3.3 support is required.");
        }
        res = d3dApp.Run();
    }
#if defined(_WIN32)
    ReleaseMutex(mutex);
    CloseHandle(mutex);
#endif
    return res;
}
