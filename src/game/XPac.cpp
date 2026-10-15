// Port of XPac.cpp: the player character and its shots.
#include "XGame.h"
#include "../core/config.h"
#include "../framework/d3dtextr.h"
#include "../framework/d3dutil.h"
#include "../gfx/device.h"

D3DVECTOR GetVectorFromDir(int dir) {
    if (dir == 0) return D3DVECTOR(0.0f, 0.0f, 1.0f);
    if (dir == 1) return D3DVECTOR(1.0f, 0.0f, 0.0f);
    if (dir == 2) return D3DVECTOR(0.0f, 0.0f, -1.0f);
    if (dir == 3) return D3DVECTOR(-1.0f, 0.0f, 0.0f);
    return D3DVECTOR(0.0f, 0.0f, 0.0f);
}

// ============================================================ XPacFireList
XPacFireList::XPacFireList(XGameObject* g) : game(g) {
    for (auto& f : fireArray) f = nullptr;
}

XPacFireList::~XPacFireList() { Empty(); }

void XPacFireList::CreateFire() {
    int index = GetNullIndex();
    if (index < 0) return;
    game->app->GetSoundList()->Play(0xee, 100, 0);
    fireArray[index] = new XPacFire(game, game->pac->GetFaceDirection());
    fireArray[index]->SetWorldPos(game->pac->GetWorldPos());
    D3DVECTOR dirVector = GetVectorFromDir(game->pac->GetFaceDirection());
    fireArray[index]->ShiftWorldPos(D3DVectorMultiply(dirVector, 20.0f));
    fireArray[index]->SnapInterpolation();
}

void XPacFireList::Empty() {
    for (auto& f : fireArray) {
        delete f;
        f = nullptr;
    }
}

void XPacFireList::CheckCollisionWithEnemy() {
    for (int i = 0; i < 3; i++) {
        if (!fireArray[i] || fireArray[i]->GetState() == XPacFire::NON) continue;
        for (DWORD e = 0; e < 30; e++) {
            XEnemyBase* enemy = game->enemyList->GetAt(e);
            if (enemy && enemy->GetState() != XEnemyBase::NON && enemy->GetState() != XEnemyBase::DYING &&
                enemy->CheckCollisionRadius(fireArray[i])) {
                if (enemy->Breakable()) {
                    enemy->DecreaseShield(fireArray[i]->GetDirection());
                    game->particleList->CreatePacFireHit(fireArray[i]->GetWorldPos());
                }
                fireArray[i]->SetState(XPacFire::NON);
            }
        }
    }
}

void XPacFireList::ProcessFrame() {
    for (auto& f : fireArray) {
        if (!f) continue;
        f->ProcessFrame();
        if (f->GetState() == XPacFire::NON) {
            delete f;
            f = nullptr;
        }
    }
}

void XPacFireList::Render(const D3DMATRIX& view) {
    for (auto* f : fireArray)
        if (f) f->Render2(view);
}

int XPacFireList::GetNullIndex() {
    for (int i = 0; i < 3; i++)
        if (!fireArray[i]) return i;
    return -1;
}

// ============================================================ XPacFire
XPacFire::XPacFire(XGameObject* g, int dir)
    : JObject3D_Particle(g->app, 24.0f, 24.0f), game(g), state(NON), localTime(0), direction(dir) {
    SetCollisionRadius(8.0f);
    state = MOVE;
}

XPacFire::~XPacFire() {}

void XPacFire::Render2(const D3DMATRIX& view) {
    if (state == NON) return;
    _SetWorldTransform2(view);
    _SetRenderState();
    Device* d = app->GetD3DDevice();
    for (int i = 0; i < 3; i++) {
        if (i == 0) SetTexture(0xa1, 0.0f, 0.0f, 1.0f, 1.0f);
        else _SetTextureRandomly();
        if (textureResId != 0) d->SetTexture(0, D3DTextr_GetSurface(textureResId));
        d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_LVERTEX, vertexList, vertexCount, indexList, indexCount, 0);
    }
    game->app->GetD3DDevice()->SetTexture(0, nullptr);
    _ResetRenderState();
    _ResetWorldTransform();
}

void XPacFire::ProcessFrame() {
    localTime = localTime + 1;
    if (state != MOVE) return;
    ShiftWorldPos(D3DVectorMultiply(GetVectorFromDir(direction), 2.5f));
    int mx = game->map->ConvertPosXToMapX(worldPos.x);
    int mz = game->map->ConvertPosZToMapZ(worldPos.z);
    if (game->map->IsBlocked_Fire(mx, mz)) {
        state = NON;
        localTime = 0;
    }
}

