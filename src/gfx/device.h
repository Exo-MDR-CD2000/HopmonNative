// A Direct3D 7 style immediate-mode device implemented on OpenGL 3.3.
// Only the subset of D3D7 / DirectDraw used by Hopmon is provided, but it is
// emulated faithfully (fixed-function lighting, flat/gouraud shading, blend
// modes, cull mode, texture stage 0 ops). Rendering enhancements live here too.
#pragma once
#include "../core/d3dtypes.h"
#include "../core/assets.h"
#include "gl.h"
#include <vector>

struct SDL_Window;

class Texture {
public:
    ~Texture();
    static Texture* Create(const Image& img, bool colorKey, uint32_t keyRGB, bool mipmaps);
    void Update(const Image& img, bool colorKey, uint32_t keyRGB);
    int Width() const { return width_; }
    int Height() const { return height_; }
    GLuint Id() const { return id_; }
    bool HasMips() const { return mips_; }
    // A mod image with its own transparency (originals only have colour-key cut-outs).
    bool SoftAlpha() const { return softAlpha_; }

private:
    int width_ = 0, height_ = 0;
    GLuint id_ = 0;
    bool mips_ = false;
    bool softAlpha_ = false;
};

// Emulates a DirectDraw off-screen surface holding a sprite sheet.
class Surface {
public:
    explicit Surface(const Image& img);
    ~Surface();
    int Width() const { return image_.LogicalWidth(); }    // layout size (= the original's)
    int Height() const { return image_.LogicalHeight(); }
    // COLORREF semantics of the original DDSetColorKey: CLR_INVALID = use pixel (0,0)
    void SetColorKey(COLORREF key);
    void ClearColorKey();
    bool KeyEnabled() const { return keyEnabled_; }
    Texture* Tex();
    const Image& GetImage() const { return image_; }

private:
    Image image_;
    Texture* tex_ = nullptr;
    bool dirty_ = true;
    bool keyEnabled_ = false;
    uint32_t keyRGB_ = 0;
};

struct FrameLayout {
    int windowW = 640, windowH = 480;     // drawable size in pixels
    int destX = 0, destY = 0, destW = 640, destH = 480;  // where the game image goes in the window
    int fboW = 640, fboH = 480;           // internal render target size
    float virtualW = 640.f;               // logical width (>= 640 in widescreen), height is always 480
    float offsetX = 0.f;                  // logical x of the original 640x480 area inside virtual space
};

class Device {
public:
    bool Init(SDL_Window* window);
    void Shutdown();

    // Frame management (called by the application framework, not by game code)
    void BeginFrame();
    void Present();
    const FrameLayout& Layout() const { return layout_; }
    // Widescreen HUD placement: how many 640x480-space pixels edge-anchored HUD elements move
    // outward (left items by -HudEdgeShift(), right items by +HudEdgeShift()). 0 unless
    // aspect = widescreen and hud_layout = 1.
    int HudEdgeShift() const;
    // HUD size (enhancement): while set, 2D blits are scaled by `scale` around the virtual-space
    // point (anchorX, anchorY). The game's HUD code wraps each HUD group with these.
    void BeginHudScale(float scale, float anchorX, float anchorY);
    void EndHudScale();
    // fov_scale applies only while a level is on screen (set by the game every frame).
    void SetGameplayView(bool on) { gameplayView_ = on; }
    // Screen area used by the top-left HUD group this frame (virtual space), so the FPS counter
    // can step out of its way at large HUD sizes. Reset every frame.
    void SetHudTopLeftExtent(float right, float bottom) { hudTLRight_ = right; hudTLBottom_ = bottom; }
    float HudTopLeftRight() const { return hudTLRight_; }
    float HudTopLeftBottom() const { return hudTLBottom_; }
    void OnSettingsChanged();
    bool SaveScreenshot(const char* path);

    // --- IDirect3DDevice7 subset ---
    HRESULT BeginScene();
    HRESULT EndScene();
    HRESULT Clear(DWORD count, const RECT* rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil);
    HRESULT SetRenderState(D3DRENDERSTATETYPE s, DWORD v);
    HRESULT GetRenderState(D3DRENDERSTATETYPE s, DWORD* v);
    HRESULT SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE s, DWORD v);
    HRESULT GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE s, DWORD* v);
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE t, const D3DMATRIX* m);
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE t, D3DMATRIX* m);
    HRESULT SetMaterial(const D3DMATERIAL7* m);
    HRESULT GetMaterial(D3DMATERIAL7* m);
    HRESULT SetLight(DWORD index, const D3DLIGHT7* l);
    HRESULT LightEnable(DWORD index, BOOL enable);
    HRESULT SetTexture(DWORD stage, Texture* t);
    HRESULT GetTexture(DWORD stage, Texture** t);
    HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, const void* verts, DWORD numVerts,
                                 const WORD* indices, DWORD numIndices, DWORD flags);
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE type, DWORD fvf, const void* verts, DWORD numVerts, DWORD flags);

    // --- DirectDraw back-buffer operations, in the original 640x480 pixel space ---
    HRESULT BltToBackBuffer(const RECT* dst, Surface* src, const RECT* srcRect, bool useColorKey);
    HRESULT ColorFill(const RECT* dst, uint32_t rgb);

    // Overlay UI (options menu): quad in 480-high virtual space, alpha blended, texture x colour.
    void DrawUIQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Texture* t,
                    const float rgba[4]);

    int ScreenWidth() const { return 640; }
    int ScreenHeight() const { return 480; }

    // Optional hook: extra world-space offset (used by frame interpolation)
    void SetWorldOffset(const D3DVECTOR& v) { worldOffset_ = v; }

private:
    float hudScale_ = 1.f, hudAnchorX_ = 0.f, hudAnchorY_ = 0.f;
    bool gameplayView_ = false;
    float hudTLRight_ = -1e9f, hudTLBottom_ = 0.f;
    void ComputeLayout();
    void CreateTargets();
    void DestroyTargets();
    void ApplyState(DWORD fvf, bool twoD);
    void Draw2DQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Texture* t,
                    bool alphaTest, const float* color, bool linear);
    GLuint CompileProgram(const char* vs, const char* fs);

    SDL_Window* window_ = nullptr;
    FrameLayout layout_;
    GLuint sceneFbo_ = 0, sceneColorRb_ = 0, sceneDepthRb_ = 0;
    GLuint resolveFbo_ = 0, resolveTex_ = 0;
    int samples_ = 0;
    bool inScene_ = false;

    GLuint prog_ = 0, progPresent_ = 0;
    GLuint vao_ = 0, vbo_ = 0, ibo_ = 0;
    struct Uniforms {
        GLint world, view, proj, lighting, matDiffuse, matAmbient, matEmissive, ambient, lightOn, lightDir,
            lightDiffuse, flat, texOn, colorOp, colorArg1, colorArg2, alphaOp, alphaArg1, alphaArg2, alphaTest,
            hasVertexColor, perPixel, tex, constColor, screen2D, viewport2D, alphaFix;
    } u_{};
    GLint presentTexLoc_ = -1, presentGammaLoc_ = -1;

    DWORD rs_[D3DRENDERSTATE_MAX];
    DWORD tss_[D3DTSS_MAX];
    D3DMATRIX world_, view_, proj_;
    D3DMATERIAL7 material_{};
    D3DLIGHT7 light_{};
    bool lightEnabled_ = false;
    Texture* texture_ = nullptr;
    D3DVECTOR worldOffset_;
    float maxAniso_ = 1.f;
};

Device& GetDevice();
