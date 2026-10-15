// Port of XChara.cpp: crystals, particles, enemy projectiles.
#include "XGame.h"
#include "../framework/d3dtextr.h"
#include "../framework/d3dutil.h"
#include "../gfx/device.h"

// Render-only randomness (the original drew it from the gameplay RNG on every rendered frame;
// separating it keeps gameplay independent of the display frame rate).
static uint32_t g_renderRand = 12345;
long RenderRandom(long range) {
    g_renderRand = g_renderRand * 214013u + 2531011u;
    return range > 0 ? (long)((g_renderRand >> 16) & 0x7FFF) % range : 0;
}

// ============================================================ XCrystalList
XCrystalList::XCrystalList(XGameObject* g) : game(g), takenCount(0), crystalLeft(0) {
    for (auto& c : crystalArray) c = nullptr;
}

XCrystalList::~XCrystalList() { Empty(); }

XCrystal* XCrystalList::CreateCrystal() {
    int i = GetNullIndex();
    if (i < 0) return nullptr;
    crystalLeft = crystalLeft + 1;
    crystalArray[i] = new XCrystal(game);
    return crystalArray[i];
}

void XCrystalList::Empty() {
    takenCount = 0;
    crystalLeft = 0;
    for (auto& c : crystalArray) {
        delete c;
        c = nullptr;
    }
}

void XCrystalList::CheckCollisionWithPac() {
    XPac::STATE s = game->pac->GetState();
    if (s == XPac::NON || s == XPac::LEVEL_COMPLETED || s == XPac::DYING || s == XPac::WAIT) return;
    for (auto* c : crystalArray) {
        if (c && c->GetState() == XCrystal::PROCESS && game->pac->CheckCollisionRadius(c)) {
            takenCount = takenCount + 1;
            c->StartTaken(takenCount);
        }
    }
}

void XCrystalList::DoVanishCrystal() {
    if (takenCount <= 0) return;
    for (auto* c : crystalArray) {
        if (c && c->GetState() == XCrystal::TAKEN && c->GetTakenPosition() == takenCount) {
            c->StartVanishing();
            takenCount = takenCount - 1;
            crystalLeft = crystalLeft - 1;
            return;
        }
    }
}

void XCrystalList::StartGameOver() {
    for (auto* c : crystalArray) {
        if (c && c->GetState() == XCrystal::TAKEN) {
            c->StartGameOver();
            takenCount = takenCount - 1;
        }
    }
}

void XCrystalList::ProcessRotate(DWORD frameCount) {
    for (auto* c : crystalArray)
        if (c) c->ProcessRotate(frameCount);
}

void XCrystalList::ProcessFrame() {
    for (auto& c : crystalArray) {
        if (!c) continue;
        c->ProcessFrame();
        if (c->GetState() == XCrystal::NON) {
            delete c;
            c = nullptr;
        }
    }
}

void XCrystalList::Render() {
    for (auto* c : crystalArray)
        if (c) c->Render();
}

int XCrystalList::GetNullIndex() {
    for (int i = 0; i < 30; i++)
        if (!crystalArray[i]) return i;
    return -1;
}

// ============================================================ XCrystal
XCrystal::XCrystal(XGameObject* g)
    : JObject3DX(g->app, 0xab), game(g), state(NON), localTime(0), takenPosition(0), dirVector(0, 0, 0),
      referencePos(0, 0, 0) {
    shadow = new JObject3D_Shadow(g->app, 20.0f, 20.0f);
    shadow->SetTexture(0xa3, 0.0f, 0.0f, 1.0f, 1.0f);
    JObject3DX::RotateBy(JAXIS_X, -1.5707964f);
    JObject3DX::ScaleBy(30.0f);
    ShiftWorldPos_OnGround();
    ShiftWorldPos(JAXIS_Y, 30.0f);
    SetCollisionRadius(8.0f);
    SetFlatShade(1);
    state = PROCESS;
}

