// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include <SDL3/SDL.h>
namespace alien_crush::host {
class Presentation {
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* source_{};
    SDL_Texture* stage_{};
    int sw{},sh{},iw{},ih{};
public:
    Presentation(const char* title,int width,int height);
    ~Presentation();
    Presentation(const Presentation&)=delete;
    Presentation& operator=(const Presentation&)=delete;
    SDL_Window* window() const { return window_; }
    void show(const Frame&);
};
}
