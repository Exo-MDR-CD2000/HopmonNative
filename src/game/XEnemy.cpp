// Port of XEnemy.cpp: all enemy types and their AI.
#include <vector>
#include "XGame.h"
#include "../framework/d3dutil.h"

static inline int Wrap4(int v) { return ((v % 4) + 4) % 4; }  // MSVC signed % with sign fix-up

static void StepMapPos(int dir, int& x, int& z) {
    if (dir == 0) z = z - 1;
    else if (dir == 1) x = x + 1;
    else if (dir == 2) z = z + 1;
    else if (dir == 3) x = x - 1;
}

// ============================================================ XEnemyList
XEnemyList::XEnemyList(XGameObject* g) : game(g) {
    for (auto& e : enemyArray) e = nullptr;
}

XEnemyList::~XEnemyList() { Empty(); }

XEnemyBase* XEnemyList::CreateEnemy(BYTE item) {
    int i = GetNullIndex();
    if (i < 0) return nullptr;
    switch (item) {
        case 'a': enemyArray[i] = new XEnemy_Ghost(game); break;
        case 'b': enemyArray[i] = new XEnemy_Cube(game); break;
        case 'c': enemyArray[i] = new XEnemy_Kurage(game); break;
        case 'd': enemyArray[i] = new XEnemy_UFO(game); break;
        case 'e': enemyArray[i] = new XEnemy_Sesame(game); break;
        case 'm': enemyArray[i] = new XNeedleBall(game, 0); break;
        case 'n': enemyArray[i] = new XNeedleBall(game, 2); break;
        case 'o': enemyArray[i] = new XNeedleBallFast(game, 0); break;
        case 'p': enemyArray[i] = new XNeedleBallFast(game, 2); break;
        case 'q': enemyArray[i] = new XNeedleBox(game); break;
        case 'r': enemyArray[i] = new XNeedleBoxDrop(game); break;
        case 'x': enemyArray[i] = new XGate(game, 0); break;
        case 'y': enemyArray[i] = new XGate(game, 1); break;
        default: break;
    }
    return enemyArray[i];
}

void XEnemyList::Empty() {
    for (auto& e : enemyArray) {
        delete e;
        e = nullptr;
    }
}

void XEnemyList::CheckCollisionWithPac() {
    XPac::STATE s = game->pac->GetState();
    if (s == XPac::NON || s == XPac::LEVEL_COMPLETED || s == XPac::DYING) return;
    for (auto* e : enemyArray) {
        if (e && e->GetState() != XEnemyBase::NON && e->GetState() != XEnemyBase::DYING &&
            game->pac->CheckCollisionRadius(e)) {
            game->pac->StartDying();
            return;
        }
    }
}

void XEnemyList::ProcessRotate(DWORD frameCount) {
    for (auto* e : enemyArray)
        if (e) e->ProcessRotate(frameCount);
}

void XEnemyList::ProcessFrame() {
    for (auto& e : enemyArray) {
        if (!e) continue;
        e->ProcessFrame();
        if (e->GetState() == XEnemyBase::NON) {
            delete e;
            e = nullptr;
        }
    }
}

void XEnemyList::Render() {
    for (auto* e : enemyArray)
        if (e) e->Render();
}

int XEnemyList::GetNullIndex() {
    for (int i = 0; i < 30; i++)
        if (!enemyArray[i]) return i;
    return -1;
}

// ============================================================ XEnemyBase
XEnemyBase::XEnemyBase(XGameObject* g, WORD resId)
    : JObject3DX(g->app, resId), game(g), state(NON), localTime(0), mapPosX(-1), mapPosZ(-1), refrencePos(0, 0, 0),
      direction(-1), shadow(nullptr), faceToDir(1), breakable(1), shield(1) {
    shadow = new JObject3D_Shadow(g->app, 1.0f, 1.0f);
    shadow->SetTexture(0xa3, 0.0f, 0.0f, 1.0f, 1.0f);
    state = WAIT;
}

XEnemyBase::~XEnemyBase() {
    delete shadow;
    shadow = nullptr;
}

void XEnemyBase::Render() {
    if (state == NON) return;
    JObject3DX::Render();
    shadow->SetWorldPos(D3DVECTOR(worldPos.x, 1.0f, worldPos.z));
    shadow->Render();
}

void XEnemyBase::SetDirection(int dir) {
    if (faceToDir) {
        if (dir == 0) RotateTo(JAXIS_Y, 0.0f);
        else if (dir == 1) RotateTo(JAXIS_Y, 1.5707964f);
        else if (dir == 2) RotateTo(JAXIS_Y, 3.1415927f);
        else if (dir == 3) RotateTo(JAXIS_Y, 4.712389f);
    }
    direction = dir;
}

