#include "d3dfile.h"
#include "d3dutil.h"
#include "../core/assets.h"
#include "../core/log.h"
#include "../gfx/device.h"
#include <cctype>
#include <cstdlib>
#include <map>

namespace {

// ---- Generic text .X reader --------------------------------------------------
struct XNode {
    std::string type, name;
    std::vector<double> nums;
    std::vector<std::string> strings;
    std::vector<XNode> children;
};

class XTokenizer {
public:
    XTokenizer(const char* p, const char* e) : p_(p), e_(e) {}
    // token kinds: 0 end, 1 '{', 2 '}', 3 number, 4 identifier, 5 string
    int Next(std::string& tok, double& num) {
        for (;;) {
            while (p_ < e_ && (isspace((unsigned char)*p_) || *p_ == ';' || *p_ == ',')) p_++;
            if (p_ >= e_) return 0;
            if (*p_ == '/' && p_ + 1 < e_ && p_[1] == '/') { SkipLine(); continue; }
            if (*p_ == '#') { SkipLine(); continue; }
            break;
        }
        char c = *p_;
        if (c == '{') { p_++; return 1; }
        if (c == '}') { p_++; return 2; }
        if (c == '"') {
            const char* s = ++p_;
            while (p_ < e_ && *p_ != '"') p_++;
            tok.assign(s, p_);
            if (p_ < e_) p_++;
            return 5;
        }
        if (isdigit((unsigned char)c) || c == '-' || c == '+' || c == '.') {
            char* end = nullptr;
            num = strtod(p_, &end);
            if (end && end > p_) { p_ = end; return 3; }
        }
        const char* s = p_;
        while (p_ < e_ && (isalnum((unsigned char)*p_) || *p_ == '_' || *p_ == '-' || *p_ == '.')) p_++;
        if (p_ == s) p_++;
        tok.assign(s, p_);
        return 4;
    }
    void SkipLine() { while (p_ < e_ && *p_ != '\n') p_++; }

private:
    const char* p_;
    const char* e_;
};

// Parses the body of a template (after '{') into `node` until the matching '}'.
void ParseBody(XTokenizer& tz, XNode& node) {
    std::string tok, pendingType, pendingName;
    double num;
    int state = 0;  // 0 = data, 1 = saw identifier (type), 2 = saw type + name
    for (;;) {
        int k = tz.Next(tok, num);
        if (k == 0) return;
        if (k == 2) return;
        if (k == 1) {
            XNode child;
            if (state == 0) {
                // reference "{ name }" or anonymous block
                child.type = "__ref";
            } else {
                child.type = pendingType;
                child.name = pendingName;
            }
            ParseBody(tz, child);
            if (child.type == "__ref" && child.children.empty() && child.nums.empty() && !child.strings.empty())
                child.name = child.strings[0];
            node.children.push_back(std::move(child));
            state = 0;
            pendingType.clear();
            pendingName.clear();
            continue;
        }
        if (k == 3) {
            if (state != 0) {  // identifier followed by number: identifier was data (shouldn't happen)
                node.strings.push_back(pendingType);
                state = 0;
            }
            node.nums.push_back(num);
            continue;
        }
        if (k == 5) {
            node.strings.push_back(tok);
            continue;
        }
        if (k == 4) {
            if (state == 0) { pendingType = tok; state = 1; }
            else if (state == 1) { pendingName = tok; state = 2; }
            else { node.strings.push_back(pendingType); pendingType = pendingName; pendingName = tok; }
        }
    }
}

bool ParseXText(const std::vector<uint8_t>& data, XNode& root) {
    if (data.size() < 16 || memcmp(data.data(), "xof ", 4) != 0 || memcmp(data.data() + 8, "txt ", 4) != 0) return false;
    const char* p = (const char*)data.data() + 16;
    XTokenizer tz(p, (const char*)data.data() + data.size());
    root.type = "__root";
    ParseBody(tz, root);
    return true;
}

// ---- Interpretation, mirroring d3dfile.cpp ParseMesh/ParseMeshMaterialList ----

void AddFace(CD3DFileObject* mesh, DWORD mat, const uint32_t* face, DWORD count) {
    if (mat >= 16) return;
    if (mesh->numMaterials < mat + 1) mesh->numMaterials = mat + 1;
    // index write position: after all indices of materials 0..mat (materials are filled in order)
    size_t pos = 0;
    for (DWORD i = 0; i <= mat; i++) pos += mesh->materials[i].numIndices;
    while (count--) {
        uint32_t n = face[0];
        const uint32_t* f = face + 1;
        for (uint32_t i = 2; i < n; i++) {
            mesh->materials[mat].numIndices += 3;
            if (pos + 3 <= mesh->indices.size()) {
                mesh->indices[pos] = (WORD)f[0];
                mesh->indices[pos + 1] = (WORD)face[i];      // == f[i-1]
                mesh->indices[pos + 2] = (WORD)f[i];
            }
            pos += 3;
        }
        face = f + n;  // next face record: [count, i0, i1, ...]
    }
}

void ComputeNormals(CD3DFileObject* mesh) {
    std::vector<D3DVECTOR> normals(mesh->vertices.size());
    for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
        WORD a = mesh->indices[i], b = mesh->indices[i + 1], c = mesh->indices[i + 2];
        const D3DVERTEX &v1 = mesh->vertices[a], &v2 = mesh->vertices[b], &v3 = mesh->vertices[c];
        D3DVECTOR p1(v1.x, v1.y, v1.z), p2(v2.x, v2.y, v2.z), p3(v3.x, v3.y, v3.z);
        D3DVECTOR n = Normalize(CrossProduct(p2 - p1, p3 - p2));
        normals[a] += n;
        normals[b] += n;
        normals[c] += n;
    }
    for (size_t i = 0; i < normals.size(); i++) {
        if (Magnitude(normals[i]) < 0.1f) normals[i] = D3DVECTOR(0, 0, 1);
        normals[i] = Normalize(normals[i]);
        mesh->vertices[i].nx = normals[i].x;
        mesh->vertices[i].ny = normals[i].y;
        mesh->vertices[i].nz = normals[i].z;
    }
}

