// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <cstdint>
namespace Sexy {
// The game has one pointer. Scene interruptions cancel its capture without
// depending on SDL's global touch-to-mouse finger surviving UIKit cancellation.
class SingleTouchInput {
    bool down = false;
    int64_t device = 0, finger = 0;
public:
    bool Begin(int64_t touch, int64_t id) {
        if (down) return false;
        down = true; device = touch; finger = id; return true;
    }
    bool Matches(int64_t touch, int64_t id) const {
        return down && device == touch && finger == id;
    }
    bool End(int64_t touch, int64_t id) {
        if (!Matches(touch,id)) return false;
        down = false; return true;
    }
    void Cancel() { down = false; }
};
}
