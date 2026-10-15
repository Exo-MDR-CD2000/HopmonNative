// Port of JObject3D.cpp: 3D object base classes used by every game entity.
#pragma once
#include "../core/d3dtypes.h"
#include "../framework/interp.h"
#include "JLib.h"

class CD3DApplication;
class CD3DFile;

class JObject3DBase : public Interp::Tracked {
public:
    explicit JObject3DBase(CD3DApplication* app);
    virtual ~JObject3DBase();
    void SetWorldPos(JAXIS axis, float v);
    void SetWorldPos(const D3DVECTOR& v) { worldPos = v; }
    void ShiftWorldPos(JAXIS axis, float v);
    void ShiftWorldPos(const D3DVECTOR& v) { worldPos += v; }
    void ShiftWorldPos_OnGround();
    D3DVECTOR GetWorldPos() const { return worldPos; }
    void SetCollisionRadius(float r) { collisionRadius = r; }
    int CheckCollisionRadius(JObject3DBase* other);
    void ScaleTo(float s);
    void RotateTo(JAXIS axis, float a);
    virtual void Shift(const D3DVECTOR& v) { (void)v; }
    virtual void ScaleBy(float s) { (void)s; }
    virtual void RotateBy(JAXIS axis, float a) { (void)axis; (void)a; }
    virtual void GetBox(D3DVECTOR& vmin, D3DVECTOR& vmax) { vmin = vmax = D3DVECTOR(); }
    virtual void Render() {}

    // interpolation (render only)
    void SnapshotPrev() override;
    void SnapInterpolation() { SnapshotPrev(); }

protected:
    virtual void _SetRenderState() {}
    virtual void _ResetRenderState() {}
    void _SetWorldTransform();
    void _ResetWorldTransform();
    D3DMATRIX _InterpolationMatrix() const;

    CD3DApplication* app;
    D3DVECTOR worldPos;
    float collisionRadius;
    float rotatedAngleX, rotatedAngleY, rotatedAngleZ;
    float scaledSize;

    D3DVECTOR prevWorldPos;
    float prevAngleX = 0, prevAngleY = 0, prevAngleZ = 0;
};

class JObject3DList {
public:
    JObject3DList() {}
    virtual ~JObject3DList() { Empty(); }
    void Append(JObject3DBase* o) { list.Append(o); }
    void Empty();
    void Remove(DWORD index);
    DWORD Count() const { return list.Count(); }
    JObject3DBase* GetAt(DWORD index) const { return (JObject3DBase*)list.GetData(index); }
    void Render();

protected:
    JBaseList list;
};

class JObject3DX : public JObject3DBase {
public:
    JObject3DX(CD3DApplication* app, WORD resId);
    ~JObject3DX() override;
    void Shift(const D3DVECTOR& v) override;
    void ScaleBy(float s) override;
    void RotateBy(JAXIS axis, float a) override;
    void GetBox(D3DVECTOR& vmin, D3DVECTOR& vmax) override;
    void Render() override;
    void AppendExtraFrame(WORD resId);
    void SetCurrentFrame(int f) { currentFrame = f; }
    void SetFlatShade(int f) { flatShade = f; }

protected:
    void _SetRenderState() override;
    void _ResetRenderState() override;
    int currentFrame;
    CD3DFile* objArray[10];
    int flatShade;
    DWORD stateShadeMode;
};

class JObject3D : public JObject3DBase {
public:
    enum ALPHA_STATE { ALPHA_NONE = 0, ALPHA_PARTICLE = 1, ALPHA_SHADOW = 2, ALPHA_TRANSPARENT = 3 };
    enum FILTER_STATE { FILTER_NONE = 0, FILTER_POINT = 1, FILTER_LINEAR = 2 };

    JObject3D(CD3DApplication* app, DWORD vertexCount, DWORD indexCount);
    ~JObject3D() override;
    void Shift(const D3DVECTOR& v) override;
    void ScaleBy(float s) override;
    void RotateBy(JAXIS axis, float a) override;
    void GetBox(D3DVECTOR& vmin, D3DVECTOR& vmax) override;
    void Render() override;
    virtual void SetTexture(WORD resId, float u1, float v1, float u2, float v2);
    void SetColor(WORD r, WORD g, WORD b, WORD a);
    void SetAlpha(WORD a);
    void SetAlphaFactor(float f);
    DWORD GetVertexCount() const { return vertexCount; }
    D3DLVERTEX GetVertexAt(DWORD i) const { return vertexList[i]; }
    void SetVertexAt(DWORD i, const D3DLVERTEX& v) { vertexList[i] = v; }
    DWORD GetIndexCount() const { return indexCount; }
    WORD GetIndexAt(DWORD i) const { return indexList[i]; }
    void SetIndexAt(DWORD i, WORD v) { indexList[i] = v; }
    void UsePerspective(int p) { usePerspective = p; }
    void SetAlphaState(ALPHA_STATE s) { alphaState = s; }
    void SetFilterState(FILTER_STATE s) { filterState = s; }

protected:
    void _SetRenderState() override;
    void _ResetRenderState() override;
    DWORD vertexCount;
    D3DLVERTEX* vertexList;
    DWORD indexCount;
    WORD* indexList;
    WORD textureResId;
    DWORD stateLighting, statePerspective, stateAlphaBlend, stateSrcBlend, stateDestBlend, stateSpecular,
        stateZWriteEnable, stateColorOp, stateColorArg1, stateColorArg2, stateAlphaOp, stateAlphaArg1,
        stateAlphaArg2, stateMinFilter, stateMagFilter, stateMipFilter;
    int usePerspective;
    ALPHA_STATE alphaState;
    FILTER_STATE filterState;
};

class JObject3D_Rect : public JObject3D {
public:
    JObject3D_Rect(CD3DApplication* app, float width, float height);
    void SetTexture(WORD resId, float u1, float v1, float u2, float v2) override;
    virtual void Render2(const D3DMATRIX& view);

protected:
    void _SetWorldTransform2(const D3DMATRIX& view);
};

class JObject3D_Shadow : public JObject3D_Rect {
public:
    JObject3D_Shadow(CD3DApplication* app, float width, float height);
};

class JObject3D_Particle : public JObject3D_Rect {
public:
    JObject3D_Particle(CD3DApplication* app, float width, float height);
};

void SetLVertexFromVector(D3DLVERTEX* lv, const D3DVECTOR& v);
void SetLVertexFromVertex(D3DLVERTEX* lv, const D3DVERTEX& v);
void SetVertexFromLVertex(D3DVERTEX* v, const D3DLVERTEX& lv);
