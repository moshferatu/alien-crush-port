// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/animation_scene.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <stdexcept>
namespace alien_crush::host {
// All images upload once per simulation tick. Intermediate refreshes only draw
// GPU layers. Blend two complete opaque scenes so changing transparent poses
// have correct alpha, without a dark fringe or two separated moving sprites.
class AnimationPresentation {
    struct Layer {std::uint64_t id;render::MotionPosition position;SDL_FRect source;render::MotionPosition pose_offset;std::uint64_t actor_group;};
    struct Picture {
        SDL_Texture* background{};SDL_Texture* atlas{};
        std::vector<Layer> layers;
        int width{},height{},atlas_height{};
        int padding{};
        render::MotionPosition scroll;
        ~Picture() {SDL_DestroyTexture(background);SDL_DestroyTexture(atlas);}
    } pictures_[2];
    SDL_Renderer* renderer_;
    SDL_Texture* old_scene_{};SDL_Texture* new_scene_{};
    int current_{},width_{},height_{};
    std::uint64_t frame_{},key_{};
    bool valid_{},linked_{};
    static void check(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
    SDL_Texture* texture(int width,int height,int access) {
        auto* result=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,static_cast<SDL_TextureAccess>(access),width,height);
        check(result!=nullptr);check(SDL_SetTextureScaleMode(result,SDL_SCALEMODE_NEAREST));return result;
    }
public:
    explicit AnimationPresentation(SDL_Renderer* renderer):renderer_(renderer) {}
    ~AnimationPresentation() {SDL_DestroyTexture(old_scene_);SDL_DestroyTexture(new_scene_);}
    void reset() {valid_=linked_=false;}
    void update(const render::AnimationScene& scene,std::uint64_t frame) {
        linked_=valid_ && frame==frame_+1 && key_==scene.key &&
            pictures_[current_].width==scene.background.width && pictures_[current_].height==scene.background.height;
        frame_=frame;key_=scene.key;valid_=true;current_=1-current_;
        auto& output=pictures_[current_];
        const auto& bg=scene.background;
        const int padding=scene.scroll_period;
        if(output.width!=bg.width || output.height!=bg.height || output.padding!=padding) {
            SDL_DestroyTexture(output.background);output.background=texture(bg.width+2*padding,bg.height+2*padding,SDL_TEXTUREACCESS_STREAMING);
            output.width=bg.width;output.height=bg.height;output.padding=padding;
        }
        output.scroll=scene.scroll;
        if(padding) {
            const int w=bg.width+2*padding,h=bg.height+2*padding;
            std::vector<std::uint32_t> pixels(static_cast<std::size_t>(w)*h);
            for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
                int px=x-padding,py=y-padding;
                while(px<0) px+=padding;while(px>=bg.width) px-=padding;
                while(py<0) py+=padding;while(py>=bg.height) py-=padding;
                pixels[y*w+x]=bg.pixels[py*bg.width+px];
            }
            check(SDL_UpdateTexture(output.background,nullptr,pixels.data(),w*4));
        } else check(SDL_UpdateTexture(output.background,nullptr,bg.pixels.data(),bg.width*4));
        output.layers.clear();
        int x=0,y=0,row=0;
        for(const auto& layer:scene.layers) {
            if(layer.image.width>256) throw std::runtime_error("Animation sprite too wide");
            if(x+layer.image.width>256) {x=0;y+=row;row=0;}
            output.layers.push_back({layer.id,layer.position,{float(x),float(y),float(layer.image.width),float(layer.image.height)},layer.pose_offset,layer.actor_group});
            x+=layer.image.width;row=std::max(row,layer.image.height);
        }
        const int height=std::max(1,y+row);
        if(output.atlas_height!=height) {
            SDL_DestroyTexture(output.atlas);output.atlas=texture(256,height,SDL_TEXTUREACCESS_STREAMING);
            check(SDL_SetTextureBlendMode(output.atlas,SDL_BLENDMODE_BLEND));output.atlas_height=height;
        }
        std::vector<std::uint32_t> pixels(static_cast<std::size_t>(256)*height);
        for(std::size_t i=0;i<scene.layers.size();++i) {
            const auto& layer=scene.layers[i];const auto& rect=output.layers[i].source;
            for(int row_index=0;row_index<layer.image.height;++row_index)
                std::copy_n(layer.image.pixels.begin()+row_index*layer.image.width,layer.image.width,
                    pixels.begin()+(static_cast<int>(rect.y)+row_index)*256+static_cast<int>(rect.x));
        }
        check(SDL_UpdateTexture(output.atlas,nullptr,pixels.data(),256*4));
    }
    void draw(SDL_Texture* target,int width,int height,double fraction) {
        if(width!=width_ || height!=height_) {
            SDL_DestroyTexture(old_scene_);SDL_DestroyTexture(new_scene_);
            old_scene_=texture(width,height,SDL_TEXTUREACCESS_TARGET);
            new_scene_=texture(width,height,SDL_TEXTUREACCESS_TARGET);
            check(SDL_SetTextureBlendMode(new_scene_,SDL_BLENDMODE_BLEND));
            width_=width;height_=height;
        }
        const double t=linked_?std::clamp(fraction,0.0,1.0):1;
        const auto& now=pictures_[current_];const auto& before=pictures_[1-current_];
        const double sx=double(width)/now.width,sy=double(height)/now.height;
        render::MotionPosition scroll_delta;
        if(linked_ && now.padding && now.padding==before.padding) {
            scroll_delta={std::remainder(now.scroll.x-before.scroll.x,double(now.padding)),
                std::remainder(now.scroll.y-before.scroll.y,double(now.padding))};
        }
        const auto draw_scene=[&](SDL_Texture* surface,bool previous) {
            check(SDL_SetRenderTarget(renderer_,surface));
            const auto& background=previous && linked_?before:now;
            const auto offset=previous && linked_?t:t-1;
            const SDL_FRect background_rect{float((-background.padding-scroll_delta.x*offset)*sx),
                float((-background.padding-scroll_delta.y*offset)*sy),
                float((now.width+2*background.padding)*sx),float((now.height+2*background.padding)*sy)};
            check(SDL_RenderTexture(renderer_,background.background,nullptr,&background_rect));
            const auto find_actor=[](const Picture& picture,const Layer& layer) {
                return std::find_if(picture.layers.begin(),picture.layers.end(),[&](const auto& candidate) {
                    return layer.actor_group?candidate.actor_group==layer.actor_group:candidate.id==layer.id;
                });
            };
            const auto anchor=[](const Layer& layer) {return render::MotionPosition{
                layer.position.x-layer.pose_offset.x,layer.position.y-layer.pose_offset.y};};
            const auto continuous=[&](const Layer& a,const Layer& b) {
                const auto from=anchor(a),to=anchor(b);
                return std::abs(from.x-to.x)<=32 && std::abs(from.y-to.y)<=32;
            };
            const auto draw_pose=[&](const Layer& pose,const Layer& old,const Layer& current,SDL_Texture* atlas) {
                const auto a=anchor(old),b=anchor(current);
                const bool matched=std::abs(a.x-b.x)<=32 && std::abs(a.y-b.y)<=32;
                const auto p=matched?render::MotionPosition{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}:b;
                const SDL_FRect dest{float((p.x+pose.pose_offset.x)*sx),float((p.y+pose.pose_offset.y)*sy),float(pose.source.w*sx),float(pose.source.h*sy)};
                check(SDL_RenderTexture(renderer_,atlas,&pose.source,&dest));
            };
            // Keep each complete pose, even when its sprite count changes.
            // Otherwise a two-piece launcher becoming one piece loses part of
            // its previous pose before the blend has even started.
            if(previous && linked_) {
                for(const auto& old:before.layers) {
                    const auto current=find_actor(now,old);
                    if(current!=now.layers.end() && continuous(old,*current)) draw_pose(old,old,*current,before.atlas);
                }
                for(const auto& layer:now.layers) {
                    const auto old=find_actor(before,layer);
                    if(old==before.layers.end() || !continuous(*old,layer)) draw_pose(layer,layer,layer,now.atlas);
                }
            } else for(const auto& layer:now.layers) {
                const auto old=linked_?find_actor(before,layer):before.layers.end();
                draw_pose(layer,old==before.layers.end()?layer:*old,layer,now.atlas);
            }
        };
        if(t>=1) {draw_scene(target,false);return;}
        if(t<=0) {draw_scene(target,true);return;}
        draw_scene(old_scene_,true);draw_scene(new_scene_,false);
        check(SDL_SetRenderTarget(renderer_,target));
        check(SDL_RenderTexture(renderer_,old_scene_,nullptr,nullptr));
        check(SDL_SetTextureAlphaModFloat(new_scene_,static_cast<float>(t)));
        check(SDL_RenderTexture(renderer_,new_scene_,nullptr,nullptr));
    }
};
}
