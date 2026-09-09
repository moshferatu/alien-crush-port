// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/render/score_panel.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
PadInput input_at(unsigned n) {
    if(n<600) return recording::canonical_input(n);
    if(n==600 || n==1050) return {Pad::run,0};
    if(n==650 || n==900) return {Pad::select,0};
    return {};
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and output prefix");
        const auto path=core::path_from_utf8(argv[1]);const auto rom=core::read_file(path);
        auto runtime=make_reference_runtime();runtime->load(path);
        const auto panel=render::ScorePanel::from_rom(rom);
        const auto table=std::make_unique<render::TablePlayfield>(rom);
        std::optional<diagnostics::ReferenceSnapshot> previous;
        render::TableTransitionHold hold;
        recording::Recording replay;
        render::Image preview;
        std::vector<std::byte> final_ram;
        std::vector<std::uint32_t> final_pixels;
        std::vector<std::int16_t> final_audio;
        unsigned menu_frames=0,returned_stitched=0;
        for(unsigned n=0;n<1200;++n) {
            const auto frame=runtime->advance(input_at(n));
            const std::vector<std::uint32_t> pixels(frame.pixels.begin(),frame.pixels.end());
            const std::vector<std::int16_t> audio(frame.stereo.begin(),frame.stereo.end());
            const auto ram=runtime->work_ram();const auto state=diagnostics::observe_alien_state(ram);
            require(state.high_score_menu==(n>=650 && n<900),"Pause score-viewer call boundaries");
            menu_frames+=state.high_score_menu;
            if(n>=600) {
                require(!state.title_menu && !state.demo_play,"Score viewer preserves ordinary game session");
                for(unsigned rank=0;rank<5;++rank) {
                    require(state.high_scores[rank]==1000000,"Viewing scores does not alter rankings");
                    for(unsigned letter=0;letter<3;++letter)
                        require(ram[0x122b+letter*6+rank]==std::byte{0x40},"Viewing scores preserves original name bytes");
                }
                const auto video=diagnostics::decode_reference_snapshot(runtime->save());
                const auto camera=std::to_integer<unsigned>(ram[0x16])|(std::to_integer<unsigned>(ram[0x17])<<8);
                const auto output=previous?table->compose(video,*previous,state.table_code,camera,ram):std::nullopt;
                const auto shown=hold.present(output,video.frame,state.table_code,camera,ram);
                if(n>=700 && n<900) require(!output && !shown,"Score screen uses original viewport without held playfield");
                if(n>=1100 && output) ++returned_stitched;
                previous=video;
            }
            if(n==599) replay.initial=runtime->save();
            if(n>=600) replay.inputs.push_back(input_at(n));
            if(n==799) preview=panel.compose(state);
            if(n==999) require(!state.high_score_menu && ram[0x7a]==std::byte{1},"Stale viewer mode after exit does not retain score-table layout");
            if(n==1199) {final_ram.assign(ram.begin(),ram.end());final_pixels=pixels;final_audio=audio;}
        }
        require(menu_frames==250 && returned_stitched>0,"Score viewer opens, exits and resumes stitched play");
        runtime->restore(replay.initial);
        for(unsigned i=0;i<replay.inputs.size();++i) {
            const auto frame=recording::advance(*runtime,replay.inputs[i]);
            if(i==199) require(panel.compose(diagnostics::observe_alien_state(runtime->work_ram())).pixels==preview.pixels,"Score-table panel restores exactly");
            if(i+1==replay.inputs.size()) {
                require(std::equal(final_ram.begin(),final_ram.end(),runtime->work_ram().begin()),"Score viewer restore RAM parity");
                require(std::equal(final_pixels.begin(),final_pixels.end(),frame.pixels.begin(),frame.pixels.end()),"Score viewer restore video parity");
                require(std::equal(final_audio.begin(),final_audio.end(),frame.stereo.begin(),frame.stereo.end()),"Score viewer restore audio parity");
            }
        }
        auto output=core::path_from_utf8(argv[2]);output+=".acrec";recording::write(output,replay);
        output=core::path_from_utf8(argv[2]);output+=".ppm";
        std::ofstream image(output,std::ios::binary);image<<"P6\n256 96\n255\n";
        for(const auto color:preview.pixels) for(const auto shift:{16,8,0}) image.put(static_cast<char>(color>>shift));
        require(static_cast<bool>(image),"Write score-table preview");
        std::cout<<"250 score-viewer frames, unchanged five scores/names, resumed stitched play; exact restore passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}