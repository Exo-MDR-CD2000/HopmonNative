// Port of XTitle.cpp: logo, title menu, level selection, info screen, ending.
#include "XGame.h"

// ============================================================ XTitleBackground
XTitleBackground::XTitleBackground(XApplication* a) : app(a) {
    backUpper = new JObject3D_Rect(app, 720.0f, 400.0f);
    backLower = new JObject3D_Rect(app, 720.0f, 400.0f);
    textureOffset = 0.0f;
    backUpper->RotateBy(JAXIS_X, 3.1415927f);
    backUpper->ShiftWorldPos(JAXIS_Y, 120.0f);
    backUpper->UsePerspective(1);
    backUpper->SetFilterState(JObject3D::FILTER_POINT);
    backLower->ShiftWorldPos(JAXIS_Y, -120.0f);
    backLower->UsePerspective(1);
    backLower->SetFilterState(JObject3D::FILTER_LINEAR);
}

XTitleBackground::~XTitleBackground() {
    delete backUpper;
    backUpper = nullptr;
    delete backLower;
    backLower = nullptr;
}

void XTitleBackground::ProcessFrame(DWORD frameCount) { textureOffset = (float)frameCount * 0.02f + textureOffset; }

void XTitleBackground::Render() {
    // texture scrolling is a render-time value; interpolate it for smooth motion
    float off = textureOffset;
    backUpper->SetTexture(0xc1, 0.0f, off, 18.0f, off + 10.0f);
    backUpper->Render();
    backLower->SetTexture(0xc1, 0.0f, -off, 18.0f, 10.0f - off);
    backLower->Render();
}

// ============================================================ XLogoObject
XLogoObject::XLogoObject(XApplication* a) : XProcessObject(a), camera(nullptr), logo(nullptr), backWhite(nullptr) {
    app->GetMidiList()->Stop();
    app->GetSurfaceList()->CreateSurface(0x13c, 0);
    camera = new XCamera(app);
    camera->ResetForTitle();
    logo = new JSprite(app->GetSurfaceList(), 0x13c, 1);
    backWhite = new JObject3D_Rect(app, 600.0f, 600.0f);
    backWhite->RotateBy(JAXIS_X, -1.5707964f);
    backWhite->UsePerspective(0);
    backWhite->SetTexture(0x14c, 0.0f, 0.0f, 1.0f, 1.0f);
    StartProcess(0);
}

XLogoObject::~XLogoObject() {
    delete camera;
    camera = nullptr;
    delete logo;
    logo = nullptr;
    delete backWhite;
    backWhite = nullptr;
    app->GetSurfaceList()->DestroySurface(0x13c);
}

void XLogoObject::ProcessFrame(DWORD frameCount) {
    if (state == NON) return;
    for (DWORD f = 0; f < frameCount; f++) {
        tick = tick + 1;
        if (tick == 0x32) app->GetSoundList()->Play(0x13d, 100, 0);
        if (tick > 0x3b5) {
            app->SetState(XApplication::START_TITLE);
            state = NON;
            return;
        }
    }
}

void XLogoObject::Render() {
    if (state == NON) return;
    camera->DoSetTransform();
    backWhite->Render();
    logo->SetCenterPos(app->GetScreenWidth() / 2, app->GetScreenHeight() / 2);
    logo->DrawScale(app->GetBackBuffer(), 1.0f);
}

// ============================================================ XRegisterInfoObject
XRegisterInfoObject::XRegisterInfoObject(XApplication* a)
    : XProcessObject(a), camera(nullptr), background(nullptr), cursorLeft(nullptr), cursorRight(nullptr),
      menuBack(nullptr), screenShot1(nullptr), screenShot2(nullptr), regInfo(nullptr) {
    app->GetMidiList()->Play(3);
    JSurfaceList* sl = app->GetSurfaceList();
    sl->CreateSurface(0xd0, 1);
    sl->CreateSurface(0xd1, 1);
    sl->CreateSurface(200, 1);
    sl->CreateSurface(0x142, 0);
    sl->CreateSurface(0x143, 0);
    sl->CreateSurface(0x141, 0);
    camera = new XCamera(app);
    camera->ResetForTitle();
    background = new XTitleBackground(app);
    cursorLeft = new JSprite(sl, 0xd0, 2);
    cursorRight = new JSprite(sl, 0xd1, 2);
    menuBack = new JSprite(sl, 200, 2);
    screenShot1 = new JSprite(sl, 0x142, 1);
    screenShot2 = new JSprite(sl, 0x143, 1);
    regInfo = new JSprite(sl, 0x141, 1);
    StartFadeIn(0);
}

