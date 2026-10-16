#include <cstdlib>
#include "device.h"
#include "../core/config.h"
#include "../core/log.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>

static Device g_device;
Device& GetDevice() { return g_device; }

// ---------------------------------------------------------------------------
// Shaders: D3D7 fixed-function subset
// ---------------------------------------------------------------------------
static const char* kVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aColor;
layout(location=4) in vec2 aUV;
uniform mat4 uWorld, uView, uProj;
uniform bool uLighting, uLightOn, uHasVertexColor, uScreen2D;
uniform vec4 uMatDiffuse, uMatAmbient, uMatEmissive, uAmbient, uLightDiffuse;
uniform vec3 uLightDir;
uniform vec4 uViewport2D;
out vec4 vColor;
flat out vec4 vColorFlat;
out vec2 vUV;
out vec3 vNormal;
vec4 d3dLight(vec3 n) {
    vec3 c = uMatEmissive.rgb + uAmbient.rgb * uMatAmbient.rgb;
    if (uLightOn) c += uLightDiffuse.rgb * uMatDiffuse.rgb * max(dot(n, uLightDir), 0.0);
    return vec4(clamp(c, 0.0, 1.0), clamp(uMatDiffuse.a, 0.0, 1.0));
}
void main() {
    if (uScreen2D) {
        gl_Position = vec4(aPos.x * uViewport2D.x + uViewport2D.z, aPos.y * uViewport2D.y + uViewport2D.w, 0.0, 1.0);
        vColor = aColor; vColorFlat = aColor; vUV = aUV; vNormal = vec3(0.0);
        return;
    }
    vec4 wp = uWorld * vec4(aPos, 1.0);
    vec4 cp = uProj * (uView * wp);
    cp.z = 2.0 * cp.z - cp.w;          // D3D clip z [0,w] -> GL [-w,w]
    gl_Position = cp;
    vec3 n = mat3(uWorld) * aNormal;   // D3D7 default: normals are not renormalized
    vNormal = n;
    vec4 col = uLighting ? d3dLight(n) : (uHasVertexColor ? aColor : vec4(1.0));
    vColor = col; vColorFlat = col; vUV = aUV;
}
)GLSL";

static const char* kFS = R"GLSL(
#version 330 core
in vec4 vColor;
flat in vec4 vColorFlat;
in vec2 vUV;
in vec3 vNormal;
uniform sampler2D uTex;
uniform bool uTexOn, uFlat, uAlphaTest, uPerPixel, uLighting, uLightOn;
uniform int uColorOp, uColorArg1, uColorArg2, uAlphaOp, uAlphaArg1, uAlphaArg2;
uniform vec4 uConstColor;
uniform vec4 uMatDiffuse, uMatAmbient, uMatEmissive, uAmbient, uLightDiffuse;
uniform vec3 uLightDir;
// Native (mods): blend modes that ignore alpha - additive glows (ONE, ONE) and darkening
// shadows (ZERO, SRCCOLOR) - get a mod image's own transparency applied to its colour instead:
// 1 = fade to black (adds nothing), 2 = fade to white (darkens nothing). 0 = off (originals).
uniform int uAlphaFix;
out vec4 fragColor;
vec4 pick(int a, vec4 diff, vec4 tex) { return a == 2 ? tex : diff; }
vec4 op(int o, vec4 a1, vec4 a2) {
    if (o == 2) return a1;               // SELECTARG1
    if (o == 3) return a2;               // SELECTARG2
    if (o == 4) return a1 * a2;          // MODULATE
    if (o == 5) return clamp(a1 * a2 * 2.0, 0.0, 1.0);
    if (o == 6) return clamp(a1 * a2 * 4.0, 0.0, 1.0);
    if (o == 7) return clamp(a1 + a2, 0.0, 1.0);
    return a2;
}
void main() {
    vec4 diff = uFlat ? vColorFlat : vColor;
    if (uPerPixel && uLighting && !uFlat) {
        vec3 n = normalize(vNormal);
        vec3 c = uMatEmissive.rgb + uAmbient.rgb * uMatAmbient.rgb;
        if (uLightOn) c += uLightDiffuse.rgb * uMatDiffuse.rgb * max(dot(n, uLightDir), 0.0);
        diff = vec4(clamp(c, 0.0, 1.0), clamp(uMatDiffuse.a, 0.0, 1.0));
    }
    vec4 result = diff;
    if (uTexOn && uColorOp != 1) {
        vec4 t = texture(uTex, vUV);
        result.rgb = op(uColorOp, pick(uColorArg1, diff, t), pick(uColorArg2, diff, t)).rgb;
        result.a = uAlphaOp == 1 ? diff.a : op(uAlphaOp, pick(uAlphaArg1, diff, t), pick(uAlphaArg2, diff, t)).a;
    }
    if (uAlphaTest && result.a < 0.5) discard;
    if (uAlphaFix != 0 && uTexOn) {
        float ta = texture(uTex, vUV).a;
        result.rgb = uAlphaFix == 1 ? result.rgb * ta : mix(vec3(1.0), result.rgb, ta);
    }
    fragColor = result * uConstColor;
}
)GLSL";

