// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "animation_presentation.hpp"
#include "alien_crush/render/sharp_pixel_scaler.hpp"
#include <stdexcept>
#include <algorithm>
namespace alien_crush::host {
namespace { void checked(bool ok) { if(!ok) throw std::runtime_error(SDL_GetError()); } }
Presentation::Presentation(const char* title,int width,int height,bool hidden) {
    checked(SDL_CreateWindowAndRenderer(title,width,height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(hidden?SDL_WINDOW_HIDDEN:0),&window_,&renderer_));
    SDL_SetRenderVSync(renderer_,0); // Auxiliary windows never wait on another monitor.
}
Presentation::~Presentation() { animation_.reset(); SDL_DestroyTexture(foreground_);SDL_DestroyTexture(ball_);SDL_DestroyTexture(stage_); SDL_DestroyTexture(source_); SDL_DestroyRenderer(renderer_); SDL_DestroyWindow(window_); }
void Presentation::reset_smoothing() {motion_.reset();layered_=animated_=false;if(animation_) animation_->reset();}
float Presentation::refresh_rate() const {
    const auto* mode=SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_));
    return mode?mode->refresh_rate:0;
}
void Presentation::show(const Frame& input,std::uint32_t aspect_width,std::uint32_t aspect_height,bool bottom_aligned,const Frame& background_tile,
    const render::TableBallBackdrop* backdrop,const render::Sprite* ball,render::MotionPosition position,double fraction,const render::AnimationScene* animation) {
    auto frame=input;
    if(frame.pixels.empty()) return;
    animated_=animation!=nullptr;
    if(!animated_ && animation_) animation_->reset();
    layered_=!animated_ && backdrop && ball && backdrop->image.width==frame.width && backdrop->image.height==frame.height &&
        backdrop->image.pixels.size()==frame.pixels.size() && backdrop->sprites.size()==frame.pixels.size() &&
        ball->width==16 && ball->height==16 && ball->image.pixels.size()==256 && background_tile.pixels.empty();
    if(layered_) {frame.pixels=backdrop->image.pixels;motion_.update(frame.number,position);}
    else motion_.reset();
    int width{},height{}; checked(SDL_GetRenderOutputSize(renderer_,&width,&height));
    const bool sideways=rotation_==90 || rotation_==270;
    auto layout=render::calculate_scale_layout(sideways?height:width,sideways?width:height,frame.width,frame.height,aspect_width,aspect_height);
    if(!layout.valid()) return;
    if(layered_ || animated_) {
        // Four or more target pixels per console pixel retain fractional motion
        // before the final sharp downsample, like ZAMN's GPU layer renderer.
        layout.stage_width=frame.width*std::max(4,layout.prescale_x);
        layout.stage_height=frame.height*std::max(4,layout.prescale_y);
    }
    render::AnimationScene padded_animation;
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
        if(animated_) {
            padded_animation=*animation;
            padded_animation.background={padded_width,padded_height,extended};
            for(int y=0;y<input.height;++y)
                std::copy_n(animation->background.pixels.begin()+y*input.width,input.width,
                    padded_animation.background.pixels.begin()+(y+extra_y)*padded_width+extra_x);
            for(auto& layer:padded_animation.layers) {layer.position.x+=extra_x;layer.position.y+=extra_y;}
            animation=&padded_animation;
            layout.stage_width=padded_width*std::max(4,layout.prescale_x);
            layout.stage_height=padded_height*std::max(4,layout.prescale_y);
        }
        frame.width=padded_width;frame.height=padded_height;frame.pixels=extended;
    }
    if(sw!=frame.width || sh!=frame.height) {
        SDL_DestroyTexture(foreground_);foreground_=nullptr;
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
    if(layered_) {
        if(!ball_) {
            ball_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,16,16);
            checked(ball_!=nullptr);checked(SDL_SetTextureScaleMode(ball_,SDL_SCALEMODE_NEAREST));
            checked(SDL_SetTextureBlendMode(ball_,SDL_BLENDMODE_BLEND));
        }
        if(!foreground_) {
            foreground_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,frame.width,frame.height);
            checked(foreground_!=nullptr);checked(SDL_SetTextureScaleMode(foreground_,SDL_SCALEMODE_NEAREST));
            checked(SDL_SetTextureBlendMode(foreground_,SDL_BLENDMODE_BLEND));
        }
        std::vector<std::uint32_t> foreground(frame.pixels.size());
        for(std::size_t i=0;i<foreground.size();++i) if(backdrop->sprites[i]) foreground[i]=frame.pixels[i];
        checked(SDL_UpdateTexture(ball_,nullptr,ball->image.pixels.data(),16*4));
        checked(SDL_UpdateTexture(foreground_,nullptr,foreground.data(),frame.width*4));
    }
    if(animated_) {
        if(!animation_) animation_=std::make_unique<AnimationPresentation>(renderer_);
        animation_->update(*animation,frame.number);
    }
    render_stage(fraction);
    SDL_FRect dest{static_cast<float>(width-layout.destination.width)/2,static_cast<float>(height-layout.destination.height)/2,static_cast<float>(layout.destination.width),static_cast<float>(layout.destination.height)};
    // Rotation is around the destination center. Shift by the unused physical
    // window height so even sideways tables meet the bottom edge after rotation.
    if(bottom_aligned) dest.y+=static_cast<float>(height-(sideways?layout.destination.width:layout.destination.height))/2;
    destination_=dest;
    if(!deferred_) {render_window();checked(SDL_RenderPresent(renderer_));++pictures_;if(layered_ || animated_) ++smoothed_pictures_;}
}
void Presentation::render_stage(double fraction) {
    if(animated_) {animation_->draw(stage_,iw,ih,fraction);checked(SDL_SetRenderTarget(renderer_,nullptr));return;}
    checked(SDL_SetRenderTarget(renderer_,stage_));
    checked(SDL_RenderTexture(renderer_,source_,nullptr,nullptr));
    if(layered_) {
        const auto position=motion_.position(fraction);
        const auto sx=static_cast<double>(iw)/sw,sy=static_cast<double>(ih)/sh;
        const SDL_FRect ball_rect{static_cast<float>(position.x*sx),static_cast<float>(position.y*sy),
            static_cast<float>(16*sx),static_cast<float>(16*sy)};
        checked(SDL_RenderTexture(renderer_,ball_,nullptr,&ball_rect));
        checked(SDL_RenderTexture(renderer_,foreground_,nullptr,nullptr));
    }
    checked(SDL_SetRenderTarget(renderer_,nullptr));
}
void Presentation::render_window() {
    checked(SDL_SetRenderDrawColor(renderer_,0,0,0,255)); checked(SDL_RenderClear(renderer_));
    if(stage_) checked(SDL_RenderTextureRotated(renderer_,stage_,nullptr,&destination_,rotation_,nullptr,SDL_FLIP_NONE));
}
void Presentation::repeat(double fraction) {
    if(layered_ || animated_) render_stage(fraction);
    render_window();
    checked(SDL_RenderPresent(renderer_));
    ++pictures_;if(layered_ || animated_) ++smoothed_pictures_;
}
render::Image Presentation::capture() {
    render_window();
    auto* pixels=SDL_RenderReadPixels(renderer_,nullptr);
    checked(pixels!=nullptr);
    auto* converted=SDL_ConvertSurface(pixels,SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(pixels);checked(converted!=nullptr);
    render::Image result{converted->w,converted->h,std::vector<std::uint32_t>(static_cast<std::size_t>(converted->w)*converted->h)};
    for(int y=0;y<converted->h;++y)
        std::copy_n(reinterpret_cast<const std::uint32_t*>(static_cast<const unsigned char*>(converted->pixels)+y*converted->pitch),converted->w,result.pixels.begin()+y*converted->w);
    SDL_DestroySurface(converted);return result;
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
