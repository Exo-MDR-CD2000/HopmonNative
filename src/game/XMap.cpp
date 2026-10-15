// Port of XMap.cpp: level table, XCamera, XPreviewMap (level select), XMap.
#include "XGame.h"
#include "../framework/d3dutil.h"
#include "../gfx/device.h"

WORD GetMapResId(XApplication* app) {
    static const WORD world0[15] = {0x11c, 0x115, 0x116, 0x117, 0x118, 0x119, 0x11a, 0x11b,
                                    0x114, 0x109, 0x10a, 0x10b, 0x10c, 0x10d, 0x10e};
    int world = app->GetCurrentWorld();
    int stage = app->currentLevel % 15;
    if (stage < 0 || stage > 14) return 0x11c;
    if (world == 0) return world0[stage];
    if (world == 1) return (WORD)(0x11d + stage);
    if (world == 2) return (WORD)(300 + stage);
    return 0x11c;
}

// ============================================================ XCamera
XCamera::XCamera(XApplication* a)
    : app(a), state(NON), localTime(0), lookAtPos(0, 0, 0), refPos(0, 0, 0), distance(0), verticalAngle(0),
      horizontalAngle(0), viewDir(0) {
    D3DUtil_SetIdentityMatrix(viewMatrix);
}

XCamera::~XCamera() {}

void XCamera::ResetForGame(XPac* pac) {
    distance = 420.0f;
    verticalAngle = 0.7853982f;
    horizontalAngle = 3.1415927f;
    viewDir = 0;
    D3DVECTOR wp = pac->GetWorldPos();
    lookAtPos = D3DVECTOR(wp.x, 0.0f, wp.z);
    refPos = D3DVECTOR(wp.x, 0.0f, wp.z);
    UpdateViewMatrix();
    state = WAIT;
    localTime = 0;
    havePrev = false;
}

void XCamera::ResetForTitle() {
    distance = 500.0f;
    verticalAngle = 0.0f;
    horizontalAngle = 3.1415927f;
    viewDir = 0;
    lookAtPos = D3DVECTOR(0, 0, 0);
    refPos = D3DVECTOR(0, 0, 0);
    UpdateViewMatrix();
    state = WAIT;
    localTime = 0;
    havePrev = false;
}

void XCamera::SnapshotPrev() {
    prevLookAt = lookAtPos;
    prevDistance = distance;
    prevVertical = verticalAngle;
    prevHorizontal = horizontalAngle;
    havePrev = true;
}

void XCamera::DoSetTransform() {
    if (state == NON) return;
    float a = Interp::Alpha();
    if (a < 1.f && havePrev && SquareMagnitude(lookAtPos - prevLookAt) < 60.f * 60.f) {
        // Render-only: rebuild the view from interpolated camera parameters.
        float dist = Interp::Lerp(prevDistance, distance, a);
        float va = Interp::Lerp(prevVertical, verticalAngle, a);
        float ha = Interp::Lerp(prevHorizontal, horizontalAngle, a);
        D3DVECTOR look = Interp::Lerp(prevLookAt, lookAtPos, a);
        D3DVECTOR ref = look;  // refPos always tracks lookAtPos horizontally
        ref.y = Interp::Lerp(prevLookAt.y, refPos.y, a);
        D3DVECTOR viewPos;
        viewPos.y = ref.y + sinf(va) * dist;
        viewPos.x = sinf(ha) * (cosf(va) * dist) + ref.x;
        viewPos.z = cosf(ha) * (cosf(va) * dist) + ref.z;
        D3DMATRIX m;
        D3DUtil_SetViewMatrix(m, viewPos, look, D3DVECTOR(0.0f, 1.0f, 0.0f));
        renderView_ = m;
    } else {
        renderView_ = viewMatrix;
    }
    app->GetD3DDevice()->SetTransform(D3DTRANSFORMSTATE_VIEW, &renderView_);
}

void XCamera::StartRotatingLeft() {
    viewDir = (viewDir + 3) % 4;
    state = ROTATE_LEFT;
    localTime = 0;
}

void XCamera::StartRotatingRight() {
    viewDir = (viewDir + 1) % 4;
    state = ROTATE_RIGHT;
    localTime = 0;
}

void XCamera::StartLevelCompleted() {
    state = LEVEL_COMPLETED;
    localTime = 0;
}