void XPacFire::_SetTextureRandomly() {
    const float a = 0.33333334f, b = 0.6666667f;
    switch (RenderRandom(9)) {
        case 0: SetTexture(0xa0, 0.0f, 0.0f, a, a); break;
        case 1: SetTexture(0xa0, a, 0.0f, b, a); break;
        case 2: SetTexture(0xa0, b, 0.0f, 1.0f, a); break;
        case 3: SetTexture(0xa0, 0.0f, a, a, b); break;
        case 4: SetTexture(0xa0, a, a, b, b); break;
        case 5: SetTexture(0xa0, b, a, 1.0f, b); break;
        case 6: SetTexture(0xa0, 0.0f, b, a, 1.0f); break;
        case 7: SetTexture(0xa0, a, b, b, 1.0f); break;
        case 8: SetTexture(0xa0, b, b, 1.0f, 1.0f); break;
    }
}

// ============================================================ XPac
XPac::XPac(XGameObject* g)
    : JObject3DX(g->app, 0x94), game(g), state(NON), localTime(0), mapPosX(-1), mapPosZ(-1), faceDirection(0),
      moveDirection(0), refrencePos(0, 0, 0), shadow(nullptr), pacMoveFrame(0), fireEnergy(0) {
    shadow = new JObject3D_Shadow(g->app, 40.0f, 40.0f);
    AppendExtraFrame(0x95);
    AppendExtraFrame(0x96);
    AppendExtraFrame(199);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(45.0f);
    SetCollisionRadius(13.0f);
    shadow->SetTexture(0xa3, 0.0f, 0.0f, 1.0f, 1.0f);
}

XPac::~XPac() {
    delete shadow;
    shadow = nullptr;
}

void XPac::Render() {
    if (state == NON) return;
    JObject3DX::Render();
    shadow->SetWorldPos(D3DVECTOR(worldPos.x, 1.0f, worldPos.z));
    shadow->Render();
}

void XPac::Reset() {
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 3.0f);
    RotateTo(JAXIS_Y, 0.0f);
    faceDirection = 0;
    moveDirection = 0;
    SetCurrentFrame(0);
    state = WAIT;
    localTime = 0;
    SnapInterpolation();
}

void XPac::SetFaceDirection(int dir) {
    faceDirection = dir;
    if (dir == 0) RotateTo(JAXIS_Y, 0.0f);
    else if (dir == 1) RotateTo(JAXIS_Y, 1.5707964f);
    else if (dir == 2) RotateTo(JAXIS_Y, 3.1415927f);
    else if (dir == 3) RotateTo(JAXIS_Y, 4.712389f);
}

static void StepMapPos(int dir, int& x, int& z) {
    if (dir == 0) z = z - 1;
    else if (dir == 1) x = x + 1;
    else if (dir == 2) z = z + 1;
    else if (dir == 3) x = x - 1;
}

void XPac::StartMoving(int dir) {
    if (dir < 0 || state != WAIT) return;
    SetFaceDirection(dir);
    moveDirection = dir;
    int newPosX = mapPosX, newPosZ = mapPosZ;
    StepMapPos(dir, newPosX, newPosZ);
    if (game->map->IsBlocked_Pac(newPosX, newPosZ)) return;
    game->app->GetSoundList()->Play(0xff, 100, 0);
    refrencePos = worldPos;
    mapPosX = newPosX;
    mapPosZ = newPosZ;
    // each carried crystal makes the hop 3 ticks slower ([cheats] no_crystal_slowdown removes that)
    pacMoveFrame = GetConfig().noCrystalSlowdown ? 0x28u : (unsigned)(game->crystalList->GetTakenCount() * 3 + 0x28);
    state = MOVE;
    localTime = 0;
}

void XPac::StartArrow(int dir) {
    if (dir < 0 || state != WAIT) return;
    moveDirection = dir;
    int newPosX = mapPosX, newPosZ = mapPosZ;
    StepMapPos(dir, newPosX, newPosZ);
    if (game->map->IsBlocked_Pac(newPosX, newPosZ)) return;
    SetCurrentFrame(3);
    refrencePos = worldPos;
    mapPosX = newPosX;
    mapPosZ = newPosZ;
    state = ON_ARROW;
    localTime = 0;
}

