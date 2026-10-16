#include "d3dfile.h"
#include <SDL3/SDL.h>
#include "d3dutil.h"
#include "../core/assets.h"
#include "../core/log.h"
#include "../core/mods.h"
#include "../../third_party/cgltf.h"
#include <cmath>
#include <cstring>
#include <functional>
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
            if (materials[i].tex) {
                // a mod model's texture, modulated by the lit material colour
                static const D3DTEXTURESTAGESTATETYPE kStates[6] = {D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2,
                                                                   D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2};
                static const DWORD kValues[6] = {D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
                                                 D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE};
                DWORD saved[6];
                Texture* texSaved = nullptr;
                dev->GetTexture(0, &texSaved);
                for (int k = 0; k < 6; k++) {
                    dev->GetTextureStageState(0, kStates[k], &saved[k]);
                    dev->SetTextureStageState(0, kStates[k], kValues[k]);
                }
                dev->SetTexture(0, materials[i].tex.get());
                dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_VERTEX, vertices.data(), (DWORD)vertices.size(),
                                          indices.data() + offset, n, 0);
                dev->SetTexture(0, texSaved);
                for (int k = 0; k < 6; k++) dev->SetTextureStageState(0, kStates[k], saved[k]);
            } else {
                dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_VERTEX, vertices.data(), (DWORD)vertices.size(),
                                          indices.data() + offset, n, 0);
            }
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

// ---- Native (mods): replacement models ------------------------------------------------------
// A replacement is flattened into plain meshes with every frame transform applied, because the
// game moves, scales and rotates models by editing their vertices directly (like the originals,
// which have no frame transforms that matter).

void FlattenInto(const CD3DFileObject& o, const D3DMATRIX& parent, CD3DFileObject* out) {
    D3DMATRIX m = o.mat * parent;
    if (o.hasMeshData) {
        auto c = CloneTree(o);
        c->children.clear();
        D3DUtil_SetIdentityMatrix(c->mat);
        for (auto& v : c->vertices) {
            D3DVERTEX t = v;
            D3DMath_VertexMatrixMultiply(t, v, m);
            v = t;
            float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (len > 1e-6f) { v.nx /= len; v.ny /= len; v.nz /= len; }
        }
        out->children.push_back(std::move(c));
    }
    for (auto& ch : o.children) FlattenInto(*ch, o.hasMeshData ? parent : m, out);
}

std::unique_ptr<CD3DFileObject> Flatten(const CD3DFileObject& root) {
    auto out = std::make_unique<CD3DFileObject>("D3DFile_Root");
    D3DMATRIX id;
    D3DUtil_SetIdentityMatrix(id);
    for (auto& c : root.children) FlattenInto(*c, id, out.get());
    return out;
}

bool MeshBox(const CD3DFileObject& root, D3DVECTOR& mn, D3DVECTOR& mx) {
    bool any = false;
    std::function<void(const CD3DFileObject&)> visit = [&](const CD3DFileObject& o) {
        if (o.hasMeshData)
            for (auto& v : o.vertices) {
                if (!any) { mn = mx = D3DVECTOR(v.x, v.y, v.z); any = true; }
                mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
                mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
            }
        for (auto& c : o.children) visit(*c);
    };
    visit(root);
    return any;
}

// Scales and moves a model to fill the same box as the original: the same largest size, centred
// on the same point, standing on the same floor.
void FitTo(CD3DFileObject& root, const CD3DFileObject& original) {
    D3DVECTOR a0, a1, b0, b1;
    if (!MeshBox(original, a0, a1) || !MeshBox(root, b0, b1)) return;
    float sa = std::max({a1.x - a0.x, a1.y - a0.y, a1.z - a0.z});
    float sb = std::max({b1.x - b0.x, b1.y - b0.y, b1.z - b0.z});
    if (sb <= 1e-6f) return;
    float k = sa / sb;
    float cx = (b0.x + b1.x) * 0.5f, cz = (b0.z + b1.z) * 0.5f;
    float tx = (a0.x + a1.x) * 0.5f, tz = (a0.z + a1.z) * 0.5f;
    std::function<void(CD3DFileObject&)> visit = [&](CD3DFileObject& o) {
        if (o.hasMeshData)
            for (auto& v : o.vertices) {
                v.x = (v.x - cx) * k + tx;
                v.y = (v.y - b0.y) * k + a0.y;
                v.z = (v.z - cz) * k + tz;
            }
        for (auto& c : o.children) visit(*c);
    };
    visit(root);
}

