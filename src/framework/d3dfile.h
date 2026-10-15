// Replacement for the DX7 SDK CD3DFile/CD3DFileObject (d3dfile.cpp) with a
// built-in parser for the text .X meshes stored in Hopmon.exe (RT_RCDATA).
#pragma once
#include "../core/d3dtypes.h"
#include <string>
#include <vector>
#include <memory>

class Device;

struct MeshMaterialData {
    D3DMATERIAL7 mtrl{};
    std::string texture;
    DWORD numIndices = 0;
};

class CD3DFileObject {
public:
    explicit CD3DFileObject(const std::string& name);
    void Render(Device* dev, bool alpha);
    std::string name;
    D3DMATRIX mat;
    bool hasMeshData = false;
    std::vector<D3DVERTEX> vertices;
    std::vector<WORD> indices;
    MeshMaterialData materials[16];
    DWORD numMaterials = 0;
    bool hasAlpha = false;
    std::vector<std::unique_ptr<CD3DFileObject>> children;  // child list (m_pChild + m_pNext chain)
};

class CD3DFile {
public:
    CD3DFile();
    ~CD3DFile();
    HRESULT LoadFromResource(WORD resId);
    HRESULT Render(Device* dev);
    void Scale(float s);
    void Shift(const D3DVECTOR& v);
    void RotateX(float a);
    void RotateY(float a);
    void RotateZ(float a);
    void GetBoxMin(D3DVECTOR& out);
    void GetBoxMax(D3DVECTOR& out);
    template <class F> void ForEachMesh(F f) {
        if (root_)
            for (auto& c : root_->children) Visit(c.get(), f);
    }

private:
    template <class F> void Visit(CD3DFileObject* o, F& f) {
        if (o->hasMeshData) f(*o);
        for (auto& c : o->children) Visit(c.get(), f);
    }
    std::unique_ptr<CD3DFileObject> root_;
};
