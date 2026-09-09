// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace alien_crush::gameplay {
struct LauncherContact {
    std::int16_t ball_velocity_y{};
    bool applied{};
    bool consume_contact{};
};
// Vertical, moving type-6 launcher response only: surface $60, basis $0100/$0000,
// and collision-mode bit 7 set. Geometry/contact detection remains the caller's
// job. Velocities are signed original 8.8 words; output preserves 16-bit wrap.
// Translation of 03:8B89..8BF8 and the five arithmetic shifts at 03:8D04.
LauncherContact vertical_launcher_contact(std::int16_t ball_velocity_y,
    std::int16_t launcher_velocity_y,bool new_contact) noexcept;
}
