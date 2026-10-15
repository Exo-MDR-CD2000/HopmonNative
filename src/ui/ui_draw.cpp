#include "ui_draw.h"
#include "../gfx/device.h"
#include "../../third_party/font8x8_basic.h"

UIDraw::~UIDraw() { delete font_; }

void UIDraw::EnsureFont() {
    if (font_) return;
    // 16 x 8 glyph atlas (128 x 64 px): white glyph pixels, everything else black (colour-keyed out)
    Image img;
    img.width = 128;
    img.height = 64;
    img.bpp = 32;
    img.pixels.assign((size_t)img.width * img.height, 0xFF000000u);
    for (int ch = 0; ch < 128; ch++) {
        int gx = (ch % 16) * 8, gy = (ch / 16) * 8;
        for (int row = 0; row < 8; row++) {
            unsigned bits = (unsigned char)font8x8_basic[ch][row];
            for (int col = 0; col < 8; col++)
                if (bits & (1u << col)) img.pixels[(size_t)(gy + row) * img.width + gx + col] = 0xFFFFFFFFu;
        }
    }
    font_ = Texture::Create(img, true, 0x000000, false);
}

void UIDraw::Begin(Device* d) {
    dev_ = d;
    EnsureFont();
    const FrameLayout& L = d->Layout();
    left_ = -L.offsetX;
    right_ = 640.f + L.offsetX;
}

void UIDraw::Rect(float x0, float y0, float x1, float y1, UIColor c) {
    float rgba[4] = {c.r, c.g, c.b, c.a};
    dev_->DrawUIQuad(x0, y0, x1, y1, 0, 0, 0, 0, nullptr, rgba);
}

void UIDraw::Text(float x, float y, const std::string& s, float scale, UIColor c, bool shadow) {
    if (shadow) Text(x + scale, y + scale, s, scale, UIColor{0, 0, 0, c.a * 0.7f}, false);
    float rgba[4] = {c.r, c.g, c.b, c.a};
    float g = 8.f * scale;
    for (unsigned char ch : s) {
        if (ch > 32 && ch < 128) {
            float u0 = (ch % 16) * 8 / 128.f, v0 = (ch / 16) * 8 / 64.f;
            dev_->DrawUIQuad(x, y, x + g, y + g, u0, v0, u0 + 8 / 128.f, v0 + 8 / 64.f, font_, rgba);
        }
        x += g;
    }
}
