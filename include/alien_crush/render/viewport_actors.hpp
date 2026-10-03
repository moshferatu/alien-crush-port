// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/animation_scene.hpp"
#include "alien_crush/assets/actor_assets.hpp"
namespace alien_crush::render {
// RAM and SAT must describe the same sprite-assembly epoch. The displayed
// viewport uses the previous video snapshot, not the latest actor positions.
inline unsigned identify_viewport_actors(AnimationScene& scene,const assets::ActorAssets& assets,
    std::span<const std::byte> ram,std::span<const std::uint16_t> sat,std::uint64_t frame) {
    if(ram.size()!=8192 || sat.size()!=256) return 0;
    std::array<std::byte,8192> projected;
    std::copy(ram.begin(),ram.end(),projected.begin());
    projected[0x50]=std::byte{10}; // Original reserved score/ball HUD slots.
    const auto pieces=assets::assemble_table_actors(assets,projected);
    std::array<const assets::ActorSprite*,64> owners{};
    std::array<unsigned,64> ordinals{},next{};
    for(const auto& piece:pieces) {
        ordinals[piece.slot]=next[piece.actor]++;
        if(std::equal(piece.words.begin(),piece.words.end(),sat.begin()+piece.slot*4)) owners[piece.slot]=&piece;
    }
    const auto word=[&](unsigned lo,unsigned hi) {return std::to_integer<unsigned>(ram[lo])|
        (std::to_integer<unsigned>(ram[hi])<<8);};
    unsigned identified=0;
    for(auto& layer:scene.layers) {
        const auto slot=static_cast<unsigned>(layer.id);
        if(slot>=64) continue;
        if(const auto* piece=owners[slot]) {
            const unsigned actor=piece->actor,type=std::to_integer<unsigned>(ram[0x1500+actor]);
            layer.id=(static_cast<std::uint64_t>(type)<<32)|(actor<<8)|ordinals[slot];
            layer.actor_group=layer.id&~std::uint64_t{255};
            layer.swings=type==2 || type==3;
            const double x=static_cast<std::int16_t>(word(0x15c0+actor,0x1600+actor))-32;
            const double y=static_cast<std::int16_t>(word(0x1680+actor,0x16c0+actor))-64-static_cast<int>(word(0x16,0x17));
            layer.pose_offset={layer.position.x-x,layer.position.y-y};
            ++identified;
        } else {
            // HUD slots are fixed. Unverified dynamic slots must never match
            // an unrelated actor from a previous frame by slot number alone.
            layer.id=(std::uint64_t{1}<<63)|(slot<10?slot:(frame<<8)|slot);
        }
    }
    return identified;
}
}
