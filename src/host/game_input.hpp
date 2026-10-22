// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cabinet_config.hpp"
#include "launch_input.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>
namespace alien_crush::host {
// Scancodes resolved once from the key names in alien-crush.ini, so reading the
// keyboard every frame is an array lookup.
struct ResolvedKeys {
    std::array<std::vector<SDL_Scancode>,kPadButtonCount> pads;
    std::vector<SDL_Scancode> launch;
};
inline std::vector<SDL_Scancode> resolve_key_names(const std::vector<std::string>& names,std::string_view field) {
    std::vector<SDL_Scancode> resolved;
    for(const auto& name:names) {
        const auto scancode=SDL_GetScancodeFromName(name.c_str());
        // A name a hand-edited file gets wrong is named and skipped, and the
        // rest of the input's keys still work.
        if(scancode==SDL_SCANCODE_UNKNOWN) std::cerr<<"Unknown key name \""<<name<<"\" bound to input."<<field<<"; ignoring it.\n";
        else resolved.push_back(scancode);
    }
    return resolved;
}
inline ResolvedKeys resolve_keys(const InputBindings& bindings) {
    ResolvedKeys resolved;
    for(std::size_t i=0;i<kPadButtonCount;++i)
        resolved.pads[i]=resolve_key_names(bindings.keys[i],pad_button_field(static_cast<PadButton>(i)));
    resolved.launch=resolved.pads[static_cast<std::size_t>(PadButton::one)];
    return resolved;
}

// The keys the game itself answers to, outside the pad.
struct HostKeys {
    std::vector<SDL_Scancode> smoothing,next_window,fullscreen,next_monitor;
    static bool has(const std::vector<SDL_Scancode>& keys,SDL_Scancode key) {return std::ranges::find(keys,key)!=keys.end();}
};
inline HostKeys resolve_host_keys(const InputBindings& bindings) {
    return {resolve_key_names(bindings.smoothing_keys,"smoothing"),resolve_key_names(bindings.next_window_keys,"next_window"),
        resolve_key_names(bindings.fullscreen_keys,"fullscreen"),resolve_key_names(bindings.next_monitor_keys,"next_monitor")};
}

// The keyboard, as alien-crush.ini binds it. While the ball is seated in the
// shooter a key bound to I sends I and nothing else -- its other inputs are
// suppressed, so a key that is I and Run launches rather than pausing --
// and a press that began as a launch stays one until it is let go, so the ball
// leaving the shooter cannot turn the held key into a pause.
class KeyboardInput {
    ResolvedKeys keys_;
    bool launching_{};
public:
    KeyboardInput():KeyboardInput(default_input_bindings()) {}
    explicit KeyboardInput(const InputBindings& bindings):keys_(resolve_keys(bindings)) {}
    PadInput read(std::span<const bool> keys,bool in_shooter) {
        const auto held=[&](SDL_Scancode k){return static_cast<std::size_t>(k)<keys.size() && keys[k];};
        const bool launch_held=std::ranges::any_of(keys_.launch,held);
        launching_=launch_held && (launching_ || in_shooter);
        const auto suppressed=[&](SDL_Scancode k){return launching_ && std::ranges::find(keys_.launch,k)!=keys_.launch.end();};
        PadInput input;
        for(std::size_t i=0;i<kPadButtonCount;++i)
            if(std::ranges::any_of(keys_.pads[i],[&](SDL_Scancode k){return held(k) && !suppressed(k);}))
                input.player_one|=pad_button_bit(static_cast<PadButton>(i));
        if(launching_) input.player_one|=Pad::button_i;
        return input;
    }
};

// The first SDL gamepad: shoulders or the d-pad's sides work the flippers,
// South is I, East is II (nudge), Start is Run and Back is Select.
inline PadInput gamepad_game_input(std::span<const bool> buttons,const FlipperInputs& flippers={}) {
    auto button=[&](SDL_GamepadButton b){return static_cast<std::size_t>(b)<buttons.size() && buttons[b];};
    PadInput input;
    if(button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) input.player_one|=flipper_pad(flippers.left,false);
    if(button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) input.player_one|=flipper_pad(flippers.right,true);
    if(button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) input.player_one|=Pad::left;
    if(button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) input.player_one|=Pad::right;
    if(button(SDL_GAMEPAD_BUTTON_DPAD_UP)) input.player_one|=Pad::up;
    if(button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) input.player_one|=Pad::down;
    if(button(SDL_GAMEPAD_BUTTON_SOUTH)) input.player_one|=Pad::button_i;
    if(button(SDL_GAMEPAD_BUTTON_EAST)) input.player_one|=Pad::button_ii;
    if(button(SDL_GAMEPAD_BUTTON_START)) input.player_one|=Pad::run;
    if(button(SDL_GAMEPAD_BUTTON_BACK)) input.player_one|=Pad::select;
    return input;
}
// The default keys and the gamepad together, outside the shooter.
inline PadInput device_game_input(std::span<const bool> keys,std::span<const bool> buttons) {
    KeyboardInput keyboard;
    auto input=keyboard.read(keys,false);
    input.player_one|=gamepad_game_input(buttons).player_one;
    return input;
}
}