static const char* kPresentVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=4) in vec2 aUV;
out vec2 vUV;
void main() { gl_Position = vec4(aPos.xy, 0.0, 1.0); vUV = aUV; }
)GLSL";
static const char* kPresentFS = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform float uGamma;  // 1.0 = unchanged; brightness setting as a gamma curve
out vec4 fragColor;
void main() { fragColor = vec4(pow(texture(uTex, vUV).rgb, vec3(uGamma)), 1.0); }
)GLSL";

// ---------------------------------------------------------------------------
// Texture / Surface
// ---------------------------------------------------------------------------
Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}

Texture* Texture::Create(const Image& img, bool colorKey, uint32_t keyRGB, bool mipmaps) {
    Texture* t = new Texture();
    t->mips_ = mipmaps;
    glGenTextures(1, &t->id_);
    t->Update(img, colorKey, keyRGB);
    return t;
}

void Texture::Update(const Image& img, bool colorKey, uint32_t keyRGB) {
    width_ = img.width;
    height_ = img.height;
    softAlpha_ = img.hasAlpha;
    std::vector<uint32_t> px(img.pixels.size());
    for (size_t i = 0; i < px.size(); i++) {
        uint32_t c = img.pixels[i];
        uint32_t rgb = c & 0xFFFFFF;
        // a mod's PNG with transparency uses its own alpha; everything else the colour key
        uint32_t a = img.hasAlpha ? (c >> 24) : ((colorKey && rgb == keyRGB) ? 0 : 0xFF);
        // store as RGBA bytes
        uint8_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        px[i] = (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | a << 24;
    }
    if (colorKey || img.hasAlpha) {
        // Transparent pixels keep the key colour (e.g. black), which smooth filtering and mipmaps
        // would blend into the visible edge as a dark fringe. Give them the colour of their visible
        // neighbours instead (alpha stays 0, so point-sampled output is unchanged).
        int w = img.width, h = img.height;
        std::vector<uint8_t> filled(px.size());
        for (size_t i = 0; i < px.size(); i++) filled[i] = (px[i] >> 24) != 0;
        for (int pass = 0; pass < 4; pass++) {
            std::vector<uint32_t> next = px;
            std::vector<uint8_t> nextFilled = filled;
            bool any = false;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    size_t i = (size_t)y * w + x;
                    if (filled[i]) continue;
                    unsigned sr = 0, sg = 0, sb = 0, n = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = x + dx, ny = y + dy;
                            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                            size_t j = (size_t)ny * w + nx;
                            if (!filled[j]) continue;
                            sr += px[j] & 0xFF; sg += (px[j] >> 8) & 0xFF; sb += (px[j] >> 16) & 0xFF;
                            n++;
                        }
                    if (!n) continue;
                    next[i] = (sr / n) | (sg / n) << 8 | (sb / n) << 16;  // alpha 0
                    nextFilled[i] = 1;
                    any = true;
                }
            px.swap(next);
            filled.swap(nextFilled);
            if (!any) break;
        }
    }
    glBindTexture(GL_TEXTURE_2D, id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (mips_) glGenerateMipmap(GL_TEXTURE_2D);
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

Surface::Surface(const Image& img) : image_(img) {}
Surface::~Surface() { delete tex_; }

void Surface::SetColorKey(COLORREF key) {
    keyEnabled_ = true;
    if (key == CLR_INVALID) {
        keyRGB_ = image_.pixels.empty() ? 0 : (image_.pixels[0] & 0xFFFFFF);
    } else {
        // COLORREF is 0x00BBGGRR
        keyRGB_ = (key & 0xFF) << 16 | (key & 0xFF00) | ((key >> 16) & 0xFF);
    }
    dirty_ = true;
}

void Surface::ClearColorKey() {
    keyEnabled_ = false;
    dirty_ = true;
}

Texture* Surface::Tex() {
    if (!tex_) {
        tex_ = Texture::Create(image_, keyEnabled_, keyRGB_, false);
        dirty_ = false;
    } else if (dirty_) {
        tex_->Update(image_, keyEnabled_, keyRGB_);
        dirty_ = false;
    }
    return tex_;
}

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------
static void SetIdentity(D3DMATRIX& m) {
    m = D3DMATRIX();
    m._11 = m._22 = m._33 = m._44 = 1.f;
}

GLuint Device::CompileProgram(const char* vs, const char* fs) {
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[4096];
            glGetShaderInfoLog(s, sizeof log, nullptr, log);
            Log("Shader compile error:\n%s", log);
        }
        return s;
    };
    GLuint p = glCreateProgram();
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        Log("Program link error:\n%s", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

bool Device::Init(SDL_Window* window) {
    window_ = window;
    if (!hgl::Load()) return false;
    Log("OpenGL %s | %s", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));

    prog_ = CompileProgram(kVS, kFS);
    progPresent_ = CompileProgram(kPresentVS, kPresentFS);
#define U(n) u_.n = glGetUniformLocation(prog_, "u" #n[0] == 0 ? "" : nullptr)
    auto loc = [&](const char* n) { return glGetUniformLocation(prog_, n); };
    u_.world = loc("uWorld"); u_.view = loc("uView"); u_.proj = loc("uProj");
    u_.lighting = loc("uLighting"); u_.matDiffuse = loc("uMatDiffuse"); u_.matAmbient = loc("uMatAmbient");
    u_.matEmissive = loc("uMatEmissive"); u_.ambient = loc("uAmbient"); u_.lightOn = loc("uLightOn");
    u_.lightDir = loc("uLightDir"); u_.lightDiffuse = loc("uLightDiffuse"); u_.flat = loc("uFlat");
    u_.texOn = loc("uTexOn"); u_.colorOp = loc("uColorOp"); u_.colorArg1 = loc("uColorArg1");
    u_.colorArg2 = loc("uColorArg2"); u_.alphaOp = loc("uAlphaOp"); u_.alphaArg1 = loc("uAlphaArg1");
    u_.alphaArg2 = loc("uAlphaArg2"); u_.alphaTest = loc("uAlphaTest"); u_.alphaFix = loc("uAlphaFix"); u_.hasVertexColor = loc("uHasVertexColor");
    u_.perPixel = loc("uPerPixel"); u_.tex = loc("uTex"); u_.constColor = loc("uConstColor");
    u_.screen2D = loc("uScreen2D"); u_.viewport2D = loc("uViewport2D");
#undef U
    presentTexLoc_ = glGetUniformLocation(progPresent_, "uTex");
    presentGammaLoc_ = glGetUniformLocation(progPresent_, "uGamma");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
    if (glProvokingVertex) glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);  // D3D flat shading uses vertex 0
    if (hgl::HasExtension("GL_ARB_texture_filter_anisotropic") || hgl::HasExtension("GL_EXT_texture_filter_anisotropic"))
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso_);

    // D3D7 default state
    for (auto& v : rs_) v = 0;
    for (auto& v : tss_) v = 0;
    rs_[D3DRENDERSTATE_TEXTUREPERSPECTIVE] = TRUE;
    rs_[D3DRENDERSTATE_ZENABLE] = TRUE;
    rs_[D3DRENDERSTATE_SHADEMODE] = D3DSHADE_GOURAUD;
    rs_[D3DRENDERSTATE_ZWRITEENABLE] = TRUE;
    rs_[D3DRENDERSTATE_SRCBLEND] = D3DBLEND_ONE;
    rs_[D3DRENDERSTATE_DESTBLEND] = D3DBLEND_ZERO;
    rs_[D3DRENDERSTATE_CULLMODE] = D3DCULL_CCW;
    rs_[D3DRENDERSTATE_LIGHTING] = TRUE;
    rs_[D3DRENDERSTATE_AMBIENT] = 0;
    tss_[D3DTSS_COLOROP] = D3DTOP_MODULATE;
    tss_[D3DTSS_COLORARG1] = D3DTA_TEXTURE;
    tss_[D3DTSS_COLORARG2] = D3DTA_CURRENT;
    tss_[D3DTSS_ALPHAOP] = D3DTOP_SELECTARG1;
    tss_[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
    tss_[D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
    tss_[D3DTSS_MAGFILTER] = D3DTFG_POINT;
    tss_[D3DTSS_MINFILTER] = D3DTFG_POINT;
    tss_[D3DTSS_MIPFILTER] = D3DTFP_NONE;
    SetIdentity(world_);
    SetIdentity(view_);
    SetIdentity(proj_);
    OnSettingsChanged();
    return true;
}

void Device::Shutdown() {
    DestroyTargets();
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (prog_) glDeleteProgram(prog_);
    if (progPresent_) glDeleteProgram(progPresent_);
    vbo_ = ibo_ = vao_ = prog_ = progPresent_ = 0;
}

void Device::ComputeLayout() {
    const Config& cfg = GetConfig();
    int w = 640, h = 480;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    FrameLayout L;
    L.windowW = w;
    L.windowH = h;
    float A = (float)w / (float)h;
    if (cfg.aspect == AspectMode::Stretch) {
        L.destX = 0; L.destY = 0; L.destW = w; L.destH = h;
    } else if (cfg.aspect == AspectMode::Widescreen && A >= 4.f / 3.f) {
        L.destX = 0; L.destY = 0; L.destW = w; L.destH = h;
        L.virtualW = 480.f * A;
        L.offsetX = (L.virtualW - 640.f) * 0.5f;
    } else {
        // pillar/letterbox to 4:3
        if (A > 4.f / 3.f) {
            L.destH = h; L.destW = (int)(h * 4.f / 3.f + 0.5f);
        } else {
            L.destW = w; L.destH = (int)(w * 3.f / 4.f + 0.5f);
        }
        L.destX = (w - L.destW) / 2;
        L.destY = (h - L.destH) / 2;
    }
    float s = cfg.renderScale;
    L.fboW = std::max(1, (int)(L.destW * s + 0.5f));
    L.fboH = std::max(1, (int)(L.destH * s + 0.5f));
    GLint maxRb = 8192;
    L.fboW = std::min(L.fboW, (int)maxRb);
    L.fboH = std::min(L.fboH, (int)maxRb);
    bool changed = L.fboW != layout_.fboW || L.fboH != layout_.fboH || !sceneFbo_;
    layout_ = L;
    if (changed) CreateTargets();
}

void Device::DestroyTargets() {
    if (sceneFbo_) glDeleteFramebuffers(1, &sceneFbo_);
    if (sceneColorRb_) glDeleteRenderbuffers(1, &sceneColorRb_);
    if (sceneDepthRb_) glDeleteRenderbuffers(1, &sceneDepthRb_);
    if (resolveFbo_) glDeleteFramebuffers(1, &resolveFbo_);
    if (resolveTex_) glDeleteTextures(1, &resolveTex_);
    sceneFbo_ = sceneColorRb_ = sceneDepthRb_ = resolveFbo_ = resolveTex_ = 0;
}

void Device::CreateTargets() {
    DestroyTargets();
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    samples_ = std::min(GetConfig().msaa, (int)maxSamples);
    if (samples_ < 2) samples_ = 0;
    int W = layout_.fboW, H = layout_.fboH;
    glGenRenderbuffers(1, &sceneColorRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneColorRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA8, W, H);
    glGenRenderbuffers(1, &sceneDepthRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRb_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_DEPTH24_STENCIL8, W, H);
    glGenFramebuffers(1, &sceneFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sceneColorRb_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRb_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Log("Scene framebuffer incomplete");
    glGenTextures(1, &resolveTex_);
    glBindTexture(GL_TEXTURE_2D, resolveTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glGenFramebuffers(1, &resolveFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveTex_, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Log("Render target %dx%d, MSAA %dx", W, H, samples_);
}

void Device::OnSettingsChanged() {
    if (window_) {
        layout_.fboW = -1;
        ComputeLayout();
    }
}

void Device::BeginFrame() {
    hudTLRight_ = -1e9f;
    hudTLBottom_ = 0.f;
    ComputeLayout();
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glViewport(0, 0, layout_.fboW, layout_.fboH);
}

HRESULT Device::BeginScene() {
    inScene_ = true;
    return D3D_OK;
}

HRESULT Device::EndScene() {
    inScene_ = false;
    return D3D_OK;
}

void Device::Present() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
    glBlitFramebuffer(0, 0, layout_.fboW, layout_.fboH, 0, 0, layout_.fboW, layout_.fboH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, layout_.windowW, layout_.windowH);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(layout_.destX, layout_.windowH - layout_.destY - layout_.destH, layout_.destW, layout_.destH);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(progPresent_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resolveTex_);
    glUniform1i(presentTexLoc_, 0);
    // brightness 150% -> exponent 0.67 (lighter mid-tones), 50% -> 2.0 (darker); black and white stay put
    glUniform1f(presentGammaLoc_, 100.f / (float)std::clamp(GetConfig().brightness, 50, 150));
    struct V { float x, y, z, u, v; } q[4] = {{-1, -1, 0, 0, 0}, {1, -1, 0, 1, 0}, {-1, 1, 0, 0, 1}, {1, 1, 0, 1, 1}};
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof q, q, GL_STREAM_DRAW);
    for (int i = 0; i < 6; i++) glDisableVertexAttribArray(i);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)12);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

bool Device::SaveScreenshot(const char* path) {
    int W = layout_.fboW, H = layout_.fboH;
    std::vector<uint8_t> px((size_t)W * H * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFbo_);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    FILE* f = OpenFileUtf8(path, "wb");
    if (!f) return false;
    int stride = (W * 3 + 3) & ~3;
    uint32_t fileSize = 54 + stride * H;
    uint8_t hdr[54] = {'B', 'M'};
    auto w32 = [&](int o, uint32_t v) { hdr[o] = v; hdr[o + 1] = v >> 8; hdr[o + 2] = v >> 16; hdr[o + 3] = v >> 24; };
    w32(2, fileSize); w32(10, 54); w32(14, 40); w32(18, W); w32(22, H);
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    std::vector<uint8_t> row(stride, 0);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            const uint8_t* p = &px[((size_t)y * W + x) * 4];
            row[x * 3] = p[2]; row[x * 3 + 1] = p[1]; row[x * 3 + 2] = p[0];
        }
        fwrite(row.data(), 1, stride, f);
    }
    fclose(f);
    return true;
}