std::unique_ptr<CD3DFileObject> ParseXRoot(const std::vector<uint8_t>& data) {
    XNode tree;
    if (!ParseXText(data, tree)) return nullptr;
    std::map<std::string, const XNode*> globals;
    for (auto& c : tree.children)
        if (c.type == "Material" && !c.name.empty()) globals[c.name] = &c;
    auto root = std::make_unique<CD3DFileObject>("D3DFile_Root");
    CollectFrames(tree, root.get(), globals);
    return root;
}

std::shared_ptr<Texture> GltfTexture(const cgltf_texture* t, const std::string& baseDir, bool* hasAlpha) {
    if (!t || !t->image) return nullptr;
    const cgltf_image* img = t->image;
    Image im;
    bool ok = false;
    if (img->buffer_view && img->buffer_view->buffer && img->buffer_view->buffer->data) {
        const uint8_t* p = (const uint8_t*)img->buffer_view->buffer->data + img->buffer_view->offset;
        ok = Assets::DecodeImageMemory(p, img->buffer_view->size, &im);
    } else if (img->uri) {
        std::string uri = img->uri;
        if (uri.rfind("data:", 0) == 0) {
            size_t comma = uri.find(";base64,");
            if (comma != std::string::npos) {
                std::string b64 = uri.substr(comma + 8);
                size_t size = b64.size() / 4 * 3;
                while (!b64.empty() && b64.back() == '=') { b64.pop_back(); size--; }
                void* data = nullptr;
                cgltf_options opt{};
                if (cgltf_load_buffer_base64(&opt, size, b64.c_str(), &data) == cgltf_result_success) {
                    ok = Assets::DecodeImageMemory((const uint8_t*)data, size, &im);
                    free(data);
                }
            }
        } else {
            std::vector<char> path(uri.begin(), uri.end());
            path.push_back(0);
            cgltf_decode_uri(path.data());
            ok = Assets::DecodeImageFile(baseDir + path.data(), &im);
        }
    }
    if (!ok) return nullptr;
    if (im.hasAlpha) *hasAlpha = true;
    return std::shared_ptr<Texture>(Texture::Create(im, false, 0, true));
}

// glTF 2.0 (.glb / .gltf): every triangle primitive of every node in the default scene becomes a
// mesh, with the node transforms applied. glTF is right-handed and Direct3D left-handed, so Z is
// flipped and the triangle order reversed (the asset export does the reverse, so a model
// exported, edited and put back keeps its exact place and size).
// File access for cgltf through SDL, which takes UTF-8 paths on every system (cgltf's own fopen
// can't open non-ASCII paths on Windows).
cgltf_result GltfRead(const cgltf_memory_options*, const cgltf_file_options*, const char* path, cgltf_size* size,
                      void** data) {
    size_t n = 0;
    void* d = SDL_LoadFile(path, &n);
    if (!d) return cgltf_result_file_not_found;
    *size = n;
    *data = d;
    return cgltf_result_success;
}
void GltfRelease(const cgltf_memory_options*, const cgltf_file_options*, void* data) { SDL_free(data); }

