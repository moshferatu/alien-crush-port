// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/render/title_logo.hpp"
#include "alien_crush/render/score_panel.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and preview path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        const auto logo=render::title_logo(rom);
        require(logo.width==192 && logo.height==144 && logo.pixels.size()==192*144,"Logo bounds");
        auto invalid=rom;invalid[0]^=std::byte{1};
        bool rejected=false;
        try {(void)render::title_logo(invalid);} catch(const std::runtime_error&) {rejected=true;}
        require(rejected,"Reject changed ROM");
        const auto patterns=assets::decode_word_rle(std::span(rom).subspan(0x24125),0x4000);
        std::array<std::uint16_t,512> palette{};
        for(unsigned i=0;i<512;++i) palette[i]=static_cast<std::uint16_t>(std::to_integer<unsigned>(rom[0x38bd0+i*2])|
            (std::to_integer<unsigned>(rom[0x38bd1+i*2])<<8));
        auto runtime=make_reference_runtime();runtime->load(path);
        const auto panel=render::ScorePanel::from_rom(rom);
        std::optional<state::SaveStateContainer> menu_save;
        render::Image menu_preview;
        unsigned pattern_matches=0,palette_matches=0,logo_matches=0;
        for(unsigned n=0;n<360;++n) {
            (void)runtime->advance(recording::canonical_input(n));
            const auto observed=diagnostics::observe_alien_state(runtime->work_ram());
            require(observed.title_menu==(n>=14 && n<318),"Canonical title entry and exit boundaries");
            if(n==239) {menu_preview=panel.compose(observed);menu_save=runtime->save();}
            if(n>=300) continue;
            const auto video=diagnostics::decode_reference_snapshot(runtime->save());
            if(std::equal(patterns.words.begin(),patterns.words.end(),video.vram.begin()+0x4000)) ++pattern_matches;
            if(std::equal(palette.begin()+256,palette.begin()+272,video.palette.begin()+256)) ++palette_matches;
            // Original SAT/VRAM, recolored with the original initial palette:
            // the live title cycles colors, whereas cabinet artwork is static.
            auto colors=palette;colors[0]=0;
            const auto image=render::compose(video.vram,colors,video.sat,{256,240,0,0,0,0x40});
            bool equal=true;
            for(int y=0;y<144;++y)
                equal&=std::equal(logo.pixels.begin()+y*192,logo.pixels.begin()+(y+1)*192,image.pixels.begin()+(y+14)*256+32);
            if(equal) ++logo_matches;
        }
        std::cout<<"Title pattern words="<<patterns.words.size()<<", matches: patterns="<<pattern_matches
                 <<", initial sprite palette="<<palette_matches<<", settled logo="<<logo_matches<<'\n';
        require(patterns.words.size()==8192 && pattern_matches==283,"ROM stream must match naturally loaded title VRAM");
        require(palette_matches==194,"Initial title palette must occur in replay");
        require(logo_matches==128,"ROM logo must match original settled SAT and VRAM");
        const std::vector<std::byte> final_ram(runtime->work_ram().begin(),runtime->work_ram().end());
        runtime->restore(*menu_save);
        require(panel.compose(diagnostics::observe_alien_state(runtime->work_ram())).pixels==menu_preview.pixels,"Quick restore returns to menu presentation");
        for(unsigned n=240;n<360;++n) (void)runtime->advance(recording::canonical_input(n));
        require(!diagnostics::observe_alien_state(runtime->work_ram()).title_menu,"Restored menu exits normally");
        require(std::equal(final_ram.begin(),final_ram.end(),runtime->work_ram().begin()),"Menu observation preserves exact replay RAM");
        auto menu_path=core::path_from_utf8(argv[2]);menu_path+=".score.ppm";
        std::ofstream score_preview(menu_path,std::ios::binary);score_preview<<"P6\n256 96\n255\n";
        for(const auto color:menu_preview.pixels) for(const auto shift:{16,8,0}) score_preview.put(static_cast<char>(color>>shift));
        require(static_cast<bool>(score_preview),"Write menu score preview");
        std::ofstream preview(core::path_from_utf8(argv[2]),std::ios::binary);
        preview<<"P6\n192 144\n255\n";
        for(const auto color:logo.pixels) for(const auto shift:{16,8,0}) preview.put(static_cast<char>(color>>shift));
        require(static_cast<bool>(preview),"Write local preview");
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