void ParseMaterial(const XNode& n, CD3DFileObject* mesh, DWORD index) {
    if (index >= 16 || n.nums.size() < 11) return;
    D3DMATERIAL7 m;
    memset(&m, 0, sizeof m);
    m.diffuse = {(float)n.nums[0], (float)n.nums[1], (float)n.nums[2], (float)n.nums[3]};
    m.ambient = m.diffuse;
    m.power = (float)n.nums[4];
    m.specular = {(float)n.nums[5], (float)n.nums[6], (float)n.nums[7], 0.f};
    m.emissive = {(float)n.nums[8], (float)n.nums[9], (float)n.nums[10], 0.f};
    std::string tex;
    for (auto& c : n.children)
        if (c.type == "TextureFilename" && !c.strings.empty()) tex = c.strings[0];
    mesh->materials[index].mtrl = m;
    mesh->materials[index].texture = tex;
    if (m.diffuse.a < 1.0f) mesh->hasAlpha = true;
}

std::unique_ptr<CD3DFileObject> ParseMesh(const XNode& n, const std::map<std::string, const XNode*>& globals) {
    if (n.nums.empty()) return nullptr;
    size_t k = 0;
    auto take = [&]() -> double { return k < n.nums.size() ? n.nums[k++] : 0.0; };
    DWORD nv = (DWORD)take();
    auto mesh = std::make_unique<CD3DFileObject>(n.name);
    mesh->vertices.resize(nv);
    for (DWORD i = 0; i < nv; i++) {
        D3DVERTEX& v = mesh->vertices[i];
        memset(&v, 0, sizeof v);
        v.x = (float)take();
        v.y = (float)take();
        v.z = (float)take();
    }
    DWORD nf = (DWORD)take();
    std::vector<uint32_t> faceData;
    std::vector<size_t> faceStart;
    size_t numIdx = 0;
    for (DWORD f = 0; f < nf; f++) {
        faceStart.push_back(faceData.size());
        uint32_t cnt = (uint32_t)take();
        faceData.push_back(cnt);
        for (uint32_t j = 0; j < cnt; j++) faceData.push_back((uint32_t)take());
        if (cnt >= 2) numIdx += (cnt - 2) * 3;
    }
    mesh->indices.assign(numIdx, 0);
    mesh->hasMeshData = true;
    bool hasNormals = false, hasMaterials = false;
    for (auto& c : n.children) {
        if (c.type == "MeshMaterialList") {
            DWORD nm = c.nums.size() > 0 ? (DWORD)c.nums[0] : 0;
            DWORD nmf = c.nums.size() > 1 ? (DWORD)c.nums[1] : 0;
            if (nm == 1 || nmf != nf) {
                if (nf) AddFace(mesh.get(), 0, faceData.data(), nf);
            } else {
                for (DWORD mat = 0; mat < nm; mat++)
                    for (DWORD face = 0; face < nmf; face++)
                        if (2 + face < c.nums.size() && (DWORD)c.nums[2 + face] == mat)
                            AddFace(mesh.get(), mat, &faceData[faceStart[face]], 1);
            }
            DWORD mi = 0;
            for (auto& m : c.children) {
                if (m.type == "Material") ParseMaterial(m, mesh.get(), mi++);
                else if (m.type == "__ref") {
                    auto it = globals.find(m.name);
                    if (it != globals.end()) ParseMaterial(*it->second, mesh.get(), mi++);
                }
            }
            hasMaterials = true;
        } else if (c.type == "MeshNormals") {
            DWORD nn = c.nums.empty() ? 0 : (DWORD)c.nums[0];
            if (nn == nv && c.nums.size() >= 1 + 3 * (size_t)nn) {
                for (DWORD i = 0; i < nv; i++) {
                    mesh->vertices[i].nx = (float)c.nums[1 + i * 3];
                    mesh->vertices[i].ny = (float)c.nums[2 + i * 3];
                    mesh->vertices[i].nz = (float)c.nums[3 + i * 3];
                }
                hasNormals = true;
            }
        } else if (c.type == "MeshTextureCoords") {
            DWORD nt = c.nums.empty() ? 0 : (DWORD)c.nums[0];
            if (nt == nv && c.nums.size() >= 1 + 2 * (size_t)nt)
                for (DWORD i = 0; i < nv; i++) {
                    mesh->vertices[i].tu = (float)c.nums[1 + i * 2];
                    mesh->vertices[i].tv = (float)c.nums[2 + i * 2];
                }
        }
    }
    if (!hasMaterials && nf) AddFace(mesh.get(), 0, faceData.data(), nf);
    if (!hasNormals) ComputeNormals(mesh.get());
    return mesh;
}