XRegisterInfoObject::~XRegisterInfoObject() {
    delete camera;
    delete background;
    delete cursorLeft;
    delete cursorRight;
    delete menuBack;
    delete screenShot1;
    delete screenShot2;
    delete regInfo;
    JSurfaceList* sl = app->GetSurfaceList();
    sl->DestroySurface(0xd0);
    sl->DestroySurface(0xd1);
    sl->DestroySurface(200);
    sl->DestroySurface(0x142);
    sl->DestroySurface(0x143);
    sl->DestroySurface(0x141);
}

void XRegisterInfoObject::ProcessFrame(DWORD frameCount) {
    if (state == NON) return;
    tick = tick + frameCount;
    if (state == FADEIN) {
        ProcessFadeIn();
        if (tick > 0x77) StartProcess(0);
    } else if (state == FADEOUT) {
        ProcessFadeOut();
        if (tick > 0x77) {
            app->SetState(XApplication::START_SELLEVEL);
            state = NON;
            return;
        }
    }
    background->ProcessFrame(frameCount);
    if (state == PROCESS) {
        int blink = (tick % 0x32) > 0x18;
        cursorLeft->SetCurrentFrame(blink);
        cursorRight->SetCurrentFrame(blink);
        JDInput* in = app->GetDInput();
        in->UpdateInputState();
        int keyButton = CheckButtonPressed(in);
        if (in->GetKeyboardPressed(DIK_ESCAPE)) keyButton = 1;
        if (keyButton) {
            app->GetSoundList()->Play(0xed, 100, 0);
            StartFadeOut(0);
        }
    }
}

void XRegisterInfoObject::Render() {
    if (state == NON) return;
    camera->DoSetTransform();
    background->Render();
    int cx = app->GetScreenWidth() / 2;
    int cy = app->GetScreenHeight() / 2;
    IDirectDrawSurface7* bb = app->GetBackBuffer();
    screenShot1->SetCenterPos(cx - 0x9f, cy - 0x5c);
    screenShot1->Draw(bb);
    screenShot2->SetCenterPos(cx + 0x9f, cy - 0x5c);
    screenShot2->Draw(bb);
    regInfo->SetCenterPos(cx, cy + 0x70);
    regInfo->Draw(bb);
    menuBack->SetCenterPos(cx, cy + 0xd0);
    menuBack->SetCurrentFrame(1);
    menuBack->Draw(bb);
    if (state == PROCESS) {
        int x0 = menuBack->GetPosIntX();
        int lw = cursorLeft->GetWidth();
        int x1 = menuBack->GetPosIntX();
        int mw = menuBack->GetWidth();
        int y = menuBack->GetPosIntY();
        cursorLeft->SetPosition((x0 - lw / 2) - 4, y + 2);
        cursorLeft->Draw(bb);
        cursorRight->SetPosition(x1 + 4 + mw, y + 2);
        cursorRight->Draw(bb);
    }
    if (state == FADEIN || state == FADEOUT) DrawFade();
}