void XCamera::StartGameOver() {
    state = GAME_OVER;
    localTime = 0;
}

void XCamera::IncrVerticalAngle(DWORD frameCount) {
    verticalAngle = (float)frameCount * 0.008726646f + verticalAngle;
    if (verticalAngle > 1.4207964f) verticalAngle = 1.4207964f;
}

void XCamera::DecrVerticalAngle(DWORD frameCount) {
    verticalAngle = verticalAngle - (float)frameCount * 0.008726646f;
    if (verticalAngle < 0.2f) verticalAngle = 0.2f;
}

void XCamera::ProcessFrame(XPac* pac) {
    localTime = localTime + 1;
    if (state == WAIT) {
        D3DVECTOR wp = pac->GetWorldPos();
        lookAtPos = D3DVECTOR(wp.x, 0.0f, wp.z);
        refPos = D3DVECTOR(wp.x, 0.0f, wp.z);
        UpdateViewMatrix();
    } else if (state == ROTATE_LEFT || state == ROTATE_RIGHT) {
        float angleUnit = 0.02617994f;
        if (state == ROTATE_LEFT) angleUnit = -0.02617994f;
        horizontalAngle = horizontalAngle + angleUnit;
        D3DVECTOR wp = pac->GetWorldPos();
        lookAtPos = D3DVECTOR(wp.x, 0.0f, wp.z);
        refPos = D3DVECTOR(wp.x, 0.0f, wp.z);
        UpdateViewMatrix();
        if (localTime > 0x3b) {
            state = WAIT;
            localTime = 0;
        }
    } else if (state == LEVEL_COMPLETED) {
        lookAtPos.y = lookAtPos.y + 0.7f;
        refPos.y = refPos.y + 0.7f;
        distance = distance - 0.5f;
        if (distance < 140.0f) distance = 140.0f;
        verticalAngle = verticalAngle + 0.0012217305f;
        if (verticalAngle > 1.4207964f) verticalAngle = 1.4207964f;
        horizontalAngle = horizontalAngle + -0.008726646f;
        UpdateViewMatrix();
    } else if (state == GAME_OVER) {
        distance = distance + 0.5f;
        verticalAngle = verticalAngle + 0.0012217305f;
        if (verticalAngle > 1.4207964f) verticalAngle = 1.4207964f;
        horizontalAngle = horizontalAngle + -0.008726646f;
        UpdateViewMatrix();
    }
}

void XCamera::UpdateViewMatrix() {
    float vDistance = sinf(verticalAngle) * distance;
    float hDistance = cosf(verticalAngle) * distance;
    D3DVECTOR viewPos;
    viewPos.y = refPos.y + vDistance;
    viewPos.x = sinf(horizontalAngle) * hDistance + refPos.x;
    viewPos.z = cosf(horizontalAngle) * hDistance + refPos.z;
    D3DUtil_SetViewMatrix(viewMatrix, viewPos, lookAtPos, D3DVECTOR(0.0f, 1.0f, 0.0f));
    renderView_ = viewMatrix;
}