XCrystal::~XCrystal() {
    delete shadow;
    shadow = nullptr;
}

void XCrystal::Render() {
    if (state == NON) return;
    JObject3DX::Render();
    if (state == PROCESS) {
        shadow->SetWorldPos(D3DVECTOR(worldPos.x, 1.0f, worldPos.z));
        shadow->Render();
    }
}

void XCrystal::StartTaken(int pos) {
    game->app->GetSoundList()->Play(0xe8, 100, 0);
    takenPosition = pos;
    state = TAKEN;
    localTime = 0;
}

void XCrystal::StartVanishing() {
    game->app->GetSoundList()->Play(0xe9, 100, 0);
    state = VANISHING;
    localTime = 0;
}

void XCrystal::StartGameOver() {
    long a = Random(0x168);
    dirVector.y = 0.0f;
    dirVector.x = sinf((float)a * 0.017453292f);
    dirVector.z = cosf((float)a * 0.017453292f);
    referencePos = worldPos;
    state = GAME_OVER;
    localTime = 0;
}

void XCrystal::ProcessRotate(DWORD frameCount) {
    if (state == PROCESS) RotateBy(JAXIS_Y, (float)frameCount * 0.012217305f);
    else if (state == VANISHING) RotateBy(JAXIS_Y, (float)frameCount * 0.06981317f);
    else if (state == GAME_OVER) RotateBy(JAXIS_Y, (float)frameCount * 0.06981317f);
}

void XCrystal::ProcessFrame() {
    localTime = localTime + 1;
    if (state == TAKEN) {
        SetWorldPos(game->pac->GetWorldPos());
        ShiftWorldPos(JAXIS_Y, (float)(takenPosition - 1) * 12.0f + 20.0f);
    } else if (state == VANISHING) {
        ShiftWorldPos(JAXIS_Y, 0.8f);
        if (worldPos.y > 800.0f) {
            state = NON;
            localTime = 0;
        }
    } else if (state == GAME_OVER) {
        float movedDistance = ((float)localTime / 160.0f) * 160.0f;
        float jumpHeight;
        if (localTime < 0xa0) jumpHeight = sinf(((float)localTime / 160.0f) * 3.1415927f) * 60.0f;
        else jumpHeight = (float)(localTime - 0xa0) * -1.4f;
        D3DVECTOR moved = D3DVectorMultiply(dirVector, movedDistance);
        moved.y = jumpHeight;
        SetWorldPos(referencePos + moved);
        if (worldPos.y < -20.0f) {
            state = NON;
            localTime = 0;
        }
    }
}

// ============================================================ XParticleList
XParticleList::XParticleList(XGameObject* g) : game(g), _arrayPointer(0) {
    for (auto& p : particleArray) p = nullptr;
}

XParticleList::~XParticleList() { Empty(); }

void XParticleList::Empty() {
    for (auto& p : particleArray) {
        delete p;
        p = nullptr;
    }
}

void XParticleList::ProcessFrame() {
    for (auto& p : particleArray) {
        if (!p) continue;
        p->ProcessFrame();
        if (p->GetState() == XParticleBase::NON) {
            delete p;
            p = nullptr;
        }
    }
}

void XParticleList::Render(const D3DMATRIX& view) {
    for (auto* p : particleArray)
        if (p) p->Render2(view);
}

void XParticleList::CreateEnemyExplode(D3DVECTOR pos) {
    BYTE volume = game->GetVolumeOfPosition(pos);
    game->app->GetSoundList()->Play(0xeb, volume, 0);
    for (int i = 0; i < 0x1e; i++) {
        int index = GetNullIndex();
        if (index < 0) break;
        XParticle_EnemyExplode* particle = new XParticle_EnemyExplode(game, pos);
        int verticalAngle = (int)Random(0xb4);
        long horizontalAngle = Random(0x168);
        float vDistance = sinf((float)verticalAngle * 0.017453292f);
        float hDistance = cosf((float)verticalAngle * 0.017453292f);
        D3DVECTOR v;
        v.y = vDistance;
        v.x = sinf((float)horizontalAngle * 0.017453292f) * hDistance;
        v.z = cosf((float)horizontalAngle * 0.017453292f) * hDistance;
        particle->SetDirVector(v);
        particleArray[index] = particle;
    }
}

