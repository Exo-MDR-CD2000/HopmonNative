#include "d3dutil.h"
#include <SDL3/SDL.h>
#include <cfloat>

void D3DUtil_SetIdentityMatrix(D3DMATRIX& m) {
    m._12 = m._13 = m._14 = m._21 = m._23 = m._24 = 0.f;
    m._31 = m._32 = m._34 = m._41 = m._42 = m._43 = 0.f;
    m._11 = m._22 = m._33 = m._44 = 1.f;
}

void D3DUtil_SetTranslateMatrix(D3DMATRIX& m, float tx, float ty, float tz) {
    D3DUtil_SetIdentityMatrix(m);
    m._41 = tx;
    m._42 = ty;
    m._43 = tz;
}

void D3DUtil_SetTranslateMatrix(D3DMATRIX& m, const D3DVECTOR& v) { D3DUtil_SetTranslateMatrix(m, v.x, v.y, v.z); }

void D3DUtil_SetScaleMatrix(D3DMATRIX& m, float sx, float sy, float sz) {
    D3DUtil_SetIdentityMatrix(m);
    m._11 = sx;
    m._22 = sy;
    m._33 = sz;
}

void D3DUtil_SetRotateXMatrix(D3DMATRIX& m, float a) {
    D3DUtil_SetIdentityMatrix(m);
    m._22 = cosf(a);
    m._23 = sinf(a);
    m._32 = -sinf(a);
    m._33 = cosf(a);
}

void D3DUtil_SetRotateYMatrix(D3DMATRIX& m, float a) {
    D3DUtil_SetIdentityMatrix(m);
    m._11 = cosf(a);
    m._13 = -sinf(a);
    m._31 = sinf(a);
    m._33 = cosf(a);
}

void D3DUtil_SetRotateZMatrix(D3DMATRIX& m, float a) {
    D3DUtil_SetIdentityMatrix(m);
    m._11 = cosf(a);
    m._12 = sinf(a);
    m._21 = -sinf(a);
    m._22 = cosf(a);
}

void D3DUtil_SetRotationMatrix(D3DMATRIX& m, const D3DVECTOR& axis, float a) {
    float c = cosf(a), s = sinf(a);
    D3DVECTOR v = Normalize(axis);
    m._11 = v.x * v.x * (1.f - c) + c;
    m._12 = v.x * v.y * (1.f - c) - v.z * s;
    m._13 = v.x * v.z * (1.f - c) + v.y * s;
    m._21 = v.y * v.x * (1.f - c) + v.z * s;
    m._22 = v.y * v.y * (1.f - c) + c;
    m._23 = v.y * v.z * (1.f - c) - v.x * s;
    m._31 = v.z * v.x * (1.f - c) - v.y * s;
    m._32 = v.z * v.y * (1.f - c) + v.x * s;
    m._33 = v.z * v.z * (1.f - c) + c;
    m._14 = m._24 = m._34 = 0.f;
    m._41 = m._42 = m._43 = 0.f;
    m._44 = 1.f;
}

HRESULT D3DUtil_SetViewMatrix(D3DMATRIX& m, const D3DVECTOR& from, const D3DVECTOR& at, const D3DVECTOR& worldUp) {
    D3DVECTOR view = at - from;
    float len = Magnitude(view);
    if (len < 1e-6f) return (HRESULT)0x80070057;
    view /= len;
    float dot = DotProduct(worldUp, view);
    D3DVECTOR up = worldUp - dot * view;
    len = Magnitude(up);
    if (len < 1e-6f) {
        up = D3DVECTOR(0.f, 1.f, 0.f) - view.y * view;
        len = Magnitude(up);
        if (len < 1e-6f) {
            up = D3DVECTOR(0.f, 0.f, 1.f) - view.z * view;
            len = Magnitude(up);
            if (len < 1e-6f) return (HRESULT)0x80070057;
        }
    }
    up /= len;
    D3DVECTOR right = CrossProduct(up, view);
    D3DUtil_SetIdentityMatrix(m);
    m._11 = right.x; m._12 = up.x; m._13 = view.x;
    m._21 = right.y; m._22 = up.y; m._23 = view.y;
    m._31 = right.z; m._32 = up.z; m._33 = view.z;
    m._41 = -DotProduct(from, right);
    m._42 = -DotProduct(from, up);
    m._43 = -DotProduct(from, view);
    return S_OK;
}

HRESULT D3DUtil_SetProjectionMatrix(D3DMATRIX& m, float fov, float aspect, float zn, float zf) {
    if (fabs((double)(zf - zn)) < 0.01) return (HRESULT)0x80070057;
    if (fabs(sin((double)(fov / 2.f))) < 0.01) return (HRESULT)0x80070057;
    float w = aspect * (cosf(fov / 2.f) / sinf(fov / 2.f));
    float h = 1.0f * (cosf(fov / 2.f) / sinf(fov / 2.f));
    float Q = zf / (zf - zn);
    m = D3DMATRIX();
    m._11 = w;
    m._22 = h;
    m._33 = Q;
    m._34 = 1.0f;
    m._43 = -Q * zn;
    return S_OK;
}

void D3DUtil_InitMaterial(D3DMATERIAL7& m, float r, float g, float b, float a) {
    memset(&m, 0, sizeof m);
    m.diffuse.r = m.ambient.r = r;
    m.diffuse.g = m.ambient.g = g;
    m.diffuse.b = m.ambient.b = b;
    m.diffuse.a = m.ambient.a = a;
}