// ============================================================ shared map building
namespace {

struct MapGrid {
    BYTE* mapArray;
    int mapSizeX, mapSizeZ;
};

// Loads the ASCII level (stops at the first empty line or a line starting with '_'),
// surrounded by a one-cell border of '.'.
void LoadMapStrings(int resId, JStringList& mapStrList) {
    JStringList rawStrList;
    GetStringListFromResource(rawStrList, resId);
    for (DWORD i = 0; i < rawStrList.Count(); i++) {
        JString rawStr = rawStrList.GetString(i);
        if (rawStr.Length() == 0 || rawStr.c_str()[0] == '_') break;
        mapStrList.Append(rawStr);
    }
}

void FillMapArray(const JStringList& mapStrList, BYTE* arr, int sizeX, int sizeZ) {
    memset(arr, '.', (size_t)sizeZ * sizeX);
    for (int z = 1; z < sizeZ - 1; z++)
        for (int x = 1; x < sizeX - 1; x++) {
            const JString& s = mapStrList.GetString(z - 1);
            if ((unsigned)(x - 1) < s.Length()) arr[z * sizeX + x] = (BYTE)s.c_str()[x - 1];
        }
}

int ConvertMapItemToInt(BYTE item) { return (item < 0x31 || item > 0x33) ? 0 : item - 0x30; }

void CreateFloor(CD3DApplication* app, float chipSize, WORD tex, JObject3DList& list, BYTE item, float x, float z) {
    JObject3D_Rect* rect = new JObject3D_Rect(app, chipSize, chipSize);
    switch (item) {
        case '!': rect->SetTexture(tex, 0.75f, 0.25f, 1.0f, 0.5f); break;
        case '[': rect->SetTexture(tex, 0.0f, 0.5f, 0.25f, 0.75f); break;
        case '>': rect->SetTexture(tex, 0.25f, 0.5f, 0.5f, 0.75f); break;
        case ']': rect->SetTexture(tex, 0.5f, 0.5f, 0.75f, 0.75f); break;
        case '<': rect->SetTexture(tex, 0.75f, 0.5f, 1.0f, 0.75f); break;
        case '#': rect->SetTexture(tex, 0.0f, 0.25f, 0.25f, 0.5f); break;
        case '=': rect->SetTexture(tex, 0.25f, 0.25f, 0.5f, 0.5f); break;
        case '*': rect->SetTexture(tex, 0.5f, 0.25f, 0.75f, 0.5f); break;
        default: rect->SetTexture(tex, 0.0f, 0.0f, 0.25f, 0.25f); break;
    }
    rect->Shift(D3DVECTOR(x, 0.0f, z));
    list.Append(rect);
}

void CreateWalls(CD3DApplication* app, float chipSize, WORD tex, const MapGrid& g, JObject3DList& list, int level,
                 int mapX, int mapZ, float x, float z) {
    float posY = (float)(level - 1) * chipSize + chipSize / 2.0f;
    int idxL = mapZ * g.mapSizeX - 1 + mapX;
    int idxR = mapZ * g.mapSizeX + 1 + mapX;
    int idxT = (mapZ - 1) * g.mapSizeX + mapX;
    int idxB = (mapZ + 1) * g.mapSizeX + mapX;
    char vL = (char)ConvertMapItemToInt(g.mapArray[idxL]) - (char)level;
    char vR = (char)ConvertMapItemToInt(g.mapArray[idxR]) - (char)level;
    char vT = (char)ConvertMapItemToInt(g.mapArray[idxT]) - (char)level;
    char vB = (char)ConvertMapItemToInt(g.mapArray[idxB]) - (char)level;
    if (vL < 0) {
        JObject3D_Rect* r = new JObject3D_Rect(app, chipSize, chipSize);
        r->SetTexture(tex, 0.5f, 0.0f, 0.75f, 0.25f);
        r->RotateBy(JAXIS_Y, 1.5707964f);
        r->RotateBy(JAXIS_Z, 1.5707964f);
        r->Shift(D3DVECTOR(x - chipSize / 2.0f, posY, z));
        list.Append(r);
    }
    if (vR < 0) {
        JObject3D_Rect* r = new JObject3D_Rect(app, chipSize, chipSize);
        r->SetTexture(tex, 0.75f, 0.0f, 1.0f, 0.25f);
        r->RotateBy(JAXIS_Y, -1.5707964f);
        r->RotateBy(JAXIS_Z, -1.5707964f);
        r->Shift(D3DVECTOR(chipSize / 2.0f + x, posY, z));
        list.Append(r);
    }
    if (vT < 0) {
        JObject3D_Rect* r = new JObject3D_Rect(app, chipSize, chipSize);
        r->SetTexture(tex, 0.75f, 0.0f, 1.0f, 0.25f);
        r->RotateBy(JAXIS_Y, 3.1415927f);
        r->RotateBy(JAXIS_X, 1.5707964f);
        r->Shift(D3DVECTOR(x, posY, chipSize / 2.0f + z));
        list.Append(r);
    }
    if (vB < 0) {
        JObject3D_Rect* r = new JObject3D_Rect(app, chipSize, chipSize);
        r->SetTexture(tex, 0.5f, 0.0f, 0.75f, 0.25f);
        r->RotateBy(JAXIS_X, -1.5707964f);
        r->Shift(D3DVECTOR(x, posY, z - chipSize / 2.0f));
        list.Append(r);
    }
}

void CreateRoofAndWalls(CD3DApplication* app, float chipSize, WORD tex, const MapGrid& g, JObject3DList& list, BYTE item,
                        int x, int z, float posX, float posZ) {
    float roofPosY = 0.0f;
    if (item == '1') {
        CreateWalls(app, chipSize, tex, g, list, 1, x, z, posX, posZ);
        roofPosY = chipSize;
    } else if (item == '2') {
        CreateWalls(app, chipSize, tex, g, list, 1, x, z, posX, posZ);
        CreateWalls(app, chipSize, tex, g, list, 2, x, z, posX, posZ);
        roofPosY = chipSize * 2.0f;
    } else if (item == '3') {
        CreateWalls(app, chipSize, tex, g, list, 1, x, z, posX, posZ);
        CreateWalls(app, chipSize, tex, g, list, 2, x, z, posX, posZ);
        CreateWalls(app, chipSize, tex, g, list, 3, x, z, posX, posZ);
        roofPosY = chipSize * 3.0f;
    }
    JObject3D_Rect* rect = new JObject3D_Rect(app, chipSize, chipSize);
    rect->SetTexture(tex, 0.25f, 0.0f, 0.5f, 0.25f);
    rect->Shift(D3DVECTOR(posX, roofPosY, posZ));
    list.Append(rect);
}

bool IsFloorItem(BYTE c) {
    return c == '-' || c == '+' || c == '!' || c == '[' || c == ']' || c == '>' || c == '<' || c == '#' || c == '=' ||
           c == '*';
}

// Merges every quad of the temporary list into one indexed mesh, as the original did.
JObject3D* MergeObjects(CD3DApplication* app, JObject3DList& tempObjList, WORD tex) {
    DWORD totalVertexCount = 0, totalIndexCount = 0;
    for (DWORD i = 0; i < tempObjList.Count(); i++) {
        JObject3D* obj = (JObject3D*)tempObjList.GetAt(i);
        totalVertexCount += obj->GetVertexCount();
        totalIndexCount += obj->GetIndexCount();
    }
    JObject3D* mapObject = new JObject3D(app, totalVertexCount, totalIndexCount);
    mapObject->SetTexture(tex, 0.0f, 0.0f, 1.0f, 1.0f);
    DWORD currentVertexNo = 0, currentIndexNo = 0;
    for (DWORD q = 0; q < tempObjList.Count(); q++) {
        JObject3D* obj2 = (JObject3D*)tempObjList.GetAt(q);
        for (DWORD ix = 0; ix < obj2->GetIndexCount(); ix++) {
            WORD v = (WORD)(obj2->GetIndexAt(ix) + (WORD)currentVertexNo);
            mapObject->SetIndexAt(currentIndexNo++, v);
        }
        for (DWORD vx = 0; vx < obj2->GetVertexCount(); vx++) mapObject->SetVertexAt(currentVertexNo++, obj2->GetVertexAt(vx));
    }
    return mapObject;
}

}  // namespace

