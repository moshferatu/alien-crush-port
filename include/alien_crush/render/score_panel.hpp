// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
namespace alien_crush::render {
// Fixed reserves keep digit size stable as scores gain digits. Uses original
// HUD artwork for numbers and small host-authored bitmap labels.
class ScorePanel {
    std::array<Image,10> digits_;
public:
    explicit ScorePanel(std::array<Image,10> digits);
    static ScorePanel from_rom(std::span<const std::byte>);
    Image compose(const diagnostics::AlienState&) const;
};
}
