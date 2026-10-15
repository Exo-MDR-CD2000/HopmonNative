// Ports of the DirectX 7 SDK sample helpers (d3dutil.cpp / d3dmath.cpp) as used by Hopmon.
#pragma once
#include "../core/d3dtypes.h"

void D3DUtil_SetIdentityMatrix(D3DMATRIX& m);
void D3DUtil_SetTranslateMatrix(D3DMATRIX& m, float tx, float ty, float tz);
void D3DUtil_SetTranslateMatrix(D3DMATRIX& m, const D3DVECTOR& v);
void D3DUtil_SetScaleMatrix(D3DMATRIX& m, float sx, float sy, float sz);
void D3DUtil_SetRotateXMatrix(D3DMATRIX& m, float a);
void D3DUtil_SetRotateYMatrix(D3DMATRIX& m, float a);
void D3DUtil_SetRotateZMatrix(D3DMATRIX& m, float a);
void D3DUtil_SetRotationMatrix(D3DMATRIX& m, const D3DVECTOR& axis, float a);
HRESULT D3DUtil_SetViewMatrix(D3DMATRIX& m, const D3DVECTOR& from, const D3DVECTOR& at, const D3DVECTOR& up);
HRESULT D3DUtil_SetProjectionMatrix(D3DMATRIX& m, float fov, float aspect, float zNear, float zFar);
void D3DUtil_InitMaterial(D3DMATERIAL7& m, float r, float g, float b, float a);
// Hopmon's modified version: (type, diffuse grey level, specular grey level, x, y, z)
void D3DUtil_InitLight(D3DLIGHT7& l, D3DLIGHTTYPE type, float diffuse, float specular, float x, float y, float z);

void D3DMath_MatrixMultiply(D3DMATRIX& q, const D3DMATRIX& a, const D3DMATRIX& b);
HRESULT D3DMath_MatrixInvert(D3DMATRIX& q, const D3DMATRIX& a);
HRESULT D3DMath_VectorMatrixMultiply(D3DVECTOR& out, const D3DVECTOR& in, const D3DMATRIX& m);
HRESULT D3DMath_VertexMatrixMultiply(D3DVERTEX& out, const D3DVERTEX& in, const D3DMATRIX& m);

// JStandard helpers
void Randomize();
long Random(long range);
long Round(float f);
D3DVECTOR D3DVectorMultiply(const D3DVECTOR& v, float s);
float D3DFindDistance(const D3DVECTOR& a, const D3DVECTOR& b);
