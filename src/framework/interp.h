// Render-side interpolation between the game's fixed 10 ms logic ticks.
// Purely visual: objects snapshot their pose before each FrameMove, and the
// renderer draws them blended between that snapshot and the current pose.
#pragma once
#include "../core/d3dtypes.h"

namespace Interp {

class Tracked {
public:
    Tracked();
    virtual ~Tracked();
    virtual void SnapshotPrev() = 0;
};

void BeginFrameMove();   // snapshot everything (called before FrameMove)
void EndFrameMove();
bool Enabled();
float Alpha();           // 0 = previous tick pose, 1 = current pose
void SetSuppressed(bool s);  // e.g. during scene transitions

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline D3DVECTOR Lerp(const D3DVECTOR& a, const D3DVECTOR& b, float t) {
    return D3DVECTOR(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t));
}

}  // namespace Interp
