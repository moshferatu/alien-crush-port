// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "launch_input.hpp"
#include <SDL3/SDL.h>
namespace alien_crush::host {
inline PadInput device_game_input(std::span<const bool> keys,std::span<const bool> buttons) {
    auto key=[&](SDL_Scancode k){return static_cast<std::size_t>(k)<keys.size() && keys[k];};
    auto button=[&](SDL_GamepadButton b){return static_cast<std::size_t>(b)<buttons.size() && buttons[b];};
    PadInput input;
    if(key(SDL_SCANCODE_LSHIFT)||key(SDL_SCANCODE_LEFT)||button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)||button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) input.player_one|=Pad::left;
    if(key(SDL_SCANCODE_RSHIFT)||key(SDL_SCANCODE_RIGHT)||button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)||button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) input.player_one|=Pad::right;
    if(button(SDL_GAMEPAD_BUTTON_SOUTH)) input.player_one|=Pad::button_i;
    if(key(SDL_SCANCODE_X)||button(SDL_GAMEPAD_BUTTON_EAST)) input.player_one|=Pad::button_ii;
    if(key(SDL_SCANCODE_RETURN)||button(SDL_GAMEPAD_BUTTON_START)) input.player_one|=Pad::run;
    if(key(SDL_SCANCODE_BACKSPACE)||button(SDL_GAMEPAD_BUTTON_BACK)) input.player_one|=Pad::select;
    if(key(SDL_SCANCODE_UP)||button(SDL_GAMEPAD_BUTTON_DPAD_UP)) input.player_one|=Pad::up;
    if(key(SDL_SCANCODE_DOWN)||button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) input.player_one|=Pad::down;
    return input;
}
}