void XParticleList::CreateEnemyFireSmokePurple(D3DVECTOR pos) {
    int i = GetNullIndex();
    if (i >= 0) particleArray[i] = new XParticle_EnemyFireSmokePurple(game, pos);
}

void XParticleList::CreateEnemyFireSmokeGreen(D3DVECTOR pos) {
    int i = GetNullIndex();
    if (i >= 0) particleArray[i] = new XParticle_EnemyFireSmokeGreen(game, pos);
}

void XParticleList::CreatePacFireHit(D3DVECTOR pos) {
    int i = GetNullIndex();
    if (i < 0) return;
    BYTE volume = game->GetVolumeOfPosition(pos);
    game->app->GetSoundList()->Play(0xea, volume, 0);
    particleArray[i] = new XParticle_PacFireHit(game, pos, 70.0f, 0x10);
}

void XParticleList::CreatePacVanish(D3DVECTOR pos) {
    int i = GetNullIndex();
    if (i < 0) return;
    game->app->GetSoundList()->Play(0xf1, 100, 0);
    particleArray[i] = new XParticle_PacFireHit(game, pos, 150.0f, 0x96);
}

int XParticleList::GetNullIndex() {
    WORD start = _arrayPointer;
    int index = -1;
    do {
        if (index != -1) return index;
        if (!particleArray[_arrayPointer]) index = _arrayPointer;
        _arrayPointer = (WORD)((_arrayPointer + 1) % 0x96);
    } while (_arrayPointer != start);
    return index;
}

// ============================================================ particles
XParticleBase::XParticleBase(XGameObject* g, D3DVECTOR pos, float size)
    : JObject3D_Particle(g->app, size, size), game(g), state(NON), localTime(0), refrencePos(pos) {
    SetWorldPos(pos);
    state = PROCESS;
}

XParticleBase::~XParticleBase() {}

void XParticleBase::Render2(const D3DMATRIX& view) {
    if (state != NON) JObject3D_Rect::Render2(view);
}

XParticle_EnemyExplode::XParticle_EnemyExplode(XGameObject* g, D3DVECTOR pos)
    : XParticleBase(g, pos, 50.0f), dirVector(0, 0, 0) {
    JObject3D_Rect::SetTexture(0xa2, 0.0f, 0.0f, 1.0f, 1.0f);
}

void XParticle_EnemyExplode::ProcessFrame() {
    localTime = localTime + 1;
    if (state != PROCESS) return;
    float scaleValue = 0.0f, movedDistance = 0.0f;
    if (localTime < 0x78) {
        scaleValue = 1.0f - (float)localTime / 120.0f;
        movedDistance = sinf(((float)localTime / 120.0f) * 1.5707964f) * 55.0f;
    }
    ScaleTo(scaleValue);
    SetWorldPos(refrencePos + D3DVectorMultiply(dirVector, movedDistance));
    if (localTime > 0x77) {
        state = NON;
        localTime = 0;
    }
}

XParticle_EnemyFireSmokePurple::XParticle_EnemyFireSmokePurple(XGameObject* g, D3DVECTOR pos)
    : XParticleBase(g, pos, 18.0f) {
    JObject3D_Rect::SetTexture(0xaa, 0.0f, 0.0f, 1.0f, 1.0f);
}

void XParticle_EnemyFireSmokePurple::ProcessFrame() {
    localTime = localTime + 1;
    if (state != PROCESS) return;
    float scaleValue = 0.0f;
    if (localTime < 0x32) scaleValue = 1.0f - (float)localTime / 50.0f;
    ScaleTo(scaleValue);
    if (localTime > 0x31) {
        state = NON;
        localTime = 0;
    }
}