// The level-complete sequence: jingle, camera swing, Hopmon flies up (inlined in the original's
// ProcessFrame; a separate function here so test automation can start it too).
void XPac::StartLevelCompleted() {
    game->app->GetSoundList()->Play(0xf0, 100, 0);
    game->camera->StartLevelCompleted();
    SetCurrentFrame(3);
    state = LEVEL_COMPLETED;
    localTime = 0;
}

void XPac::StartDying() {
    if (state == NON || state == DYING) return;
    game->app->GetMidiList()->Stop();
    game->app->GetSoundList()->Play(0xef, 100, 0);
    game->camera->StartGameOver();
    game->crystalList->StartGameOver();
    SetCurrentFrame(3);
    refrencePos = worldPos;
    state = DYING;
    localTime = 0;
}

void XPac::ProcessRotate(DWORD frameCount) {
    if (state == LEVEL_COMPLETED) {
        RotateBy(JAXIS_Y, (float)frameCount * 0.06981317f);
    } else if (state == DYING) {
        float a = (float)frameCount * 0.13962634f;
        if (faceDirection == 0) RotateBy(JAXIS_X, -a);
        else if (faceDirection == 2) RotateBy(JAXIS_X, a);
        else if (faceDirection == 1) RotateBy(JAXIS_Z, a);
        else if (faceDirection == 3) RotateBy(JAXIS_Z, -a);
    }
}

void XPac::ProcessFrame() {
    localTime = localTime + 1;
    if (GetConfig().infiniteFireEnergy) fireEnergy = 1000;  // [cheats] gauge always full
    if (fireEnergy < 1000) {
        fireEnergy = fireEnergy + 3;
        if (fireEnergy > 1000) {
            game->app->GetSoundList()->Play(0xfe, 100, 0);
            fireEnergy = 1000;
        }
    }

    if (state == MOVE) {
        float movedDistance = 0.0f, jumpHeight = 0.0f;
        if (localTime < pacMoveFrame) {
            movedDistance = ((float)localTime / (float)pacMoveFrame) * game->map->GetChipSize();
            jumpHeight = sinf(((float)localTime / (float)pacMoveFrame) * 3.1415927f) * 15.0f;
            unsigned fifth = pacMoveFrame / 5;
            if (localTime < fifth * 2) SetCurrentFrame(1);
            else if (localTime < fifth * 3) SetCurrentFrame(0);
            else SetCurrentFrame(2);
        } else {
            movedDistance = game->map->GetChipSize();
            jumpHeight = 0.0f;
            SetCurrentFrame(0);
        }
        D3DVECTOR moved = D3DVectorMultiply(GetVectorFromDir(moveDirection), movedDistance);
        moved.y = jumpHeight;
        SetWorldPos(refrencePos + moved);
        if (pacMoveFrame <= localTime) {
            state = WAIT;
            localTime = 0;
        }
    } else if (state == ON_ARROW) {
        float movedDistance2;
        if (localTime < 0x14) movedDistance2 = ((float)localTime / 20.0f) * game->map->GetChipSize();
        else movedDistance2 = game->map->GetChipSize();
        SetWorldPos(refrencePos + D3DVectorMultiply(GetVectorFromDir(moveDirection), movedDistance2));
        if (localTime > 0x13) {
            SetCurrentFrame(0);
            state = WAIT;
            localTime = 0;
        }
    } else if (state == LEVEL_COMPLETED) {
        ShiftWorldPos(JAXIS_Y, 0.7f);
        if (worldPos.y > 500.0f) {
            game->particleList->CreatePacVanish(worldPos);
            game->StartProcess(1);
            state = NON;
        }
    } else if (state == DYING) {
        float jumpHeight2 = sinf(((float)localTime / 200.0f) * 3.1415927f) * 140.0f;
        SetWorldPos(refrencePos + D3DVECTOR(0.0f, jumpHeight2, 0.0f));
        if (worldPos.y - collisionRadius < 0.0f) {
            game->particleList->CreateEnemyExplode(worldPos);
            game->StartProcess(2);
            state = NON;
        }
    }

    if (state == WAIT) {
        if (!game->map->IsWarpZone(mapPosX, mapPosZ)) {
            if (game->map->IsArrow(mapPosX, mapPosZ)) StartArrow(game->map->GetArrowDirection(mapPosX, mapPosZ));
        } else if (localTime != 0 && localTime % 0x50 == 0) {
            if (game->crystalList->GetCrystalLeft() == 0) {
                StartLevelCompleted();
            } else {
                game->crystalList->DoVanishCrystal();
            }
        }
    }
}
