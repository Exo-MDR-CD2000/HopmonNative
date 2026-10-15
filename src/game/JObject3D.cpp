#include "JObject3D.h"
#include "../framework/d3dapp.h"
#include "../framework/d3dfile.h"
#include "../framework/d3dtextr.h"
#include "../framework/d3dutil.h"
#include "../gfx/device.h"

// ===================================================== JObject3DList
void JObject3DList::Empty() {
    for (DWORD i = 0; i < Count(); i++) delete GetAt(i);
    list.Empty();
}

void JObject3DList::Remove(DWORD index) {
    delete GetAt(index);
    list.Remove(index);
}

void JObject3DList::Render() {
    for (DWORD i = 0; i < Count(); i++)
        if (GetAt(i)) GetAt(i)->Render();
}

// ===================================================== JObject3DBase
JObject3DBase::JObject3DBase(CD3DApplication* a)
    : app(a), worldPos(0.f, 0.f, 0.f), collisionRadius(0.f), rotatedAngleX(0.f), rotatedAngleY(0.f),
      rotatedAngleZ(0.f), scaledSize(1.0f) {
    prevWorldPos = worldPos;
    prevAngleX = prevAngleY = prevAngleZ = NAN;  // "fresh": no interpolation until the first snapshot
}

JObject3DBase::~JObject3DBase() {}

void JObject3DBase::SetWorldPos(JAXIS axis, float v) {
    if (axis == JAXIS_X) worldPos.x = v;
    else if (axis == JAXIS_Y) worldPos.y = v;
    else worldPos.z = v;
}

void JObject3DBase::ShiftWorldPos(JAXIS axis, float v) {
    if (axis == JAXIS_X) worldPos.x += v;
    else if (axis == JAXIS_Y) worldPos.y += v;
    else worldPos.z += v;
}

void JObject3DBase::ShiftWorldPos_OnGround() {
    D3DVECTOR vmin, vmax;
    GetBox(vmin, vmax);
    worldPos.y = -vmin.y;
}

int JObject3DBase::CheckCollisionRadius(JObject3DBase* other) {
    return D3DFindDistance(worldPos, other->worldPos) < collisionRadius + other->collisionRadius;
}

void JObject3DBase::ScaleTo(float s) {
    ScaleBy(1.0f / scaledSize);
    ScaleBy(s);
}

void JObject3DBase::RotateTo(JAXIS axis, float a) {
    float cur = axis == JAXIS_X ? -rotatedAngleX : axis == JAXIS_Y ? -rotatedAngleY : -rotatedAngleZ;
    RotateBy(axis, cur);
    RotateBy(axis, a);
}

void JObject3DBase::SnapshotPrev() {
    prevWorldPos = worldPos;
    prevAngleX = rotatedAngleX;
    prevAngleY = rotatedAngleY;
    prevAngleZ = rotatedAngleZ;
}

D3DMATRIX JObject3DBase::_InterpolationMatrix() const {
    D3DMATRIX world;
    float a = Interp::Alpha();
    D3DVECTOR pos = worldPos;
    D3DMATRIX rot;
    D3DUtil_SetIdentityMatrix(rot);
    if (a < 1.f && !std::isnan(prevAngleX)) {
        D3DVECTOR d = worldPos - prevWorldPos;
        if (SquareMagnitude(d) < 60.f * 60.f) pos = Interp::Lerp(prevWorldPos, worldPos, a);
        float k = -(1.f - a);
        float dx = rotatedAngleX - prevAngleX, dy = rotatedAngleY - prevAngleY, dz = rotatedAngleZ - prevAngleZ;
        const float lim = 1.0f;  // larger jumps are instantaneous re-orientations (RotateTo)
        D3DMATRIX m;
        if (dz != 0.f && fabsf(dz) < lim) { D3DUtil_SetRotateZMatrix(m, k * dz); rot = rot * m; }
        if (dy != 0.f && fabsf(dy) < lim) { D3DUtil_SetRotateYMatrix(m, k * dy); rot = rot * m; }
        if (dx != 0.f && fabsf(dx) < lim) { D3DUtil_SetRotateXMatrix(m, k * dx); rot = rot * m; }
    }
    D3DMATRIX t;
    D3DUtil_SetTranslateMatrix(t, pos.x, pos.y, pos.z);
    world = rot * t;
    return world;
}