XParticle_EnemyFireSmokeGreen::XParticle_EnemyFireSmokeGreen(XGameObject* g, D3DVECTOR pos)
    : XParticleBase(g, pos, 17.0f) {
    JObject3D_Rect::SetTexture(0xb7, 0.0f, 0.0f, 1.0f, 1.0f);
}

void XParticle_EnemyFireSmokeGreen::ProcessFrame() {
    localTime = localTime + 1;
    if (state != PROCESS) return;
    float scaleValue = 0.0f;
    if (localTime < 0x32) scaleValue = 1.0f - (float)localTime / 50.0f;
    ScaleTo(scaleValue);
    if (localTime > 0x31) {
        state = NON;
        localTime = 0;
    }
}

XParticle_PacFireHit::XParticle_PacFireHit(XGameObject* g, D3DVECTOR pos, float size, unsigned frames)
    : XParticleBase(g, pos, size), processFrame(frames), renderCount(2) {}

void XParticle_PacFireHit::Render2(const D3DMATRIX& view) {
    if (state == NON) return;
    _SetWorldTransform2(view);
    _SetRenderState();
    Device* d = app->GetD3DDevice();
    for (unsigned i = 0; i < renderCount; i++) {
        _SetTextureRandomly();
        if (textureResId != 0) d->SetTexture(0, D3DTextr_GetSurface(textureResId));
        d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_LVERTEX, vertexList, vertexCount, indexList, indexCount, 0);
    }
    game->app->GetD3DDevice()->SetTexture(0, nullptr);
    _ResetRenderState();
    _ResetWorldTransform();
}

void XParticle_PacFireHit::ProcessFrame() {
    localTime = localTime + 1;
    if (state != PROCESS) return;
    float scaleValue = 0.2f;
    if (localTime < processFrame) scaleValue = 1.0f - ((float)localTime / (float)processFrame) * 0.8f;
    ScaleTo(scaleValue);
    if (processFrame >> 1 <= localTime) renderCount = 1;
    if (processFrame <= localTime) {
        state = NON;
        localTime = 0;
    }
}