void XEnemyBase::StartMoving(int dir) {
    if (state != WAIT) return;
    int newPosX = mapPosX, newPosZ = mapPosZ;
    StepMapPos(dir, newPosX, newPosZ);
    game->map->UpdateEnemyMap(mapPosX, mapPosZ, newPosX, newPosZ);
    mapPosX = newPosX;
    mapPosZ = newPosZ;
    SetDirection(dir);
    refrencePos = worldPos;
    state = MOVE;
    localTime = 0;
}

void XEnemyBase::StartDying(int dir) {
    game->map->ZeroEnemyMap(mapPosX, mapPosZ);
    SetCurrentFrame(0);
    SetDirection(Wrap4(dir + 2));
    refrencePos = worldPos;
    state = DYING;
    localTime = 0;
}

void XEnemyBase::StartFiring() {
    refrencePos = worldPos;
    state = FIRE;
    localTime = 0;
}

void XEnemyBase::DecreaseShield(int dir) {
    shield = shield - 1;
    if (shield < 1) StartDying(dir);
}

void XEnemyBase::ProcessRotate(DWORD frameCount) {
    if (state == DYING) SubProcessDyingRotate(frameCount);
    else if (state == MOVE) SubProcessMoveRotate(frameCount);
    else if (state == FIRE) SubProcessFireRotate(frameCount);
}

void XEnemyBase::SubProcessDyingRotate(DWORD frameCount) {
    float a = (float)frameCount * 0.13962634f;
    if (direction == 0) RotateBy(JAXIS_X, -a);
    else if (direction == 2) RotateBy(JAXIS_X, a);
    else if (direction == 1) RotateBy(JAXIS_Z, a);
    else if (direction == 3) RotateBy(JAXIS_Z, -a);
}

void XEnemyBase::ProcessFrame() {
    localTime = localTime + 1;
    if (state == DYING) SubProcessDying();
    else if (state == MOVE) SubProcessMove();
    else if (state == FIRE) SubProcessFire();
    if (state == WAIT) SubProcessWait();
}

void XEnemyBase::SubProcessDying() {
    float jumpHeight = sinf(((float)localTime / 160.0f) * 3.1415927f) * 120.0f;
    SetWorldPos(refrencePos + D3DVECTOR(0.0f, jumpHeight, 0.0f));
    if (worldPos.y - collisionRadius < 0.0f) {
        game->particleList->CreateEnemyExplode(worldPos);
        state = NON;
        localTime = 0;
    }
}

int XEnemyBase::IsBlocked(int dir) {
    int newPosX = mapPosX, newPosZ = mapPosZ;
    StepMapPos(dir, newPosX, newPosZ);
    return game->map->IsBlocked_Enemy(newPosX, newPosZ);
}

int XEnemyBase::FindDirectionToPac() {
    int pacX = game->pac->GetMapPosX();
    int pacZ = game->pac->GetMapPosZ();
    int diffX = abs(pacX - mapPosX);
    int diffZ = abs(pacZ - mapPosZ);
    int dirToPac = -1;
    if (diffX == 0 && diffZ == 0) dirToPac = -1;
    else if (diffZ < diffX) dirToPac = mapPosX < pacX ? 1 : 3;
    else if (diffX < diffZ) dirToPac = mapPosZ < pacZ ? 2 : 0;
    else if (diffX == diffZ) {
        if (Random(2) == 0) dirToPac = mapPosX < pacX ? 1 : 3;
        else dirToPac = mapPosZ < pacZ ? 2 : 0;
    }
    return dirToPac;
}

void XEnemyBase::DefaultProcessWait() {
    std::vector<int> possibleDirList;
    int dirRight = Wrap4(direction + 1);
    int dirBack = Wrap4(direction + 2);
    int dirLeft = Wrap4(direction + 3);
    if (!IsBlocked(direction)) {
        possibleDirList.push_back(direction);
        possibleDirList.push_back(direction);
    }
    if (!IsBlocked(dirRight)) possibleDirList.push_back(dirRight);
    if (!IsBlocked(dirLeft)) possibleDirList.push_back(dirLeft);
    int dirToPac = FindDirectionToPac();
    if (dirToPac >= 0 && dirToPac != dirBack && !IsBlocked(dirToPac)) possibleDirList.push_back(dirToPac);
    if (possibleDirList.empty()) {
        if (!IsBlocked(dirBack)) StartMoving(dirBack);
    } else {
        long index = Random((long)possibleDirList.size());
        StartMoving(possibleDirList[(size_t)index]);
    }
}