// ============================================================ XSelectLevelObject
XSelectLevelObject::XSelectLevelObject(XApplication* a)
    : XProcessObject(a), camera(nullptr), background(nullptr), map(nullptr), cursorLeft(nullptr),
      cursorRight(nullptr), cursor2Left(nullptr), cursor2Right(nullptr), textUnreg(nullptr), textLevel(nullptr),
      levelLED(nullptr), numLevel(nullptr), cursorPos(-1), inSelectLevel(0) {
    menuItems[0] = menuItems[1] = menuItems[2] = nullptr;
    app->GetMidiList()->Play(0);
    JSurfaceList* sl = app->GetSurfaceList();
    sl->CreateSurface(0xd0, 1);
    sl->CreateSurface(0xd1, 1);
    sl->CreateSurface(0xce, 1);
    sl->CreateSurface(0xcf, 1);
    sl->CreateSurface(0xf7, 1);
    sl->CreateSurface(0xb9, 1);
    sl->CreateSurface(0x140, 1);
    sl->CreateSurface(0xba, 1);
    sl->CreateSurface(0xcd, 1);
    sl->CreateSurface(0xcc, 1);
    sl->CreateSurface(200, 1);
    camera = new XCamera(app);
    camera->ResetForTitle();
    numLevel = new JNumber(sl, 0xba, 10);
    numLevel->SetDisplayZero(1);
    numLevel->SetNumDigits(2);
    background = new XTitleBackground(app);
    cursorLeft = new JSprite(sl, 0xd0, 2);
    cursorRight = new JSprite(sl, 0xd1, 2);
    cursor2Left = new JSprite(sl, 0xce, 2);
    cursor2Right = new JSprite(sl, 0xcf, 2);
    textUnreg = new JSprite(sl, 0xf7, 1);
    textLevel = new JSprite(sl, 0xb9, 1);
    levelLED = new JSprite(sl, 0x140, 3);
    menuItems[0] = new JSprite(sl, 0xcd, 2);
    menuItems[1] = new JSprite(sl, 0xcc, 2);
    menuItems[2] = new JSprite(sl, 200, 2);
    CreatePreviewMap();
    StartFadeIn(0);
}

XSelectLevelObject::~XSelectLevelObject() {
    delete camera;
    delete numLevel;
    delete background;
    delete map;
    delete cursorLeft;
    delete cursorRight;
    delete cursor2Left;
    delete cursor2Right;
    delete textUnreg;
    delete textLevel;
    delete levelLED;
    for (auto*& m : menuItems) {
        delete m;
        m = nullptr;
    }
    JSurfaceList* sl = app->GetSurfaceList();
    for (WORD id : {0xd0, 0xd1, 0xce, 0xcf, 0xf7, 0xb9, 0x140, 0xba, 0xcd, 0xcc, 200}) sl->DestroySurface(id);
}

