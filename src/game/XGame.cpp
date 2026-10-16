// Port of XGame.cpp: XStatusDisplay, XProcessObject, XGameObject.
#include "XGame.h"
#include "../core/config.h"
#include "../core/log.h"
#include <cstdlib>
#include "../framework/d3dutil.h"
#include "../gfx/device.h"

// ============================================================ XStatusDisplay
XStatusDisplay::XStatusDisplay(XGameObject* g) : game(g) {
    JSurfaceList* sl = game->app->GetSurfaceList();
    textLevel = new JSprite(sl, 0xb9, 1);
    textCrystals = new JSprite(sl, 0xbc, 1);
    textFireReady = new JSprite(sl, 0xbd, 2);
    fireLED = new JSprite(sl, 0xc0, 2);
    numLevel = new JNumber(sl, 0xba, 10);
    numCrystals = new JNumber(sl, 0xba, 10);
    textLevel->SetPosition(0xc, 4);
    int x = textLevel->GetWidth();
    numLevel->SetPosition(x + 0x14, 5);
    numLevel->SetDisplayZero(1);
    numLevel->SetNumDigits(2);
    numLevel->SetShowAllDigits(true);
    x = x + 0x2c + numLevel->GetDigitWidth() * 2;
    textCrystals->SetPosition(x, 4);
    numCrystals->SetPosition(x + 8 + textCrystals->GetWidth(), 5);
    numCrystals->SetDisplayZero(1);
    numCrystals->SetNumDigits(2);
}

XStatusDisplay::~XStatusDisplay() {
    delete textCrystals;
    delete textLevel;
    delete textFireReady;
    delete numCrystals;
    delete numLevel;
    delete fireLED;
}

void XStatusDisplay::Draw(IDirectDrawSurface7* dest) {
    // Enhancement: in widescreen the left group moves to the left edge and the fire gauge to the
    // right edge (dx = 0 reproduces the original layout exactly).
    int dx = GetDevice().HudEdgeShift();
    // Enhancement: hud_scale resizes each group around its own screen corner (1.0 = original).
    const float hs = GetConfig().hudScale;
    GetDevice().BeginHudScale(hs, (float)-dx, 0.f);  // level / crystals: top-left corner
    textLevel->SetPosition(0xc - dx, 4);
    int x = textLevel->GetWidth();
    numLevel->SetPosition(x + 0x14 - dx, 5);
    // native: room for as many digits as the pack's highest level number (two for the original)
    int levelDigits = 2;
    for (int n = game->app->maxLevel + 1; n >= 100; n /= 10) levelDigits++;
    x = x + 0x2c + numLevel->GetDigitWidth() * levelDigits;
    textCrystals->SetPosition(x - dx, 4);
    numCrystals->SetPosition(x + 8 + textCrystals->GetWidth() - dx, 5);
    textLevel->Draw(dest);
    textCrystals->Draw(dest);
    numLevel->SetNumber(game->app->currentLevel + 1);
    numLevel->Draw(dest);
    numCrystals->SetNumber(game->crystalList->GetCrystalLeft());
    numCrystals->Draw(dest);
    {
        float ax = (float)-dx;
        float right = (float)(numCrystals->GetPosX() + numCrystals->GetWidth());
        float bottom = (float)(4 + std::max(textLevel->GetHeight(), numLevel->GetHeight() + 1));
        GetDevice().SetHudTopLeftExtent(ax + (right - ax) * hs, bottom * hs);
    }
    int w = game->app->GetScreenWidth() + dx;
    int h = game->app->GetScreenHeight();
    GetDevice().BeginHudScale(hs, (float)w, (float)h);  // fire gauge: bottom-right corner
    int energy = game->pac->GetFireEnergy();
    textFireReady->SetCurrentFrame(energy / 100 > 9);
    textFireReady->SetPosition(w - 0x11e, h - 0x1f);
    textFireReady->Draw(dest);
    int ledX = w - 0x117 + textFireReady->GetWidth();
    for (int i = 0; i < 10; i++) {
        fireLED->SetCurrentFrame(i < energy / 100);
        fireLED->SetPosition(ledX, h - 0x1c);
        fireLED->Draw(dest);
        ledX = ledX + 1 + fireLED->GetWidth() / 3;
    }
    GetDevice().EndHudScale();
}