// ============================================================ XPreviewMap
XPreviewMap::XPreviewMap(XApplication* a, int resId)
    : app(a), mapArray(nullptr), mapObject(nullptr), chipSize(10.0f), mapSizeX(0), mapSizeZ(0), mapTextureId(0) {
    if (app->GetCurrentWorld() == 0) mapTextureId = 0x9c;
    else if (app->GetCurrentWorld() == 1) mapTextureId = 0xfb;
    else mapTextureId = 0xfc;
    _LoadMapArray(resId);
    _CreateMapObject();
    mapObject->RotateBy(JAXIS_X, -0.5235988f);
}

XPreviewMap::~XPreviewMap() {
    delete[] mapArray;
    mapArray = nullptr;
    delete mapObject;
    mapObject = nullptr;
}

void XPreviewMap::ProcessFrame(DWORD frameCount) {
    mapObject->RotateBy(JAXIS_X, 0.5235988f);
    mapObject->RotateBy(JAXIS_Y, (float)frameCount * 0.0052359877f);
    mapObject->RotateBy(JAXIS_X, -0.5235988f);
}

void XPreviewMap::Render() { mapObject->Render(); }

void XPreviewMap::_LoadMapArray(int resId) {
    JStringList mapStrList;
    LoadMapStrings(resId, mapStrList);
    mapSizeX = 0;
    for (DWORD i = 0; i < mapStrList.Count(); i++)
        if ((unsigned)mapSizeX < mapStrList.GetString(i).Length()) mapSizeX = (int)mapStrList.GetString(i).Length();
    mapSizeX += 2;
    mapSizeZ = (int)mapStrList.Count() + 2;
    mapArray = new BYTE[(size_t)mapSizeZ * mapSizeX];
    FillMapArray(mapStrList, mapArray, mapSizeX, mapSizeZ);
}