std::unique_ptr<CD3DFileObject> LoadGltf(const std::string& path) {
    cgltf_options opt{};
    opt.file.read = GltfRead;
    opt.file.release = GltfRelease;
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &data) != cgltf_result_success) {
        Log("Mods: %s is not a valid glTF file", path.c_str());
        return nullptr;
    }
    if (cgltf_load_buffers(&opt, data, path.c_str()) != cgltf_result_success) {
        Log("Mods: %s: could not load its buffers", path.c_str());
        cgltf_free(data);
        return nullptr;
    }
    std::string baseDir = path.substr(0, path.find_last_of("/\\") + 1);
    auto root = std::make_unique<CD3DFileObject>("D3DFile_Root");
    int skipped = 0;
    std::function<void(const cgltf_node*)> visit = [&](const cgltf_node* node) {
        if (node->mesh) {
            float m[16];
            cgltf_node_transform_world(node, m);
            float nm[9] = {m[0], m[1], m[2], m[4], m[5], m[6], m[8], m[9], m[10]};
            for (cgltf_size pi = 0; pi < node->mesh->primitives_count; pi++) {
                const cgltf_primitive& prim = node->mesh->primitives[pi];
                if (prim.type != cgltf_primitive_type_triangles) { skipped++; continue; }
                const cgltf_accessor *pos = nullptr, *nrm = nullptr, *uv = nullptr;
                for (cgltf_size a = 0; a < prim.attributes_count; a++) {
                    const cgltf_attribute& at = prim.attributes[a];
                    if (at.type == cgltf_attribute_type_position) pos = at.data;
                    else if (at.type == cgltf_attribute_type_normal) nrm = at.data;
                    else if (at.type == cgltf_attribute_type_texcoord && at.index == 0) uv = at.data;
                }
                if (!pos || pos->count == 0) continue;
                if (pos->count > 65535) {
                    Log("Mods: %s: a mesh has more than 65535 vertices; skipped", path.c_str());
                    skipped++;
                    continue;
                }
                auto mesh = std::make_unique<CD3DFileObject>(node->name ? node->name : "mesh");
                mesh->vertices.resize(pos->count);
                for (cgltf_size i = 0; i < pos->count; i++) {
                    float p[3] = {0, 0, 0}, n[3] = {0, 0, 1}, t[2] = {0, 0};
                    cgltf_accessor_read_float(pos, i, p, 3);
                    if (nrm) cgltf_accessor_read_float(nrm, i, n, 3);
                    if (uv) cgltf_accessor_read_float(uv, i, t, 2);
                    float wx = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
                    float wy = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
                    float wz = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
                    float nx = nm[0] * n[0] + nm[3] * n[1] + nm[6] * n[2];
                    float ny = nm[1] * n[0] + nm[4] * n[1] + nm[7] * n[2];
                    float nz = nm[2] * n[0] + nm[5] * n[1] + nm[8] * n[2];
                    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }
                    D3DVERTEX& v = mesh->vertices[i];
                    memset(&v, 0, sizeof v);
                    v.x = wx; v.y = wy; v.z = -wz;
                    v.nx = nx; v.ny = ny; v.nz = -nz;
                    v.tu = t[0]; v.tv = t[1];
                }
                cgltf_size ni = prim.indices ? prim.indices->count : pos->count;
                ni -= ni % 3;
                mesh->indices.resize(ni);
                for (cgltf_size i = 0; i < ni; i += 3) {
                    cgltf_size a = prim.indices ? cgltf_accessor_read_index(prim.indices, i) : i;
                    cgltf_size b = prim.indices ? cgltf_accessor_read_index(prim.indices, i + 1) : i + 1;
                    cgltf_size c = prim.indices ? cgltf_accessor_read_index(prim.indices, i + 2) : i + 2;
                    mesh->indices[i] = (WORD)a;      // reversed winding for the flipped Z
                    mesh->indices[i + 1] = (WORD)c;
                    mesh->indices[i + 2] = (WORD)b;
                }
                mesh->hasMeshData = true;
                mesh->numMaterials = 1;
                MeshMaterialData& md = mesh->materials[0];
                md.numIndices = (DWORD)ni;
                float col[4] = {1, 1, 1, 1}, em[3] = {0, 0, 0};
                if (prim.material) {
                    const cgltf_material* mat = prim.material;
                    if (mat->has_pbr_metallic_roughness) {
                        memcpy(col, mat->pbr_metallic_roughness.base_color_factor, sizeof col);
                        md.tex = GltfTexture(mat->pbr_metallic_roughness.base_color_texture.texture, baseDir,
                                             &mesh->hasAlpha);
                    }
                    memcpy(em, mat->emissive_factor, sizeof em);
                    if (mat->alpha_mode == cgltf_alpha_mode_blend) mesh->hasAlpha = true;
                }
                if (col[3] < 1.f) mesh->hasAlpha = true;
                memset(&md.mtrl, 0, sizeof md.mtrl);
                md.mtrl.diffuse = {col[0], col[1], col[2], col[3]};
                md.mtrl.ambient = md.mtrl.diffuse;
                md.mtrl.emissive = {em[0], em[1], em[2], 0.f};
                if (!nrm) ComputeNormals(mesh.get());
                root->children.push_back(std::move(mesh));
            }
        }
        for (cgltf_size c = 0; c < node->children_count; c++) visit(node->children[c]);
    };
    const cgltf_scene* scene = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
    if (scene) {
        for (cgltf_size i = 0; i < scene->nodes_count; i++) visit(scene->nodes[i]);
    } else {
        for (cgltf_size i = 0; i < data->nodes_count; i++)
            if (!data->nodes[i].parent) visit(&data->nodes[i]);
    }
    cgltf_free(data);
    if (skipped) Log("Mods: %s: %d part(s) skipped (only triangle meshes are used)", path.c_str(), skipped);
    if (root->children.empty()) {
        Log("Mods: %s has no triangle meshes", path.c_str());
        return nullptr;
    }
    return root;
}