HRESULT Device::Clear(DWORD, const RECT*, DWORD flags, D3DCOLOR color, float z, DWORD) {
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glViewport(0, 0, layout_.fboW, layout_.fboH);
    glDisable(GL_SCISSOR_TEST);
    GLbitfield mask = 0;
    if (flags & D3DCLEAR_TARGET) {
        glColorMask(1, 1, 1, 1);
        glClearColor(((color >> 16) & 0xFF) / 255.f, ((color >> 8) & 0xFF) / 255.f, (color & 0xFF) / 255.f, 1.f);
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (flags & D3DCLEAR_ZBUFFER) {
        glDepthMask(GL_TRUE);
        glClearDepth(z);
        mask |= GL_DEPTH_BUFFER_BIT;
    }
    glClear(mask);
    return D3D_OK;
}

HRESULT Device::SetRenderState(D3DRENDERSTATETYPE s, DWORD v) {
    if ((unsigned)s < D3DRENDERSTATE_MAX) rs_[s] = v;
    return D3D_OK;
}
HRESULT Device::GetRenderState(D3DRENDERSTATETYPE s, DWORD* v) {
    if ((unsigned)s < D3DRENDERSTATE_MAX && v) *v = rs_[s];
    return D3D_OK;
}
HRESULT Device::SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE s, DWORD v) {
    if (stage == 0 && (unsigned)s < D3DTSS_MAX) tss_[s] = v;
    return D3D_OK;
}
HRESULT Device::GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE s, DWORD* v) {
    if (stage == 0 && (unsigned)s < D3DTSS_MAX && v) *v = tss_[s];
    return D3D_OK;
}
HRESULT Device::SetTransform(D3DTRANSFORMSTATETYPE t, const D3DMATRIX* m) {
    if (!m) return E_FAIL;
    if (t == D3DTRANSFORMSTATE_WORLD) world_ = *m;
    else if (t == D3DTRANSFORMSTATE_VIEW) view_ = *m;
    else if (t == D3DTRANSFORMSTATE_PROJECTION) proj_ = *m;
    return D3D_OK;
}
HRESULT Device::GetTransform(D3DTRANSFORMSTATETYPE t, D3DMATRIX* m) {
    if (!m) return E_FAIL;
    if (t == D3DTRANSFORMSTATE_WORLD) *m = world_;
    else if (t == D3DTRANSFORMSTATE_VIEW) *m = view_;
    else if (t == D3DTRANSFORMSTATE_PROJECTION) *m = proj_;
    return D3D_OK;
}
HRESULT Device::SetMaterial(const D3DMATERIAL7* m) {
    if (m) material_ = *m;
    return D3D_OK;
}
HRESULT Device::GetMaterial(D3DMATERIAL7* m) {
    if (m) *m = material_;
    return D3D_OK;
}
HRESULT Device::SetLight(DWORD index, const D3DLIGHT7* l) {
    if (index == 0 && l) light_ = *l;
    return D3D_OK;
}
HRESULT Device::LightEnable(DWORD index, BOOL enable) {
    if (index == 0) lightEnabled_ = enable != 0;
    return D3D_OK;
}
HRESULT Device::SetTexture(DWORD stage, Texture* t) {
    if (stage == 0) texture_ = t;
    return D3D_OK;
}
HRESULT Device::GetTexture(DWORD stage, Texture** t) {
    if (t) *t = stage == 0 ? texture_ : nullptr;
    return D3D_OK;
}

