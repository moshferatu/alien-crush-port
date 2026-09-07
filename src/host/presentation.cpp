// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "alien_crush/render/sharp_pixel_scaler.hpp"
#include <stdexcept>
namespace alien_crush::host {
namespace { void checked(bool ok) { if(!ok) throw std::runtime_error(SDL_GetError()); } }
Presentation::Presentation(const char* title,int width,int height) {
    checked(SDL_CreateWindowAndRenderer(title,width,height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY,&window_,&renderer_));
    SDL_SetRenderVSync(renderer_,0); // A single game clock, independent of display refresh.
}
Presentation::~Presentation() { SDL_DestroyTexture(stage_); SDL_DestroyTexture(source_); SDL_DestroyRenderer(renderer_); SDL_DestroyWindow(window_); }
void Presentation::show(const Frame& frame) {
    if(frame.pixels.empty()) return;
    int width{},height{}; checked(SDL_GetRenderOutputSize(renderer_,&width,&height));
    const auto layout=render::calculate_scale_layout(width,height,frame.width,frame.height,4,3);
    if(!layout.valid()) return;
    if(sw!=frame.width || sh!=frame.height) {
        SDL_DestroyTexture(source_); source_=nullptr;
        source_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,frame.width,frame.height);
        checked(source_!=nullptr); checked(SDL_SetTextureScaleMode(source_,SDL_SCALEMODE_NEAREST));
        sw=frame.width; sh=frame.height;
    }
    if(iw!=layout.stage_width || ih!=layout.stage_height) {
        SDL_DestroyTexture(stage_); stage_=nullptr;
        stage_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,layout.stage_width,layout.stage_height);
        checked(stage_!=nullptr); checked(SDL_SetTextureScaleMode(stage_,SDL_SCALEMODE_LINEAR));
        iw=layout.stage_width; ih=layout.stage_height;
    }
    checked(SDL_UpdateTexture(source_,nullptr,frame.pixels.data(),frame.width*4));
    checked(SDL_SetRenderTarget(renderer_,stage_)); checked(SDL_RenderTexture(renderer_,source_,nullptr,nullptr));
    checked(SDL_SetRenderTarget(renderer_,nullptr)); checked(SDL_SetRenderDrawColor(renderer_,0,0,0,255)); checked(SDL_RenderClear(renderer_));
    const SDL_FRect dest{static_cast<float>(layout.destination.x),static_cast<float>(layout.destination.y),static_cast<float>(layout.destination.width),static_cast<float>(layout.destination.height)};
    checked(SDL_RenderTexture(renderer_,stage_,nullptr,&dest)); checked(SDL_RenderPresent(renderer_));
}
}
