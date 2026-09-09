// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/gameplay/launcher_contact.hpp"
#include <bit>
namespace alien_crush::gameplay {
namespace {
std::int16_t wrap(int value) noexcept {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
int damp(int value) noexcept {
    // Arithmetic shift rounds negative fractions down, not toward zero.
    return value>=0?value/32:-((-value+31)/32);
}
}
LauncherContact vertical_launcher_contact(std::int16_t ball_velocity_y,
    std::int16_t launcher_velocity_y,bool new_contact) noexcept {
    if(!new_contact) {
        if(ball_velocity_y<0) return {ball_velocity_y,false,false};
        if(launcher_velocity_y>=0) {
            if(launcher_velocity_y>=ball_velocity_y) return {ball_velocity_y,false,false};
            return {wrap(damp(-static_cast<int>(ball_velocity_y))),true,false};
        }
    }
    const int reflected=ball_velocity_y<0?ball_velocity_y:-static_cast<int>(ball_velocity_y);
    return {wrap(damp(reflected)+launcher_velocity_y),true,true};
}
}
