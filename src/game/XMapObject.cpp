// Port of XMapObject.cpp: static/active scenery (cannons, volcano, steam vents, poles, trees).
#include "XGame.h"

// ============================================================ XMapObjectList
XMapObjectList::XMapObjectList(XGameObject* g) : game(g) {
    for (auto& o : objArray) o = nullptr;
}

XMapObjectList::~XMapObjectList() { Empty(); }

XMapObjectBase* XMapObjectList::CreateMapObject(BYTE item) {
    if (item == 'Z') return nullptr;
    int i = GetNullIndex();
    if (i < 0) return nullptr;
    switch (item) {
        case 'A': objArray[i] = new XCannonUp(game); break;
        case 'N': objArray[i] = new XCannon(game, 0); break;
        case 'S': objArray[i] = new XCannon(game, 2); break;
        case 'E': objArray[i] = new XCannon(game, 1); break;
        case 'W': objArray[i] = new XCannon(game, 3); break;
        case 'V': objArray[i] = new XVolcano(game); break;
        case '!': objArray[i] = new XSteam(game); break;
        case 'P':
            if (game->app->GetCurrentWorld() == 0) objArray[i] = new XPole(game);
            else if (game->app->GetCurrentWorld() == 1) objArray[i] = new XTree(game);
            else objArray[i] = new XShortPole(game);
            break;
        default: break;
    }
    return objArray[i];
}

void XMapObjectList::Empty() {
    for (auto& o : objArray) {
        delete o;
        o = nullptr;
    }
}

void XMapObjectList::ProcessRotate(DWORD frameCount) {
    for (auto* o : objArray)
        if (o) o->ProcessRotate(frameCount);
}

void XMapObjectList::ProcessFrame() {
    for (auto& o : objArray) {
        if (!o) continue;
        o->ProcessFrame();
        if (o->GetState() == XMapObjectBase::NON) {
            delete o;
            o = nullptr;
        }
    }
}

void XMapObjectList::Render() {
    for (auto* o : objArray)
        if (o) o->Render();
}

int XMapObjectList::GetNullIndex() {
    for (int i = 0; i < 25; i++)
        if (!objArray[i]) return i;
    return -1;
}

// ============================================================ XMapObjectBase
XMapObjectBase::XMapObjectBase(XGameObject* g, WORD resId)
    : JObject3DX(g->app, resId), game(g), state(NON), localTime(0), direction(-1) {
    shadow = new JObject3D_Shadow(g->app, 1.0f, 1.0f);
    shadow->SetTexture(0xa3, 0.0f, 0.0f, 1.0f, 1.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    state = PROCESS;
}

XMapObjectBase::~XMapObjectBase() {
    delete shadow;
    shadow = nullptr;
}

void XMapObjectBase::Render() {
    if (state == NON) return;
    JObject3DX::Render();
    shadow->SetWorldPos(D3DVECTOR(worldPos.x, 1.0f, worldPos.z));
    shadow->Render();
}

void XMapObjectBase::SetDirection(int dir, int rotate) {
    if (rotate) {
        if (dir == 0) RotateTo(JAXIS_Y, 0.0f);
        else if (dir == 1) RotateTo(JAXIS_Y, 1.5707964f);
        else if (dir == 2) RotateTo(JAXIS_Y, 3.1415927f);
        else if (dir == 3) RotateTo(JAXIS_Y, 4.712389f);
    }
    direction = dir;
}

// ============================================================ concrete objects
XVolcano::XVolcano(XGameObject* g) : XMapObjectBase(g, 0xb0) {
    shadow->ScaleBy(0.0f);
    JObject3DX::ScaleBy(135.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 2.0f);
}

void XVolcano::ProcessFrame() {
    localTime = localTime + 1;
    if (state == PROCESS && localTime > 0x5db) {
        if (localTime < 0x9c4) {
            if (localTime % 0x32 == 0) {
                D3DVECTOR p(worldPos.x, worldPos.y + 20.0f, worldPos.z);
                game->enemyFireList->CreateVolcanoFire(p);
            }
        } else {
            localTime = 0;
        }
    }
}

XCannonUp::XCannonUp(XGameObject* g) : XMapObjectBase(g, 0xa6) {
    shadow->ScaleBy(60.0f);
    JObject3DX::ScaleBy(80.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
}

void XCannonUp::ProcessFrame() {
    localTime = localTime + 1;
    if (state == PROCESS && localTime % 200 == 0) {
        D3DVECTOR p(worldPos.x, worldPos.y + 10.0f, worldPos.z);
        game->enemyFireList->CreateCannonUpFire(p);
    }
}

XCannon::XCannon(XGameObject* g, int dir) : XMapObjectBase(g, 0xac) {
    shadow->ScaleBy(55.0f);
    JObject3DX::ScaleBy(80.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
    SetDirection(dir, 1);
}

void XCannon::ProcessFrame() {
    localTime = localTime + 1;
    if (state == PROCESS && localTime % 0x1c2 == 0) game->enemyFireList->CreateCannonFire(worldPos, direction);
}

XPole::XPole(XGameObject* g) : XMapObjectBase(g, 0xa7) {
    shadow->ScaleBy(60.0f);
    JObject3DX::ScaleBy(120.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
}

XShortPole::XShortPole(XGameObject* g) : XMapObjectBase(g, 0x13b) {
    shadow->ScaleBy(55.0f);
    JObject3DX::ScaleBy(145.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
}

XTree::XTree(XGameObject* g) : XMapObjectBase(g, 0xa8) {
    shadow->ScaleBy(90.0f);
    JObject3DX::ScaleBy(130.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
}

XSteam::XSteam(XGameObject* g) : XMapObjectBase(g, 0xffff) {
    shadow->ScaleBy(0.0f);
    SetWorldPos(JAXIS_Y, 0.0f);
}

void XSteam::ProcessFrame() {
    localTime = localTime + 1;
    if (state == PROCESS) {
        if (localTime == 200) {
            BYTE vol = game->GetVolumeOfPosition(worldPos);
            game->app->GetSoundList()->Play(0x13e, vol, 0);
        }
        if (localTime > 200 && localTime % 10 == 0) game->enemyFireList->CreateSteamFire(worldPos);
        if (localTime > 400) localTime = 0;
    }
}