// ============================================================ XProcessObject
XProcessObject::XProcessObject(XApplication* a) : app(a), state(NON), tick(0), processType(0), processFinished(0) {
    fadeLeft = new JSprite(app->GetSurfaceList(), 0xc3, 1);
    fadeRight = new JSprite(app->GetSurfaceList(), 0xc2, 1);
}

XProcessObject::~XProcessObject() {
    delete fadeLeft;
    fadeLeft = nullptr;
    delete fadeRight;
    fadeRight = nullptr;
}

void XProcessObject::StartFadeIn(int type) {
    _ResetFadeIn();
    processType = type;
    state = FADEIN;
    tick = 0;
}

void XProcessObject::StartFadeOut(int type) {
    _ResetFadeOut();
    processType = type;
    state = FADEOUT;
    tick = 0;
}

void XProcessObject::StartProcess(int type) {
    processType = type;
    state = PROCESS;
    tick = 0;
}

void XProcessObject::ProcessFadeIn() {
    _ResetFadeIn();
    int t = (int)tick - 0x14;
    if (t < 0) t = 0;
    else if (t > 100) t = 100;
    float d = ((float)t / 100.0f) * (float)(app->GetScreenWidth() / 2);
    fadeLeft->ShiftPosX(-d);
    fadeRight->ShiftPosX(d);
}

void XProcessObject::ProcessFadeOut() {
    _ResetFadeOut();
    int t = (int)tick;
    if (t > 100) t = 100;
    float d = ((float)t / 100.0f) * (float)(app->GetScreenWidth() / 2);
    fadeLeft->ShiftPosX(d);
    fadeRight->ShiftPosX(-d);
}

void XProcessObject::_ResetFadeIn() {
    int half = app->GetScreenWidth() / 2;
    fadeLeft->SetDrawSize(half, app->GetScreenHeight());
    fadeLeft->SetPosition(0, 0);
    fadeRight->SetDrawSize(half, app->GetScreenHeight());
    fadeRight->SetPosition(half, 0);
}

void XProcessObject::_ResetFadeOut() {
    int half = app->GetScreenWidth() / 2;
    fadeLeft->SetDrawSize(half, app->GetScreenHeight());
    fadeLeft->SetPosition(-half, 0);
    fadeRight->SetDrawSize(half, app->GetScreenHeight());
    fadeRight->SetPosition(app->GetScreenWidth(), 0);
}

void XProcessObject::DrawFade() {
    IDirectDrawSurface7* bb = app->GetBackBuffer();
    float extra = GetDevice().Layout().offsetX;
    if (extra <= 0.f) {
        // Original behaviour: DirectDraw drops blits that are partly off-screen.
        fadeLeft->Draw(bb);
        fadeRight->Draw(bb);
        return;
    }
    // Widescreen: widen each shutter so it also covers the extended side area.
    JSurface* sl = app->GetSurfaceList()->GetByResId(0xc3);
    JSurface* sr = app->GetSurfaceList()->GetByResId(0xc2);
    int half = app->GetScreenWidth() / 2, h = app->GetScreenHeight();
    int ext = (int)(extra + 1.f);
    int lx = fadeLeft->GetPosIntX(), rx = fadeRight->GetPosIntX();
    if (sl && lx + half > -ext) {
        RECT src{0, 0, sl->GetWidth(), sl->GetHeight()};
        RECT dst{lx - ext, 0, lx + half, h};
        bb->Blt(&dst, sl->GetDDSurface(), &src, false);
    }
    if (sr && rx < 640 + ext) {
        RECT src{0, 0, sr->GetWidth(), sr->GetHeight()};
        RECT dst{rx, 0, rx + half + ext, h};
        bb->Blt(&dst, sr->GetDDSurface(), &src, false);
    }
}