std::unique_ptr<CD3DFileObject> LoadOriginal(WORD resId) {
    const auto* data = Assets::RCData(resId);
    if (!data) return nullptr;
    auto root = ParseXRoot(*data);
    if (!root) Log("D3DFile: resource %u is not a text .X file", resId);
    return root;
}

// A mod's replacement for model `resId`, or nullptr.
std::unique_ptr<CD3DFileObject> LoadReplacement(WORD resId) {
    const char* name = Mods::ModelName(resId);
    const Mods::ModInfo* from = nullptr;
    std::string path = Mods::FindFile("models", name, {"glb", "gltf", "x"}, &from);
    if (path.empty()) return nullptr;
    std::string ext = path.substr(path.find_last_of('.') + 1);
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    std::unique_ptr<CD3DFileObject> root;
    if (ext == "x") {
        std::vector<uint8_t> bytes;
        if (Assets::ReadFile(path, &bytes)) root = ParseXRoot(bytes);
        if (!root) Log("Mods: %s is not a text .x file (binary .x isn't supported)", path.c_str());
    } else {
        root = LoadGltf(path);
    }
    if (!root) {
        Log("Mods: using the original %s model", name);
        return nullptr;
    }
    root = Flatten(*root);
    std::string fit = from ? from->ini.Get("models", name) : "";
    for (char& c : fit) c = (char)tolower((unsigned char)c);
    if (fit == "fit" || fit == "original") {
        if (auto orig = LoadOriginal(resId)) FitTo(*root, *orig);
    }
    Log("Mods: model %s from %s%s", name, from ? from->id.c_str() : "?", (fit == "fit" || fit == "original") ? " (fitted)" : "");
    return root;
}

// ---- Native (asset export): an original model as .glb ------------------------------------
void Put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; i++) b.push_back((uint8_t)(v >> (8 * i)));
}
void PutF(std::vector<uint8_t>& b, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    Put32(b, v);
}
}  // namespace

void CD3DFile::ClearCache() { g_parsed.clear(); }