static GLenum MapBlend(DWORD b) {
    switch (b) {
        case D3DBLEND_ZERO: return GL_ZERO;
        case D3DBLEND_ONE: return GL_ONE;
        case D3DBLEND_SRCCOLOR: return GL_SRC_COLOR;
        case D3DBLEND_INVSRCCOLOR: return GL_ONE_MINUS_SRC_COLOR;
        case D3DBLEND_SRCALPHA: return GL_SRC_ALPHA;
        case D3DBLEND_INVSRCALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case D3DBLEND_DESTALPHA: return GL_DST_ALPHA;
        case D3DBLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case D3DBLEND_DESTCOLOR: return GL_DST_COLOR;
        case D3DBLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
    }
    return GL_ONE;
}

void Device::ApplyState(DWORD fvf, bool twoD) {
    const Config& cfg = GetConfig();
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glViewport(0, 0, layout_.fboW, layout_.fboH);
    glUseProgram(prog_);
    glUniform1i(u_.screen2D, twoD);
    glUniform4f(u_.constColor, 1, 1, 1, 1);
    glUniform1i(u_.alphaFix, 0);
    if (twoD) return;

    // depth
    if (rs_[D3DRENDERSTATE_ZENABLE]) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(rs_[D3DRENDERSTATE_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);
    // blending
    if (rs_[D3DRENDERSTATE_ALPHABLENDENABLE]) {
        glEnable(GL_BLEND);
        glBlendFunc(MapBlend(rs_[D3DRENDERSTATE_SRCBLEND]), MapBlend(rs_[D3DRENDERSTATE_DESTBLEND]));
    } else {
        glDisable(GL_BLEND);
    }
    // culling: D3DCULL_CCW removes triangles that appear counter-clockwise on screen, i.e. the
    // front faces are the visually clockwise ones (GL_CW in GL's convention).
    DWORD cull = rs_[D3DRENDERSTATE_CULLMODE];
    if (cull == D3DCULL_NONE) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glFrontFace(cull == D3DCULL_CCW ? GL_CW : GL_CCW);
        glCullFace(GL_BACK);
    }

    // transforms (with optional widescreen projection and interpolation offset)
    D3DMATRIX world = world_;
    world._41 += worldOffset_.x;
    world._42 += worldOffset_.y;
    world._43 += worldOffset_.z;
    D3DMATRIX proj = proj_;
    proj._11 *= 640.f / layout_.virtualW;
    if (cfg.fovScale != 1.f && gameplayView_) {  // levels only: menus, title and ending keep the original lens
        proj._11 /= cfg.fovScale;
        proj._22 /= cfg.fovScale;
    }
    glUniformMatrix4fv(u_.world, 1, GL_FALSE, world.m());
    glUniformMatrix4fv(u_.view, 1, GL_FALSE, view_.m());
    glUniformMatrix4fv(u_.proj, 1, GL_FALSE, proj.m());

    // lighting
    bool lighting = rs_[D3DRENDERSTATE_LIGHTING] != 0 && (fvf & 0x010);  // needs normals
    glUniform1i(u_.lighting, lighting);
    glUniform1i(u_.hasVertexColor, (fvf & 0x040) != 0);
    glUniform4fv(u_.matDiffuse, 1, &material_.diffuse.r);
    glUniform4fv(u_.matAmbient, 1, &material_.ambient.r);
    glUniform4fv(u_.matEmissive, 1, &material_.emissive.r);
    DWORD a = rs_[D3DRENDERSTATE_AMBIENT];
    glUniform4f(u_.ambient, ((a >> 16) & 0xFF) / 255.f, ((a >> 8) & 0xFF) / 255.f, (a & 0xFF) / 255.f, ((a >> 24) & 0xFF) / 255.f);
    bool lightOn = lightEnabled_ && light_.type == D3DLIGHT_DIRECTIONAL;
    glUniform1i(u_.lightOn, lightOn);
    if (lightOn) {
        D3DVECTOR d = light_.direction;
        float len = Magnitude(d);
        if (len > 0) d = d / len;
        glUniform3f(u_.lightDir, -d.x, -d.y, -d.z);
        glUniform4fv(u_.lightDiffuse, 1, &light_.diffuse.r);
    }
    glUniform1i(u_.flat, rs_[D3DRENDERSTATE_SHADEMODE] == D3DSHADE_FLAT);
    glUniform1i(u_.perPixel, cfg.perPixelLighting);

    // texture stage 0
    bool texOn = texture_ != nullptr && (fvf & 0xF00) != 0;
    glUniform1i(u_.texOn, texOn);
    glUniform1i(u_.colorOp, tss_[D3DTSS_COLOROP]);
    glUniform1i(u_.colorArg1, tss_[D3DTSS_COLORARG1]);
    glUniform1i(u_.colorArg2, tss_[D3DTSS_COLORARG2]);
    glUniform1i(u_.alphaOp, tss_[D3DTSS_ALPHAOP]);
    glUniform1i(u_.alphaArg1, tss_[D3DTSS_ALPHAARG1]);
    glUniform1i(u_.alphaArg2, tss_[D3DTSS_ALPHAARG2]);
    glUniform1i(u_.alphaTest, 0);
    if (texOn && texture_->SoftAlpha() && rs_[D3DRENDERSTATE_ALPHABLENDENABLE]) {
        DWORD src = rs_[D3DRENDERSTATE_SRCBLEND], dst = rs_[D3DRENDERSTATE_DESTBLEND];
        if (src == D3DBLEND_ONE) glUniform1i(u_.alphaFix, 1);
        else if ((src == D3DBLEND_ZERO && dst == D3DBLEND_SRCCOLOR) || (src == D3DBLEND_DESTCOLOR && dst == D3DBLEND_ZERO))
            glUniform1i(u_.alphaFix, 2);
    }
    if (texOn) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_->Id());
        glUniform1i(u_.tex, 0);
        bool linMag = tss_[D3DTSS_MAGFILTER] >= D3DTFG_LINEAR || cfg.smoothTextures;
        bool linMin = tss_[D3DTSS_MINFILTER] >= D3DTFG_LINEAR || cfg.smoothTextures;
        GLint minF = linMin ? GL_LINEAR : GL_NEAREST;
        if (texture_->HasMips() && (cfg.anisotropy > 1 || cfg.smoothTextures))
            minF = linMin ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minF);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linMag ? GL_LINEAR : GL_NEAREST);
        if (maxAniso_ > 1.f)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY,
                            (texture_->HasMips() && linMin) ? std::min((float)cfg.anisotropy, maxAniso_) : 1.f);
    }
}