void JObject3DBase::_SetWorldTransform() {
    D3DMATRIX worldMatrix = _InterpolationMatrix();
    app->GetD3DDevice()->SetTransform(D3DTRANSFORMSTATE_WORLD, &worldMatrix);
}

void JObject3DBase::_ResetWorldTransform() {
    D3DMATRIX worldMatrix;
    D3DUtil_SetIdentityMatrix(worldMatrix);
    app->GetD3DDevice()->SetTransform(D3DTRANSFORMSTATE_WORLD, &worldMatrix);
}

// ===================================================== JObject3DX
JObject3DX::JObject3DX(CD3DApplication* a, WORD resId)
    : JObject3DBase(a), currentFrame(0), flatShade(0), stateShadeMode(0) {
    for (auto& o : objArray) o = nullptr;
    if (resId != 0xFFFF) {
        objArray[0] = new CD3DFile();
        objArray[0]->LoadFromResource(resId);
    }
}

JObject3DX::~JObject3DX() {
    for (auto& o : objArray) {
        delete o;
        o = nullptr;
    }
}

void JObject3DX::Shift(const D3DVECTOR& v) {
    for (int i = 0; i < 10 && objArray[i]; i++) objArray[i]->Shift(v);
}

void JObject3DX::ScaleBy(float s) {
    scaledSize *= s;
    for (int i = 0; i < 10 && objArray[i]; i++) objArray[i]->Scale(s);
}

void JObject3DX::RotateBy(JAXIS axis, float a) {
    if (axis == JAXIS_X) {
        rotatedAngleX += a;
        for (int i = 0; i < 10 && objArray[i]; i++) objArray[i]->RotateX(a);
    } else if (axis == JAXIS_Y) {
        rotatedAngleY += a;
        for (int i = 0; i < 10 && objArray[i]; i++) objArray[i]->RotateY(a);
    } else {
        rotatedAngleZ += a;
        for (int i = 0; i < 10 && objArray[i]; i++) objArray[i]->RotateZ(a);
    }
}

void JObject3DX::GetBox(D3DVECTOR& vmin, D3DVECTOR& vmax) {
    if (!objArray[0]) {
        vmin = D3DVECTOR(0.f, 0.f, 0.f);
        vmax = D3DVECTOR(0.f, 0.f, 0.f);
    } else {
        objArray[0]->GetBoxMin(vmin);
        objArray[0]->GetBoxMax(vmax);
    }
}

void JObject3DX::Render() {
    if (currentFrame < 0 || currentFrame >= 10 || !objArray[currentFrame]) return;
    _SetWorldTransform();
    _SetRenderState();
    objArray[currentFrame]->Render(app->GetD3DDevice());
    _ResetRenderState();
    _ResetWorldTransform();
}

void JObject3DX::AppendExtraFrame(WORD resId) {
    int i = 0;
    while (i < 10 && objArray[i]) i++;
    if (i < 10) {
        objArray[i] = new CD3DFile();
        objArray[i]->LoadFromResource(resId);
    }
}

void JObject3DX::_SetRenderState() {
    Device* d = app->GetD3DDevice();
    d->GetRenderState(D3DRENDERSTATE_SHADEMODE, &stateShadeMode);
    d->SetRenderState(D3DRENDERSTATE_SHADEMODE, flatShade ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);
}

void JObject3DX::_ResetRenderState() { app->GetD3DDevice()->SetRenderState(D3DRENDERSTATE_SHADEMODE, stateShadeMode); }

