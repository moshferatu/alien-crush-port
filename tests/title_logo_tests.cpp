// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/render/title_logo.hpp"
#include "alien_crush/render/score_panel.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/rom_image.hpp"
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
        const render::TitleLogoAnimation animation(rom);
        require(animation.frame(0).pixels==logo.pixels,"Animation starts with original blue artwork");
        require(animation.frame(0).pixels==animation.frame(animation.duration()).pixels,"Animation wraps exactly");
        require(animation.frame(animation.duration()-1).pixels==logo.pixels,"Loop returns to blue without a cut");
        require(animation.frame(600).pixels!=logo.pixels,"Reveal reaches the readable logo");
        require(animation.frame(600).pixels!=animation.frame(616).pixels,"Readable logo glow continues animating");
        require(logo.width==192 && logo.height==144 && logo.pixels.size()==192*144,"Logo bounds");
        auto invalid=rom;invalid[0]^=std::byte{1};
        bool rejected=false;
        try {(void)render::title_logo(invalid);} catch(const std::runtime_error&) {rejected=true;}
        require(rejected,"Reject changed ROM");
        {
            // The supported ROM is found in a folder whatever it is called; a
            // changed copy and a copy that is not a .pce are passed over.
            const auto folder=std::filesystem::temp_directory_path()/"alien-crush-find-rom-test";
            std::filesystem::remove_all(folder);
            std::filesystem::create_directories(folder);
            const auto write=[&](const char* name,const std::vector<std::byte>& bytes) {
                std::ofstream out(folder/name,std::ios::binary);
                out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
            };
            write("a.pce",invalid);
            write("b.bin",rom);
            require(rom::find_supported_rom(folder).empty(),"No supported ROM in the folder");
            write("My Copy.PCE",rom);
            require(rom::find_supported_rom(folder)==folder/"My Copy.PCE","Supported ROM found under any name");
            std::filesystem::remove_all(folder);
        }
        const auto patterns=assets::decode_word_rle(std::span(rom).subspan(0x24125),0x4000);
        std::array<std::uint16_t,512> palette{};
        for(unsigned i=0;i<512;++i) palette[i]=static_cast<std::uint16_t>(std::to_integer<unsigned>(rom[0x38bd0+i*2])|
            (std::to_integer<unsigned>(rom[0x38bd1+i*2])<<8));
        auto runtime=make_reference_runtime();runtime->load(path);
        const auto boot=runtime->save();
        const auto panel=render::ScorePanel::from_rom(rom);
        std::optional<state::SaveStateContainer> menu_save;
        render::Image menu_preview;
        unsigned pattern_matches=0,palette_matches=0,logo_matches=0;
        for(unsigned n=0;n<360;++n) {
            const auto live=runtime->advance(recording::canonical_input(n));
            const std::vector<std::uint32_t> live_pixels(live.pixels.begin(),live.pixels.end());
            const auto observed=diagnostics::observe_alien_state(runtime->work_ram());
            require(observed.title_menu==(n>=14 && n<318),"Canonical title entry and exit boundaries");
            if(n==239) {menu_preview=panel.compose(observed);menu_save=runtime->save();}
            if(n>=300) continue;
            const auto video=diagnostics::decode_reference_snapshot(runtime->save());
            if(n==239) {
                const auto tile=render::compose(video.vram,video.palette,video.sat,
                    {96,96,video.scroll_x,video.scroll_y,video.memory_width,0x80});
                for(int y=0;y<live.height;++y) for(int x=0;x<16;++x)
                    require(live_pixels[y*live.width+x]==tile.pixels[(y%96)*96+x],
                        "Title background repeat matches live scrolling phase and palette");
            }
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
        runtime->restore(boot);
        unsigned animated_matches=0;
        for(unsigned n=0;n<1200;++n) {
            (void)runtime->advance({});
            if(n<380) continue;
            const auto video=runtime->video_snapshot();
            auto colors=video.palette;colors[0]=0;
            const auto art=render::compose(video.vram,colors,video.sat,{256,240,0,0,0,0x40});
            // Natural entrance settles two pixels below the Run-skipped pose
            // used for cabinet art. Compare the entire logo, not selected colors.
            for(std::uint64_t tick=0;tick<animation.duration();tick+=2) {
                const auto& candidate=animation.frame(tick);
                bool equal=true;
                for(int y=0;y<144 && equal;++y) equal=std::equal(candidate.pixels.begin()+y*192,
                    candidate.pixels.begin()+(y+1)*192,art.pixels.begin()+(y+16)*256+32);
                if(equal) {++animated_matches;break;}
            }
        }
        std::cout<<"Original animated logo frames matched="<<animated_matches<<'\n';
        require(animated_matches==820,"Animation reproduces every observed original reveal/glow image");
        for(const unsigned tick:{0u,184u,344u,600u,616u}) {
            const auto& art=animation.frame(tick);
            auto file=core::path_from_utf8(argv[2]);file+=".animation-"+std::to_string(tick)+".ppm";
            std::ofstream output(file,std::ios::binary);output<<"P6\n192 144\n255\n";
            for(const auto color:art.pixels) for(const auto shift:{16,8,0}) output.put(static_cast<char>(color>>shift));
            require(static_cast<bool>(output),"Write animation preview");
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
