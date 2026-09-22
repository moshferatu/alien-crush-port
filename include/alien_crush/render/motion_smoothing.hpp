// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace alien_crush::render {
struct MotionPosition { double x{},y{}; };
// Presentation history only. Never extrapolate through collisions or interpolate
// a spawn, teleport, load, missing frame, or change of scene.
class MotionSmoothing {
    MotionPosition previous_{},current_{};
    std::uint64_t frame_{};
    bool valid_{};
public:
    void reset() {valid_=false;}
    void update(std::uint64_t frame,MotionPosition position) {
        const bool linked=valid_ && frame==frame_+1 &&
            std::abs(position.x-current_.x)<=32 && std::abs(position.y-current_.y)<=32;
        previous_=linked?current_:position;
        current_=position;frame_=frame;valid_=true;
    }
    MotionPosition position(double fraction) const {
        const auto t=std::clamp(fraction,0.0,1.0);
        return {previous_.x+(current_.x-previous_.x)*t,previous_.y+(current_.y-previous_.y)*t};
    }
};
}