// ===================================================== JObject3D
JObject3D::JObject3D(CD3DApplication* a, DWORD vc, DWORD ic)
    : JObject3DBase(a), vertexCount(vc), vertexList(nullptr), indexCount(ic), indexList(nullptr), textureResId(0),
      stateLighting(0), statePerspective(0), stateAlphaBlend(0), stateSrcBlend(0), stateDestBlend(0),
      stateSpecular(0), stateZWriteEnable(0), stateColorOp(0), stateColorArg1(0), stateColorArg2(0),
      stateAlphaOp(0), stateAlphaArg1(0), stateAlphaArg2(0), stateMinFilter(0), stateMagFilter(0),
      stateMipFilter(0), usePerspective(0), alphaState(ALPHA_NONE), filterState(FILTER_NONE) {
    vertexList = new D3DLVERTEX[vertexCount];
    memset(vertexList, 0, sizeof(D3DLVERTEX) * vertexCount);
    indexList = new WORD[indexCount];
    memset(indexList, 0, sizeof(WORD) * indexCount);
    SetColor(0xff, 0xff, 0xff, 0xff);
}

JObject3D::~JObject3D() {
    delete[] vertexList;
    delete[] indexList;
}

static void TransformLVertices(D3DLVERTEX* list, DWORD n, const D3DMATRIX& m) {
    D3DVERTEX src, dst;
    memset(&src, 0, sizeof src);
    memset(&dst, 0, sizeof dst);
    for (DWORD i = 0; i < n; i++) {
        SetVertexFromLVertex(&src, list[i]);
        D3DMath_VertexMatrixMultiply(dst, src, m);
        SetLVertexFromVertex(&list[i], dst);
    }
}

void JObject3D::Shift(const D3DVECTOR& v) {
    D3DMATRIX m;
    D3DUtil_SetTranslateMatrix(m, v);
    TransformLVertices(vertexList, vertexCount, m);
}

void JObject3D::ScaleBy(float s) {
    scaledSize *= s;
    D3DMATRIX m;
    D3DUtil_SetScaleMatrix(m, s, s, s);
    TransformLVertices(vertexList, vertexCount, m);
}

void JObject3D::RotateBy(JAXIS axis, float a) {
    D3DMATRIX m;
    if (axis == JAXIS_X) {
        rotatedAngleX += a;
        D3DUtil_SetRotateXMatrix(m, a);
    } else if (axis == JAXIS_Y) {
        rotatedAngleY += a;
        D3DUtil_SetRotateYMatrix(m, a);
    } else {
        rotatedAngleZ += a;
        D3DUtil_SetRotateZMatrix(m, a);
    }
    TransformLVertices(vertexList, vertexCount, m);
}

void JObject3D::GetBox(D3DVECTOR& vmin, D3DVECTOR& vmax) {
    vmin = D3DVECTOR(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f);
    vmax = D3DVECTOR(1.1754944e-38f, 1.1754944e-38f, 1.1754944e-38f);
    for (DWORD i = 0; i < vertexCount; i++) {
        const D3DLVERTEX& v = vertexList[i];
        if (v.x < vmin.x) vmin.x = v.x;
        if (v.y < vmin.y) vmin.y = v.y;
        if (v.z < vmin.z) vmin.z = v.z;
        if (v.x > vmax.x) vmax.x = v.x;
        if (v.y > vmax.y) vmax.y = v.y;
        if (v.z > vmax.z) vmax.z = v.z;
    }
}

void JObject3D::Render() {
    _SetWorldTransform();
    Device* d = app->GetD3DDevice();
    if (textureResId != 0) d->SetTexture(0, D3DTextr_GetSurface(textureResId));
    _SetRenderState();
    d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_LVERTEX, vertexList, vertexCount, indexList, indexCount, 0);
    d->SetTexture(0, nullptr);
    _ResetRenderState();
    _ResetWorldTransform();
}

void JObject3D::SetTexture(WORD resId, float, float, float, float) { textureResId = resId; }

void JObject3D::SetColor(WORD r, WORD g, WORD b, WORD a) {
    for (DWORD i = 0; i < vertexCount; i++)
        vertexList[i].color = (DWORD)a << 24 | (DWORD)r << 16 | (DWORD)g << 8 | (DWORD)b;
}