// Parsed meshes cached per resource id; each CD3DFile gets its own copy (the game edits vertices).
std::map<WORD, std::vector<CD3DFileObject*>> g_cache;

void CollectFrames(const XNode& n, CD3DFileObject* parent, const std::map<std::string, const XNode*>& globals);

}  // namespace

CD3DFileObject::CD3DFileObject(const std::string& n) : name(n) {
    D3DUtil_SetIdentityMatrix(mat);
    for (auto& m : materials) m = MeshMaterialData();
    D3DUtil_InitMaterial(materials[0].mtrl, 1, 1, 1, 1);
    memset(&materials[0].mtrl, 0, sizeof(D3DMATERIAL7));  // the SDK zeroes the table right after
}

void CD3DFileObject::Render(Device* dev, bool alpha) {
    if (!hasMeshData) {
        if (!children.empty()) {
            D3DMATRIX old;
            dev->GetTransform(D3DTRANSFORMSTATE_WORLD, &old);
            D3DMATRIX nw = mat * old;
            dev->SetTransform(D3DTRANSFORMSTATE_WORLD, &nw);
            for (auto& c : children) c->Render(dev, alpha);
            dev->SetTransform(D3DTRANSFORMSTATE_WORLD, &old);
        }
        return;
    }
    size_t offset = 0;
    for (DWORD i = 0; i <= numMaterials && i < 16; i++) {
        DWORD n = materials[i].numIndices;
        if (!n) continue;
        if (alpha == hasAlpha) {
            dev->SetMaterial(&materials[i].mtrl);
            dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_VERTEX, vertices.data(), (DWORD)vertices.size(),
                                      indices.data() + offset, n, 0);
        }
        offset += n;
    }
    for (auto& c : children) c->Render(dev, alpha);
}

namespace {
void CollectFrames(const XNode& n, CD3DFileObject* parent, const std::map<std::string, const XNode*>& globals) {
    for (auto& c : n.children) {
        if (c.type == "Mesh") {
            auto m = ParseMesh(c, globals);
            if (m) parent->children.push_back(std::move(m));
        } else if (c.type == "Frame") {
            auto f = std::make_unique<CD3DFileObject>(c.name);
            for (auto& fc : c.children)
                if (fc.type == "FrameTransformMatrix" && fc.nums.size() >= 16)
                    for (int i = 0; i < 16; i++) (&f->mat._11)[i] = (float)fc.nums[i];
            CollectFrames(c, f.get(), globals);
            parent->children.push_back(std::move(f));
        }
    }
}

std::unique_ptr<CD3DFileObject> CloneTree(const CD3DFileObject& o) {
    auto c = std::make_unique<CD3DFileObject>(o.name);
    c->mat = o.mat;
    c->hasMeshData = o.hasMeshData;
    c->vertices = o.vertices;
    c->indices = o.indices;
    for (int i = 0; i < 16; i++) c->materials[i] = o.materials[i];
    c->numMaterials = o.numMaterials;
    c->hasAlpha = o.hasAlpha;
    for (auto& ch : o.children) c->children.push_back(CloneTree(*ch));
    return c;
}

std::map<WORD, std::unique_ptr<CD3DFileObject>> g_parsed;
}  // namespace