static GLenum MapPrim(D3DPRIMITIVETYPE t, DWORD n, GLsizei* count) {
    switch (t) {
        case D3DPT_POINTLIST: *count = n; return GL_POINTS;
        case D3DPT_LINELIST: *count = n; return GL_LINES;
        case D3DPT_LINESTRIP: *count = n; return GL_LINE_STRIP;
        case D3DPT_TRIANGLESTRIP: *count = n; return GL_TRIANGLE_STRIP;
        case D3DPT_TRIANGLEFAN: *count = n; return GL_TRIANGLE_FAN;
        default: *count = n; return GL_TRIANGLES;
    }
}

static void SetupAttribs(DWORD fvf) {
    for (int i = 0; i < 6; i++) glDisableVertexAttribArray(i);
    if (fvf == D3DFVF_VERTEX) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(D3DVERTEX), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(D3DVERTEX), (void*)12);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(D3DVERTEX), (void*)24);
        glVertexAttrib4f(2, 1, 1, 1, 1);
    } else {  // D3DFVF_LVERTEX
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(D3DLVERTEX), (void*)0);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(D3DLVERTEX), (void*)16);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(D3DLVERTEX), (void*)24);
        glVertexAttrib4f(1, 0, 0, 0, 0);
    }
}

HRESULT Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, const void* verts, DWORD numVerts,
                                     const WORD* indices, DWORD numIndices, DWORD) {
    if (!verts || !indices || !numIndices) return D3D_OK;
    ApplyState(fvf, false);
    size_t stride = fvf == D3DFVF_VERTEX ? sizeof(D3DVERTEX) : sizeof(D3DLVERTEX);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, stride * numVerts, verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(WORD) * numIndices, indices, GL_STREAM_DRAW);
    SetupAttribs(fvf);
    GLsizei count;
    GLenum mode = MapPrim(type, numIndices, &count);
    glDrawElements(mode, count, GL_UNSIGNED_SHORT, (void*)0);
    return D3D_OK;
}

