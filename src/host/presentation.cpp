// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "alien_crush/render/sharp_pixel_scaler.hpp"
#include <stdexcept>
#include <algorithm>
namespace alien_crush::host {
namespace { void checked(bool ok) { if(!ok) throw std::runtime_error(SDL_GetError()); } }
Presentation::Presentation(const char* title,int width,int height,bool hidden) {
    checked(SDL_CreateWindowAndRenderer(title,width,height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(hidden?SDL_WINDOW_HIDDEN:0),&window_,&renderer_));
    SDL_SetRenderVSync(renderer_,0); // Auxiliary windows never wait on another monitor.
}
Presentation::~Presentation() { SDL_DestroyTexture(stage_); SDL_DestroyTexture(source_); SDL_DestroyRenderer(renderer_); SDL_DestroyWindow(window_); }
void Presentation::show(const Frame& input,std::uint32_t aspect_width,std::uint32_t aspect_height,bool bottom_aligned,const Frame& background_tile) {
    auto frame=input;
    if(frame.pixels.empty()) return;
    int width{},height{}; checked(SDL_GetRenderOutputSize(renderer_,&width,&height));
    const bool sideways=rotation_==90 || rotation_==270;
    auto layout=render::calculate_scale_layout(sideways?height:width,sideways?width:height,frame.width,frame.height,aspect_width,aspect_height);
    if(!layout.valid()) return;
    std::vector<std::uint32_t> extended;
    if(!background_tile.pixels.empty() && background_tile.width>0 && background_tile.height>0) {
        // Extend in source pixels before sharp scaling and rotation, preserving
        // the pattern phase at both edges of the centered original viewport.
        const int available_width=sideways?height:width,available_height=sideways?width:height;
        const int extra_x=((available_width-layout.destination.width)*frame.width+2*layout.destination.width-1)/(2*layout.destination.width);
        const int extra_y=((available_height-layout.destination.height)*frame.height+2*layout.destination.height-1)/(2*layout.destination.height);
        const int padded_width=frame.width+2*extra_x,padded_height=frame.height+2*extra_y;
        extended.resize(static_cast<std::size_t>(padded_width)*padded_height);
        for(int y=0;y<padded_height;++y) for(int x=0;x<padded_width;++x) {
            const int tx=((x-extra_x)%background_tile.width+background_tile.width)%background_tile.width;
            const int ty=((y-extra_y)%background_tile.height+background_tile.height)%background_tile.height;
            extended[static_cast<std::size_t>(y)*padded_width+x]=background_tile.pixels[static_cast<std::size_t>(ty)*background_tile.width+tx];
        }
        for(int y=0;y<frame.height;++y)
            std::copy_n(frame.pixels.begin()+y*frame.width,frame.width,extended.begin()+(y+extra_y)*padded_width+extra_x);
        layout.destination.width=(layout.destination.width*padded_width+frame.width/2)/frame.width;
        layout.destination.height=(layout.destination.height*padded_height+frame.height/2)/frame.height;
        layout.stage_width=padded_width*layout.prescale_x;
        layout.stage_height=padded_height*layout.prescale_y;
        frame.width=padded_width;frame.height=padded_height;frame.pixels=extended;
    }
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
    checked(SDL_SetRenderTarget(renderer_,nullptr));
    SDL_FRect dest{static_cast<float>(width-layout.destination.width)/2,static_cast<float>(height-layout.destination.height)/2,static_cast<float>(layout.destination.width),static_cast<float>(layout.destination.height)};
    // Rotation is around the destination center. Shift by the unused physical
    // window height so even sideways tables meet the bottom edge after rotation.
    if(bottom_aligned) dest.y+=static_cast<float>(height-(sideways?layout.destination.width:layout.destination.height))/2;
    destination_=dest;
    if(!deferred_) repeat();
}
void Presentation::repeat() {
    checked(SDL_SetRenderDrawColor(renderer_,0,0,0,255)); checked(SDL_RenderClear(renderer_));
    if(stage_) checked(SDL_RenderTextureRotated(renderer_,stage_,nullptr,&destination_,rotation_,nullptr,SDL_FLIP_NONE));
    checked(SDL_RenderPresent(renderer_));
}
void Presentation::panel(const char* text) {
    int width{},height{}; checked(SDL_GetRenderOutputSize(renderer_,&width,&height));
    checked(SDL_SetRenderDrawColor(renderer_,0,0,0,255)); checked(SDL_RenderClear(renderer_));
    const auto scale=static_cast<float>(std::max(1,std::min(width/256,height/80)));
    checked(SDL_SetRenderScale(renderer_,scale,scale));
    checked(SDL_SetRenderDrawColor(renderer_,232,152,48,255));
    checked(SDL_RenderDebugText(renderer_,8,8,text));
    checked(SDL_SetRenderScale(renderer_,1,1)); checked(SDL_RenderPresent(renderer_));
}
}