void JObject3D::SetAlpha(WORD a) {
    for (DWORD i = 0; i < vertexCount; i++) vertexList[i].color = (DWORD)a << 24 | (vertexList[i].color & 0xFFFFFF);
}

void JObject3D::SetAlphaFactor(float f) { SetAlpha((WORD)Round(f * 255.0f)); }

void JObject3D::_SetRenderState() {
    Device* d = app->GetD3DDevice();
    d->GetRenderState(D3DRENDERSTATE_LIGHTING, &stateLighting);
    d->GetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE, &statePerspective);
    if (alphaState != ALPHA_NONE) {
        d->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &stateAlphaBlend);
        d->GetRenderState(D3DRENDERSTATE_SRCBLEND, &stateSrcBlend);
        d->GetRenderState(D3DRENDERSTATE_DESTBLEND, &stateDestBlend);
        d->GetRenderState(D3DRENDERSTATE_SPECULARENABLE, &stateSpecular);
        d->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE, &stateZWriteEnable);
        if (alphaState == ALPHA_TRANSPARENT) {
            d->GetTextureStageState(0, D3DTSS_COLOROP, &stateColorOp);
            d->GetTextureStageState(0, D3DTSS_COLORARG1, &stateColorArg1);
            d->GetTextureStageState(0, D3DTSS_COLORARG2, &stateColorArg2);
            d->GetTextureStageState(0, D3DTSS_ALPHAOP, &stateAlphaOp);
            d->GetTextureStageState(0, D3DTSS_ALPHAARG1, &stateAlphaArg1);
            d->GetTextureStageState(0, D3DTSS_ALPHAARG2, &stateAlphaArg2);
        }
    }
    if (filterState != FILTER_NONE) {
        d->GetTextureStageState(0, D3DTSS_MINFILTER, &stateMinFilter);
        d->GetTextureStageState(0, D3DTSS_MAGFILTER, &stateMagFilter);
        d->GetTextureStageState(0, D3DTSS_MIPFILTER, &stateMipFilter);
    }
    d->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
    d->SetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE, usePerspective);
    if (alphaState == ALPHA_PARTICLE) {
        d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
        d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
        d->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, FALSE);
        d->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
    } else if (alphaState == ALPHA_SHADOW) {
        d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO);
        d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCCOLOR);
        d->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, FALSE);
        d->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
    } else if (alphaState == ALPHA_TRANSPARENT) {
        d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
        d->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
        d->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, FALSE);
        d->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
        d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }
    if (filterState == FILTER_POINT) {
        d->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFG_POINT);
        d->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_POINT);
        d->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTFP_POINT);
    } else if (filterState == FILTER_LINEAR) {
        d->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFG_LINEAR);
        d->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
        d->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTFP_LINEAR);
    }
}

void JObject3D::_ResetRenderState() {
    Device* d = app->GetD3DDevice();
    d->SetRenderState(D3DRENDERSTATE_LIGHTING, stateLighting);
    d->SetRenderState(D3DRENDERSTATE_TEXTUREPERSPECTIVE, statePerspective);
    if (alphaState != ALPHA_NONE) {
        d->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, stateAlphaBlend);
        d->SetRenderState(D3DRENDERSTATE_SRCBLEND, stateSrcBlend);
        d->SetRenderState(D3DRENDERSTATE_DESTBLEND, stateDestBlend);
        d->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, stateSpecular);
        d->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, stateZWriteEnable);
        if (alphaState == ALPHA_TRANSPARENT) {
            d->SetTextureStageState(0, D3DTSS_COLOROP, stateColorOp);
            d->SetTextureStageState(0, D3DTSS_COLORARG1, stateColorArg1);
            d->SetTextureStageState(0, D3DTSS_COLORARG2, stateColorArg2);
            d->SetTextureStageState(0, D3DTSS_ALPHAOP, stateAlphaOp);
            d->SetTextureStageState(0, D3DTSS_ALPHAARG1, stateAlphaArg1);
            d->SetTextureStageState(0, D3DTSS_ALPHAARG2, stateAlphaArg2);
        }
    }
    if (filterState != FILTER_NONE) {
        d->SetTextureStageState(0, D3DTSS_MINFILTER, stateMinFilter);
        d->SetTextureStageState(0, D3DTSS_MAGFILTER, stateMagFilter);
        d->SetTextureStageState(0, D3DTSS_MIPFILTER, stateMipFilter);
    }
}

