// Port of XTitle.cpp: logo, title menu, level selection, info screen, ending.
#include "XGame.h"
#include "../core/mods.h"
#include <SDL3/SDL.h>

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
      levelLED(nullptr), rowArrowLeft(nullptr), rowArrowRight(nullptr), numLevel(nullptr), cursorPos(-1),
      inSelectLevel(0) {
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
    numLevel->SetShowAllDigits(true);
    background = new XTitleBackground(app);
    cursorLeft = new JSprite(sl, 0xd0, 2);
    cursorRight = new JSprite(sl, 0xd1, 2);
    cursor2Left = new JSprite(sl, 0xce, 2);
    cursor2Right = new JSprite(sl, 0xcf, 2);
    textUnreg = new JSprite(sl, 0xf7, 1);
    textLevel = new JSprite(sl, 0xb9, 1);
    levelLED = new JSprite(sl, 0x140, 3);
    rowArrowLeft = new JSprite(sl, 0xcf, 2);
    rowArrowRight = new JSprite(sl, 0xce, 2);
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
    delete rowArrowLeft;
    delete rowArrowRight;
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
        // Native: hold to scroll, Up/Down jump 10. A press acts at once (Left/Right wrap round at
        // the ends, as in the original); holding repeats after 0.4 s at ~8 steps a second, and
        // Left/Right speed up to ~30 a second after 2 s. Repeats stop at the ends instead of wrapping.
        const unsigned long now = (unsigned long)SDL_GetTicks();
        auto held = [&](int d) -> bool {
            switch (d) {
                case -1: return in->GetKeyboardState(DIK_LEFT) || in->GetJoyDirState(0, 3);
                case 1: return in->GetKeyboardState(DIK_RIGHT) || in->GetJoyDirState(0, 1);
                case 10: return in->GetKeyboardState(DIK_UP) || in->GetJoyDirState(0, 0);
                case -10: return in->GetKeyboardState(DIK_DOWN) || in->GetJoyDirState(0, 2);
            }
            return 0;
        };
        int pressed = left ? -1 : right ? 1 : up ? 10 : down ? -10 : 0;
        if (pressed) {
            holdDir = pressed;
            holdStart = now;
            nextRepeat = now + 400;
            lastStepSound = 0;  // a press always clicks
            StepLevel(pressed, pressed == -1 || pressed == 1);
        } else if (holdDir && held(holdDir)) {
            int steps = 0;
            while (now >= nextRepeat && steps < 4) {
                bool fast = (holdDir == -1 || holdDir == 1) && now - holdStart >= 2000;
                nextRepeat += fast ? 33 : 125;
                StepLevel(holdDir, false);
                steps++;
            }
            if (now >= nextRepeat) nextRepeat = now;  // don't build up a backlog after a pause
        } else {
            holdDir = 0;
        }
        if (previewDirty && (holdDir == 0 || now - holdStart < 2000)) {
            previewDirty = false;
            CreatePreviewMap();
        }
        if (!pressed && keyButton) {
            snd->Play(0xed, 100, 0);
            inSelectLevel = 0;
            holdDir = 0;
            if (previewDirty) {
                previewDirty = false;
                CreatePreviewMap();
            }
        }
    }
}

// Native: moves the level choice. A press wraps round at the ends (as the original did);
// repeats and the 10-level jumps stop at the first/last level.
void XSelectLevelObject::StepLevel(int delta, bool wrap) {
    int last = app->SelectableLevel();
    int cur = app->currentLevel, next = cur + delta;
    if (wrap) {
        if (next < 0) next = last;
        else if (next > last) next = 0;
    } else {
        next = next < 0 ? 0 : next > last ? last : next;
    }
    if (next == cur) return;
    app->currentLevel = next;
    const unsigned long now = (unsigned long)SDL_GetTicks();
    if (lastStepSound == 0 || now - lastStepSound >= 90) {  // fast scrolling clicks ~11 times a second
        app->GetSoundList()->Play(0xec, 100, 0);
        lastStepSound = now ? now : 1;
    }
    // while scrolling fast, the preview is rebuilt when it slows or stops
    bool fast = holdDir != 0 && (unsigned long)SDL_GetTicks() - holdStart >= 2000 && (holdDir == -1 || holdDir == 1);
    if (fast) previewDirty = true;
    else CreatePreviewMap();
}