int XPreviewMap::_ConvertMapItemToInt(BYTE item) { return ConvertMapItemToInt(item); }

void XPreviewMap::_CreateFloor(JObject3DList& list, BYTE item, float x, float z) {
    CreateFloor(app, chipSize, mapTextureId, list, item, x, z);
}

void XPreviewMap::_CreateWalls(JObject3DList& list, int level, int mapX, int mapZ, float x, float z) {
    CreateWalls(app, chipSize, mapTextureId, MapGrid{mapArray, mapSizeX, mapSizeZ}, list, level, mapX, mapZ, x, z);
}

void XPreviewMap::_CreateMapObject() {
    float mapCenterX = ((float)mapSizeX * chipSize) / 2.0f;
    float mapCenterZ = ((float)mapSizeZ * chipSize) / 2.0f;
    JObject3DList tempObjList;
    MapGrid g{mapArray, mapSizeX, mapSizeZ};
    for (int z = 0; z < mapSizeZ; z++) {
        for (int x = 0; x < mapSizeX; x++) {
            int idx = z * mapSizeX + x;
            float posX = ((float)x * chipSize + chipSize / 2.0f) - mapCenterX;
            float posZ = ((float)(mapSizeZ - z) * chipSize - chipSize / 2.0f) - mapCenterZ;
            BYTE c = mapArray[idx];
            if (c == '.') continue;
            if (c == '@' || c == '$') {
                mapArray[idx] = '-';
                _CreateFloor(tempObjList, mapArray[idx], posX, posZ);
            } else if (c >= 'A' && c <= 'Z') {
                _CreateFloor(tempObjList, '-', posX, posZ);
            } else if (c >= 'a' && c <= 'z') {
                mapArray[idx] = '-';
                _CreateFloor(tempObjList, mapArray[idx], posX, posZ);
            } else if (IsFloorItem(c)) {
                _CreateFloor(tempObjList, c, posX, posZ);
            } else if (c > 0x30 && c < 0x34) {
                CreateRoofAndWalls(app, chipSize, mapTextureId, g, tempObjList, c, x, z, posX, posZ);
            }
        }
    }
    mapObject = MergeObjects(app, tempObjList, mapTextureId);
}

// ============================================================ XMap
XMap::XMap(XApplication* a, int resId, XPac* pac, XEnemyList* enemies, XMapObjectList* objects, XCrystalList* crystals)
    : app(a), mapArray(nullptr), enemyMapArray(nullptr), ufoMapArray(nullptr), mapObject(nullptr),
      background(nullptr), chipSize(40.0f), mapSizeX(0), mapSizeZ(0), mapTextureId(0) {
    if (app->GetCurrentWorld() == 0) mapTextureId = 0x9c;
    else if (app->GetCurrentWorld() == 1) mapTextureId = 0xfb;
    else mapTextureId = 0xfc;
    _LoadMapArray(resId);
    _CreateMapObject(pac, enemies, objects, crystals);
    _CreateBackground();
}

XMap::~XMap() {
    delete[] mapArray;
    delete[] enemyMapArray;
    delete[] ufoMapArray;
    delete mapObject;
    delete background;
}

void XMap::ProcessFrame() {}

void XMap::Render() {
    background->Render();
    mapObject->Render();
}

int XMap::IsBlocked_Pac(int x, int z) {
    if (_IsOutsideMap(x, z)) return 1;
    BYTE c = mapArray[z * mapSizeX + x];
    if (c == '.' || (c > 0x40 && c < 0x5b) || (c > 0x30 && c < 0x34)) return 1;
    BYTE e = enemyMapArray[z * mapSizeX + x];
    if (e == 'x' || e == 'y') return 1;
    return 0;
}

int XMap::IsBlocked_Fire(int x, int z) {
    if (_IsOutsideMap(x, z)) return 1;
    BYTE c = mapArray[z * mapSizeX + x];
    if ((c < 0x41 || c > 0x5a) && (c < 0x31 || c > 0x33)) return 0;
    return 1;
}