// ===================================================== JObject3D_Rect
JObject3D_Rect::JObject3D_Rect(CD3DApplication* a, float w, float h) : JObject3D(a, 4, 6) {
    float hw = w / 2.0f, hh = h / 2.0f;
    SetLVertexFromVector(&vertexList[0], D3DVECTOR(-hw, 0.0f, hh));
    SetLVertexFromVector(&vertexList[1], D3DVECTOR(hw, 0.0f, hh));
    SetLVertexFromVector(&vertexList[2], D3DVECTOR(-hw, 0.0f, -hh));
    SetLVertexFromVector(&vertexList[3], D3DVECTOR(hw, 0.0f, -hh));
    static const WORD idx[6] = {0, 1, 3, 0, 3, 2};
    for (DWORD i = 0; i < indexCount; i++) indexList[i] = idx[i];
}

void JObject3D_Rect::SetTexture(WORD resId, float u1, float v1, float u2, float v2) {
    JObject3D::SetTexture(resId, u1, v1, u2, v2);
    vertexList[0].tu = u1 + 0.0001f; vertexList[0].tv = v1 + 0.0001f;
    vertexList[1].tu = u2 - 0.0001f; vertexList[1].tv = v1 + 0.0001f;
    vertexList[2].tu = u1 + 0.0001f; vertexList[2].tv = v2 - 0.0001f;
    vertexList[3].tu = u2 - 0.0001f; vertexList[3].tv = v2 - 0.0001f;
}

void JObject3D_Rect::Render2(const D3DMATRIX& view) {
    _SetWorldTransform2(view);
    Device* d = app->GetD3DDevice();
    if (textureResId != 0) d->SetTexture(0, D3DTextr_GetSurface(textureResId));
    _SetRenderState();
    d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_LVERTEX, vertexList, vertexCount, indexList, indexCount, 0);
    d->SetTexture(0, nullptr);
    _ResetRenderState();
    _ResetWorldTransform();
}

void JObject3D_Rect::_SetWorldTransform2(const D3DMATRIX& view) {
    D3DMATRIX worldMatrix, viewMatrix = view;
    D3DMath_MatrixInvert(worldMatrix, viewMatrix);
    D3DMATRIX interp = _InterpolationMatrix();  // translation part honours interpolation
    worldMatrix._41 = interp._41;
    worldMatrix._42 = interp._42;
    worldMatrix._43 = interp._43;
    app->GetD3DDevice()->SetTransform(D3DTRANSFORMSTATE_WORLD, &worldMatrix);
}

JObject3D_Shadow::JObject3D_Shadow(CD3DApplication* a, float w, float h) : JObject3D_Rect(a, w, h) {
    UsePerspective(1);
    SetAlphaState(ALPHA_SHADOW);
    SetFilterState(FILTER_LINEAR);
}

JObject3D_Particle::JObject3D_Particle(CD3DApplication* a, float w, float h) : JObject3D_Rect(a, w, h) {
    JObject3D::RotateBy(JAXIS_X, -1.5707964f);
    UsePerspective(0);
    SetAlphaState(ALPHA_PARTICLE);
    SetFilterState(FILTER_LINEAR);
}

void SetLVertexFromVector(D3DLVERTEX* lv, const D3DVECTOR& v) { lv->x = v.x; lv->y = v.y; lv->z = v.z; }
void SetLVertexFromVertex(D3DLVERTEX* lv, const D3DVERTEX& v) { lv->x = v.x; lv->y = v.y; lv->z = v.z; }
void SetVertexFromLVertex(D3DVERTEX* v, const D3DLVERTEX& lv) { v->x = lv.x; v->y = lv.y; v->z = lv.z; }
