// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/render/score_panel.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <algorithm>
#include <fstream>
#include "cabinet_presentation_check.hpp"
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
PadInput input_at(unsigned n,const recording::Recording& route) {
    if(n<2450) return route.inputs.at(n-2150);
    if(n<2800) return recording::canonical_input(n);
    // Release Left; pulse I (shared launch/right-flipper button) for later balls.
    return {static_cast<std::uint16_t>(n%240<100?Pad::button_i:0),0};
}
}
int main(int argc,char** argv) {
    try {
        require(argc==4,"Expected ROM, ordinary return recording and output prefix");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        const auto panel=render::ScorePanel::from_rom(rom);
        const auto table=std::make_unique<render::TablePlayfield>(rom);
        auto runtime=make_reference_runtime();runtime->load(path);
        const auto route=recording::read(core::path_from_utf8(argv[2]));
        runtime->restore(route.initial);
        require(route.inputs.size()==300 && route.plunger.empty() && route.initial.frame_number==2150,"Expected ordinary bonus return fixture");
        const bool timed=runtime->supports_display_observation();
        if(timed) runtime->observe_display(true);
        recording::Recording ending;
        CabinetPresentationCheck cabinet_check;
        std::optional<diagnostics::ReferenceSnapshot> previous;
        render::TableTransitionHold hold;
        render::Image preview;
        std::vector<std::byte> final_ram;
        std::vector<std::uint32_t> final_pixels;
        std::vector<std::int16_t> final_audio;
        unsigned drops=0,game_over_frames=0,stitched_frames=0;
        std::uint8_t last_ball=3;
        constexpr std::array<unsigned,3> drop_frames{2845,3760,4732};
        for(unsigned n=2150;n<5700;++n) {
            const auto frame=runtime->advance(input_at(n,route));
            const std::vector<std::uint32_t> pixels(frame.pixels.begin(),frame.pixels.end());
            const std::vector<std::int16_t> audio(frame.stereo.begin(),frame.stereo.end());
            const auto ram=runtime->work_ram();
            const auto state=diagnostics::observe_alien_state(ram);
            require(!state.demo_play && ram[0x7c]==std::byte{},"Ordinary drains remain outside practice and demo");
            if(state.ball_counter!=last_ball) {
                require(drops<drop_frames.size() && n+1==drop_frames[drops] && state.ball_counter+1==last_ball,"Original ball consumption boundaries");
                ++drops;last_ball=state.ball_counter;
            }
            require(state.game_over==(n+1>=4732 && n+1<5503),"Final-ball game-over presentation boundaries");
            game_over_frames+=state.game_over;
            if(n>=4731 && n<5503) require(state.score_points==22200,"Final score remains 22,200 through game over");
            if(n==4799) preview=panel.compose(state);
            if(n==5516) require(state.title_menu && !state.game_over && state.score_points==0,"Title clears game-over presentation and current score");
            if(n==4699) ending.initial=runtime->save();
            if(n>=4700) ending.inputs.push_back(input_at(n,route));
            if(n>=4400) {
                const auto video=diagnostics::decode_reference_snapshot(runtime->save());
                const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            cabinet_check.check(*table,video,previous?&*previous:nullptr,ram,timed?&raster:nullptr);
                const auto camera=std::to_integer<unsigned>(ram[0x16])|(std::to_integer<unsigned>(ram[0x17])<<8);
                const auto output=previous?table->compose(video,*previous,state.table_code,camera,ram,timed?&raster:nullptr):std::nullopt;
                const auto presented=hold.present(output,video.frame,state.table_code,camera,ram);
                if(state.title_menu) require(!output && !presented,"Title discards any stitched game-over table");
                if(output && timed) {
                    ++stitched_frames;
                    const unsigned offset=state.table_code==0?render::table_half_offset:0;
                    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<256;++x) {
                        if((state.table_code==0 && y<240-render::table_half_offset) || (state.table_code==1 && y>=render::table_half_offset)) continue;
                        if((y>=16 && y<32 && x>=16 && x<112) || (y>=32 && y<48 && x>=232 && x<248)) continue;
                        if(output->pixels[(y+offset)*256+x]!=pixels[y*256+x]) {
                            auto debug=core::path_from_utf8(argv[3]);debug+=".mismatch.ppm";
                            std::ofstream image(debug,std::ios::binary);image<<"P6\n512 240\n255\n";
                            for(unsigned row=0;row<240;++row) for(unsigned col=0;col<512;++col) {
                                const auto color=col<256?pixels[row*256+col]:output->pixels[(row+offset)*256+col-256];
                                for(const auto shift:{16,8,0}) image.put(static_cast<char>(color>>shift));
                            }
                            throw std::runtime_error("Game-over stitched active pixels differ at frame "+std::to_string(n+1)+" x="+std::to_string(x)+" y="+std::to_string(y));
                        }
                    }
                }
                previous=video;
            }
            if(n==5699) {final_ram.assign(ram.begin(),ram.end());final_pixels=pixels;final_audio=audio;}
        }
        require(drops==3 && game_over_frames==771,"All three drains and complete game-over interval covered");
        require(cabinet_check.verified_match_frames>100,"Losing match digits remain visible through the result");
        if(timed) require(stitched_frames>0,"Last-ball route exercises stitched rendering");
        runtime->restore(ending.initial);
        for(unsigned i=0;i<ending.inputs.size();++i) {
            const auto frame=recording::advance(*runtime,ending.inputs[i]);
            if(i==99) require(panel.compose(diagnostics::observe_alien_state(runtime->work_ram())).pixels==preview.pixels,"Game-over score panel restores exactly");
            if(i+1==ending.inputs.size()) {
                require(std::equal(final_ram.begin(),final_ram.end(),runtime->work_ram().begin()),"Game-over restore RAM parity");
                require(std::equal(final_pixels.begin(),final_pixels.end(),frame.pixels.begin(),frame.pixels.end()),"Game-over restore video parity");
                require(std::equal(final_audio.begin(),final_audio.end(),frame.stereo.begin(),frame.stereo.end()),"Game-over restore audio parity");
            }
        }
        auto prefix=core::path_from_utf8(argv[3]);auto output=prefix;output+=".acrec";recording::write(output,ending);
        output=prefix;output+=".ppm";std::ofstream image(output,std::ios::binary);image<<"P6\n256 96\n255\n";
        for(const auto color:preview.pixels) for(const auto shift:{16,8,0}) image.put(static_cast<char>(color>>shift));
        require(static_cast<bool>(image),"Write game-over score preview");
        std::cout<<"Three drains, 771 game-over frames, title return; "<<stitched_frames<<" stitched frames; exact restore passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
