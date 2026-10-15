#include "interp.h"
#include "d3dapp.h"
#include "../core/config.h"
#include <algorithm>
#include <vector>

namespace {
std::vector<Interp::Tracked*>& Registry() {
    static std::vector<Interp::Tracked*> r;
    return r;
}
bool g_suppressed = false;
}  // namespace

namespace Interp {

Tracked::Tracked() { Registry().push_back(this); }
Tracked::~Tracked() {
    auto& r = Registry();
    auto it = std::find(r.begin(), r.end(), this);
    if (it != r.end()) {
        *it = r.back();
        r.pop_back();
    }
}

void BeginFrameMove() {
    for (Tracked* t : Registry()) t->SnapshotPrev();
}

void EndFrameMove() {}

bool Enabled() { return GetConfig().interpolation && !g_suppressed; }

float Alpha() { return Enabled() ? AppTickAlpha() : 1.f; }

void SetSuppressed(bool s) { g_suppressed = s; }

}  // namespace Interp