int XMap::IsBlocked_Enemy(int x, int z) {
    if (_IsOutsideMap(x, z)) return 1;
    BYTE c = mapArray[z * mapSizeX + x];
    if (c == '.' || c == '+' || (c > 0x40 && c < 0x5b) || (c > 0x30 && c < 0x34)) return 1;
    return enemyMapArray[z * mapSizeX + x] == '0' ? 0 : 1;
}

int XMap::IsBlocked_UFO(int x, int z) {
    if (_IsOutsideMap(x, z)) return 1;
    BYTE c = mapArray[z * mapSizeX + x];
    if (c == '.' || c == 'P' || c == '3') return 1;
    return ufoMapArray[z * mapSizeX + x] == '0' ? 0 : 1;
}

int XMap::IsWarpZone(int x, int z) {
    if (_IsOutsideMap(x, z)) return 0;
    return mapArray[z * mapSizeX + x] == '*';
}

int XMap::IsArrow(int x, int z) {
    if (_IsOutsideMap(x, z)) return 0;
    BYTE c = mapArray[z * mapSizeX + x];
    return c == '[' || c == ']' || c == '<' || c == '>';
}

int XMap::GetArrowDirection(int x, int z) {
    if (_IsOutsideMap(x, z)) return -1;
    switch (mapArray[z * mapSizeX + x]) {
        case '[': return 0;
        case '>': return 1;
        case ']': return 2;
        case '<': return 3;
    }
    return -1;
}

int XMap::ConvertPosXToMapX(float x) { return (int)((x + ((float)mapSizeX * chipSize) / 2.0f) / chipSize); }

int XMap::ConvertPosZToMapZ(float z) {
    return (mapSizeZ - 1) - (int)((z + ((float)mapSizeZ * chipSize) / 2.0f) / chipSize);
}

void XMap::UpdateEnemyMap(int ox, int oz, int nx, int nz) {
    BYTE v = enemyMapArray[oz * mapSizeX + ox];
    enemyMapArray[oz * mapSizeX + ox] = '0';
    enemyMapArray[nz * mapSizeX + nx] = v;
}

void XMap::ZeroEnemyMap(int x, int z) { enemyMapArray[z * mapSizeX + x] = '0'; }

void XMap::UpdateUFOMap(int ox, int oz, int nx, int nz) {
    BYTE v = ufoMapArray[oz * mapSizeX + ox];
    ufoMapArray[oz * mapSizeX + ox] = '0';
    ufoMapArray[nz * mapSizeX + nx] = v;
}

void XMap::_LoadMapArray(int resId) {
    JStringList mapStrList;
    LoadMapStrings(resId, mapStrList);
    mapSizeX = 0;
    for (DWORD i = 0; i < mapStrList.Count(); i++)
        if ((unsigned)mapSizeX < mapStrList.GetString(i).Length()) mapSizeX = (int)mapStrList.GetString(i).Length();
    mapSizeX += 2;
    mapSizeZ = (int)mapStrList.Count() + 2;
    size_t n = (size_t)mapSizeZ * mapSizeX;
    mapArray = new BYTE[n];
    enemyMapArray = new BYTE[n];
    memset(enemyMapArray, '0', n);
    ufoMapArray = new BYTE[n];
    memset(ufoMapArray, '0', n);
    FillMapArray(mapStrList, mapArray, mapSizeX, mapSizeZ);
}

int XMap::_ConvertMapItemToInt(BYTE item) { return ConvertMapItemToInt(item); }

void XMap::_CreateFloor(JObject3DList& list, BYTE item, float x, float z) {
    CreateFloor(app, chipSize, mapTextureId, list, item, x, z);
}

void XMap::_CreateWalls(JObject3DList& list, int level, int mapX, int mapZ, float x, float z) {
    CreateWalls(app, chipSize, mapTextureId, MapGrid{mapArray, mapSizeX, mapSizeZ}, list, level, mapX, mapZ, x, z);
}

int XMap::_IsOutsideMap(int x, int z) {
    if (x < 0 || mapSizeX <= x) return 1;
    if (z < 0 || mapSizeZ <= z) return 1;
    return 0;
}

