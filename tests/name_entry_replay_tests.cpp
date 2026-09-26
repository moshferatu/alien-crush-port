// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/render/score_panel.hpp"
#include "../src/host/launch_input.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
PadInput input_at(unsigned n,const diagnostics::AlienState& state) {
    PadInput input{};
    if(n<9656 && state.score_points<1100000) {
        if(n%9<4) input.player_one|=Pad::button_i;
        if((n+3)%9<4) input.player_one|=Pad::left;
        if(n%181==100) input.player_one|=Pad::button_ii;
    }
    if(n==9800 || n==9808) input.player_one=Pad::right;
    if(n==9804) input.player_one=Pad::left;
    if(n==9802 || n==9803 || n==9806 || n==9807 || (n>=9810 && n<9850)) input.player_one=Pad::run;
    if(n==10100 || n==10160 || n==10220) input.player_one=Pad::run;
    return input;
}
}
int main(int argc,char** argv) {
 try {
    require(argc==4,"Expected ROM, clear recording and output prefix");
    const auto path=core::path_from_utf8(argv[1]);
    auto runtime=make_reference_runtime();runtime->load(path);
    const auto panel=render::ScorePanel::from_rom(core::read_file(path));
    const auto route=recording::read(core::path_from_utf8(argv[2]));
    require(route.initial.frame_number==4800 && route.inputs.size()==700 && route.plunger.empty(),"Expected successful room-3 clear fixture");
    runtime->restore(route.initial);
    for(const auto input:route.inputs) runtime->advance(input);
    recording::Recording replay;
    render::Image preview;
    std::vector<std::byte> final_ram;
    std::vector<std::uint32_t> final_pixels;
    std::vector<std::int16_t> final_audio;
    bool title_seen=false,new_game=false;
    unsigned first_ranked=0,viewer_frames=0,exit_frames=0;
    render::PanelHold hold;
    render::BallNumber ball;
    render::Image rankings;
    host::GameInputRouter input_router;
    for(unsigned n=5500;n<10500;++n) {
        const auto input=input_router.route(input_at(n,diagnostics::observe_alien_state(runtime->work_ram())),runtime->work_ram());
        const auto frame=runtime->advance(input);
        const std::vector<std::uint32_t> pixels(frame.pixels.begin(),frame.pixels.end());
        const std::vector<std::int16_t> audio(frame.stereo.begin(),frame.stereo.end());
        const auto ram=runtime->work_ram();const auto state=diagnostics::observe_alien_state(ram);
        require(!state.demo_play,"Qualifying route stays outside demo");
        if(state.high_scores[0]>1000000 && !first_ranked) first_ranked=n+1;
        if(n+1>=9656) {
            require(state.high_scores[0]==1505700,"Original score sort promotes 1,505,700 to first place");
            for(unsigned rank=1;rank<5;++rank) {
                require(state.high_scores[rank]==1000000,"Other ranked scores remain at defaults");
                for(unsigned letter=0;letter<3;++letter) require(ram[0x122b+letter*6+rank]==std::byte{0x40},"Editing winner preserves other names");
            }
            const std::array<unsigned,3> expected{n>=9800?0x41U:0x40U,n>=9804?0x5bU:0x40U,n>=9808?0x41U:0x40U};
            for(unsigned letter=0;letter<3;++letter) require(std::to_integer<unsigned>(ram[0x122b+letter*6])==expected[letter],"Original character increment, decrement wrap and advance");
            const std::array<char,3> initials{n>=9800?'A':'.',n>=9804?' ':'.',n>=9808?'A':'.'};
            require(state.high_score_initials[0]==initials,"DMD initials follow editing, confirmation and the next game");
        }
        if(n>=9700 && n<9900) require(state.high_score_menu,"Ranking layout spans name editing and confirmation pause");
        viewer_frames+=state.high_score_menu;
        const auto& shown=hold.update(panel,state,ball);
        if(state.high_score_menu) rankings=shown;
        if(n>=9900 && !state.high_score_menu && !state.title_menu && !title_seen) {
            // Practice ends one call deeper (01:448D) than a normal game.
            ++exit_frames;
            require(state.title_transition,"Every practice ranking-to-title frame is recognized");
            require(shown.pixels==rankings.pixels,"DMD keeps the rankings until the title");
        }
        if(n==9803 || n==9807) require(ram[0x120c]!=std::byte{0x80},"First two held Enter presses advance without submitting");
        if(n==9810) require(ram[0x120c]==std::byte{0x80},"Third Enter press confirms name entry");
        if(n==9899) require(state.high_score_menu,"Confirmation keeps ranked-score layout");
        if(n==9999) {require(state.title_menu && !state.high_score_menu,"Confirmed name returns to title");title_seen=true;}
        if(n>10300 && !state.title_menu && state.ball_counter==3 && state.score_points==0) new_game=true;
        if(n==9399) replay.initial=runtime->save();
        if(n>=9400) replay.inputs.push_back(input);
        if(n==9808) preview=panel.compose(state);
        if(n==10499) {final_ram.assign(ram.begin(),ram.end());final_pixels=pixels;final_audio=audio;}
    }
    require(first_ranked==9656 && title_seen && new_game,"Qualification, confirmation, title and new game covered");
    require(exit_frames>=20,"Regression covers the practice table reload and title reset");
    runtime->restore(replay.initial);
    for(unsigned i=0;i<replay.inputs.size();++i) {
        const auto frame=recording::advance(*runtime,replay.inputs[i]);
        if(i==408) require(panel.compose(diagnostics::observe_alien_state(runtime->work_ram())).pixels==preview.pixels,"Name-entry score panel restores exactly");
        if(i+1==replay.inputs.size()) {
            require(std::equal(final_ram.begin(),final_ram.end(),runtime->work_ram().begin()),"Name-entry restore RAM parity");
            require(std::equal(final_pixels.begin(),final_pixels.end(),frame.pixels.begin(),frame.pixels.end()),"Name-entry restore video parity");
            require(std::equal(final_audio.begin(),final_audio.end(),frame.stereo.begin(),frame.stereo.end()),"Name-entry restore audio parity");
        }
    }
    auto output=core::path_from_utf8(argv[3]);output+=".acrec";recording::write(output,replay);
    output=core::path_from_utf8(argv[3]);output+=".ppm";std::ofstream image(output,std::ios::binary);image<<"P6\n256 96\n255\n";
    for(const auto color:preview.pixels)for(const auto shift:{16,8,0})image.put(static_cast<char>(color>>shift));
    require(static_cast<bool>(image),"Write name-entry score preview");
    std::cout<<"1,505,700 ranked at 9656; name 41/5B/41 confirmed and retained into new game; "<<viewer_frames<<" score-viewer frames; exact restore passed\n";
    return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
