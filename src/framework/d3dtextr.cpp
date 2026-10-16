#include "d3dtextr.h"
#include "../core/assets.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../gfx/device.h"
#include <algorithm>
#include <map>

namespace {
struct TextureContainer {
    WORD resId = 0;
    DWORD stage = 0, flags = 0;
    Image image;
    Texture* tex = nullptr;
};
std::map<WORD, TextureContainer> g_textures;
}  // namespace

HRESULT D3DTextr_CreateTextureFromResource(WORD resId, DWORD flags, DWORD stage) {
    if (resId == 0) return (HRESULT)0x80070057;
    if (g_textures.count(resId)) return S_OK;
    TextureContainer tc;
    tc.resId = resId;
    tc.stage = stage;
    tc.flags = flags;
    if (!Assets::LoadBitmapImage(resId, &tc.image)) {
        Log("Texture resource %u missing", resId);
        return E_FAIL;
    }
    // D3DTEXTR_TRANSPARENTWHITE (1) / TRANSPARENTBLACK (2): pure white / black pixels become
    // transparent when the texture is created (see Restore). Used for the steam puff (0xdc).
    g_textures[resId] = std::move(tc);
    return S_OK;
}

// Visual upgrade ("enhanced_shadows"): the blob-shadow texture (0xa3) is a hard-edged disc that is
// multiplied onto the floor. Blurring it gives a soft penumbra; the white border stays white, so
// the shadow's size and position are unchanged.
static Image SoftenShadow(const Image& src) {
    Image img = src;
    int w = img.width, h = img.height;
    if (w < 4 || h < 4) return img;
    int r = std::max(1, w / 10);
    std::vector<float> a(w * h), b(w * h);
    for (int i = 0; i < w * h; i++) a[i] = (float)((img.pixels[i] >> 8) & 0xFF);  // green as luminance
    auto at = [&](const std::vector<float>& v, int x, int y) {
        return (x < 0 || y < 0 || x >= w || y >= h) ? 255.f : v[y * w + x];
    };
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float sum = 0;
                for (int k = -r; k <= r; k++) sum += at(a, x + k, y);
                b[y * w + x] = sum / (2 * r + 1);
            }
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float sum = 0;
                for (int k = -r; k <= r; k++) sum += at(b, x, y + k);
                a[y * w + x] = sum / (2 * r + 1);
            }
    }
    for (int i = 0; i < w * h; i++) {
        uint32_t v = (uint32_t)std::min(255.f, std::max(0.f, a[i] + 0.5f));
        img.pixels[i] = 0xFF000000u | (v << 16) | (v << 8) | v;
    }
    return img;
}

static void Restore(TextureContainer& tc) {
    if (tc.tex) return;
    if (tc.resId == 0xa3 && GetConfig().enhancedShadows) {
        tc.tex = Texture::Create(SoftenShadow(tc.image), false, 0, true);
        return;
    }
    bool key = (tc.flags & 3) != 0;
    uint32_t keyRGB = (tc.flags & 2) ? 0x000000 : 0xFFFFFF;
    tc.tex = Texture::Create(tc.image, key, keyRGB, true);
}

Texture* D3DTextr_GetSurface(WORD resId) {
    auto it = g_textures.find(resId);
    if (it == g_textures.end()) return nullptr;
    Restore(it->second);
    return it->second.tex;
}

HRESULT D3DTextr_DestroyTexture(WORD resId) {
    auto it = g_textures.find(resId);
    if (it == g_textures.end()) return E_FAIL;
    delete it->second.tex;
    g_textures.erase(it);
    return S_OK;
}

void D3DTextr_InvalidateAllTextures() {
    for (auto& kv : g_textures) {
        delete kv.second.tex;
        kv.second.tex = nullptr;
    }
}

void D3DTextr_InvalidateTexture(WORD resId) {
    auto it = g_textures.find(resId);
    if (it == g_textures.end()) return;
    delete it->second.tex;
    it->second.tex = nullptr;  // re-created (with current settings) on next use
}

void D3DTextr_RestoreAllTextures() {
    for (auto& kv : g_textures) Restore(kv.second);
}

void D3DTextr_ReloadAll() {
    for (auto it = g_textures.begin(); it != g_textures.end();) {
        delete it->second.tex;
        it->second.tex = nullptr;
        if (it->first >= 0x8000) {
            it = g_textures.erase(it);
            continue;
        }
        Image img;
        if (Assets::LoadBitmapImage(it->first, &img)) it->second.image = std::move(img);
        ++it;
    }
}

void D3DTextr_FreeAll() {
    D3DTextr_InvalidateAllTextures();
    g_textures.clear();
}

Surface* DDLoadBitmap(WORD resId, int* width, int* height) {
    *width = -1;
    *height = -1;
    Image img;
    if (!Assets::LoadBitmapImage(resId, &img)) return nullptr;
    *width = img.LogicalWidth();
    *height = img.LogicalHeight();
    return new Surface(img);
}

HRESULT DDSetColorKey(Surface* s, COLORREF rgb) {
    if (!s) return E_FAIL;
    s->SetColorKey(rgb);
    return S_OK;
}