// ============================================================ XGameObject
XGameObject::XGameObject(XApplication* a)
    : XProcessObject(a), camera(nullptr), map(nullptr), pac(nullptr), enemyList(nullptr), mapObjList(nullptr),
      pacFireList(nullptr), enemyFireList(nullptr), particleList(nullptr), crystalList(nullptr),
      statusDisplay(nullptr) {
    JSurfaceList* sl = app->GetSurfaceList();
    sl->CreateSurface(0xbc, 1);
    sl->CreateSurface(0xb9, 1);
    sl->CreateSurface(0xbd, 1);
    sl->CreateSurface(0xba, 1);
    sl->CreateSurface(0xc0, 1);
    camera = new XCamera(a);
    pac = new XPac(this);
    enemyList = new XEnemyList(this);
    mapObjList = new XMapObjectList(this);
    pacFireList = new XPacFireList(this);
    enemyFireList = new XEnemyFireList(this);
    particleList = new XParticleList(this);
    crystalList = new XCrystalList(this);
    statusDisplay = new XStatusDisplay(this);
    InitLevel();
}

XGameObject::~XGameObject() {
    delete map;
    map = nullptr;
    delete camera;
    camera = nullptr;
    delete pac;
    pac = nullptr;
    delete enemyList;
    enemyList = nullptr;
    delete mapObjList;
    mapObjList = nullptr;
    delete pacFireList;
    pacFireList = nullptr;
    delete enemyFireList;
    enemyFireList = nullptr;
    delete particleList;
    particleList = nullptr;
    delete crystalList;
    crystalList = nullptr;
    delete statusDisplay;
    statusDisplay = nullptr;
    JSurfaceList* sl = app->GetSurfaceList();
    for (WORD id : {0xbc, 0xb9, 0xbd, 0xba, 0xc0}) sl->DestroySurface(id);
}

int XGameObject::GetCurrentLevel() const { return app->currentLevel; }

