// Plain-data equivalents of the Direct3D 7 / DirectDraw types used by Hopmon.
// Layouts and semantics match the originals (row-vector math, left-handed).
#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>

#ifdef _WIN32
// On Windows the basic types come from the platform headers so the two never disagree.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef near
#undef far
#define HOPMON_HAVE_WIN_TYPES 1
#else
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef int32_t  HRESULT;
typedef int32_t  BOOL;
typedef uint32_t COLORREF;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define S_OK 0
#define E_FAIL ((HRESULT)0x80004005)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define CLR_INVALID 0xFFFFFFFFu
#endif
typedef uint32_t D3DCOLOR;
#define D3D_OK 0

struct D3DVECTOR {
    float x, y, z;
    D3DVECTOR() : x(0), y(0), z(0) {}
    D3DVECTOR(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }
    D3DVECTOR& operator+=(const D3DVECTOR& v) { x += v.x; y += v.y; z += v.z; return *this; }
    D3DVECTOR& operator-=(const D3DVECTOR& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    D3DVECTOR& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    D3DVECTOR& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }
};
inline D3DVECTOR operator+(const D3DVECTOR& a, const D3DVECTOR& b) { return D3DVECTOR(a.x + b.x, a.y + b.y, a.z + b.z); }
inline D3DVECTOR operator-(const D3DVECTOR& a, const D3DVECTOR& b) { return D3DVECTOR(a.x - b.x, a.y - b.y, a.z - b.z); }
inline D3DVECTOR operator-(const D3DVECTOR& a) { return D3DVECTOR(-a.x, -a.y, -a.z); }
inline D3DVECTOR operator*(const D3DVECTOR& v, float s) { return D3DVECTOR(v.x * s, v.y * s, v.z * s); }
inline D3DVECTOR operator*(float s, const D3DVECTOR& v) { return D3DVECTOR(v.x * s, v.y * s, v.z * s); }
inline D3DVECTOR operator/(const D3DVECTOR& v, float s) { return D3DVECTOR(v.x / s, v.y / s, v.z / s); }
inline bool operator==(const D3DVECTOR& a, const D3DVECTOR& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline float SquareMagnitude(const D3DVECTOR& v) { return v.x * v.x + v.y * v.y + v.z * v.z; }
inline float Magnitude(const D3DVECTOR& v) { return sqrtf(SquareMagnitude(v)); }
inline D3DVECTOR Normalize(const D3DVECTOR& v) { return v / Magnitude(v); }
inline float DotProduct(const D3DVECTOR& a, const D3DVECTOR& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline D3DVECTOR CrossProduct(const D3DVECTOR& a, const D3DVECTOR& b) {
    return D3DVECTOR(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

struct D3DMATRIX {
    float _11, _12, _13, _14;
    float _21, _22, _23, _24;
    float _31, _32, _33, _34;
    float _41, _42, _43, _44;
    D3DMATRIX() { memset(this, 0, sizeof(*this)); }
    float& operator()(int r, int c) { return (&_11)[r * 4 + c]; }
    const float& operator()(int r, int c) const { return (&_11)[r * 4 + c]; }
    const float* m() const { return &_11; }
};
// a*b with row vectors (same as d3dvec.inl operator*)
inline D3DMATRIX operator*(const D3DMATRIX& a, const D3DMATRIX& b) {
    D3DMATRIX r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a(i, k) * b(k, j);
            r(i, j) = s;
        }
    return r;
}

struct D3DCOLORVALUE { float r, g, b, a; };

struct D3DVERTEX {      // D3DFVF_VERTEX (0x112)
    float x, y, z;
    float nx, ny, nz;
    float tu, tv;
};

struct D3DLVERTEX {     // D3DFVF_LVERTEX (0x1E2)
    float x, y, z;
    DWORD dwReserved;
    D3DCOLOR color;
    D3DCOLOR specular;
    float tu, tv;
};

struct D3DMATERIAL7 {
    D3DCOLORVALUE diffuse;
    D3DCOLORVALUE ambient;
    D3DCOLORVALUE specular;
    D3DCOLORVALUE emissive;
    float power;
};

enum D3DLIGHTTYPE { D3DLIGHT_POINT = 1, D3DLIGHT_SPOT = 2, D3DLIGHT_DIRECTIONAL = 3 };

struct D3DLIGHT7 {
    D3DLIGHTTYPE type;
    D3DCOLORVALUE diffuse, specular, ambient;
    D3DVECTOR position, direction;
    float range, falloff, attenuation0, attenuation1, attenuation2, theta, phi;
};

#ifndef HOPMON_HAVE_WIN_TYPES
struct RECT { int32_t left, top, right, bottom; };
#endif

// --- D3D7 enumerations actually used by the game --------------------------
enum D3DTRANSFORMSTATETYPE { D3DTRANSFORMSTATE_WORLD = 1, D3DTRANSFORMSTATE_VIEW = 2, D3DTRANSFORMSTATE_PROJECTION = 3 };

enum D3DRENDERSTATETYPE {
    D3DRENDERSTATE_TEXTUREPERSPECTIVE = 4,
    D3DRENDERSTATE_ZENABLE = 7,
    D3DRENDERSTATE_SHADEMODE = 9,
    D3DRENDERSTATE_ZWRITEENABLE = 14,
    D3DRENDERSTATE_SRCBLEND = 19,
    D3DRENDERSTATE_DESTBLEND = 20,
    D3DRENDERSTATE_CULLMODE = 22,
    D3DRENDERSTATE_DITHERENABLE = 26,
    D3DRENDERSTATE_ALPHABLENDENABLE = 27,
    D3DRENDERSTATE_FOGENABLE = 28,
    D3DRENDERSTATE_SPECULARENABLE = 29,
    D3DRENDERSTATE_LIGHTING = 137,
    D3DRENDERSTATE_AMBIENT = 139,
    D3DRENDERSTATE_MAX = 256
};

enum { D3DSHADE_FLAT = 1, D3DSHADE_GOURAUD = 2 };
enum { D3DCULL_NONE = 1, D3DCULL_CW = 2, D3DCULL_CCW = 3 };
enum D3DBLEND {
    D3DBLEND_ZERO = 1, D3DBLEND_ONE = 2, D3DBLEND_SRCCOLOR = 3, D3DBLEND_INVSRCCOLOR = 4,
    D3DBLEND_SRCALPHA = 5, D3DBLEND_INVSRCALPHA = 6, D3DBLEND_DESTALPHA = 7, D3DBLEND_INVDESTALPHA = 8,
    D3DBLEND_DESTCOLOR = 9, D3DBLEND_INVDESTCOLOR = 10
};

enum D3DTEXTURESTAGESTATETYPE {
    D3DTSS_COLOROP = 1, D3DTSS_COLORARG1 = 2, D3DTSS_COLORARG2 = 3,
    D3DTSS_ALPHAOP = 4, D3DTSS_ALPHAARG1 = 5, D3DTSS_ALPHAARG2 = 6,
    D3DTSS_MAGFILTER = 16, D3DTSS_MINFILTER = 17, D3DTSS_MIPFILTER = 18,
    D3DTSS_MAX = 32
};
enum { D3DTOP_DISABLE = 1, D3DTOP_SELECTARG1 = 2, D3DTOP_SELECTARG2 = 3, D3DTOP_MODULATE = 4 };
enum { D3DTA_DIFFUSE = 0, D3DTA_CURRENT = 1, D3DTA_TEXTURE = 2 };
enum { D3DTFG_POINT = 1, D3DTFG_LINEAR = 2 };  // == D3DTFN_*
enum { D3DTFP_NONE = 1, D3DTFP_POINT = 2, D3DTFP_LINEAR = 3 };

enum D3DPRIMITIVETYPE { D3DPT_POINTLIST = 1, D3DPT_LINELIST = 2, D3DPT_LINESTRIP = 3,
                        D3DPT_TRIANGLELIST = 4, D3DPT_TRIANGLESTRIP = 5, D3DPT_TRIANGLEFAN = 6 };
#define D3DFVF_VERTEX  0x112
#define D3DFVF_LVERTEX 0x1E2

#define D3DCLEAR_TARGET  1
#define D3DCLEAR_ZBUFFER 2

#ifndef D3DX_PI
#define D3DX_PI 3.14159265358979323846
#endif
#define g_PI       3.14159265358979323846f
#define g_2_PI     6.28318530717958623200f
#define g_PI_DIV_2 1.57079632679489655800f