void XSelectLevelObject::Render() {
    if (state == NON) return;
    camera->DoSetTransform();
    background->Render();
    map->Render();
    int cx = app->GetScreenWidth() / 2;
    int cy = app->GetScreenHeight() / 2;
    IDirectDrawSurface7* bb = app->GetBackBuffer();
    // "LEVEL nn" is centred; native: numbers longer than two digits shift it left to stay centred
    numLevel->SetNumber(app->currentLevel + 1);
    int extra = numLevel->GetWidth() - numLevel->GetDigitWidth() * 2;
    textLevel->SetPosition(cx - 0x4c - extra / 2, cy - 200);
    textLevel->Draw(bb);
    int tw = textLevel->GetWidth();
    numLevel->SetPosition(cx - 0x44 + tw - extra / 2, cy - 0xc6);
    numLevel->Draw(bb);
    // Native: the row shows at most kRowLevels lights (the original's 45). Longer packs scroll the
    // row to keep the selected level in view, with arrows at the ends where more levels are hidden.
    // With 45 levels or fewer this draws exactly what the original did.
    const int kRowLevels = 45;
    const int total = app->maxLevel + 1;
    const int shown = total < kRowLevels ? total : kRowLevels;
    int first = 0;
    if (total > kRowLevels) {
        first = app->currentLevel - kRowLevels / 2;
        if (first > total - kRowLevels) first = total - kRowLevels;
        if (first < 0) first = 0;
    }
    int ledW = levelLED->GetWidth();
    int step = ledW / 4;
    int rowX = cx - (step * (shown - 1)) / 2;
    int posX = rowX;
    int numH = numLevel->GetHeight();
    int rowY = cy - 0xb4 + numH;
    for (int i = first; i < first + shown; i++) {
        if (i == app->currentLevel) levelLED->SetCurrentFrame(2);
        else if (app->SelectableLevel() < i) levelLED->SetCurrentFrame(0);
        else levelLED->SetCurrentFrame(1);
        levelLED->SetPosition(posX, rowY);
        levelLED->Draw(bb);
        posX = step + posX;
    }
    if (first > 0) {
        rowArrowLeft->SetCurrentFrame(0);
        rowArrowLeft->SetPosition(rowX - rowArrowLeft->GetWidth() / 2 - 6, rowY - 2);
        rowArrowLeft->Draw(bb);
    }
    if (first + shown < total) {
        rowArrowRight->SetCurrentFrame(0);
        rowArrowRight->SetPosition(rowX + step * (shown - 1) + step + 6, rowY - 2);
        rowArrowRight->Draw(bb);
    }
    // Native: a scrollbar under the row for packs longer than the row. The thumb shows which part of
    // the pack the row is showing (its length is the share that fits), and small marks on the track
    // show where each of the pack's worlds starts. Not drawn for 45 levels or fewer.
    if (total > kRowLevels) {
        const int lx0 = rowX, lx1 = rowX + step * shown;  // the row's extent
        const int ty0 = rowY + levelLED->GetHeight() + 7, ty1 = ty0 + 7;
        auto fill = [&](int x0, int y0, int x1, int y1, uint32_t rgb) {
            if (x1 <= x0 || y1 <= y0) return;
            RECT r{x0, y0, x1, y1};
            bb->ColorFill(&r, rgb);
        };
        // track: light-blue outline with cut corners, dark inside
        fill(lx0 + 1, ty0, lx1 - 1, ty1, 0x7399FF);
        fill(lx0, ty0 + 1, lx1, ty1 - 1, 0x7399FF);
        fill(lx0 + 1, ty0 + 1, lx1 - 1, ty1 - 1, 0x0E0A30);
        const int w = lx1 - lx0;
        // world marks
        const Mods::Pack& pk = Mods::ActivePack();
        for (size_t i = 1; i < pk.levelWorld.size() && (int)i < total; i++)
            if (pk.levelWorld[i] != pk.levelWorld[i - 1]) {
                int mx = lx0 + (int)((long long)w * (long long)i / total);
                fill(mx, ty0 + 2, mx + 1, ty1 - 2, 0x5A74C8);
            }
        // thumb
        int tw = (int)((long long)w * kRowLevels / total);
        if (tw < 12) tw = 12;
        int tx = lx0 + 1 + (int)((long long)(w - 2 - tw) * first / (total - kRowLevels));
        fill(tx + 1, ty0 + 1, tx + tw - 1, ty1 - 1, 0xFFEC8C);
        fill(tx, ty0 + 2, tx + tw, ty1 - 2, 0xFFEC8C);
        fill(tx + 1, ty0 + 1, tx + tw - 1, ty0 + 2, 0xFFFFDC);  // highlight along the top
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
    menuItems[0] = menuItems[1] = menuItems[2] = nullptr;
    app->GetMidiList()->Play(0);
    JSurfaceList* sl = app->GetSurfaceList();
    sl->CreateSurface(0xd0, 1);
    sl->CreateSurface(0xd1, 1);
    sl->CreateSurface(0xdb, 1);
    sl->CreateSurface(0xf8, 1);
    sl->CreateSurface(0xf7, 1);
    sl->CreateSurface(0xcd, 1);
    sl->CreateSurface(0xca, 1);  // native: OPTIONS, drawn for the original but never used by it
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
    menuItems[1] = new JSprite(sl, 0xca, 2);
    menuItems[2] = new JSprite(sl, 0xc9, 2);
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
    for (WORD id : {0xd0, 0xd1, 0xdb, 0xf8, 0xf7, 0xcd, 0xca, 0xc9}) sl->DestroySurface(id);
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
    // Native: a third entry, OPTIONS, between START and EXIT (EXIT moves from 1 to 2).
    const int last = 2;
    if (in->GetKeyboardPressed(DIK_ESCAPE)) {
        cursorPos = last;
        keyButton = 1;
    }
    JSoundList* snd = app->GetSoundList();
    if (up) {
        snd->Play(0xec, 100, 0);
        cursorPos = cursorPos - 1;
        if (cursorPos < 0) cursorPos = last;
    } else if (down) {
        snd->Play(0xec, 100, 0);
        cursorPos = cursorPos + 1;
        if (cursorPos > last) cursorPos = 0;
    } else if (keyButton) {
        if (cursorPos == 1) {
            app->OpenOptions();  // plays the same confirm sound itself
        } else {
            snd->Play(0xed, 100, 0);
            if (cursorPos == 0) StartFadeOut(0);
            else StartFadeOut(1);
        }
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
    // Native: the original centred its two entries from cy + 0x96; with three, the block starts
    // 20 px higher so EXIT stays on screen.
    int menuY = cy + 0x96 - 20;
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