void D3DUtil_InitLight(D3DLIGHT7& l, D3DLIGHTTYPE type, float diffuse, float specular, float x, float y, float z) {
    l = D3DLIGHT7{};
    l.type = type;
    l.diffuse.r = l.diffuse.g = l.diffuse.b = diffuse;
    l.specular.r = l.specular.g = l.specular.b = specular;
    l.direction = l.position = D3DVECTOR(x, y, z);
    l.attenuation0 = 1.f;
    l.range = (float)sqrt((double)FLT_MAX);
}

void D3DMath_MatrixMultiply(D3DMATRIX& q, const D3DMATRIX& a, const D3DMATRIX& b) {
    float pM[16] = {0};
    const float* pA = &a._11;
    const float* pB = &b._11;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++) pM[4 * i + j] += pA[4 * i + k] * pB[4 * k + j];
    memcpy(&q._11, pM, sizeof pM);
}

HRESULT D3DMath_MatrixInvert(D3DMATRIX& q, const D3DMATRIX& a) {
    if (fabs((double)(a._44 - 1.f)) > .001f) return (HRESULT)0x80070057;
    if (fabs((double)a._14) > .001f || fabs((double)a._24) > .001f || fabs((double)a._34) > .001f)
        return (HRESULT)0x80070057;
    float fDetInv = 1.0f / (a._11 * (a._22 * a._33 - a._23 * a._32) - a._12 * (a._21 * a._33 - a._23 * a._31) +
                            a._13 * (a._21 * a._32 - a._22 * a._31));
    q._11 = fDetInv * (a._22 * a._33 - a._23 * a._32);
    q._12 = -fDetInv * (a._12 * a._33 - a._13 * a._32);
    q._13 = fDetInv * (a._12 * a._23 - a._13 * a._22);
    q._14 = 0.0f;
    q._21 = -fDetInv * (a._21 * a._33 - a._23 * a._31);
    q._22 = fDetInv * (a._11 * a._33 - a._13 * a._31);
    q._23 = -fDetInv * (a._11 * a._23 - a._13 * a._21);
    q._24 = 0.0f;
    q._31 = fDetInv * (a._21 * a._32 - a._22 * a._31);
    q._32 = -fDetInv * (a._11 * a._32 - a._12 * a._31);
    q._33 = fDetInv * (a._11 * a._22 - a._12 * a._21);
    q._34 = 0.0f;
    q._41 = -(a._41 * q._11 + a._42 * q._21 + a._43 * q._31);
    q._42 = -(a._41 * q._12 + a._42 * q._22 + a._43 * q._32);
    q._43 = -(a._41 * q._13 + a._42 * q._23 + a._43 * q._33);
    q._44 = 1.0f;
    return S_OK;
}

HRESULT D3DMath_VectorMatrixMultiply(D3DVECTOR& out, const D3DVECTOR& v, const D3DMATRIX& m) {
    float x = v.x * m._11 + v.y * m._21 + v.z * m._31 + m._41;
    float y = v.x * m._12 + v.y * m._22 + v.z * m._32 + m._42;
    float z = v.x * m._13 + v.y * m._23 + v.z * m._33 + m._43;
    float w = v.x * m._14 + v.y * m._24 + v.z * m._34 + m._44;
    if (fabs((double)w) < 1e-5f) return (HRESULT)0x80070057;
    out.x = x / w;
    out.y = y / w;
    out.z = z / w;
    return S_OK;
}

HRESULT D3DMath_VertexMatrixMultiply(D3DVERTEX& out, const D3DVERTEX& in, const D3DMATRIX& m) {
    D3DVECTOR p(in.x, in.y, in.z), n(in.nx, in.ny, in.nz), po, no;
    HRESULT hr = D3DMath_VectorMatrixMultiply(po, p, m);
    if (FAILED(hr)) return hr;
    out.x = po.x; out.y = po.y; out.z = po.z;
    hr = D3DMath_VectorMatrixMultiply(no, n, m);
    if (FAILED(hr)) return hr;
    out.nx = no.x; out.ny = no.y; out.nz = no.z;
    return hr;
}

// --- JStandard -------------------------------------------------------------
// The original used the MSVC CRT rand(): reproduce its LCG so distributions match.
static uint32_t g_randState = 1;
static int msvc_rand() {
    g_randState = g_randState * 214013u + 2531011u;
    return (int)((g_randState >> 16) & 0x7FFF);
}

void Randomize() { g_randState = (uint32_t)SDL_GetTicks(); }

long Random(long range) {
    if (range < 1) return 0;
    return msvc_rand() % range;
}

long Round(float f) { return (long)(f < 0.f ? f - 0.5f : f + 0.5f); }

D3DVECTOR D3DVectorMultiply(const D3DVECTOR& v, float s) { return D3DVECTOR(v.x * s, v.y * s, v.z * s); }

float D3DFindDistance(const D3DVECTOR& a, const D3DVECTOR& b) {
    float dx = fabsf(a.x - b.x), dy = fabsf(a.y - b.y), dz = fabsf(a.z - b.z);
    return sqrtf(dx * dx + dy * dy + dz * dz);
}