void XMap::_CreateMapObject(XPac* pac, XEnemyList* enemies, XMapObjectList* objects, XCrystalList* crystals) {
    float mapCenterX = ((float)mapSizeX * chipSize) / 2.0f;
    float mapCenterZ = ((float)mapSizeZ * chipSize) / 2.0f;
    JObject3DList tempObjList;
    MapGrid g{mapArray, mapSizeX, mapSizeZ};
    for (int z = 0; z < mapSizeZ; z++) {
        for (int x = 0; x < mapSizeX; x++) {
            int idx = z * mapSizeX + x;
            float posX = ((float)x * chipSize + chipSize / 2.0f) - mapCenterX;
            float posZ = ((float)(mapSizeZ - z) * chipSize - chipSize / 2.0f) - mapCenterZ;
            BYTE c = mapArray[idx];
            if (c == '.') continue;
            if (c == '@') {
                pac->SetWorldPos(JAXIS_X, posX);
                pac->SetWorldPos(JAXIS_Z, posZ);
                pac->SetMapPos(x, z);
                mapArray[idx] = '-';
                _CreateFloor(tempObjList, mapArray[idx], posX, posZ);
            } else if (c == '$') {
                XCrystal* crystal = crystals->CreateCrystal();
                crystal->SetWorldPos(JAXIS_X, posX);
                crystal->SetWorldPos(JAXIS_Z, posZ);
                mapArray[idx] = '-';
                _CreateFloor(tempObjList, mapArray[idx], posX, posZ);
            } else if (c >= 'A' && c <= 'Z') {
                XMapObjectBase* mapObj = objects->CreateMapObject(c);
                if (mapObj) {
                    mapObj->SetWorldPos(JAXIS_X, posX);
                    mapObj->SetWorldPos(JAXIS_Z, posZ);
                }
                _CreateFloor(tempObjList, '-', posX, posZ);
                if (c == 'V') {
                    mapArray[(z - 1) * mapSizeX - 1 + x] = 'Z';
                    mapArray[(z - 1) * mapSizeX + x] = 'Z';
                    mapArray[(z - 1) * mapSizeX + 1 + x] = 'Z';
                    mapArray[z * mapSizeX - 1 + x] = 'Z';
                    mapArray[z * mapSizeX + 1 + x] = 'Z';
                    mapArray[(z + 1) * mapSizeX - 1 + x] = 'Z';
                    mapArray[(z + 1) * mapSizeX + x] = 'Z';
                    mapArray[(z + 1) * mapSizeX + 1 + x] = 'Z';
                }
            } else if (c >= 'a' && c <= 'z') {
                XEnemyBase* enemy = enemies->CreateEnemy(c);
                enemy->SetWorldPos(JAXIS_X, posX);
                enemy->SetWorldPos(JAXIS_Z, posZ);
                enemy->SetMapPos(x, z);
                if (c == 'd') ufoMapArray[idx] = c;
                else enemyMapArray[idx] = c;
                mapArray[idx] = '-';
                _CreateFloor(tempObjList, mapArray[idx], posX, posZ);
            } else if (IsFloorItem(c)) {
                _CreateFloor(tempObjList, c, posX, posZ);
                if (c == '!') {
                    XMapObjectBase* mapObj2 = objects->CreateMapObject(c);
                    if (mapObj2) {
                        mapObj2->SetWorldPos(JAXIS_X, posX);
                        mapObj2->SetWorldPos(JAXIS_Z, posZ);
                    }
                }
            } else if (c > 0x30 && c < 0x34) {
                CreateRoofAndWalls(app, chipSize, mapTextureId, g, tempObjList, c, x, z, posX, posZ);
            }
        }
    }
    mapObject = MergeObjects(app, tempObjList, mapTextureId);
}

void XMap::_CreateBackground() {
    D3DVECTOR mapMinVertex, mapMaxVertex;
    mapObject->GetBox(mapMinVertex, mapMaxVertex);
    float backSizeX = (float)(mapSizeX + 4) * chipSize;
    float backSizeZ = (float)(mapSizeZ + 4) * chipSize;
    background = new JObject3D_Rect(app, backSizeX, backSizeZ);
    background->ShiftWorldPos(JAXIS_Y, -100.0f);
    background->SetTexture(0xc1, 0.0f, 0.0f, (float)(mapSizeX + 4), (float)(mapSizeZ + 4));
    background->UsePerspective(1);
    background->SetFilterState(JObject3D::FILTER_LINEAR);
}
