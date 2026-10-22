// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <optional>
namespace alien_crush::host {
class AnimationPresentation;
class Presentation {
    std::unique_ptr<AnimationPresentation> animation_;
    bool animated_{};
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* source_{};
    SDL_Texture* stage_{};
    SDL_Texture* ball_{};
    SDL_Texture* foreground_{};
    render::MotionSmoothing motion_;
    bool layered_{};
    int sw{},sh{},iw{},ih{};
    int pad_x_{},pad_y_{};
    int rotation_{};
    SDL_FRect destination_{};
    bool deferred_{};
    std::uint64_t pictures_{},smoothed_pictures_{};
    void render_stage(double fraction);
    void render_window();
public:
    // A floating window never takes the keyboard and stays above the others.
    Presentation(const char* title,int width,int height,bool hidden=false,bool floating=false,bool borderless=false);
    ~Presentation();
    Presentation(const Presentation&)=delete;
    Presentation& operator=(const Presentation&)=delete;
    SDL_Window* window() const { return window_; }
    void reveal();
    bool enable_vsync() { return SDL_SetRenderVSync(renderer_,1); }
    float refresh_rate() const;
    std::uint64_t pictures() const {return pictures_;}
    std::uint64_t smoothed_pictures() const {return smoothed_pictures_;}
    void reset_smoothing();
    void defer(bool value) { deferred_=value; }
    void repeat(double fraction=1);
    void show(const Frame&,std::uint32_t aspect_width=4,std::uint32_t aspect_height=3,bool bottom_aligned=false,
        const Frame& background_tile={},const render::TableBallBackdrop* backdrop=nullptr,
        const render::Sprite* ball=nullptr,render::MotionPosition position={},double fraction=1,const render::AnimationScene* animation=nullptr);
    // Read back the presented output for renderer regression tests/screenshots.
    render::Image capture();
    void panel(const char* text);
    // The pixel of the frame last shown that a place in the window is over,
    // through the scaling and the window's rotation; it may lie outside.
    std::optional<SDL_FPoint> frame_point(float window_x,float window_y);
    int rotation() const { return rotation_; }
    void rotation(int degrees) { rotation_=degrees; }
};
}