void XSelectLevelObject::ProcessFrame(DWORD frameCount) {
    if (state == NON) return;
    // Native: if unlock_all_levels was just switched off (pause menu) while a level beyond the
    // player's real progress is selected, step back to the highest level really reached.
    // Never true in normal play, where the selection can't pass maxSelLevel.
    if (state != FADEOUT && app->currentLevel > app->SelectableLevel()) {
        app->currentLevel = app->SelectableLevel();
        CreatePreviewMap();
    }
    tick = tick + frameCount;
    if (state == FADEIN) {
        ProcessFadeIn();
        if (tick > 0x77) {
            cursorPos = 0;
            StartProcess(0);
        }
    } else if (state == FADEOUT) {
        ProcessFadeOut();
        if (tick > 0x77) {
            if (processType == 0) {
                if (!IsRegistered() && app->maxFreeLevel < app->currentLevel) {
                    app->SetState(XApplication::START_REGINFO);
                    state = NON;
                    return;
                }
                app->SetState(XApplication::START_GAME);
                state = NON;
                return;
            }
            if (processType == 1) {
                app->currentLevel = 0;
                app->SetState(XApplication::START_TITLE);
                state = NON;
                return;
            }
        }
    }
    background->ProcessFrame(frameCount);
    map->ProcessFrame(frameCount);
    if (state != PROCESS) return;
    int blink = (tick % 0x32) > 0x18;
    cursorLeft->SetCurrentFrame(blink);
    cursorRight->SetCurrentFrame(blink);
    cursor2Left->SetCurrentFrame(blink);
    cursor2Right->SetCurrentFrame(blink);
    JDInput* in = app->GetDInput();
    in->UpdateInputState();
    bool up = in->GetKeyboardPressed(DIK_UP) || in->GetJoyDirPressed(0, 0);
    bool right = in->GetKeyboardPressed(DIK_RIGHT) || in->GetJoyDirPressed(0, 1);
    bool down = in->GetKeyboardPressed(DIK_DOWN) || in->GetJoyDirPressed(0, 2);
    bool left = in->GetKeyboardPressed(DIK_LEFT) || in->GetJoyDirPressed(0, 3);
    int keyButton = CheckButtonPressed(in);
    JSoundList* snd = app->GetSoundList();
    if (!inSelectLevel) {
        if (in->GetKeyboardPressed(DIK_ESCAPE)) {
            cursorPos = 2;
            keyButton = 1;
        }
        if (up) {
            snd->Play(0xec, 100, 0);
            cursorPos = cursorPos - 1;
            if (cursorPos < 0) cursorPos = 2;
        } else if (down) {
            snd->Play(0xec, 100, 0);
            cursorPos = cursorPos + 1;
            if (cursorPos > 2) cursorPos = 0;
        } else if (keyButton) {
            snd->Play(0xed, 100, 0);
            if (cursorPos == 0) StartFadeOut(0);
            else if (cursorPos == 1) inSelectLevel = 1;
            else StartFadeOut(1);
        }
    } else {
        if (in->GetKeyboardPressed(DIK_ESCAPE)) keyButton = 1;
        if (left) {
            snd->Play(0xec, 100, 0);
            app->currentLevel = app->currentLevel - 1;
            if (app->currentLevel < 0) app->currentLevel = app->SelectableLevel();
            CreatePreviewMap();
        } else if (right) {
            snd->Play(0xec, 100, 0);
            app->currentLevel = app->currentLevel + 1;
            if (app->SelectableLevel() < app->currentLevel) app->currentLevel = 0;
            CreatePreviewMap();
        } else if (keyButton) {
            snd->Play(0xed, 100, 0);
            inSelectLevel = 0;
        }
    }
}

void XSelectLevelObject::Render() {
    if (state == NON) return;
    camera->DoSetTransform();
    background->Render();
    map->Render();
    int cx = app->GetScreenWidth() / 2;
    int cy = app->GetScreenHeight() / 2;
    IDirectDrawSurface7* bb = app->GetBackBuffer();
    textLevel->SetPosition(cx - 0x4c, cy - 200);
    textLevel->Draw(bb);
    int tw = textLevel->GetWidth();
    numLevel->SetNumber(app->currentLevel + 1);
    numLevel->SetPosition(cx - 0x44 + tw, cy - 0xc6);
    numLevel->Draw(bb);
    int ledW = levelLED->GetWidth();
    int posX = cx - ((ledW / 4) * app->maxLevel) / 2;
    int numH = numLevel->GetHeight();
    for (int i = 0; i <= app->maxLevel; i++) {
        if (i == app->currentLevel) levelLED->SetCurrentFrame(2);
        else if (app->SelectableLevel() < i) levelLED->SetCurrentFrame(0);
        else levelLED->SetCurrentFrame(1);
        levelLED->SetPosition(posX, cy - 0xb4 + numH);
        levelLED->Draw(bb);
        posX = levelLED->GetWidth() / 4 + posX;
    }
    int menuY = cy + 0x6e;
    for (int i = 0; i < 3; i++) {
        menuItems[i]->SetCenterPos(cx, menuY);
        menuItems[i]->SetCurrentFrame(cursorPos == i);
        menuItems[i]->Draw(bb);
        menuY = menuY + 8 + menuItems[i]->GetHeight();
    }
    if (!IsRegistered()) {
        textUnreg->SetPosition(cx - 0x138, cy + 0xde);
        textUnreg->Draw(bb);
    }
    if (state == PROCESS) {
        if (!inSelectLevel) {
            if (cursorPos >= 0) {
                JSprite* m = menuItems[cursorPos];
                int x0 = m->GetPosIntX(), lw = cursorLeft->GetWidth(), x1 = m->GetPosIntX(), mw = m->GetWidth();
                int y = m->GetPosIntY();
                cursorLeft->SetPosition((x0 - lw / 2) - 4, y + 2);
                cursorLeft->Draw(bb);
                cursorRight->SetPosition(x1 + 4 + mw, y + 2);
                cursorRight->Draw(bb);
            }
        } else {
            int x0 = textLevel->GetPosIntX(), rw = cursor2Right->GetWidth();
            int nx = numLevel->GetPosX(), nw = numLevel->GetWidth();
            int y = textLevel->GetPosIntY();
            cursor2Right->SetPosition((x0 - rw / 2) - 6, y + 2);
            cursor2Right->Draw(bb);
            cursor2Left->SetPosition(nx + 6 + nw, y + 2);
            cursor2Left->Draw(bb);
        }
    }
    if (state == FADEIN || state == FADEOUT) DrawFade();
}

