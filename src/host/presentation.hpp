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
    int rotation_{};
    SDL_FRect destination_{};
    bool deferred_{};
public:
    Presentation(const char* title,int width,int height,bool hidden=false);
    ~Presentation();
    Presentation(const Presentation&)=delete;
    Presentation& operator=(const Presentation&)=delete;
    SDL_Window* window() const { return window_; }
    bool enable_vsync() { return SDL_SetRenderVSync(renderer_,1); }
    void defer(bool value) { deferred_=value; }
    void repeat();
    void show(const Frame&,std::uint32_t aspect_width=4,std::uint32_t aspect_height=3,bool bottom_aligned=false,
        const Frame& background_tile={});
    void panel(const char* text);
    int rotation() const { return rotation_; }
    void rotation(int degrees) { rotation_=degrees; }
};
}
