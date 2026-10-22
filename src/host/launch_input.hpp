// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "pad_bindings.hpp"
namespace alien_crush::host {
// Routing for every device. I is exclusively
// launch/confirm; neither flipper sends it. Enter/Run launches in the shooter.
inline PadInput route_game_input(PadInput input,bool launching,const FlipperInputs& flippers={}) {
    if(input.player_one&Pad::run) {
        input.player_one|=Pad::button_i;
        if(launching) input.player_one&=static_cast<std::uint16_t>(~Pad::run);
    }
    // PCE rejects opposing directions. As in Devil's Crush, a flipper moves
    // to its alternate input (Up, for the right flipper's defaults) only while
    // both flippers are pressed. Down gives way to anything that needs its axis.
    const auto high=static_cast<std::uint16_t>(input.player_one&0xff00);
    auto pad=static_cast<std::uint8_t>(input.player_one);
    if((pad&Pad::up) && (pad&Pad::down)) pad=static_cast<std::uint8_t>(pad&~Pad::down);
    auto resolved=resolve_opposing_directions(pad,flippers);
    if(has_opposing_directions(resolved)) resolved=resolve_opposing_directions(static_cast<std::uint8_t>(pad&~Pad::down),flippers);
    input.player_one=static_cast<std::uint16_t>(high|resolved);
    return input;
}
class GameInputRouter {
    FlipperInputs flippers_;
    bool launch_press_{};
    bool initials_press_{};
public:
    GameInputRouter()=default;
    explicit GameInputRouter(FlipperInputs flippers):flippers_(std::move(flippers)) {}
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
        return route_game_input(input,launch_press_ || initials_press_,flippers_);
    }
};
}