void XSelectLevelObject::CreatePreviewMap() {
    delete map;
    map = nullptr;
    map = new XPreviewMap(app, GetMapResId(app));
}

// ============================================================ XTitleObject
XTitleObject::XTitleObject(XApplication* a)
    : XProcessObject(a), camera(nullptr), pac(nullptr), background(nullptr), cursorLeft(nullptr),
      cursorRight(nullptr), textHopmon(nullptr), textCopyright(nullptr), textUnreg(nullptr), cursorPos(-1) {
    menuItems[0] = menuItems[1] = nullptr;
    app->GetMidiList()->Play(0);
    JSurfaceList* sl = app->GetSurfaceList();
    sl->CreateSurface(0xd0, 1);
    sl->CreateSurface(0xd1, 1);
    sl->CreateSurface(0xdb, 1);
    sl->CreateSurface(0xf8, 1);
    sl->CreateSurface(0xf7, 1);
    sl->CreateSurface(0xcd, 1);
    sl->CreateSurface(0xc9, 1);
    camera = new XCamera(app);
    camera->ResetForTitle();
    pac = new JObject3DX(app, 0x94);
    pac->RotateBy(JAXIS_X, -1.5707964f);
    pac->ScaleBy(170.0f);
    background = new XTitleBackground(app);
    cursorLeft = new JSprite(sl, 0xd0, 2);
    cursorRight = new JSprite(sl, 0xd1, 2);
    textHopmon = new JSprite(sl, 0xdb, 1);
    textCopyright = new JSprite(sl, 0xf8, 1);
    textUnreg = new JSprite(sl, 0xf7, 1);
    menuItems[0] = new JSprite(sl, 0xcd, 2);
    menuItems[1] = new JSprite(sl, 0xc9, 2);
    StartFadeIn(0);
}

XTitleObject::~XTitleObject() {
    delete camera;
    delete pac;
    delete background;
    delete cursorLeft;
    delete cursorRight;
    delete textHopmon;
    delete textCopyright;
    delete textUnreg;
    for (auto*& m : menuItems) {
        delete m;
        m = nullptr;
    }
    JSurfaceList* sl = app->GetSurfaceList();
    for (WORD id : {0xd0, 0xd1, 0xdb, 0xf8, 0xf7, 0xcd, 0xc9}) sl->DestroySurface(id);
}

