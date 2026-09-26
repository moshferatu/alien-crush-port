// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
namespace alien_crush::render {
// The ball in play, counted as a pinball cabinet counts it: an extra ball is
// played again under the same number. The cartridge keeps only the balls left
// ($46), which an award raises (03:919C, 9BB5), so the number is the highest
// the balls left have given this game. A restored state starts from them alone.
class BallNumber {
    unsigned ball_{};
public:
    unsigned update(const diagnostics::AlienState&);
    void reset() {ball_=0;}
};
// Fixed reserves keep digit size stable as scores gain digits. Uses original
// HUD artwork for numbers and small host-authored bitmap labels.
class ScorePanel {
    std::array<Image,10> digits_;
public:
    explicit ScorePanel(std::array<Image,10> digits);
    static ScorePanel from_rom(std::span<const std::byte>);
    // `ball` is the BallNumber; zero takes it from the balls left alone.
    Image compose(const diagnostics::AlienState&,unsigned ball=0) const;
};
}