HRESULT Device::DrawPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, const void* verts, DWORD numVerts, DWORD) {
    if (!verts || !numVerts) return D3D_OK;
    ApplyState(fvf, false);
    size_t stride = fvf == D3DFVF_VERTEX ? sizeof(D3DVERTEX) : sizeof(D3DLVERTEX);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, stride * numVerts, verts, GL_STREAM_DRAW);
    SetupAttribs(fvf);
    GLsizei count;
    GLenum mode = MapPrim(type, numVerts, &count);
    glDrawArrays(mode, 0, count);
    return D3D_OK;
}

void Device::Draw2DQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Texture* t,
                        bool alphaTest, const float* color, bool linear) {
    ApplyState(0, true);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    float sx = 2.f / layout_.virtualW, sy = -2.f / 480.f;
    glUniform4f(u_.viewport2D, sx, sy, layout_.offsetX * sx - 1.f, 1.f);
    glUniform1i(u_.lighting, 0);
    glUniform1i(u_.flat, 0);
    glUniform1i(u_.perPixel, 0);
    glUniform1i(u_.alphaTest, alphaTest);
    glUniform1i(u_.alphaFix, 0);
    glUniform1i(u_.texOn, t != nullptr);
    glUniform1i(u_.colorOp, D3DTOP_SELECTARG1);
    glUniform1i(u_.colorArg1, D3DTA_TEXTURE);
    glUniform1i(u_.alphaOp, D3DTOP_SELECTARG1);
    glUniform1i(u_.alphaArg1, D3DTA_TEXTURE);
    if (t) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, t->Id());
        glUniform1i(u_.tex, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    uint32_t c = 0xFFFFFFFF;
    if (color) {
        c = (uint32_t)(color[3] * 255) << 24 | (uint32_t)(color[0] * 255) << 16 | (uint32_t)(color[1] * 255) << 8 |
            (uint32_t)(color[2] * 255);
    }
    D3DLVERTEX q[4];
    memset(q, 0, sizeof q);
    q[0].x = x0; q[0].y = y0; q[0].tu = u0; q[0].tv = v0;
    q[1].x = x1; q[1].y = y0; q[1].tu = u1; q[1].tv = v0;
    q[2].x = x0; q[2].y = y1; q[2].tu = u0; q[2].tv = v1;
    q[3].x = x1; q[3].y = y1; q[3].tu = u1; q[3].tv = v1;
    for (auto& v : q) v.color = c;
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof q, q, GL_STREAM_DRAW);
    SetupAttribs(D3DFVF_LVERTEX);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

