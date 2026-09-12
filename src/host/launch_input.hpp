// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
namespace alien_crush::host {
// Device-independent routing also covers raw cabinet boards. I is exclusively
// launch/confirm; neither flipper sends it. Enter/Run launches in the shooter.
inline PadInput route_game_input(PadInput input,bool launching) {
    if(input.player_one&Pad::run) {
        input.player_one|=Pad::button_i;
        if(launching) input.player_one&=static_cast<std::uint16_t>(~Pad::run);
    }
    // PCE rejects opposing directions. As in Devil's Crush, use Up as the
    // right-flipper alternate only while both flippers are pressed.
    if((input.player_one&(Pad::left|Pad::right))==(Pad::left|Pad::right)) {
        input.player_one&=static_cast<std::uint16_t>(~(Pad::right|Pad::down));
        input.player_one|=Pad::up;
    } else if((input.player_one&(Pad::up|Pad::down))==(Pad::up|Pad::down)) {
        input.player_one&=static_cast<std::uint16_t>(~Pad::down);
    }
    return input;
}
class GameInputRouter {
    bool launch_press_{};
    bool initials_press_{};
public:
    PadInput route(PadInput input,std::span<const std::byte> ram) {
        if(!(input.player_one&Pad::run)) initials_press_=false;
        else if(ram.size()==8192 && ram[0x7a]==std::byte{2} && ram[0x76]==std::byte{})
            initials_press_|=diagnostics::observe_alien_state(ram).high_score_menu;
        if(!(input.player_one&Pad::run)) launch_press_=false;
        else launch_press_|=reference_launcher_active(ram);
        // One physical press cannot change from launch into pause. After
        // release, a fresh press outside the shooter can pause normally.
        // Run submits the entire name in the ROM; I advances one initial.
        // Keep a held confirmation owned until release, including after the
        // third letter, so it cannot turn into Run as the screen exits.
        return route_game_input(input,launch_press_ || initials_press_);
    }
};
}