bool CD3DFile::ExportOriginalGlb(WORD resId, std::vector<uint8_t>* out) {
    auto root = LoadOriginal(resId);
    if (!root) return false;
    root = Flatten(*root);
    std::vector<uint8_t> bin;
    std::string meshes, accessors, views, materials;
    int nAcc = 0, nView = 0, nMat = 0, nMesh = 0;
    std::string nodes;
    auto addView = [&](size_t offset, size_t length, int target) {
        if (nView) views += ",";
        views += "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(length) +
                 ",\"target\":" + std::to_string(target) + "}";
        return nView++;
    };
    auto num = [](float f) {
        char b[32];
        snprintf(b, sizeof b, "%.6g", std::isfinite(f) ? f : 0.f);
        return std::string(b);
    };
    for (auto& mo : root->children) {
        const CD3DFileObject& m = *mo;
        if (!m.hasMeshData || m.vertices.empty()) continue;
        // vertices: position, normal, uv (Z flipped back to glTF's right-handed axes)
        size_t vOff = bin.size();
        float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
        for (auto& v : m.vertices) {
            float p[3] = {v.x, v.y, -v.z};
            for (int k = 0; k < 3; k++) { mn[k] = std::min(mn[k], p[k]); mx[k] = std::max(mx[k], p[k]); PutF(bin, p[k]); }
        }
        size_t nOff = bin.size();
        for (auto& v : m.vertices) { PutF(bin, v.nx); PutF(bin, v.ny); PutF(bin, -v.nz); }
        size_t tOff = bin.size();
        for (auto& v : m.vertices) { PutF(bin, v.tu); PutF(bin, v.tv); }
        int vc = (int)m.vertices.size();
        int pv = addView(vOff, (size_t)vc * 12, 34962), nv = addView(nOff, (size_t)vc * 12, 34962),
            tv = addView(tOff, (size_t)vc * 8, 34962);
        auto acc = [&](int view, int count, const char* type, int comp, const std::string& extra) {
            if (nAcc) accessors += ",";
            accessors += "{\"bufferView\":" + std::to_string(view) + ",\"componentType\":" + std::to_string(comp) +
                         ",\"count\":" + std::to_string(count) + ",\"type\":\"" + type + "\"" + extra + "}";
            return nAcc++;
        };
        int pa = acc(pv, vc, "VEC3", 5126,
                     ",\"min\":[" + num(mn[0]) + "," + num(mn[1]) + "," + num(mn[2]) + "],\"max\":[" + num(mx[0]) + "," +
                         num(mx[1]) + "," + num(mx[2]) + "]");
        int na = acc(nv, vc, "VEC3", 5126, ""), ta = acc(tv, vc, "VEC2", 5126, "");
        std::string prims;
        size_t offset = 0;
        for (DWORD i = 0; i <= m.numMaterials && i < 16; i++) {
            DWORD n = m.materials[i].numIndices;
            if (!n) continue;
            size_t iOff = bin.size();
            for (DWORD k = 0; k + 2 < n; k += 3) {  // reversed winding for glTF
                WORD a = m.indices[offset + k], b = m.indices[offset + k + 1], c = m.indices[offset + k + 2];
                for (WORD w : {a, c, b}) { bin.push_back((uint8_t)w); bin.push_back((uint8_t)(w >> 8)); }
            }
            while (bin.size() % 4) bin.push_back(0);
            int iv = addView(iOff, (size_t)n * 2, 34963);
            int ia = acc(iv, (int)(n - n % 3), "SCALAR", 5123, "");
            const D3DMATERIAL7& mt = m.materials[i].mtrl;
            if (nMat) materials += ",";
            materials += "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[" + num(mt.diffuse.r) + "," + num(mt.diffuse.g) +
                         "," + num(mt.diffuse.b) + "," + num(mt.diffuse.a) +
                         "],\"metallicFactor\":0,\"roughnessFactor\":1},\"emissiveFactor\":[" + num(mt.emissive.r) + "," +
                         num(mt.emissive.g) + "," + num(mt.emissive.b) + "]" +
                         (mt.diffuse.a < 1.f ? ",\"alphaMode\":\"BLEND\"" : "") + "}";
            if (!prims.empty()) prims += ",";
            prims += "{\"attributes\":{\"POSITION\":" + std::to_string(pa) + ",\"NORMAL\":" + std::to_string(na) +
                     ",\"TEXCOORD_0\":" + std::to_string(ta) + "},\"indices\":" + std::to_string(ia) +
                     ",\"material\":" + std::to_string(nMat++) + "}";
            offset += n;
        }
        if (prims.empty()) continue;
        if (nMesh) { meshes += ","; nodes += ","; }
        meshes += "{\"primitives\":[" + prims + "]}";
        nodes += "{\"mesh\":" + std::to_string(nMesh) + "}";
        nMesh++;
    }
    if (!nMesh) return false;
    std::string sceneNodes;
    for (int i = 0; i < nMesh; i++) sceneNodes += (i ? "," : "") + std::to_string(i);
    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Hopmon Native asset export\"},\"scene\":0,"
                       "\"scenes\":[{\"nodes\":[" + sceneNodes + "]}],\"nodes\":[" + nodes + "],\"meshes\":[" + meshes +
                       "],\"materials\":[" + materials + "],\"accessors\":[" + accessors + "],\"bufferViews\":[" + views +
                       "],\"buffers\":[{\"byteLength\":" + std::to_string(bin.size()) + "}]}";
    while (json.size() % 4) json += ' ';
    while (bin.size() % 4) bin.push_back(0);
    out->clear();
    Put32(*out, 0x46546C67);  // "glTF"
    Put32(*out, 2);
    Put32(*out, (uint32_t)(12 + 8 + json.size() + 8 + bin.size()));
    Put32(*out, (uint32_t)json.size());
    Put32(*out, 0x4E4F534A);  // JSON
    out->insert(out->end(), json.begin(), json.end());
    Put32(*out, (uint32_t)bin.size());
    Put32(*out, 0x004E4942);  // BIN
    out->insert(out->end(), bin.begin(), bin.end());
    return true;
}

namespace {
}  // namespace

CD3DFile::CD3DFile() {}
CD3DFile::~CD3DFile() {}

HRESULT CD3DFile::LoadFromResource(WORD resId) {
    root_.reset();
    auto it = g_parsed.find(resId);
    if (it == g_parsed.end()) {
        // native: a mod's replacement (glTF or .x) comes first
        auto root = LoadReplacement(resId);
        if (!root) root = LoadOriginal(resId);
        if (!root) return E_FAIL;
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