HRESULT Device::BltToBackBuffer(const RECT* dst, Surface* src, const RECT* srcRect, bool useColorKey) {
    if (!src) return E_FAIL;
    RECT d = dst ? *dst : RECT{0, 0, 640, 480};
    RECT s = srcRect ? *srcRect : RECT{0, 0, src->Width(), src->Height()};
    // DirectDraw (no clipper) rejected any blit whose destination crossed the surface edge. That
    // hid sprites whose transparent padding poked past 640 px (the last fire LEDs, FPS digits).
    // We clip instead: the scene target itself bounds the drawing, so nothing visible changes
    // except that those partially-off-screen sprites now appear.
    if (d.right <= d.left || d.bottom <= d.top) return E_FAIL;
    Texture* t = src->Tex();
    // source rectangles are in the image's layout size (an HD replacement has more pixels)
    float tw = (float)src->Width(), th = (float)src->Height();
    float x0 = (float)d.left, y0 = (float)d.top, x1 = (float)d.right, y1 = (float)d.bottom;
    if (hudScale_ != 1.f) {
        x0 = hudAnchorX_ + (x0 - hudAnchorX_) * hudScale_;
        x1 = hudAnchorX_ + (x1 - hudAnchorX_) * hudScale_;
        y0 = hudAnchorY_ + (y0 - hudAnchorY_) * hudScale_;
        y1 = hudAnchorY_ + (y1 - hudAnchorY_) * hudScale_;
    }
    Draw2DQuad(x0, y0, x1, y1, s.left / tw, s.top / th, s.right / tw, s.bottom / th, t, useColorKey && src->KeyEnabled(),
               nullptr, GetConfig().smoothSprites);
    return D3D_OK;
}