void XGameObject::ProcessFrame(DWORD frameCount) {
    if (state == NON) return;
    tick = tick + frameCount;
    if (state == FADEIN) {
        ProcessFadeIn();
        if (tick > 0x77) StartProcess(0);
    } else if (state == FADEOUT) {
        ProcessFadeOut();
        if (tick > 0x77) {
            if (processType == 0) {
                app->SetState(XApplication::START_SELLEVEL);
                state = NON;
                return;
            }
            app->currentLevel = app->currentLevel + 1;
            if (app->maxLevel < app->currentLevel) {
                app->currentLevel = 0;
                app->SetState(XApplication::START_ENDING);
                state = NON;
                return;
            }
            // Native addition: only record progress for a level that was genuinely unlocked
            // (always true without the unlock_all_levels cheat, so this matches the original).
            if (app->maxSelLevel < app->currentLevel && app->currentLevel - 1 <= app->maxSelLevel) {
                app->maxSelLevel = app->currentLevel;
                app->SaveWinRegData();
            }
            if (!IsRegistered() && app->maxFreeLevel < app->currentLevel) {
                app->SetState(XApplication::START_REGINFO);
                state = NON;
                return;
            }
            InitLevel();
            return;
        }
    } else if (state == PROCESS) {
        if (processType == 1) {
            if (tick > 300) StartFadeOut(1);
        } else if (processType == 2 && tick > 300) {
            StartFadeOut(0);
        }
    }
    pac->ProcessRotate(frameCount);
    enemyList->ProcessRotate(frameCount);
    mapObjList->ProcessRotate(frameCount);
    crystalList->ProcessRotate(frameCount);
    for (DWORD f = 0; f < frameCount; f++) {
        enemyList->CheckCollisionWithPac();
        enemyFireList->CheckCollisionWithPac();
        crystalList->CheckCollisionWithPac();
        pacFireList->CheckCollisionWithEnemy();
        map->ProcessFrame();
        pac->ProcessFrame();
        camera->ProcessFrame(pac);
        crystalList->ProcessFrame();
        pacFireList->ProcessFrame();
        enemyFireList->ProcessFrame();
        particleList->ProcessFrame();
        enemyList->ProcessFrame();
        mapObjList->ProcessFrame();
    }
    JDInput* in = app->GetDInput();
    in->UpdateInputState();
    bool up = in->GetKeyboardState(DIK_UP) || in->GetJoyDirState(0, 0);
    bool right = in->GetKeyboardState(DIK_RIGHT) || in->GetJoyDirState(0, 1);
    bool down = in->GetKeyboardState(DIK_DOWN) || in->GetJoyDirState(0, 2);
    bool left = in->GetKeyboardState(DIK_LEFT) || in->GetJoyDirState(0, 3);
    bool fire = in->GetKeyboardPressed(DIK_LSHIFT) || in->GetKeyboardPressed(DIK_RSHIFT) ||
                in->GetKeyboardPressed(DIK_Z) || in->GetJoyButtonPressed(0, 0);
    bool cameraMod = in->GetKeyboardState(DIK_LCONTROL) || in->GetKeyboardState(DIK_RCONTROL) ||
                     in->GetKeyboardState(DIK_X) || in->GetJoyButtonState(0, 1);
    XPac::STATE ps = pac->GetState();
    if (ps == XPac::WAIT || ps == XPac::MOVE || ps == XPac::ON_ARROW) {
        if (in->GetKeyboardPressed(DIK_ESCAPE)) pac->StartDying();
    }
    ps = pac->GetState();
    if (ps == XPac::WAIT || ps == XPac::MOVE || ps == XPac::ON_ARROW) {
        if (fire && pac->GetFireEnergy() > 999) {
            pacFireList->CreateFire();
            if (!GetConfig().infiniteFireEnergy) pac->ResetFireEnergy();  // [cheats] keep the gauge full
        }
        if (cameraMod) {
            if (up) camera->IncrVerticalAngle(frameCount);
            else if (down) camera->DecrVerticalAngle(frameCount);
        }
        if (camera->GetState() == XCamera::WAIT) {
            if (cameraMod) {
                if (right) camera->StartRotatingLeft();
                else if (left) camera->StartRotatingRight();
            } else {
                int dir = -1;
                if (up) dir = 0;
                else if (right) dir = 1;
                else if (down) dir = 2;
                else if (left) dir = 3;
                if (dir >= 0) {
                    int d = (camera->GetViewDirection() + dir) % 4;  // C remainder, as in the original
                    if (pac->GetState() == XPac::WAIT) pac->StartMoving(d);
                    else if (pac->GetState() == XPac::ON_ARROW) pac->SetFaceDirection(d);
                }
            }
        }
    }
}

void XGameObject::Render() {
    if (state == NON) return;
    camera->DoSetTransform();
    map->Render();
    mapObjList->Render();
    crystalList->Render();
    enemyList->Render();
    pac->Render();
    particleList->Render(camera->GetViewMatrix());
    enemyFireList->Render(camera->GetViewMatrix());
    pacFireList->Render(camera->GetViewMatrix());
    statusDisplay->Draw(app->GetBackBuffer());
    if (state == FADEIN || state == FADEOUT) DrawFade();
}

void XGameObject::InitLevel() {
    app->PlayLevelMusic();  // native: the world's track (Music02/03/04 for the original's worlds)
    pac->Reset();
    enemyList->Empty();
    mapObjList->Empty();
    crystalList->Empty();
    pacFireList->Empty();
    enemyFireList->Empty();
    particleList->Empty();
    crystalList->Empty();
    delete map;
    map = nullptr;
    map = new XMap(app, GetMapResId(app), pac, enemyList, mapObjList, crystalList);
    camera->ResetForGame(pac);
    StartFadeIn(0);
}

BYTE XGameObject::GetVolumeOfPosition(D3DVECTOR pos) {
    const float normalZone = 130.0f;
    const float deadZone = 550.0f;
    if (pac->GetState() == XPac::NON) return 0;
    float distance = D3DFindDistance(pos, pac->GetWorldPos());
    if (distance < normalZone) return 100;
    if (distance > deadZone) return 0;
    return (BYTE)((BYTE)(int)(((deadZone - distance) / 420.0f) * 30.0f) + 0x46);
}