void XTitleObject::ProcessFrame(DWORD frameCount) {
    if (state == NON) return;
    tick = tick + frameCount;
    if (state == FADEIN) {
        ProcessFadeIn();
        if (tick > 0x77) {
            cursorPos = 0;
            StartProcess(0);
        }
    } else if (state == FADEOUT) {
        ProcessFadeOut();
        if (tick > 0x77) {
            if (processType == 0) {
                app->SetState(XApplication::START_SELLEVEL);
                state = NON;
                return;
            }
            if (processType == 1) {
                app->SetState(XApplication::EXIT);
                state = NON;
                return;
            }
        }
    }
    background->ProcessFrame(frameCount);
    pac->RotateBy(JAXIS_Y, (float)frameCount * 0.010471975f);
    if (state != PROCESS) return;
    int blink = (tick % 0x32) > 0x18;
    cursorLeft->SetCurrentFrame(blink);
    cursorRight->SetCurrentFrame(blink);
    JDInput* in = app->GetDInput();
    in->UpdateInputState();
    bool up = in->GetKeyboardPressed(DIK_UP) || in->GetJoyDirPressed(0, 0);
    bool down = in->GetKeyboardPressed(DIK_DOWN) || in->GetJoyDirPressed(0, 2);
    int keyButton = CheckButtonPressed(in);
    if (in->GetKeyboardPressed(DIK_ESCAPE)) {
        cursorPos = 1;
        keyButton = 1;
    }
    JSoundList* snd = app->GetSoundList();
    if (up) {
        snd->Play(0xec, 100, 0);
        cursorPos = cursorPos - 1;
        if (cursorPos < 0) cursorPos = 1;
    } else if (down) {
        snd->Play(0xec, 100, 0);
        cursorPos = cursorPos + 1;
        if (cursorPos > 1) cursorPos = 0;
    } else if (keyButton) {
        snd->Play(0xed, 100, 0);
        if (cursorPos == 0) StartFadeOut(0);
        else StartFadeOut(1);
    }
}

void XTitleObject::Render() {
    if (state == NON) return;
    camera->DoSetTransform();
    background->Render();
    pac->Render();
    int cx = app->GetScreenWidth() / 2;
    int cy = app->GetScreenHeight() / 2;
    IDirectDrawSurface7* bb = app->GetBackBuffer();
    textHopmon->SetCenterPos(cx, cy - 0xa0);
    textHopmon->Draw(bb);
    textCopyright->SetCenterPos(cx, cy - 0x5a);
    textCopyright->Draw(bb);
    int menuY = cy + 0x96;
    for (int i = 0; i < 2; i++) {
        menuItems[i]->SetCenterPos(cx, menuY);
        menuItems[i]->SetCurrentFrame(cursorPos == i);
        menuItems[i]->Draw(bb);
        menuY = menuY + 8 + menuItems[i]->GetHeight();
    }
    if (!IsRegistered()) {
        textUnreg->SetPosition(cx - 0x138, cy + 0xde);
        textUnreg->Draw(bb);
    }
    if (state == PROCESS && cursorPos >= 0) {
        JSprite* m = menuItems[cursorPos];
        int x0 = m->GetPosIntX(), lw = cursorLeft->GetWidth(), x1 = m->GetPosIntX(), mw = m->GetWidth();
        int y = m->GetPosIntY();
        cursorLeft->SetPosition((x0 - lw / 2) - 4, y + 2);
        cursorLeft->Draw(bb);
        cursorRight->SetPosition(x1 + 4 + mw, y + 2);
        cursorRight->Draw(bb);
    }
    if (state == FADEIN || state == FADEOUT) DrawFade();
}

// ============================================================ XEndingObject
XEndingObject::XEndingObject(XApplication* a)
    : XProcessObject(a), camera(nullptr), background(nullptr), textCongra(nullptr), textStaff(nullptr),
      textCopyright(nullptr), charaIndex(-1) {
    app->GetMidiList()->Play(0);
    JSurfaceList* sl = app->GetSurfaceList();
    sl->CreateSurface(0x144, 1);
    sl->CreateSurface(0x145, 1);
    sl->CreateSurface(0xf8, 1);
    camera = new XCamera(app);
    camera->ResetForTitle();
    background = new XTitleBackground(app);
    textCongra = new JSprite(sl, 0x144, 1);
    textStaff = new JSprite(sl, 0x145, 1);
    textCopyright = new JSprite(sl, 0xf8, 1);
    static const WORD models[10] = {0x94, 0x9d, 0xa4, 0xaf, 0xd4, 0xad, 0xac, 0xa6, 0xb3, 0xb1};
    for (int i = 0; i < 10; i++) chara[i] = new JObject3DX(app, models[i]);
    for (int i = 0; i < 10; i++) chara[i]->RotateBy(JAXIS_X, -1.5707964f);
    charaMaxScale[0] = 180.0f;
    charaMaxScale[1] = charaMaxScale[0] * 1.0f;
    charaMaxScale[2] = charaMaxScale[0] * 1.5f;
    charaMaxScale[3] = charaMaxScale[0] * 1.78f;
    charaMaxScale[4] = charaMaxScale[0] * 1.1f;
    charaMaxScale[5] = charaMaxScale[0] * 1.6f;
    charaMaxScale[6] = charaMaxScale[0] * 1.4f;
    charaMaxScale[7] = charaMaxScale[0] * 1.4f;
    charaMaxScale[8] = charaMaxScale[0] * 0.65f;
    charaMaxScale[9] = charaMaxScale[0] * 0.7f;
    StartFadeIn(0);
}

