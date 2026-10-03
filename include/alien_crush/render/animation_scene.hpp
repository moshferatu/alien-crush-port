// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/render/motion_smoothing.hpp"
namespace alien_crush::render {
struct AnimationLayer {
    std::uint64_t id{};
    MotionPosition position;
    Image image;
    bool foreground{};
    // Sprite layout relative to the actor. Pose changes are not movement.
    MotionPosition pose_offset;
    std::uint64_t actor_group{};
    // A flipper: the game draws its swing as a few poses, so a pose change is
    // drawn as its artwork turning about its pivot rather than a crossfade.
    bool swings{};
};
struct AnimationScene {
    Image background;
    std::vector<AnimationLayer> layers;
    std::uint64_t key{};
    // Title background scroll, in source pixels. Zero for stationary scenes.
    MotionPosition scroll;
    int scroll_period{};
};
inline std::array<Sprite,64> animation_geometry(std::span<const std::uint16_t> sat) {
    std::array<Sprite,64> result;
    constexpr int heights[]{16,32,64,64};
    for(unsigned i=0;i<64;++i) {
        auto& s=result[i];s.x=(sat[i*4+1]&1023)-32;s.y=(sat[i*4]&1023)-64;
        s.width=(sat[i*4+3]&0x100)?32:16;s.height=heights[(sat[i*4+3]>>12)&3];
    }
    return result;
}
// Lossless split: only pixels actually owned by a sprite leave the background.
// Integer origins are retained separately from fractional motion positions.
inline AnimationScene split_animation(const Image& image,const CompositionCapture& capture,
    const std::array<Sprite,64>& sprites,std::uint64_t key) {
    AnimationScene result{capture.background,{},key};
    std::array<int,64> index;index.fill(-1);
    for(std::size_t p=0;p<image.pixels.size();++p) {
        const auto owner=capture.owner[p];
        if(!owner) {result.background.pixels[p]=image.pixels[p];continue;}
        const auto slot=owner-1;const auto& sprite=sprites[slot];
        const int x=static_cast<int>(p%image.width)-sprite.x,y=static_cast<int>(p/image.width)-sprite.y;
        if(x<0 || y<0 || x>=sprite.width || y>=sprite.height) {result.background.pixels[p]=image.pixels[p];continue;}
        if(index[slot]<0) {
            index[slot]=static_cast<int>(result.layers.size());
            result.layers.push_back({static_cast<std::uint64_t>(slot),{double(sprite.x),double(sprite.y)},
                {sprite.width,sprite.height,std::vector<std::uint32_t>(static_cast<std::size_t>(sprite.width)*sprite.height)}});
        }
        result.layers[index[slot]].image.pixels[y*sprite.width+x]=image.pixels[p];
    }
    // Lower SAT indices win when fractional positions overlap between ticks.
    std::sort(result.layers.begin(),result.layers.end(),[](const auto& a,const auto& b){return a.id>b.id;});
    return result;
}
inline AnimationScene viewport_animation(const Image& original,std::span<const std::uint16_t> vram,
    std::span<const std::uint16_t> palette,std::span<const std::uint16_t> sat,const Viewport& viewport,std::uint64_t key) {
    CompositionCapture capture;
    const auto rendered=compose(vram,palette,sat,viewport,{},{},0,&capture);
    // Preserve raster effects that cannot be represented by frozen registers.
    for(std::size_t p=0;p<original.pixels.size();++p) if(original.pixels[p]!=rendered.pixels[p]) {
        capture.owner[p]=0;capture.background.pixels[p]=original.pixels[p];
    }
    return split_animation(original,capture,animation_geometry(sat),key);
}
}