void XParticle_PacFireHit::_SetTextureRandomly() {
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

// ============================================================ XEnemyFireList
XEnemyFireList::XEnemyFireList(XGameObject* g) : game(g) {
    for (auto& f : fireArray) f = nullptr;
}

XEnemyFireList::~XEnemyFireList() { Empty(); }

void XEnemyFireList::CheckCollisionWithPac() {
    XPac::STATE s = game->pac->GetState();
    if (s == XPac::NON || s == XPac::LEVEL_COMPLETED || s == XPac::DYING) return;
    for (auto* f : fireArray) {
        if (f && f->GetState() != XEnemyFireBase::NON && game->pac->CheckCollisionRadius(f)) {
            f->SetState(XEnemyFireBase::NON);
            game->pac->StartDying();
            return;
        }
    }
}

void XEnemyFireList::Empty() {
    for (auto& f : fireArray) {
        delete f;
        f = nullptr;
    }
}

void XEnemyFireList::ProcessFrame() {
    for (auto& f : fireArray) {
        if (!f) continue;
        f->ProcessFrame();
        if (f->GetState() == XEnemyFireBase::NON) {
            delete f;
            f = nullptr;
        }
    }
}

void XEnemyFireList::Render(const D3DMATRIX& view) {
    for (auto* f : fireArray)
        if (f) f->Render2(view);
}

void XEnemyFireList::CreateCannonUpFire(D3DVECTOR pos) {
    int index = GetNullIndex();
    if (index < 0) return;
    BYTE volume = game->GetVolumeOfPosition(pos);
    game->app->GetSoundList()->Play(0xf4, volume, 0);
    XEnemyFire_CannonUp* fire = new XEnemyFire_CannonUp(game, pos);
    float distance = 45.0f;
    int angle = (int)Random(0x168);
    D3DVECTOR dir(sinf((float)angle * 0.017453292f), 0.0f, cosf((float)angle * 0.017453292f));
    fire->SetValues(distance, dir);
    fireArray[index] = fire;
}

void XEnemyFireList::CreateVolcanoFire(D3DVECTOR pos) {
    int index = GetNullIndex();
    if (index < 0) return;
    BYTE volume = game->GetVolumeOfPosition(pos);
    game->app->GetSoundList()->Play(0xf4, volume, 0);
    XEnemyFire_CannonUp* fire = new XEnemyFire_CannonUp(game, pos);
    float distance = 80.0f;
    int angle = (int)Random(0x168);
    D3DVECTOR dir(sinf((float)angle * 0.017453292f), 0.0f, cosf((float)angle * 0.017453292f));
    fire->SetValues(distance, dir);
    fireArray[index] = fire;
}

void XEnemyFireList::CreateCannonFire(D3DVECTOR pos, int dir) {
    int index = GetNullIndex();
    if (index < 0) return;
    BYTE volume = game->GetVolumeOfPosition(pos);
    game->app->GetSoundList()->Play(0xf3, volume, 0);
    XEnemyFire_Cannon* fire = new XEnemyFire_Cannon(game, pos);
    fire->SetValues(GetVectorFromDir(dir));
    fireArray[index] = fire;
}

void XEnemyFireList::CreateUFOFire(D3DVECTOR pos) {
    int index = GetNullIndex();
    if (index < 0) return;
    BYTE volume = game->GetVolumeOfPosition(pos);
    game->app->GetSoundList()->Play(0xf3, volume, 0);
    fireArray[index] = new XEnemyFire_UFO(game, pos);
}

void XEnemyFireList::CreateSteamFire(D3DVECTOR pos) {
    int index = GetNullIndex();
    if (index >= 0) fireArray[index] = new XEnemyFire_Steam(game, pos);
}

int XEnemyFireList::GetNullIndex() {
    for (int i = 0; i < 60; i++)
        if (!fireArray[i]) return i;
    return -1;
}

// ============================================================ enemy projectiles
XEnemyFireBase::XEnemyFireBase(XGameObject* g, D3DVECTOR pos, float size)
    : JObject3D_Particle(g->app, size, size), game(g), state(NON), localTime(0), refrencePos(pos) {
    SetWorldPos(pos);
    state = MOVE;
}

XEnemyFireBase::~XEnemyFireBase() {}

void XEnemyFireBase::Render2(const D3DMATRIX& view) {
    if (state != NON) JObject3D_Rect::Render2(view);
}

XEnemyFire_CannonUp::XEnemyFire_CannonUp(XGameObject* g, D3DVECTOR pos)
    : XEnemyFireBase(g, pos, 18.0f), distance(0.0f), dirVector(0, 0, 0) {
    JObject3D_Rect::SetTexture(0xa9, 0.0f, 0.0f, 1.0f, 1.0f);
    SetCollisionRadius(7.0f);
}

void XEnemyFire_CannonUp::SetValues(float dist, D3DVECTOR dir) {
    distance = dist;
    dirVector = dir;
}

void XEnemyFire_CannonUp::ProcessFrame() {
    localTime = localTime + 1;
    if (state != MOVE) return;
    float movedDistance = ((float)localTime / 200.0f) * distance;
    float jumpHeight = sinf(((float)localTime / 200.0f) * 3.1415927f) * 100.0f;
    D3DVECTOR moved = D3DVectorMultiply(dirVector, movedDistance);
    moved.y = jumpHeight;
    SetWorldPos(refrencePos + moved);
    if (localTime % 8 == 0) game->particleList->CreateEnemyFireSmokePurple(worldPos);
    if (worldPos.y < 0.0f) {
        state = NON;
        localTime = 0;
    }
}

XEnemyFire_Cannon::XEnemyFire_Cannon(XGameObject* g, D3DVECTOR pos) : XEnemyFireBase(g, pos, 18.0f), dirVector(0, 0, 0) {
    JObject3D_Rect::SetTexture(0xa9, 0.0f, 0.0f, 1.0f, 1.0f);
    SetCollisionRadius(7.0f);
}

void XEnemyFire_Cannon::ProcessFrame() {
    localTime = localTime + 1;
    if (state != MOVE) return;
    ShiftWorldPos(D3DVectorMultiply(dirVector, 1.6f));
    if (localTime % 8 == 0) game->particleList->CreateEnemyFireSmokePurple(worldPos);
    int mx = game->map->ConvertPosXToMapX(worldPos.x);
    int mz = game->map->ConvertPosZToMapZ(worldPos.z);
    if (localTime > 0xf && game->map->IsBlocked_Fire(mx, mz)) {
        state = NON;
        localTime = 0;
    }
}

XEnemyFire_UFO::XEnemyFire_UFO(XGameObject* g, D3DVECTOR pos)
    : XEnemyFireBase(g, pos, 17.0f), dirVector(0.0f, -1.0f, 0.0f), turned(0) {
    JObject3D_Rect::SetTexture(0xb6, 0.0f, 0.0f, 1.0f, 1.0f);
    SetCollisionRadius(7.0f);
}

void XEnemyFire_UFO::ProcessFrame() {
    localTime = localTime + 1;
    if (state != MOVE) return;
    ShiftWorldPos(D3DVectorMultiply(dirVector, 1.2f));
    if (localTime % 9 == 0) game->particleList->CreateEnemyFireSmokeGreen(worldPos);
    if (!turned) {
        if (worldPos.y < 20.0f) {
            int pacX = game->pac->GetMapPosX();
            int pacZ = game->pac->GetMapPosZ();
            int mx = game->map->ConvertPosXToMapX(worldPos.x);
            int mz = game->map->ConvertPosZToMapZ(worldPos.z);
            int diffX = abs(pacX - mx);
            int diffZ = abs(pacZ - mz);
            int newDirection;
            if (diffX == 0 && diffZ == 0) newDirection = (int)Random(4);
            else if (diffZ < diffX) newDirection = mx < pacX ? 1 : 3;
            else if (diffX < diffZ) newDirection = mz < pacZ ? 2 : 0;
            else if (Random(2) == 0) newDirection = mx < pacX ? 1 : 3;
            else newDirection = mz < pacZ ? 2 : 0;
            dirVector = GetVectorFromDir(newDirection);
            turned = 1;
        }
    } else {
        int mx = game->map->ConvertPosXToMapX(worldPos.x);
        int mz = game->map->ConvertPosZToMapZ(worldPos.z);
        if (localTime > 0xf && game->map->IsBlocked_Fire(mx, mz)) {
            state = NON;
            localTime = 0;
        }
    }
}

XEnemyFire_Steam::XEnemyFire_Steam(XGameObject* g, D3DVECTOR pos) : XEnemyFireBase(g, pos, 18.0f) {
    SetAlphaState(ALPHA_TRANSPARENT);
    SetFilterState(FILTER_POINT);
    JObject3D_Rect::SetTexture(0xdc, 0.0f, 0.0f, 1.0f, 1.0f);
    SetCollisionRadius(7.0f);
}

void XEnemyFire_Steam::ProcessFrame() {
    localTime = localTime + 1;
    if (state != MOVE) return;
    SetWorldPos(JAXIS_Y, refrencePos.y + ((float)localTime / 80.0f) * 120.0f);
    SetAlphaFactor((1.0f - (float)localTime / 80.0f) * 0.9f + 0.1f);
    if (localTime > 0x4f) {
        state = NON;
        localTime = 0;
    }
}
