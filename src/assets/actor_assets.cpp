// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/core/rom_image.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
namespace alien_crush::assets {
std::vector<BackgroundWrite> restored_actor_backgrounds(const ActorAssets& assets,std::span<const std::byte> ram) {
    if(ram.size()!=8192) throw std::runtime_error("Background restoration requires 8192-byte work RAM");
    auto b=[&](std::size_t p){return std::to_integer<std::uint8_t>(ram[p]);};
    std::vector<BackgroundWrite> result;
    for(unsigned actor=4;actor<64;++actor) {
        const auto type=b(0x1500+actor);
        // 01:4FFB..5003 and 01:52C1..52C9 force a background upload when
        // returning to a table. Do not infer this behavior from flags alone.
        if((type!=7 && type!=20) || (b(0x1540+actor)&0xa0)!=0xa0) continue;
        const auto writes=assets.background(b(0x1800+actor),(b(0x1a00+actor)&0x20)?b(0x1a80+actor):0);
        for(const auto& write:writes)
            if(write.address>=1024) throw std::runtime_error("Restored background actor writes outside table BAT");
        result.insert(result.end(),writes.begin(),writes.end());
    }
    return result;
}
std::array<std::byte,8192> inactive_table_actor_ram(std::span<const std::byte> ram) {
    if(ram.size()!=8192) throw std::runtime_error("Inactive actors require 8192-byte work RAM");
    std::array<std::byte,8192> result{};
    for(unsigned field=0;field<32;++field)
        std::copy_n(ram.begin()+0x204+field*64,60,result.begin()+0x1504+field*64);
    // Both recovered main tables use pattern base 0200 and ten reserved HUD slots.
    // Camera is local zero; original ordering and SAT capacity remain unchanged.
    result[0x2f]=std::byte{2}; result[0x50]=std::byte{10};
    return result;
}
std::array<TableSprite,10> assemble_table_hud(std::uint32_t score_points,unsigned ball_counter) {
    if(score_points>999999900 || score_points%100 || ball_counter<1 || ball_counter>10)
        throw std::runtime_error("HUD requires settled main-table score and ball counter");
    std::array<TableSprite,10> result{};
    for(unsigned i=9;i>0;--i) {
        const auto slot=i-1,digit=score_points%10;
        score_points/=10;
        result[slot]={static_cast<std::uint8_t>(slot),{80,static_cast<std::uint16_t>(48+10*slot),
            static_cast<std::uint16_t>((20+digit)*2),0x8f}};
    }
    result[9]={9,{96,264,static_cast<std::uint16_t>((19+ball_counter)*2),0x8f}};
    return result;
}
std::vector<BackgroundWrite> decode_actor_background(std::span<const std::byte> data,std::uint8_t high_override) {
    if(data.size()<6) throw std::runtime_error("Truncated background descriptor header");
    auto b=[&](std::size_t p){return std::to_integer<unsigned>(data[p]);};
    auto word=[&](std::size_t p){return b(p)|(b(p+1)<<8);};
    const auto width=b(2),height=b(3),count=width*height;
    if(!width || !height || count>125 || data.size()<6+count*2)
        throw std::runtime_error("Invalid, wrapping or truncated background descriptor");
    std::vector<BackgroundWrite> result;
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        const auto value=static_cast<std::uint16_t>(word(6+(y*width+x)*2)+word(4));
        result.push_back({static_cast<std::uint16_t>(word(0)+y*32+x),
            static_cast<std::uint16_t>(value|(static_cast<unsigned>(high_override)<<8))});
    }
    return result;
}
std::vector<TableSprite> assemble_table_overlay(std::span<const std::byte> ram,unsigned first_slot) {
    if(ram.size()!=8192 || first_slot>64) throw std::runtime_error("Invalid table overlay RAM or starting slot");
    auto b=[&](std::size_t p){return std::to_integer<unsigned>(ram[p]);};
    const auto table=b(0x79);
    // 01:4741..4756 deliberately leaves slot 63 unused when emitting four pieces.
    if(table>=2 || (b(0x53)&0x80) || (b(0x54)&0x80) || first_slot>=60) return {};
    constexpr unsigned xs[]{132,136},ys[]{191,245}; // 01:480D..4814
    const auto y=static_cast<std::uint16_t>(ys[table]-(b(0x16)|(b(0x17)<<8)));
    const auto base=b(0x2e)|(b(0x2f)<<8);
    // ASL's carry is discarded at 47A6. The trailing pieces use only base's
    // incremented high byte, not the low byte used by the first piece.
    const auto first=static_cast<std::uint16_t>(base+0x100+((b(0x99+table)*2)&255));
    const auto blank=static_cast<std::uint16_t>(((b(0x2f)+1)&255)<<8);
    std::vector<TableSprite> result;
    for(unsigned i=0;i<4;++i) result.push_back({static_cast<std::uint8_t>(first_slot+i),
        {y,static_cast<std::uint16_t>(xs[table]+i*8),i?blank:first,0x008f}});
    return result;
}
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
    std::copy_n(image.begin()+0x8000,sequences_.size(),sequences_.begin());
}
void ActorAssets::mirror_flippers(std::span<std::byte> projected,std::span<const std::byte> live,unsigned half) const {
    if(projected.size()!=8192 || live.size()!=8192 || half>1) throw std::runtime_error("Invalid flipper projection");
    // 01:4B29..4B76 selects four four-pose sequences. The active flippers
    // already follow the actual controls, including press, hold and release.
    for(unsigned x=4;x<64;++x) {
        const auto type=std::to_integer<unsigned>(projected[0x1500+x]);
        if(type!=2 && type!=3) continue;
        const unsigned target=0x1eaf+(type-2)*8+half*4;
        for(unsigned a=4;a<64;++a) {
            if(live[0x1500+a]!=projected[0x1500+x]) continue;
            const auto active_half=std::to_integer<unsigned>(live[0x79]);
            if(active_half>1) break;
            const unsigned source=0x1eaf+(type-2)*8+active_half*4;
            for(unsigned pose=0;pose<4;++pose) if(live[0x1800+a]==sequences_[source+pose]) {
                projected[0x1800+x]=sequences_[target+pose];break;
            }
            break;
        }
    }
}
void ActorAssets::advance_animation(std::span<std::byte> ram) const {
    if(ram.size()!=8192) throw std::runtime_error("Animation requires private 8192-byte actor RAM");
    auto b=[&](unsigned p){return std::to_integer<unsigned>(ram[p]);};
    auto put=[&](unsigned p,unsigned v){ram[p]=static_cast<std::byte>(v&255);};
    for(unsigned x=4;x<64;++x) {
        const auto type=b(0x1500+x),count=b(0x1a40+x);
        // Continue an animation already in progress. Collision-triggered actors
        // at rest stay at rest; the ball and launcher never enter this path.
        if(type<7 || type>90 || !b(0x1840+x) || !(b(0x1540+x)&0x80) || count<2 || count>32) continue;
        unsigned step=b(0x1b80+x)|(b(0x1bc0+x)<<8);
        unsigned phase=b(0x1c00+x)|(b(0x1c40+x)<<8);
        if(!step || (phase>>8)>count || ((phase>>8)==count && !(step&0x8000))) continue;
        const auto pointer=std::to_integer<unsigned>(sequences_[0x1df7+type-1])|
            (std::to_integer<unsigned>(sequences_[0x1e51+type-1])<<8);
        if(pointer<0xa000 || pointer+count>0xc000) continue;
        const auto sequence=std::span(sequences_).subspan(pointer-0xa000,count);
        // Some actor tables contain movement data instead of sprite descriptors.
        // Accept only a sequence containing the actual saved visual selection.
        const bool patterns=type==26 || type==27;
        const unsigned output=(patterns?0x1a80:0x1800)+x;
        if(std::find(sequence.begin(),sequence.end(),ram[output])==sequence.end()) continue;
        bool valid=true;
        for(const auto entry:sequence) {
            const auto id=std::to_integer<std::uint8_t>(entry);
            if(patterns) {if(id>=17) valid=false;}
            else {
                try { if(type==7 || type==20) (void)background(id); else (void)pieces(id); }
                catch(const std::runtime_error&) {valid=false;}
            }
        }
        if(!valid) continue;
        // Exact fixed-point reflection from 00:FD35..FD8E, including its
        // asymmetric endpoints and byte wrapping. Only visual fields change.
        phase=(phase+step)&65535;
        if(!(step&0x8000)) {
            if((phase>>8)==count) {step=(65536-step)&65535;phase=(phase-256)&0xff00;}
        } else if((phase>>8)==0 || (phase&0x8000)) {step=(65536-step)&65535;phase=0;}
        if((phase>>8)>=count) continue;
        put(0x1b80+x,step);put(0x1bc0+x,step>>8);
        put(0x1c00+x,phase);put(0x1c40+x,phase>>8);
        ram[output]=sequence[phase>>8];
    }
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
std::vector<BackgroundWrite> ActorAssets::background(std::uint8_t id,std::uint8_t high_override) const {
    const auto address=std::to_integer<unsigned>(banks_[0x24b+id])|(std::to_integer<unsigned>(banks_[0x34b+id])<<8);
    if(address<0xa000 || address>=0xe000) throw std::runtime_error("Background actor pointer outside mapped banks");
    return decode_actor_background(std::span(banks_).subspan(address-0xa000),high_override);
}
ActorPatternTransfer ActorAssets::pattern_transfer(std::uint8_t type,std::uint8_t selection,std::uint16_t pattern_base) const {
    if(type>=80 || selection>=17) throw std::runtime_error("Dynamic actor pattern selector outside recovered tables");
    auto b=[&](std::size_t p){return std::to_integer<unsigned>(banks_[p]);};
    const auto offset=b(0x123+type*2)|(b(0x124+type*2)<<8);
    const auto destination=static_cast<std::uint16_t>((static_cast<unsigned>(pattern_base)<<5)+offset);
    const auto source=b(0x12c1+selection)|(b(0x12d2+selection)<<8);
    if(source<0xa000 || source>=0xe000 || destination>=32768)
        throw std::runtime_error("Dynamic actor pattern address outside supported banks/VRAM");
    auto decoded=decode_word_rle(std::span(banks_).subspan(source-0xa000),32768-destination);
    if(decoded.words.empty()) throw std::runtime_error("Empty dynamic actor pattern stream");
    return {destination,0x2c000+source-0xa000,decoded.consumed,std::move(decoded.words)};
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
