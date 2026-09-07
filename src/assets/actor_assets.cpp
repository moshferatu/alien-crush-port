// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
namespace alien_crush::assets {
std::vector<ActorPiece> decode_actor_pieces(std::span<const std::byte> data) {
    if(data.empty()) throw std::runtime_error("Missing actor descriptor");
    const auto count=std::to_integer<unsigned>(data[0]);
    // Y is an 8-bit index in the original routine. Reject forms that would wrap.
    if(!count || count>42 || data.size()<1+6*count) throw std::runtime_error("Invalid or truncated sprite descriptor");
    auto b=[&](std::size_t i){return std::to_integer<unsigned>(data[i]);};
    auto signed_byte=[&](std::size_t i){const auto v=static_cast<int>(b(i));return v>=128?v-256:v;};
    std::vector<ActorPiece> result;
    for(unsigned i=0;i<count;++i) {
        const auto p=1+6*i;
        result.push_back({signed_byte(p),signed_byte(p+1),static_cast<std::uint16_t>(b(p+2)|(b(p+3)<<8)),
            static_cast<std::uint16_t>(b(p+4)|(b(p+5)<<8))});
    }
    return result;
}
ActorAssets::ActorAssets(std::span<const std::byte> image) {
    if(!rom::matches_supported_rom(image.size(),rom::compute_digests(image))) throw std::runtime_error("Unsupported ROM for actor extraction");
    std::copy_n(image.begin()+0x2c000,banks_.size(),banks_.begin());
}
std::vector<ActorPiece> ActorAssets::pieces(std::uint8_t id,unsigned first_slot) const {
    const auto address=std::to_integer<unsigned>(banks_[0x24b+id])|(std::to_integer<unsigned>(banks_[0x34b+id])<<8);
    if(address<0xa000 || address>=0xe000) throw std::runtime_error("Actor pointer outside mapped banks");
    if(first_slot>=64) throw std::runtime_error("Invalid actor starting SAT slot");
    // Test capacity before reading pieces, just like 16:A014..A01F.
    if(std::to_integer<unsigned>(banks_[address-0xa000])+first_slot>=64) return {};
    try { return decode_actor_pieces(std::span(banks_).subspan(address-0xa000)); }
    catch(const std::runtime_error& e) { throw std::runtime_error(std::string(e.what())+" (descriptor "+std::to_string(id)+")"); }
}
std::vector<ActorSprite> assemble_table_actors(const ActorAssets& assets,std::span<const std::byte> ram) {
    if(ram.size()!=8192) throw std::runtime_error("Actor assembly requires 8192-byte work RAM");
    auto b=[&](std::size_t i){return std::to_integer<unsigned>(ram[i]);};
    auto word=[&](std::size_t lo,std::size_t hi){return b(lo)|(b(hi)<<8);};
    unsigned slot=b(0x50);
    if(slot>=64) throw std::runtime_error("Invalid actor starting SAT slot");
    std::vector<unsigned> order,front,back;
    for(unsigned x=4;x<64;++x) {
        const auto flags=b(0x1540+x);
        if((flags&0xa0)!=0x80) continue;
        if(flags&4) order.push_back(x);
        else if(flags&8) front.push_back(x);
        else back.push_back(x);
    }
    order.insert(order.end(),front.begin(),front.end());
    if(b(0x1500) && (b(0x1540)&0xa0)==0x80) order.push_back(0);
    order.insert(order.end(),back.begin(),back.end());
    std::vector<ActorSprite> result;
    for(const auto x:order) {
        const auto id=static_cast<std::uint8_t>(b(0x1800+x));
        const auto pieces=assets.pieces(id,slot);
        for(const auto& piece:pieces) {
            auto flags=piece.flags;
            if(b(0x1a00+x)&0x20) flags|=static_cast<std::uint16_t>(b(0x1a80+x));
            if(b(0x1a00+x)&0x80) flags|=static_cast<std::uint16_t>(b(0x1980+x)<<8);
            result.push_back({static_cast<std::uint8_t>(x),id,static_cast<std::uint8_t>(slot++),{
                static_cast<std::uint16_t>(static_cast<int>(word(0x1680+x,0x16c0+x))+piece.y-static_cast<int>(word(0x16,0x17))),
                static_cast<std::uint16_t>(static_cast<int>(word(0x15c0+x,0x1600+x))+piece.x),
                static_cast<std::uint16_t>(piece.pattern+word(0x2e,0x2f)),flags}});
        }
    }
    return result;
}
}