HRESULT Device::ColorFill(const RECT* dst, uint32_t rgb) {
    RECT d = dst ? *dst : RECT{0, 0, 640, 480};
    float c[4] = {((rgb >> 16) & 0xFF) / 255.f, ((rgb >> 8) & 0xFF) / 255.f, (rgb & 0xFF) / 255.f, 1.f};
    // Untextured quad with constant colour
    ApplyState(0, true);
    Draw2DQuad((float)d.left, (float)d.top, (float)d.right, (float)d.bottom, 0, 0, 0, 0, nullptr, false, c, false);
    return D3D_OK;
}

void Device::BeginHudScale(float scale, float anchorX, float anchorY) {
    hudScale_ = scale;
    hudAnchorX_ = anchorX;
    hudAnchorY_ = anchorY;
}

void Device::EndHudScale() { hudScale_ = 1.f; }

int Device::HudEdgeShift() const {
    const Config& cfg = GetConfig();
    if (cfg.hudLayout != 1 || layout_.offsetX <= 0.f) return 0;
    float shift = layout_.offsetX;
    if (cfg.hudMaxAspect > 0.f) shift = std::min(shift, std::max(0.f, (480.f * cfg.hudMaxAspect - 640.f) * 0.5f));
    return (int)shift;
}

void Device::DrawUIQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Texture* t,
                        const float rgba[4]) {
    ApplyState(0, true);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    float sx = 2.f / layout_.virtualW, sy = -2.f / 480.f;
    glUniform4f(u_.viewport2D, sx, sy, layout_.offsetX * sx - 1.f, 1.f);
    glUniform1i(u_.lighting, 0);
    glUniform1i(u_.flat, 0);
    glUniform1i(u_.perPixel, 0);
    glUniform1i(u_.alphaTest, 0);
    glUniform1i(u_.alphaFix, 0);
    glUniform1i(u_.texOn, t != nullptr);
    glUniform1i(u_.colorOp, D3DTOP_MODULATE);
    glUniform1i(u_.colorArg1, D3DTA_TEXTURE);
    glUniform1i(u_.colorArg2, D3DTA_DIFFUSE);
    glUniform1i(u_.alphaOp, D3DTOP_MODULATE);
    glUniform1i(u_.alphaArg1, D3DTA_TEXTURE);
    glUniform1i(u_.alphaArg2, D3DTA_DIFFUSE);
    glUniform4f(u_.constColor, 1, 1, 1, 1);
    if (t) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, t->Id());
        glUniform1i(u_.tex, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    auto b = [](float f) { return (uint32_t)(std::min(1.f, std::max(0.f, f)) * 255.f + 0.5f); };
    uint32_t c = b(rgba[3]) << 24 | b(rgba[0]) << 16 | b(rgba[1]) << 8 | b(rgba[2]);
    D3DLVERTEX q[4];
    memset(q, 0, sizeof q);
    q[0].x = x0; q[0].y = y0; q[0].tu = u0; q[0].tv = v0;
    q[1].x = x1; q[1].y = y0; q[1].tu = u1; q[1].tv = v0;
    q[2].x = x0; q[2].y = y1; q[2].tu = u0; q[2].tv = v1;
    q[3].x = x1; q[3].y = y1; q[3].tu = u1; q[3].tv = v1;
    for (auto& v : q) v.color = c;
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof q, q, GL_STREAM_DRAW);
    SetupAttribs(D3DFVF_LVERTEX);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_BLEND);
}