CD3DFile::CD3DFile() {}
CD3DFile::~CD3DFile() {}

HRESULT CD3DFile::LoadFromResource(WORD resId) {
    root_.reset();
    auto it = g_parsed.find(resId);
    if (it == g_parsed.end()) {
        const auto* data = Assets::RCData(resId);
        if (!data) return E_FAIL;
        XNode tree;
        if (!ParseXText(*data, tree)) {
            Log("D3DFile: resource %u is not a text .X file", resId);
            return E_FAIL;
        }
        std::map<std::string, const XNode*> globals;
        for (auto& c : tree.children)
            if (c.type == "Material" && !c.name.empty()) globals[c.name] = &c;
        auto root = std::make_unique<CD3DFileObject>("D3DFile_Root");
        CollectFrames(tree, root.get(), globals);
        it = g_parsed.emplace(resId, std::move(root)).first;
    }
    root_ = CloneTree(*it->second);
    return S_OK;
}

HRESULT CD3DFile::Render(Device* dev) {
    if (!root_) return S_OK;
    D3DMATERIAL7 mtrlSaved;
    D3DMATRIX matSaved;
    Texture* texSaved = nullptr;
    DWORD alphaState, srcBlend, destBlend;
    dev->GetMaterial(&mtrlSaved);
    dev->GetTexture(0, &texSaved);
    dev->GetTransform(D3DTRANSFORMSTATE_WORLD, &matSaved);
    dev->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &alphaState);
    dev->GetRenderState(D3DRENDERSTATE_SRCBLEND, &srcBlend);
    dev->GetRenderState(D3DRENDERSTATE_DESTBLEND, &destBlend);
    root_->Render(dev, false);
    dev->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    root_->Render(dev, true);
    dev->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, alphaState);
    dev->SetRenderState(D3DRENDERSTATE_SRCBLEND, srcBlend);
    dev->SetRenderState(D3DRENDERSTATE_DESTBLEND, destBlend);
    dev->SetTransform(D3DTRANSFORMSTATE_WORLD, &matSaved);
    dev->SetTexture(0, texSaved);
    dev->SetMaterial(&mtrlSaved);
    return S_OK;
}

void CD3DFile::Scale(float s) {
    ForEachMesh([&](CD3DFileObject& m) {
        for (auto& v : m.vertices) { v.x *= s; v.y *= s; v.z *= s; }
    });
}

void CD3DFile::Shift(const D3DVECTOR& d) {
    ForEachMesh([&](CD3DFileObject& m) {
        for (auto& v : m.vertices) { v.x += d.x; v.y += d.y; v.z += d.z; }
    });
}

static void RotateMeshes(CD3DFile* f, const D3DMATRIX& mat) {
    f->ForEachMesh([&](CD3DFileObject& m) {
        for (auto& v : m.vertices) {
            D3DVERTEX out = v;
            D3DMath_VertexMatrixMultiply(out, v, mat);
            v = out;
        }
    });
}

void CD3DFile::RotateX(float a) { D3DMATRIX m; D3DUtil_SetRotateXMatrix(m, a); RotateMeshes(this, m); }
void CD3DFile::RotateY(float a) { D3DMATRIX m; D3DUtil_SetRotateYMatrix(m, a); RotateMeshes(this, m); }
void CD3DFile::RotateZ(float a) { D3DMATRIX m; D3DUtil_SetRotateZMatrix(m, a); RotateMeshes(this, m); }

void CD3DFile::GetBoxMin(D3DVECTOR& out) {
    out = D3DVECTOR(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f);
    ForEachMesh([&](CD3DFileObject& m) {
        for (auto& v : m.vertices) {
            if (v.x < out.x) out.x = v.x;
            if (v.y < out.y) out.y = v.y;
            if (v.z < out.z) out.z = v.z;
        }
    });
}

void CD3DFile::GetBoxMax(D3DVECTOR& out) {
    // The original initialises the maximum with FLT_MIN (smallest positive), not -FLT_MAX.
    out = D3DVECTOR(1.1754944e-38f, 1.1754944e-38f, 1.1754944e-38f);
    ForEachMesh([&](CD3DFileObject& m) {
        for (auto& v : m.vertices) {
            if (v.x > out.x) out.x = v.x;
            if (v.y > out.y) out.y = v.y;
            if (v.z > out.z) out.z = v.z;
        }
    });
}
