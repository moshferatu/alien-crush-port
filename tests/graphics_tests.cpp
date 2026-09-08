// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool test,const char* message) {if(!test) throw std::runtime_error(message);}
template<class F> void rejects(F function,const char* message) {
    bool rejected=false; try {function();} catch(const std::runtime_error&) {rejected=true;} require(rejected,message);
}
void put32(std::vector<std::byte>& bytes,std::size_t offset,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) bytes.at(offset+i)=static_cast<std::byte>((value>>(8*i))&255);
}
}
int main() {
    try {
        std::vector<std::byte> payload(77);
        std::memcpy(payload.data(),"MDFNSVST",8); put32(payload,20,77);
        std::memcpy(payload.data()+32,"CPU",3); put32(payload,64,9);
        payload[68]=std::byte{2}; std::memcpy(payload.data()+69,"PC",2); put32(payload,71,2);
        payload[75]=std::byte{0x34}; payload[76]=std::byte{0x12};
        const auto fields=diagnostics::parse_reference_fields(payload);
        require(fields.size()==1 && fields[0].section=="CPU" && fields[0].name=="PC" && fields[0].bytes[0]==std::byte{0x34},"Named little-endian field parsing");
        auto invalid=payload; invalid.pop_back();
        rejects([&]{(void)diagnostics::parse_reference_fields(invalid);},"Truncated payload rejected");
        invalid=payload; put32(invalid,71,999);
        rejects([&]{(void)diagnostics::parse_reference_fields(invalid);},"Field cannot cross section boundary");
        invalid=payload; invalid.insert(invalid.end(),payload.begin()+32,payload.end()); put32(invalid,20,static_cast<std::uint32_t>(invalid.size()));
        rejects([&]{(void)diagnostics::parse_reference_fields(invalid);},"Ambiguous duplicate field rejected");
        payload.resize(100); require(diagnostics::parse_reference_fields(payload).size()==1,"Libretro padding ignored");

        std::array<std::uint16_t,32768> vram{};
        std::array<std::uint16_t,512> palette{};
        std::array<std::uint16_t,256> sat{};
        palette[0]=7; palette[3*16+1]=7<<3; palette[3*16+2]=7<<6;
        vram[0]=0x3080; vram[128*16]=0x4080;
        const auto bg=render::background(vram,palette,32,32);
        require(bg.width==256 && bg.height==256,"BAT dimensions");
        require(bg.pixels[0]==0xffff0000 && bg.pixels[1]==0xff00ff00,"Background two low planes and palette selection");
        require(bg.pixels[2]==0xff0000ff,"Background zero uses universal color");
        vram[128*16+8]=0x4080; palette[3*16+5]=0x1ff; palette[3*16+10]=0;
        const auto planes=render::background(vram,palette,32,32);
        require(planes.pixels[0]==0xffffffff && planes.pixels[1]==0xff000000,"Background high planes");
        vram[0]=0x3fff;
        require(render::background(vram,palette,32,32).pixels[0]==0xff0000ff,"Dormant patterns beyond VRAM are transparent");
        rejects([&]{(void)render::background(vram,palette,33,32);},"Invalid map rejected");

        vram.fill(0); palette.fill(0);
        palette[256+8]=7; palette[256+1]=7<<3;
        sat[0]=64+12; sat[1]=32+9; sat[2]=64*2; sat[3]=0x80;
        vram[64*64+48]=0x8000;
        auto sprites=render::sprites(vram,palette,sat,0);
        require(sprites[0].x==9 && sprites[0].y==12 && sprites[0].foreground,"Sprite coordinates and priority");
        require(sprites[0].image.pixels[0]==0xff0000ff && sprites[0].image.pixels[1]==0,"Sprite plane 3 and transparency");
        sat[3]=0x8880;
        sprites=render::sprites(vram,palette,sat,0);
        require(sprites[0].image.pixels[255]==0xff0000ff && sprites[0].image.pixels[0]==0,"Both sprite flips");
        sat[2]=65*2; sat[3]=0x1180; vram[65*64]=0x8000; vram[66*64]=0x8000;
        sprites=render::sprites(vram,palette,sat,0);
        require(sprites[0].width==32 && sprites[0].height==32 && sprites[0].image.pixels[0]==0xff0000ff,"Wide sprite aligns odd pattern number");
        require(sprites[0].image.pixels[16]==0xffff0000 && sprites[0].image.pixels[16*32]==0xffff0000,"Wide/tall sprite tile addressing");
        sat[2]=64*2+1; sat[3]=0x80; vram[64*64+32]=0x8000;
        require(render::sprites(vram,palette,sat,4)[0].image.pixels[0]==render::pce_color(palette[256+3]),"Two-bit alternate plane selection");
        diagnostics::ReferenceSnapshot snapshot;
        snapshot.memory_width=0x70;
        require(snapshot.bat_width()==128 && snapshot.bat_height()==64,"MWR map dimensions");
        const std::array<std::byte,10> packed{std::byte{2},std::byte{0x34},std::byte{0x12},std::byte{0x78},std::byte{0x56},std::byte{0x83},std::byte{0xbc},std::byte{0x9a},std::byte{0},std::byte{0xff}};
        const auto decoded=assets::decode_word_rle(packed,5);
        require(decoded.words==std::vector<std::uint16_t>{0x1234,0x5678,0x9abc,0x9abc,0x9abc} && decoded.consumed==9,"RLE literals, repeated words, and terminator");
        rejects([&]{(void)assets::decode_word_rle(packed,4);},"RLE output bounded");
        rejects([&]{(void)assets::decode_word_rle(std::span(packed).first(7),5);},"Truncated RLE word rejected");
        const std::array<std::byte,4> wrapping{std::byte{0x80},std::byte{0x34},std::byte{0x12},std::byte{0}};
        require(assets::decode_word_rle(wrapping,256).words.size()==256,"RLE repeat zero count preserves CPU wrap");
        std::array<std::byte,8192> ram{};
        const auto hud=assets::assemble_table_hud(123456700,10);
        const std::array<unsigned,9> hud_digits{1,2,3,4,5,6,7,0,0};
        for(unsigned i=0;i<9;++i) require(hud[i].slot==i && hud[i].words[2]==(20+hud_digits[i])*2,"HUD decimal order and trailing hundreds");
        require(hud[9].words==std::array<std::uint16_t,4>{96,264,58,143},"HUD ball counter decrement and placement");
        require(assets::assemble_table_hud(0,1)[0].words[2]==40 && assets::assemble_table_hud(999999900,1)[6].words[2]==58,"HUD zero and maximum score");
        rejects([&]{(void)assets::assemble_table_hud(1000000000,1);},"HUD score overflow rejected");
        rejects([&]{(void)assets::assemble_table_hud(101,1);},"HUD fractional hundreds rejected");
        rejects([&]{(void)assets::assemble_table_hud(0,0);},"Invalid HUD counter rejected");
        const std::array<std::byte,14> background_bytes{std::byte{0xff},std::byte{0xff},std::byte{2},std::byte{2},
            std::byte{0xff},std::byte{0xff},std::byte{2},std::byte{0},std::byte{0},std::byte{0},
            std::byte{1},std::byte{0},std::byte{0xff},std::byte{1}};
        const auto updates=assets::decode_actor_background(background_bytes,0x80);
        require(updates.size()==4 && updates[0].address==65535 && updates[1].address==0 &&
            updates[2].address==31 && updates[3].address==32,"Background row stride and address wrapping");
        require(updates[0].value==0x8001 && updates[1].value==0xffff && updates[2].value==0x8000 &&
            updates[3].value==0x81fe,"Background base carry, wrapping and high-byte OR");
        rejects([&]{(void)assets::decode_actor_background({});},"Empty background header rejected");
        rejects([&]{(void)assets::decode_actor_background(std::span(background_bytes).first(13));},"Truncated background tiles rejected");
        std::array<std::byte,258> background_bounds{};
        rejects([&]{(void)assets::decode_actor_background(background_bounds);},"Zero background dimensions rejected");
        background_bounds[2]=std::byte{125}; background_bounds[3]=std::byte{1};
        require(assets::decode_actor_background(background_bounds).size()==125,"Largest non-wrapping background descriptor accepted");
        background_bounds[2]=std::byte{126};
        rejects([&]{(void)assets::decode_actor_background(background_bounds);},"Wrapping background byte index rejected");
        ram[0x99]=std::byte{3};
        auto overlay=assets::assemble_table_overlay(ram,59);
        require(overlay.size()==4 && overlay[0].slot==59 && overlay[3].slot==62 &&
            overlay[0].words==std::array<std::uint16_t,4>{191,132,262,143} &&
            overlay[3].words==std::array<std::uint16_t,4>{191,156,256,143},"Lower table overlay and strict capacity boundary");
        require(assets::assemble_table_overlay(ram,60).empty() && assets::assemble_table_overlay(ram,64).empty(),"Overlay capacity guard");
        ram[0x79]=std::byte{1}; ram[0x9a]=std::byte{0xff};
        ram[0x16]=std::byte{0xff}; ram[0x17]=std::byte{0xff};
        ram[0x2e]=std::byte{0xff}; ram[0x2f]=std::byte{0xff};
        overlay=assets::assemble_table_overlay(ram,0);
        require(overlay[0].words==std::array<std::uint16_t,4>{246,136,509,143} &&
            overlay[1].words==std::array<std::uint16_t,4>{246,144,0,143},"Upper overlay coordinate wrap, discarded shift carry and pattern arithmetic");
        ram[0x53]=std::byte{0x80};
        require(assets::assemble_table_overlay(ram,0).empty(),"Overlay first sign guard");
        ram[0x53]=std::byte{0}; ram[0x54]=std::byte{0x80};
        require(assets::assemble_table_overlay(ram,0).empty(),"Overlay second sign guard");
        ram[0x54]=std::byte{0}; ram[0x79]=std::byte{2};
        require(assets::assemble_table_overlay(ram,0).empty(),"Non-table overlay guard");
        rejects([&]{(void)assets::assemble_table_overlay(ram,65);},"Invalid overlay slot rejected");
        rejects([&]{(void)assets::assemble_table_overlay(std::span(ram).first(8191),0);},"Truncated overlay RAM rejected");
        ram.fill(std::byte{0});
        const std::array<std::byte,8> piece_bytes{std::byte{1},std::byte{0x80},std::byte{0xff},
            std::byte{0x34},std::byte{0x12},std::byte{0x8f},std::byte{0x89},std::byte{0xff}};
        const auto pieces=assets::decode_actor_pieces(piece_bytes);
        require(pieces.size()==1 && pieces[0].y==-128 && pieces[0].x==-1 && pieces[0].pattern==0x1234 && pieces[0].flags==0x898f,
            "Actor signed offsets, little-endian words and trailing descriptor data");
        rejects([&]{(void)assets::decode_actor_pieces({});},"Empty actor descriptor rejected");
        rejects([&]{(void)assets::decode_actor_pieces(std::span(piece_bytes).first(6));},"Truncated actor piece rejected");
        std::array<std::byte,259> large_descriptor{};
        rejects([&]{(void)assets::decode_actor_pieces(large_descriptor);},"Zero piece count rejected");
        large_descriptor[0]=std::byte{42};
        require(assets::decode_actor_pieces(large_descriptor).size()==42,"Largest non-wrapping descriptor accepted");
        large_descriptor[0]=std::byte{43};
        rejects([&]{(void)assets::decode_actor_pieces(large_descriptor);},"Wrapping descriptor index rejected");
        ram[0x121e]=std::byte{0x66}; ram[0x1224]=std::byte{1}; ram[0x122a]=std::byte{2};
        ram[0x1219]=std::byte{0x10}; ram[0x121f]=std::byte{0x27}; ram[0x46]=std::byte{3}; ram[0x68]=std::byte{4};
        const auto observed=diagnostics::observe_alien_state(ram);
        require(observed.score_points==13'143'000 && observed.high_scores[0]==1'000'000 && observed.ball_counter==3 && observed.score_update_pending,"Strided 24-bit score and HUD update state");
        std::cout<<"PASS: bounded named-state parsing, background planes/palettes, sprite addressing/flips/transparency\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