// Shared "slide one chip in `frames` ticks" movement used by most enemies.
static void SlideMove(XEnemyBase* e, unsigned frames) {
    float chip = e->game->map->GetChipSize();
    float movedDistance = e->localTime < frames ? ((float)e->localTime / (float)frames) * chip : chip;
    e->SetWorldPos(e->refrencePos + D3DVectorMultiply(GetVectorFromDir(e->direction), movedDistance));
}

// Rolling rotation used by the cube and needle balls (opposite sense to the dying spin).
static void RollRotate(XEnemyBase* e, float a) {
    if (e->direction == 0) e->RotateBy(JAXIS_X, a);
    else if (e->direction == 2) e->RotateBy(JAXIS_X, -a);
    else if (e->direction == 1) e->RotateBy(JAXIS_Z, -a);
    else if (e->direction == 3) e->RotateBy(JAXIS_Z, a);
}

// ============================================================ Ghost
XEnemy_Ghost::XEnemy_Ghost(XGameObject* g) : XEnemyBase(g, 0x9d) {
    shadow->ScaleBy(40.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(50.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 9.0f);
    SetCollisionRadius(15.0f);
    faceToDir = 1;
    SetDirection((int)Random(4));
    shield = 1;
}

void XEnemy_Ghost::SubProcessMove() {
    SlideMove(this, 0x3c);
    if (localTime > 0x3b) {
        state = WAIT;
        localTime = 0;
    }
}

void XEnemy_Ghost::SubProcessWait() { DefaultProcessWait(); }

// ============================================================ Cube
XEnemy_Cube::XEnemy_Cube(XGameObject* g) : XEnemyBase(g, 0x149) {
    AppendExtraFrame(0xa4);
    SetCurrentFrame(1);
    shadow->ScaleBy(40.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(63.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 10.0f);
    SetCollisionRadius(15.0f);
    faceToDir = 0;
    SetDirection((int)Random(4));
    shield = 2;
}

void XEnemy_Cube::SubProcessMoveRotate(DWORD frameCount) { RollRotate(this, (float)frameCount * 0.02685122f); }

void XEnemy_Cube::SubProcessMove() {
    SlideMove(this, 0x41);
    SetCurrentFrame(1 < shield ? 1 : 0);
    if (localTime > 0x40) {
        state = WAIT;
        localTime = 0;
    }
}

void XEnemy_Cube::SubProcessWait() { DefaultProcessWait(); }

// ============================================================ Kurage (jellyfish)
XEnemy_Kurage::XEnemy_Kurage(XGameObject* g) : XEnemyBase(g, 0xaf) {
    AppendExtraFrame(0xae);
    shadow->ScaleBy(40.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(80.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
    SetCollisionRadius(15.0f);
    faceToDir = 0;
    SetDirection((int)Random(4));
    shield = 1;
}

void XEnemy_Kurage::SubProcessMove() {
    float chip = game->map->GetChipSize();
    float movedDistance, jumpHeight;
    if (localTime < 0x50) {
        movedDistance = ((float)localTime / 80.0f) * chip;
        jumpHeight = sinf(((float)localTime / 80.0f) * 3.1415927f) * 47.0f;
        SetCurrentFrame(1);
    } else {
        movedDistance = chip;
        jumpHeight = 0.0f;
        SetCurrentFrame(0);
    }
    D3DVECTOR moved = D3DVectorMultiply(GetVectorFromDir(direction), movedDistance);
    moved.y = jumpHeight;
    SetWorldPos(refrencePos + moved);
    if (localTime > 0x4f) {
        state = WAIT;
        localTime = 0;
    }
}

void XEnemy_Kurage::SubProcessWait() {
    if (localTime > 0x14) DefaultProcessWait();
}

// ============================================================ UFO
XEnemy_UFO::XEnemy_UFO(XGameObject* g) : XEnemyBase(g, 0xad), moveCount(0), countDownToFire(0) {
    shadow->ScaleBy(50.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(60.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 100.0f);
    SetCollisionRadius(18.0f);
    faceToDir = 0;
    SetDirection((int)Random(4));
    breakable = 0;
    countDownToFire = (int)Random(6) + 3;
}

void XEnemy_UFO::SubProcessMoveRotate(DWORD frameCount) { RotateBy(JAXIS_Y, (float)frameCount * 0.017453292f); }

void XEnemy_UFO::SubProcessFireRotate(DWORD frameCount) { RotateBy(JAXIS_Y, (float)frameCount * 0.04363323f); }

void XEnemy_UFO::SubProcessMove() {
    SlideMove(this, 100);
    if (localTime > 99) {
        moveCount = moveCount + 1;
        state = WAIT;
        localTime = 0;
    }
}

void XEnemy_UFO::SubProcessWait() {
    int newDirection = direction;
    if (moveCount > 2) {
        moveCount = 0;
        countDownToFire = countDownToFire - 1;
        if (countDownToFire < 1) {
            countDownToFire = (int)Random(5) + 2;
            if (!game->map->IsBlocked_Fire(mapPosX, mapPosZ)) {
                BYTE vol = game->GetVolumeOfPosition(worldPos);
                game->app->GetSoundList()->Play(0x13f, vol, 0);
                StartFiring();
                return;
            }
        }
        if (Random(2) == 0) {
            newDirection = FindDirectionToPac();
            if (newDirection < 0) newDirection = (int)Random(4);
        } else {
            newDirection = (int)Random(4);
        }
    }
    if (IsBlocked_UFO(newDirection)) newDirection = (int)Random(4);
    if (!IsBlocked_UFO(newDirection)) StartMoving_UFO(newDirection);
}

void XEnemy_UFO::SubProcessFire() {
    if (localTime == 0xaa) {
        game->enemyFireList->CreateUFOFire(worldPos);
    } else if (localTime > 299) {
        state = WAIT;
        localTime = 0;
    }
}

void XEnemy_UFO::StartMoving_UFO(int dir) {
    if (state != WAIT) return;
    int newPosX = mapPosX, newPosZ = mapPosZ;
    StepMapPos(dir, newPosX, newPosZ);
    game->map->UpdateUFOMap(mapPosX, mapPosZ, newPosX, newPosZ);
    mapPosX = newPosX;
    mapPosZ = newPosZ;
    SetDirection(dir);
    refrencePos = worldPos;
    state = MOVE;
    localTime = 0;
}

int XEnemy_UFO::IsBlocked_UFO(int dir) {
    int newPosX = mapPosX, newPosZ = mapPosZ;
    StepMapPos(dir, newPosX, newPosZ);
    return game->map->IsBlocked_UFO(newPosX, newPosZ);
}

// ============================================================ Sesame
XEnemy_Sesame::XEnemy_Sesame(XGameObject* g) : XEnemyBase(g, 0x14a), moveCountDown(0), movePhase(0) {
    AppendExtraFrame(0x14b);
    AppendExtraFrame(0xd2);
    AppendExtraFrame(0xd4);
    SetCurrentFrame(2);
    shadow->ScaleBy(40.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(53.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 9.0f);
    SetCollisionRadius(15.0f);
    faceToDir = 1;
    SetDirection((int)Random(4));
    shield = 2;
    moveCountDown = (int)Random(0xf) + 0xf;
}

void XEnemy_Sesame::SubProcessMove() {
    int frameOffset = shield < 2 ? 0 : 2;
    unsigned moveFrame;
    if (movePhase == 0) {
        moveFrame = 0x46;
        SetCurrentFrame(localTime < 0x23 ? frameOffset : frameOffset + 1);
    } else {
        moveFrame = 0x19;
        SetCurrentFrame(frameOffset + 1);
    }
    SlideMove(this, moveFrame);
    if (moveFrame <= localTime) {
        moveCountDown = moveCountDown - 1;
        state = WAIT;
        localTime = 0;
    }
}

void XEnemy_Sesame::SubProcessWait() {
    if (movePhase == 0) {
        if (moveCountDown < 1) movePhase = 1;
        else DefaultProcessWait();
    } else if (movePhase == 1) {
        SetCurrentFrame(shield < 2 ? 0 : 2);
        if (localTime > 0xf9) {
            moveCountDown = 10;
            movePhase = 2;
        }
    } else {
        if (moveCountDown < 1) {
            moveCountDown = (int)Random(0xf) + 0xf;
            movePhase = 0;
        }
        DefaultProcessWait();
    }
}

// ============================================================ needle balls

#define NEEDLE_WAIT_BODY                                       \
    int dirForward = direction;                                \
    int dirRight = Wrap4(direction + 1);                       \
    int dirBack = Wrap4(direction + 2);                        \
    int dirLeft = Wrap4(direction + 3);                        \
    if (!IsBlocked(dirForward)) StartMoving(dirForward);       \
    else if (!IsBlocked(dirRight)) StartMoving(dirRight);      \
    else if (!IsBlocked(dirLeft)) StartMoving(dirLeft);        \
    else if (!IsBlocked(dirBack)) StartMoving(dirBack);

XNeedleBall::XNeedleBall(XGameObject* g, int dir) : XEnemyBase(g, 0xa5) {
    shadow->ScaleBy(40.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(30.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
    SetCollisionRadius(15.0f);
    faceToDir = 0;
    SetDirection(dir);
    breakable = 0;
}

void XNeedleBall::SubProcessMoveRotate(DWORD frameCount) { RollRotate(this, (float)frameCount * 0.03141593f); }

void XNeedleBall::SubProcessMove() {
    SlideMove(this, 0x32);
    if (localTime > 0x31) {
        state = WAIT;
        localTime = 0;
    }
}

void XNeedleBall::SubProcessWait() { NEEDLE_WAIT_BODY }

XNeedleBallFast::XNeedleBallFast(XGameObject* g, int dir) : XEnemyBase(g, 0xb3) {
    shadow->ScaleBy(40.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(30.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
    SetCollisionRadius(15.0f);
    faceToDir = 0;
    SetDirection(dir);
    breakable = 0;
}

void XNeedleBallFast::SubProcessMoveRotate(DWORD frameCount) { RollRotate(this, (float)frameCount * 0.044879895f); }

void XNeedleBallFast::SubProcessMove() {
    SlideMove(this, 0x23);
    if (localTime > 0x22) {
        state = WAIT;
        localTime = 0;
    }
}

void XNeedleBallFast::SubProcessWait() { NEEDLE_WAIT_BODY }

// ============================================================ needle boxes (crushers)
XNeedleBox::XNeedleBox(XGameObject* g) : XEnemyBase(g, 0xb2) {
    shadow->ScaleBy(40.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(35.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 60.0f);
    SetCollisionRadius(15.0f);
    breakable = 0;
}

void XNeedleBox::SubProcessMove() {
    float drop = 0.0f;
    if (localTime > 199) {
        if (localTime < 0xf0) {
            int t = (int)localTime - 200;
            drop = 60.0f - cosf(((float)t / 40.0f) * 1.5707964f) * 60.0f;
        } else if (localTime < 0x186) {
            if (localTime == 0xf0) {
                BYTE vol = game->GetVolumeOfPosition(worldPos);
                game->app->GetSoundList()->Play(0xf5, vol, 0);
            }
            drop = 60.0f;
        } else if (localTime < 0x21c) {
            int t = (int)localTime - 0x186;
            drop = 60.0f - (float)t * 0.4f;
        }
    }
    SetWorldPos(refrencePos + D3DVECTOR(0.0f, -drop, 0.0f));
    if (localTime > 0x21b) {
        state = WAIT;
        localTime = 0;
    }
}

void XNeedleBox::SubProcessWait() {
    refrencePos = worldPos;
    state = MOVE;
    localTime = 0;
}

XNeedleBoxDrop::XNeedleBoxDrop(XGameObject* g) : XEnemyBase(g, 0xb1) {
    shadow->ScaleBy(40.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(35.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 60.0f);
    SetCollisionRadius(15.0f);
    breakable = 0;
}

void XNeedleBoxDrop::SubProcessMove() {
    float drop = 0.0f;
    if (localTime > 0x4f) {
        if (localTime < 0x78) {
            int t = (int)localTime - 0x50;
            drop = 60.0f - cosf(((float)t / 40.0f) * 1.5707964f) * 60.0f;
        } else {
            if (localTime == 0x78) {
                BYTE vol = game->GetVolumeOfPosition(worldPos);
                game->app->GetSoundList()->Play(0xf5, vol, 0);
            }
            drop = 60.0f;
        }
    }
    SetWorldPos(refrencePos + D3DVECTOR(0.0f, -drop, 0.0f));
}

void XNeedleBoxDrop::SubProcessWait() {
    if (game->pac->GetMapPosX() == mapPosX && game->pac->GetMapPosZ() == mapPosZ) {
        refrencePos = worldPos;
        state = MOVE;
        localTime = 0;
    }
}

// ============================================================ gate
XGate::XGate(XGameObject* g, int dir) : XEnemyBase(g, 0xb4) {
    shadow->ScaleBy(0.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(35.0f);
    ShiftWorldPos_OnGround();
    SetCollisionRadius(10.0f);
    faceToDir = 1;
    SetDirection(dir);
    shield = 1;
}
