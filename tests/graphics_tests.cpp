// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/diagnostics/display_trace.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <algorithm>
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
        {
            std::array<std::uint16_t,32768> memory{};
            std::array<std::uint16_t,512> colors{};
            std::array<std::uint16_t,256> attributes{};
            colors[0]=colors[1]=7; // Equal RGB values must not erase background opacity.
            colors[256]=7<<6; colors[257]=7<<3; colors[273]=7<<6;
            memory[0]=128; memory[128*16]=0x80;
            memory[64*64]=0xc000;
            attributes[0]=attributes[4]=64; attributes[1]=attributes[5]=32;
            attributes[2]=attributes[6]=128; attributes[7]=0x81;
            render::Viewport view{2,1};
            auto composed=render::compose(memory,colors,attributes,view);
            require(composed.pixels[0]==render::pce_color(7),"Hidden earlier sprite blocks later foreground sprite");
            require(composed.pixels[1]==render::pce_color(7<<3),"Background color zero admits rear sprite");
            attributes[3]=0x80;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(7<<3),"Foreground sprite covers opaque background");
            memory[64*64]=0x4000; attributes[2]=130; memory[65*64]=0x8000;
            require(render::compose(memory,colors,attributes,view).pixels[1]==render::pce_color(7<<6),"Transparent earlier sprite admits later sprite");
            view.control=0x80; view.scroll_x=256; view.scroll_y=256;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(7),"Scroll wraps at BAT dimensions");
            colors[1]=7<<3;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(7<<3),"Wrapped background samples nonzero pixel");
            view.scroll_x=255;
            require(render::compose(memory,colors,attributes,view).pixels[1]==render::pce_color(7<<3),"Horizontal viewport crosses BAT seam");
            view.control=0x40; attributes.fill(0);
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(colors[256]),"Sprite-only backdrop selects palette 256");
            attributes[0]=64; attributes[1]=31; attributes[2]=128; attributes[3]=0x80;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(colors[257]),"Partially clipped sprite samples visible column");
            view.control=0;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(colors[0]),"Both layers disabled select universal backdrop");
            view.width=0;
            rejects([&]{(void)render::compose(memory,colors,attributes,view);},"Empty composition rejected");
            view.width=1025;
            rejects([&]{(void)render::compose(memory,colors,attributes,view);},"Oversized composition rejected");
            view.width=2;
            rejects([&]{(void)render::compose(memory,std::span(colors).first(256),attributes,view);},"Incomplete composition palette rejected");
        }
        {
            std::array<std::uint16_t,32768> memory{};
            std::array<std::uint16_t,512> colors{};
            std::array<std::uint16_t,256> attributes{};
            colors[257]=7; colors[258]=7<<3;
            // Fifteen transparent, horizontally offscreen pieces exhaust fifteen fetches.
            for(unsigned i=0;i<15;++i) {attributes[i*4]=64; attributes[i*4+1]=900;}
            attributes[60]=64; attributes[61]=32; attributes[62]=128; attributes[63]=0x180;
            for(unsigned y=0;y<16;++y) {memory[64*64+y]=0xffff; memory[65*64+16+y]=0xffff;}
            render::Viewport view{32,17,0,0,0,0x40};
            auto limited=render::compose(memory,colors,attributes,view);
            require(limited.pixels[0]==render::pce_color(7) && limited.pixels[16]==render::pce_color(0),
                "Wide sprite at fetch sixteen retains only its left half");
            attributes[63]|=0x800;
            limited=render::compose(memory,colors,attributes,view);
            require(limited.pixels[0]==render::pce_color(7<<3) && limited.pixels[16]==render::pce_color(0),
                "Flipped wide sprite still fetches its screen-left half first");
            attributes[64]=64; attributes[65]=48; attributes[66]=128; attributes[67]=0x80;
            require(render::compose(memory,colors,attributes,view).pixels[16]==render::pce_color(0),
                "Later sprite cannot use exhausted row even after transparent fetches");
            attributes[64]=80;
            require(render::compose(memory,colors,attributes,view).pixels[16*32+16]==render::pce_color(7),
                "Fetch budget resets on each row and excludes vertically absent sprites");
            attributes[56]=63; // Only fourteen pieces intersect row fifteen now.
            require(render::compose(memory,colors,attributes,view).pixels[15*32+16]==render::pce_color(7),
                "Wide sprite consumes both remaining fetches at exact capacity");
        }
        {
            using Target=render::RasterTarget;
            using Write=render::RasterWrite;
            std::array<std::uint16_t,32768> memory{};
            std::array<std::uint16_t,512> colors{};
            std::array<std::uint16_t,256> attributes{};
            render::Viewport view{2,4,0,0,0,0};
            colors[0]=7; colors[256]=7<<3;
            const auto original_colors=colors;
            const std::array writes{Write{0,Target::palette,0,7<<6},Write{2,Target::palette,0,7<<3},
                Write{2,Target::palette,0,7},Write{3,Target::control,0,0x40}};
            auto result=render::compose_raster(memory,colors,attributes,view,writes);
            require(result.pixels==std::vector<std::uint32_t>{0xff00ff00,0xff00ff00,0xff00ff00,0xff00ff00,
                0xff0000ff,0xff0000ff,0xffff0000,0xffff0000},"Raster row-zero, same-row ordering, final row and backdrop control");
            require(colors==original_colors && view.control==0,"Raster writes preserve caller palette and registers");
            require(render::compose_raster(memory,colors,attributes,view,{}).pixels==render::compose(memory,colors,attributes,view).pixels,
                "No raster writes preserve frozen composition");
            colors[257]=7<<3; colors[1]=7<<6;
            memory[0]=128;
            for(unsigned y=0;y<8;++y) memory[128*16+y]=0xff;
            for(unsigned y=0;y<16;++y) memory[64*64+y]=0x8000;
            attributes[0]=64; attributes[1]=32; attributes[2]=128; attributes[3]=0x80;
            view={4,4};
            const auto original_memory=memory; const auto original_attributes=attributes;
            const std::array moving{Write{1,Target::sat,1,33},Write{2,Target::vram,64*64+2,0x4000}};
            result=render::compose_raster(memory,colors,attributes,view,moving);
            require(result.pixels[0]==0xffff0000 && result.pixels[4]==0xff00ff00 && result.pixels[5]==0xffff0000 &&
                result.pixels[9]==0xff00ff00 && result.pixels[10]==0xffff0000 && result.pixels[13]==0xffff0000,
                "Raster SAT and pattern writes retain absolute sprite rows and prior pixels");
            require(memory==original_memory && attributes==original_attributes,"Raster writes preserve caller VRAM and SAT");
            memory.fill(0); attributes.fill(0); memory[0]=128;
            for(unsigned y=0;y<8;++y) {
                const auto pixel=y+1;
                memory[128*16+y]=static_cast<std::uint16_t>(((pixel&1)?0x80:0)|((pixel&2)?0x8000:0));
                memory[128*16+y+8]=static_cast<std::uint16_t>(((pixel&4)?0x80:0)|((pixel&8)?0x8000:0));
                colors[pixel]=static_cast<std::uint16_t>(pixel);
            }
            view={1,4,0,0,0,0x80};
            const std::array scrolling{Write{2,Target::scroll_y,0,1}};
            result=render::compose_raster(memory,colors,attributes,view,scrolling);
            require(result.pixels==std::vector<std::uint32_t>{render::pce_color(1),render::pce_color(2),render::pce_color(4),render::pce_color(5)},
                "Raster scroll origin remains relative to full frame, not band start");
            memory.fill(0); memory[32]=128; memory[64]=129; memory[65]=130;
            for(unsigned y=0;y<8;++y) {memory[128*16+y]=0x80;memory[129*16+y]=0x8000;memory[130*16+y+8]=0x80;}
            view={1,10,0,0,0,0x80};
            const std::array mapping{Write{8,Target::memory_width,0,0x10},Write{9,Target::scroll_x,0,8}};
            result=render::compose_raster(memory,colors,attributes,view,mapping);
            require(result.pixels[8]==render::pce_color(2) && result.pixels[9]==render::pce_color(4),"Raster map width and horizontal scroll update at row boundary");
            const auto invalid_write=[&](Write write) {const std::array list{write};
                rejects([&]{(void)render::compose_raster(memory,colors,attributes,view,list);},"Invalid raster write rejected");};
            invalid_write({10,Target::control,0,0}); invalid_write({0,Target::vram,32768,0});
            invalid_write({0,Target::palette,512,0}); invalid_write({0,Target::palette,0,512});
            invalid_write({0,Target::sat,256,0}); invalid_write({0,Target::control,1,0});
            invalid_write({0,static_cast<Target>(99),0,0});
            const std::array unsorted{Write{2,Target::control,0,0},Write{1,Target::control,0,0}};
            rejects([&]{(void)render::compose_raster(memory,colors,attributes,view,unsorted);},"Backwards raster writes rejected");
            const std::vector<Write> excessive(65537);
            rejects([&]{(void)render::compose_raster(memory,colors,attributes,view,excessive);},"Raster event count bounded");
        }
        {
            const std::array rows{diagnostics::DisplayRow{0,0xc8,true},diagnostics::DisplayRow{1,0xc8,true},
                diagnostics::DisplayRow{2,0xc8,false},diagnostics::DisplayRow{3,0x88,false}};
            const auto schedule=diagnostics::display_control_schedule(rows,4);
            require(schedule.size()==4 && schedule[0].row==0 && schedule[0].value==0xc8 &&
                schedule[1].target==render::RasterTarget::burst_mode && schedule[1].value==1 &&
                schedule[2].row==2 && schedule[2].value==0 && schedule[3].row==3 && schedule[3].value==0x88,
                "Observed burst state remains explicit and consecutive equal controls coalesce");
            rejects([&]{(void)diagnostics::display_control_schedule(rows,5);},"Partial display row coverage rejected");
            auto invalid_rows=rows; invalid_rows[2].row=1;
            rejects([&]{(void)diagnostics::display_control_schedule(invalid_rows,4);},"Duplicate or unordered display rows rejected");
            rejects([&]{(void)diagnostics::display_control_schedule({},0);},"Empty display trace rejected");
        }
        {
            std::array<std::uint16_t,32768> memory{};
            std::array<std::uint16_t,512> colors{};
            std::array<std::uint16_t,256> attributes{};
            colors[0]=colors[1]=colors[257]=7; colors[256]=7<<3;
            memory[0]=memory[1]=memory[2]=128; memory[128*16]=0xff; memory[64*64]=0xffff;
            attributes[0]=64; attributes[1]=32; attributes[2]=128; attributes[3]=0x80;
            render::Viewport view{20,1};
            const std::array writes{render::PalettePixelWrite{2,257,7<<6}};
            auto result=render::compose_palette_raster(memory,colors,attributes,view,{},writes);
            require(result.pixels[0]==0xff0000ff && result.pixels[1]==0xff0000ff && result.pixels[2]==0xff00ff00 &&
                result.pixels[15]==0xff00ff00 && result.pixels[16]==0xff0000ff,
                "Pixel palette timing preserves palette identity when background and sprite RGB initially match");
            require(colors[257]==7,"Pixel palette timing preserves input colors");
            view.burst_mode=true;
            const std::array border{render::PalettePixelWrite{2,256,7<<6},render::PalettePixelWrite{2,256,7}};
            result=render::compose_palette_raster(memory,colors,attributes,view,{},border);
            require(result.pixels[0]==0xffff0000 && result.pixels[2]==0xff0000ff,"Burst mode uses palette 256 and ordered same-pixel updates");
            require(render::compose(memory,colors,attributes,view).pixels[0]==0xffff0000,"Frozen burst mode suppresses enabled sprites and background");
            const std::array mixed{render::RasterWrite{0,render::RasterTarget::palette,0,0}};
            rejects([&]{(void)render::compose_palette_raster(memory,colors,attributes,view,mixed,writes);},"Ambiguous palette timelines rejected");
            const std::array past_end{render::PalettePixelWrite{20,0,0}};
            rejects([&]{(void)render::compose_palette_raster(memory,colors,attributes,view,{},past_end);},"Pixel timing bounds enforced");
            const std::array backwards{render::PalettePixelWrite{2,0,0},render::PalettePixelWrite{1,0,0}};
            rejects([&]{(void)render::compose_palette_raster(memory,colors,attributes,view,{},backwards);},"Pixel timing order enforced");
            std::array<std::uint16_t,512> initial{},final_colors{}; final_colors[0]=4;
            const std::array trace{diagnostics::PaletteWrite{240,0,0,0,1,false},diagnostics::PaletteWrite{0,2,0,1,2,false},
                diagnostics::PaletteWrite{0,5,0,2,3,false},diagnostics::PaletteWrite{240,0,0,3,4,true}};
            const auto pixels=diagnostics::palette_pixel_schedule(initial,final_colors,trace,4,2);
            require(pixels.size()==3 && pixels[0].pixel==0 && pixels[1].pixel==2 && pixels[2].pixel==4,
                "Palette schedule distinguishes preceding blank, visible pixels, right border and completed frame");
            auto broken=trace; broken[1].before=0;
            rejects([&]{(void)diagnostics::palette_pixel_schedule(initial,final_colors,broken,4,2);},"Incomplete palette write chain rejected");
            final_colors[1]=1;
            rejects([&]{(void)diagnostics::palette_pixel_schedule(initial,final_colors,trace,4,2);},"Full final palette verification rejects unobserved changes");
        }
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
        assets::PalettePulseState pulse{0x81,0,0,0};
        for(unsigned step=1;step<=14;++step) {
            const auto next=assets::advance_palette_pulse(pulse);
            const unsigned level=step<=7?step:14-step;
            require(next.color_zero==level*73 && next.state.green==level && next.state.red==level && next.state.blue==level,"Pulse rise/fall colors");
            require(next.state.flags==(step<8?0x81:(step<14?0xc0:0)),"Pulse direction and completion flags");
            pulse=next.state;
        }
        require(!assets::advance_palette_pulse(pulse).color_zero,"Completed pulse emits no further write");
        const auto inactive=assets::advance_palette_pulse({0x40,9,8,7});
        require(!inactive.color_zero && inactive.state.flags==0x40 && inactive.state.green==9,"Inactive pulse preserves state");
        const auto wrapped=assets::advance_palette_pulse({0xc1,0,0,0});
        require(wrapped.state.green==255 && wrapped.state.flags==0xc1 && wrapped.color_zero==511,"Pulse byte underflow and VCE packing");
        std::array<std::uint16_t,512> fade_current{},fade_target{};
        fade_target[0]=511; fade_target[1]=65; fade_target[2]=1;
        auto fade=assets::advance_palette_fade(fade_current,fade_target,21);
        require(fade[0]==8 && fade[1]==0 && fade[2]==0,"Fade brightness threshold and red tie priority");
        fade=assets::advance_palette_fade(fade_current,fade_target,1);
        require(fade[1]==64 && fade[2]==1,"Fade green tie priority and blue-only step");
        for(unsigned t=21;t>0;--t) fade_current=assets::advance_palette_fade(fade_current,fade_target,t);
        require(fade_current==fade_target,"Complete 21-step fade reaches target");
        rejects([&]{(void)assets::advance_palette_fade(fade_current,fade_target,0);},"Invalid fade threshold rejected");
        fade_current[2]=2;
        rejects([&]{(void)assets::advance_palette_fade(fade_current,fade_target,1);},"Non-monotonic fade input rejected");
        for(unsigned i=0;i<512;++i) fade_current[i]=static_cast<std::uint16_t>(i);
        const auto faded=assets::advance_palette_fade_out(fade_current);
        require(faded.already_black==1 && faded.colors[511]==438 && faded.colors[73]==0 && faded.colors[1]==0,
            "Fade-out decrements every nonzero channel and counts input black");
        for(unsigned step=0;step<7;++step) fade_current=assets::advance_palette_fade_out(fade_current).colors;
        require(std::all_of(fade_current.begin(),fade_current.end(),[](auto v){return v==0;}),"All 512 PCE colors fade to black in seven steps");
        require(assets::advance_palette_fade_out(fade_current).already_black==512,"Fade-out requires final already-black completion sweep");
        fade_current[0]=512;
        rejects([&]{(void)assets::advance_palette_fade_out(fade_current);},"Invalid fade-out color rejected");
        rejects([&]{(void)assets::advance_palette_fade_out({});},"Invalid fade-out buffer rejected");
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