XEndingObject::~XEndingObject() {
    delete camera;
    delete background;
    delete textCongra;
    delete textStaff;
    delete textCopyright;
    for (auto*& c : chara) {
        delete c;
        c = nullptr;
    }
    JSurfaceList* sl = app->GetSurfaceList();
    sl->DestroySurface(0x144);
    sl->DestroySurface(0x145);
    sl->DestroySurface(0xf8);
}

void XEndingObject::ProcessFrame(DWORD frameCount) {
    if (state == NON) return;
    tick = tick + frameCount;
    if (state == FADEIN) {
        ProcessFadeIn();
        if (tick > 0x77) StartProcess(0);
    } else if (state == FADEOUT) {
        ProcessFadeOut();
        if (tick > 0x77) {
            app->SetState(XApplication::START_LOGO);
            state = NON;
            return;
        }
    }
    background->ProcessFrame(frameCount);
    if (state == PROCESS) {
        unsigned t = tick % 8000;
        charaIndex = (int)(t / 800);
        t = t % 800;
        float charaScale;
        if (t < 100) charaScale = (((float)t + 0.0f) / 100.0f) * charaMaxScale[charaIndex];
        else if (t < 700) charaScale = charaMaxScale[charaIndex];
        else charaScale = ((float)(800 - t) / 100.0f) * charaMaxScale[charaIndex];
        if (charaScale < 1.0f) charaScale = 1.0f;
        chara[charaIndex]->ScaleTo(charaScale);
        chara[charaIndex]->RotateBy(JAXIS_Y, (float)frameCount * 0.013962634f);
        JDInput* in = app->GetDInput();
        in->UpdateInputState();
        int keyButton = CheckButtonPressed(in);
        if (in->GetKeyboardPressed(DIK_ESCAPE)) keyButton = 1;
        if (keyButton) {
            app->GetSoundList()->Play(0xed, 100, 0);
            StartFadeOut(0);
        }
    }
}

void XEndingObject::Render() {
    if (state == NON) return;
    camera->DoSetTransform();
    background->Render();
    if (charaIndex >= 0) chara[charaIndex]->Render();
    int cx = app->GetScreenWidth() / 2;
    int cy = app->GetScreenHeight() / 2;
    IDirectDrawSurface7* bb = app->GetBackBuffer();
    textCongra->SetCenterPos(cx, cy - 0xa0);
    textCongra->Draw(bb);
    textStaff->SetCenterPos(cx, cy + 0x3c);
    textStaff->Draw(bb);
    textCopyright->SetCenterPos(cx, cy + 0xbe);
    textCopyright->Draw(bb);
    if (state == FADEIN || state == FADEOUT) DrawFade();
}

int CheckButtonPressed(JDInput* in) {
    if (in->GetKeyboardPressed(DIK_NUMPADENTER) || in->GetKeyboardPressed(DIK_RETURN) ||
        in->GetKeyboardPressed(DIK_LSHIFT) || in->GetKeyboardPressed(DIK_RSHIFT) ||
        in->GetKeyboardPressed(DIK_LCONTROL) || in->GetKeyboardPressed(DIK_RCONTROL) ||
        in->GetKeyboardPressed(DIK_Z) || in->GetKeyboardPressed(DIK_X) || in->GetJoyButtonPressed(0, 0) ||
        in->GetJoyButtonPressed(0, 1))
        return 1;
    return 0;
}
