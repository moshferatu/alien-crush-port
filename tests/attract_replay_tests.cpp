// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/render/score_panel.hpp"
#include <algorithm>
#include <fstream>
#include <optional>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("Expected ROM and preview path");
        auto runtime=make_reference_runtime();runtime->load(core::path_from_utf8(argv[1]));
        const auto panel=render::ScorePanel::from_rom(core::read_file(core::path_from_utf8(argv[1])));
        const std::array<unsigned,7> boundaries{1,15,1545,1563,1609,5513,5526};
        const std::array<unsigned,7> selectors{0,0,1,1,2,0,0};
        const std::array<bool,7> titles{false,true,true,false,false,false,true};
        std::size_t changes=0;
        std::optional<state::SaveStateContainer> checkpoint;
        render::Image demo_panel;
        std::vector<std::byte> final_ram;
        std::vector<std::uint32_t> final_pixels;
        std::vector<std::int16_t> final_audio;
        unsigned previous=999;
        for(unsigned n=0;n<6000;++n) {
            const auto frame=runtime->advance({});
            const auto ram=runtime->work_ram();
            const auto mode=std::to_integer<unsigned>(ram[0x76]);
            const auto observed=diagnostics::observe_alien_state(ram);
            const bool title=observed.title_menu;
            require(observed.demo_play==(mode!=0),"Observe original demo selector");
            const auto key=mode*2+static_cast<unsigned>(title);
            if(key!=previous) {
                require(changes<boundaries.size() && n+1==boundaries[changes] && mode==selectors[changes] && title==titles[changes],"Natural attract-cycle boundaries");
                ++changes;
                std::cout<<"frame="<<n+1<<" demo="<<mode<<" title="<<title<<" outer="<<std::hex
                    <<(std::to_integer<unsigned>(ram[0x1fe])|(std::to_integer<unsigned>(ram[0x1ff])<<8))<<std::dec<<'\n';
                previous=key;
            }
            if(n==1799) {
                require(observed.demo_play && !title,"Checkpoint is active demo");
                demo_panel=panel.compose(observed);checkpoint=runtime->save();
            }
            if(n==5999) {
                final_ram.assign(ram.begin(),ram.end());
                final_pixels.assign(frame.pixels.begin(),frame.pixels.end());
                final_audio.assign(frame.stereo.begin(),frame.stereo.end());
            }
        }
        require(changes==boundaries.size(),"Complete demo and return to title");
        runtime->restore(*checkpoint);
        require(panel.compose(diagnostics::observe_alien_state(runtime->work_ram())).pixels==demo_panel.pixels,"Restore demo presentation");
        for(unsigned n=1800;n<6000;++n) {
            const auto frame=runtime->advance({});
            if(n==5999) {
                require(std::equal(final_ram.begin(),final_ram.end(),runtime->work_ram().begin()),"Attract restore RAM parity");
                require(std::equal(final_pixels.begin(),final_pixels.end(),frame.pixels.begin(),frame.pixels.end()),"Attract restore video parity");
                require(std::equal(final_audio.begin(),final_audio.end(),frame.stereo.begin(),frame.stereo.end()),"Attract restore audio parity");
            }
        }
        runtime->restore(*checkpoint);
        bool returned=false;
        for(unsigned n=0;n<120;++n) {
            (void)runtime->advance({static_cast<std::uint16_t>(n<4?Pad::run:0),0});
            const auto observed=diagnostics::observe_alien_state(runtime->work_ram());
            returned|=observed.title_menu && !observed.demo_play;
        }
        require(returned,"Run exits demo to the original title menu");
        std::ofstream preview(core::path_from_utf8(argv[2]),std::ios::binary);
        preview<<"P6\n256 96\n255\n";
        for(const auto color:demo_panel.pixels) for(const auto shift:{16,8,0}) preview.put(static_cast<char>(color>>shift));
        require(static_cast<bool>(preview),"Write demo preview");
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

