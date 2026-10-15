// Minimal immediate-mode drawing for overlay UI (options menu): an embedded 8x8 bitmap font
// (public-domain font8x8 by Daniel Hepper) and filled rectangles, drawn in the game's
// 480-pixel-high virtual space on top of the finished frame.
#pragma once
#include <string>

class Device;
class Texture;

struct UIColor {
    float r, g, b, a;
};

class UIDraw {
public:
    ~UIDraw();
    void Begin(Device* d);   // call once per frame before drawing
    // Visible horizontal range of virtual space (wider than 0..640 in widescreen)
    float Left() const { return left_; }
    float Right() const { return right_; }
    void Rect(float x0, float y0, float x1, float y1, UIColor c);
    // scale: 1 = 8 px glyphs. Draws a soft drop shadow under the text.
    void Text(float x, float y, const std::string& s, float scale, UIColor c, bool shadow = true);
    float TextWidth(const std::string& s, float scale) const { return (float)s.size() * 8.f * scale; }

private:
    void EnsureFont();
    Device* dev_ = nullptr;
    Texture* font_ = nullptr;
    float left_ = 0, right_ = 640;
};
