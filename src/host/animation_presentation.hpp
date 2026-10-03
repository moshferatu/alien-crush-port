// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/animation_scene.hpp"
#include "alien_crush/render/flipper_swing.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <numbers>
#include <stdexcept>
namespace alien_crush::host {
// All images upload once per simulation tick. Intermediate refreshes only draw
// GPU layers. Blend two complete opaque scenes so changing transparent poses
// have correct alpha, without a dark fringe or two separated moving sprites.
// Flippers are the exception: a pose change is the current pose turning about
// its pivot from the previous pose's angle (render::flipper_swing), with a
// fading trail over the angles swept since the previous picture.
class AnimationPresentation {
    struct Layer {
        std::uint64_t id;render::MotionPosition position;SDL_FRect source;render::MotionPosition pose_offset;std::uint64_t actor_group;
        bool swings{};
        // Shared by every piece of a flipper, with the turn of the tick before
        // so a trail only reaches back into a swing already going the same way.
        std::optional<render::FlipperSwing> swing;
        double prior_turn{};
    };
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
    // The previous tick's flipper pieces, to measure a pose change against.
    std::vector<render::AnimationLayer> flippers_;
    // The fraction of the last draw: within [0, 1] for this tick, negative for
    // the tick before, and above one when neither drew.
    double drawn_{2};
    static void check(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
    SDL_Texture* texture(int width,int height,int access) {
        auto* result=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,static_cast<SDL_TextureAccess>(access),width,height);
        check(result!=nullptr);check(SDL_SetTextureScaleMode(result,SDL_SCALEMODE_NEAREST));return result;
    }
public:
    explicit AnimationPresentation(SDL_Renderer* renderer):renderer_(renderer) {}
    ~AnimationPresentation() {SDL_DestroyTexture(old_scene_);SDL_DestroyTexture(new_scene_);}
    void reset() {valid_=linked_=false;flippers_.clear();drawn_=2;}
    void update(const render::AnimationScene& scene,std::uint64_t frame) {
        linked_=valid_ && frame==frame_+1 && key_==scene.key &&
            pictures_[current_].width==scene.background.width && pictures_[current_].height==scene.background.height;
        frame_=frame;key_=scene.key;valid_=true;current_=1-current_;
        drawn_=drawn_>=0 && drawn_<=1?drawn_-1:2;
        auto& output=pictures_[current_];const auto& before=pictures_[1-current_];
        const auto in_group=[](std::uint64_t group) {return [group](const Layer& layer) {return layer.actor_group==group;};};
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
            Layer entry{layer.id,layer.position,{float(x),float(y),float(layer.image.width),float(layer.image.height)},layer.pose_offset,layer.actor_group,
                layer.swings && layer.actor_group,{},0};
            if(entry.swings && linked_) {
                const auto piece=std::find_if(output.layers.begin(),output.layers.end(),in_group(layer.actor_group));
                if(piece!=output.layers.end()) {entry.swing=piece->swing;entry.prior_turn=piece->prior_turn;}
                else {
                    entry.swing=render::flipper_swing(flippers_,scene.layers,layer.actor_group);
                    const auto old=std::find_if(before.layers.begin(),before.layers.end(),in_group(layer.actor_group));
                    if(old!=before.layers.end() && old->swing) entry.prior_turn=old->swing->degrees;
                }
            }
            output.layers.push_back(entry);
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
        flippers_.clear();
        for(const auto& layer:scene.layers) if(layer.swings) flippers_.push_back(layer);
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
        const double trail_from=drawn_;drawn_=t;
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
            // The current pose turned back toward the previous one's angle.
            // A display has no picture for most of the angles between, so
            // those since the previous picture are swept as a fading trail.
            // On the tick itself with no trail this is the plain, exact pose.
            const auto draw_swing=[&](const Layer& layer,const Layer& old) {
                constexpr double longest_trail=.5,trail_spacing=2;
                constexpr int most_copies=16;
                const auto& turn=*layer.swing;
                double from=std::clamp(trail_from,t-longest_trail,t);
                if(from<0 && layer.prior_turn*turn.degrees<=0) from=0;
                from=std::min(from,t);
                const auto angle_at=[&](double at) {return turn.degrees*(1-at)*std::numbers::pi/180;};
                const double left=layer.position.x,top=layer.position.y;
                const render::MotionPosition corners[]{{left,top},{left+layer.source.w,top},
                    {left,top+layer.source.h},{left+layer.source.w,top+layer.source.h}};
                double reach=0;
                for(const auto& corner:corners)
                    reach=std::max(reach,std::hypot((corner.x-turn.pivot_x)*sx,(corner.y-turn.pivot_y)*sy));
                const int copies=std::min(most_copies,
                    static_cast<int>(std::ceil(reach*std::abs(angle_at(from)-angle_at(t))/trail_spacing)));
                // Turned in source pixels and scaled afterwards, so the target's
                // pixel shape does not skew the swing.
                const auto turned=[&](double at,float opacity) {
                    const double c=std::cos(angle_at(at)),s=std::sin(angle_at(at));
                    SDL_Vertex vertices[4];
                    for(int i=0;i<4;++i) {
                        const double x=corners[i].x-turn.pivot_x,y=corners[i].y-turn.pivot_y;
                        vertices[i].position={float((turn.pivot_x+c*x-s*y)*sx),float((turn.pivot_y+s*x+c*y)*sy)};
                        vertices[i].color={1,1,1,opacity};
                        vertices[i].tex_coord={(layer.source.x+(i%2?layer.source.w:0))/256,
                            (layer.source.y+(i/2?layer.source.h:0))/float(now.atlas_height)};
                    }
                    constexpr int indices[]{0,1,2,2,1,3};
                    check(SDL_RenderGeometry(renderer_,now.atlas,vertices,4,indices,6));
                };
                // Oldest and faintest first, so the newest angles lie on top.
                for(int copy=copies;copy>=1;--copy)
                    turned(t-(t-from)*copy/copies,1-float(copy)/float(copies+1));
                if(t<1) turned(t,1);
                else draw_pose(layer,old,layer,now.atlas);
            };
            const auto draw_current=[&](const Layer& layer,const Layer& old) {
                if(layer.swing) draw_swing(layer,old);
                else draw_pose(layer,old,layer,now.atlas);
            };
            // Keep each complete pose, even when its sprite count changes.
            // Otherwise a two-piece launcher becoming one piece loses part of
            // its previous pose before the blend has even started.
            if(previous && linked_) {
                for(auto old=before.layers.begin();old!=before.layers.end();++old) {
                    const auto current=find_actor(now,*old);
                    if(current==now.layers.end() || !continuous(*old,*current)) continue;
                    if(!current->swings) {draw_pose(*old,*old,*current,before.atlas);continue;}
                    // A flipper's poses turn into one another instead of
                    // fading: both scenes draw the current one, where the
                    // previous one stood in the drawing order.
                    if(find_actor(before,*old)!=old) continue;
                    for(const auto& piece:now.layers) if(piece.actor_group==old->actor_group) draw_current(piece,*old);
                }
                for(const auto& layer:now.layers) {
                    const auto old=find_actor(before,layer);
                    if(old==before.layers.end() || !continuous(*old,layer)) draw_pose(layer,layer,layer,now.atlas);
                }
            } else for(const auto& layer:now.layers) {
                const auto old=linked_?find_actor(before,layer):before.layers.end();
                draw_current(layer,old==before.layers.end()?layer:*old);
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
