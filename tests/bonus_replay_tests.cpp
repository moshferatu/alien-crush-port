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
void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}
PadInput input_at(unsigned n,unsigned room) {
    auto input=recording::canonical_input(n);
    // Original title counters: Up 3, Down 6, Left 8, Right 3; I selects room.
    if(n>=100 && n<140) {
        const auto press=(n-100)/2;
        input.player_one=n%2?0:static_cast<std::uint16_t>(press<3?Pad::up:press<9?Pad::down:press<17?Pad::left:Pad::right);
    }
    if(n>=144 && n<144+room*2) input.player_one=n%2?0:Pad::button_i;
    return input;
}
PadInput tally_input(unsigned n) {
    if(n<360) return input_at(n,0);
    PadInput input{};
    if(n%21<10) input.player_one|=Pad::button_i;
    if((n+7)%21<10) input.player_one|=Pad::left;
    if(n%181==100) input.player_one|=Pad::button_ii;
    return input;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and output prefix");
        const auto path=core::path_from_utf8(argv[1]);
        const auto panel=render::ScorePanel::from_rom(core::read_file(path));
        const auto table=std::make_unique<render::TablePlayfield>(core::read_file(path));
        auto runtime=make_reference_runtime();
        for(unsigned room=0;room<4;++room) {
            runtime->load(path);
            std::optional<diagnostics::ReferenceSnapshot> previous;
            recording::Recording replay;
            recording::Recording loss_replay;
            std::vector<std::byte> final_ram;
            std::vector<std::uint32_t> final_pixels;
            std::vector<std::int16_t> final_audio;
            render::Image preview;
            render::Image loss_preview;
            unsigned scored=0,bonus_frames=0;
            unsigned ended_frames=0,round_ends=0;
            unsigned hidden_entry_frames=0;
            bool was_over=false,returned_to_title=false;
            const unsigned limit=room==0?3000:900;
            constexpr std::array<unsigned,3> end_frames{740,1463,2404};
            for(unsigned n=0;n<limit;++n) {
                const auto frame=runtime->advance(input_at(n,room));
                const auto ram=runtime->work_ram();
                const auto observed=diagnostics::observe_alien_state(ram);
                if(n<900 && observed.bonus_room) ++bonus_frames;
                if(n<900 && observed.score_points) ++scored;
                if(room==0) {
                    if(observed.bonus_round_over) ++ended_frames;
                    if(observed.bonus_round_over && !was_over) {
                        require(round_ends<end_frames.size() && n+1==end_frames[round_ends],"Original bonus round-end boundaries");
                        require(!observed.bonus_cleared,"Clear flag is not set during the initial round-over pause");
                        ++round_ends;
                    }
                    was_over=observed.bonus_round_over;
                    if(n==799) loss_preview=panel.compose(observed);
                    if(n==1063) require(observed.bonus_room==2 && observed.ball_counter==2,"First loss advances practice room and consumes ball");
                    if(n==1786) require(observed.bonus_room==3 && observed.ball_counter==1,"Second loss advances practice room and consumes ball");
                    if(n==2727) require(observed.ball_counter==0,"Third loss exhausts practice balls");
                    if(n==2876) {
                        require(observed.title_menu && !observed.bonus_room && !observed.bonus_round_over,"Practice exit clears bonus presentation at title");
                        returned_to_title=true;
                    }
                }
                if(n==limit-1) {
                    final_ram.assign(ram.begin(),ram.end());final_pixels.assign(frame.pixels.begin(),frame.pixels.end());
                    final_audio.assign(frame.stereo.begin(),frame.stereo.end());
                }
                if(n<=448) {
                    const auto video=diagnostics::decode_reference_snapshot(runtime->save());
                    const bool blank=render::cabinet_viewport_blank(video,previous?&*previous:nullptr,ram);
                    if(observed.bonus_room && !observed.title_menu) {
                        if(previous && !(previous->control&0xc0) && (video.control&0xc0)) {
                            require(blank,"Practice bonus entry hides its pre-fade raster in every room");
                            ++hidden_entry_frames;
                        } else require(!blank,"Practice bonus fade and gameplay remain visible");
                    }
                    previous=video;
                }
                if(n==449) {
                    require(std::to_integer<unsigned>(ram[0x7c])==1 && observed.bonus_room==room+1,"Original practice inputs select requested bonus room");
                    require(!observed.title_menu && !observed.demo_play,"Bonus room is not idle/demo");
                    preview=panel.compose(observed);replay.initial=runtime->save();
                    const auto video=diagnostics::decode_reference_snapshot(replay.initial);
                    require(!table->compose(video,*previous,observed.table_code,0,runtime->work_ram()),"Bonus keeps original playfield instead of main-table stitching");
                    render::TableTransitionHold hold;
                    (void)hold.present(render::Image{1,1,{0xff123456}},video.frame-1,0,0,runtime->work_ram());
                    require(!hold.present(std::nullopt,video.frame,observed.table_code,0,runtime->work_ram()),"Bonus clears an old main-table hold");
                }
                if(n>=450 && n<482) replay.inputs.push_back(input_at(n,room));
                if(room==0 && n==599) loss_replay.initial=runtime->save();
                if(room==0 && n>=600 && n<900) loss_replay.inputs.push_back(input_at(n,room));
            }
            require(bonus_frames==562,"Bonus play observed after original room loading");
            require(hidden_entry_frames==1,"Each practice room covers one display-enable transition");
            // This short input route earns points in rooms 1..3. Room 4's
            // coverage is entry/rendering/restore, not a claimed award or clear.
            if(room<3) require(scored>0,"Original bonus score awards observed");
            runtime->restore(replay.initial);
            require(panel.compose(diagnostics::observe_alien_state(runtime->work_ram())).pixels==preview.pixels,"Restore bonus score presentation");
            if(room==0) require(round_ends==3 && ended_frames==450 && returned_to_title,"Three 150-frame endings and original title return");
            for(unsigned n=450;n<limit;++n) {
                const auto frame=runtime->advance(input_at(n,room));
                if(n==limit-1) {
                    require(std::equal(final_ram.begin(),final_ram.end(),runtime->work_ram().begin()),"Bonus restore RAM parity");
                    require(std::equal(final_pixels.begin(),final_pixels.end(),frame.pixels.begin(),frame.pixels.end()),"Bonus restore video parity");
                    require(std::equal(final_audio.begin(),final_audio.end(),frame.stereo.begin(),frame.stereo.end()),"Bonus restore audio parity");
                }
            }
            auto output=core::path_from_utf8(argv[2]);output+=".room"+std::to_string(room+1);
            auto recording_path=output;recording_path+=".acrec";recording::write(recording_path,replay);
            output+=".ppm";std::ofstream image(output,std::ios::binary);image<<"P6\n256 96\n255\n";
            for(const auto color:preview.pixels) for(const auto shift:{16,8,0}) image.put(static_cast<char>(color>>shift));
            require(static_cast<bool>(image),"Write bonus preview");
            std::cout<<"Room "<<room+1<<": "<<bonus_frames<<" bonus frames, "<<scored<<" frames with awarded score; restore parity passed\n";
                if(room==0) {
                auto loss_path=core::path_from_utf8(argv[2]);loss_path+=".loss.acrec";recording::write(loss_path,loss_replay);
                auto preview_path=core::path_from_utf8(argv[2]);preview_path+=".loss.ppm";
                std::ofstream ended(preview_path,std::ios::binary);ended<<"P6\n256 96\n255\n";
                for(const auto color:loss_preview.pixels) for(const auto shift:{16,8,0}) ended.put(static_cast<char>(color>>shift));
                require(static_cast<bool>(ended),"Write round-over preview");
                }
        }
        // A controller-only route earns an 80,000-point results tally after
        // 13,200 points of live play. Completion is evaluated after ROUND OVER.
        runtime->load(path);
        recording::Recording tally;
        std::vector<std::byte> final_ram;
        std::vector<std::uint32_t> final_pixels;
        std::vector<std::int16_t> final_audio;
        unsigned tally_changes=0;
        std::uint32_t prior_score=0;
        for(unsigned n=0;n<3300;++n) {
            const auto frame=runtime->advance(tally_input(n));
            const auto observed=diagnostics::observe_alien_state(runtime->work_ram());
            if(n==2799) tally.initial=runtime->save();
            if(n>=2800) tally.inputs.push_back(tally_input(n));
            if(n==2835) require(observed.bonus_round_over && observed.score_points==13200,"Tally route ends live play at 13,200");
            if(n>=2836 && n<3244 && observed.score_points!=prior_score) ++tally_changes;
            if(n>=2800) require(!observed.bonus_cleared,"Tally route does not earn a clear result");
            if(n==3243) require(observed.bonus_room==2 && observed.ball_counter==2 && observed.score_points==93200,"Original results award 80,000 before consuming ball and advancing room");
            prior_score=observed.score_points;
            if(n==3299) {
                const auto ram=runtime->work_ram();final_ram.assign(ram.begin(),ram.end());
                final_pixels.assign(frame.pixels.begin(),frame.pixels.end());
                final_audio.assign(frame.stereo.begin(),frame.stereo.end());
            }
        }
        require(tally_changes>1,"Results score advances in multiple original tally steps");
        runtime->restore(tally.initial);
        for(std::size_t i=0;i<tally.inputs.size();++i) {
            const auto frame=recording::advance(*runtime,tally.inputs[i]);
            if(i+1==tally.inputs.size()) {
                require(std::equal(final_ram.begin(),final_ram.end(),runtime->work_ram().begin()),"Results tally restore RAM parity");
                require(std::equal(final_pixels.begin(),final_pixels.end(),frame.pixels.begin(),frame.pixels.end()),"Results tally restore video parity");
                require(std::equal(final_audio.begin(),final_audio.end(),frame.stereo.begin(),frame.stereo.end()),"Results tally restore audio parity");
            }
        }
        auto tally_path=core::path_from_utf8(argv[2]);tally_path+=".tally.acrec";recording::write(tally_path,tally);
        std::cout<<"Results tally: 13,200 to 93,200, "<<tally_changes<<" score changes; restore parity passed\n";
        auto clear_input=[](unsigned n) {
            if(n<360) return input_at(n,2);
            PadInput input{};
            if(n%8<4) input.player_one|=Pad::button_i;
            if((n+2)%8<4) input.player_one|=Pad::left;
            if(n%181==100) input.player_one|=Pad::button_ii;
            return input;
        };
        runtime->load(path);
        recording::Recording clear;
        unsigned clear_frames=0;
        bool next_room=false;
        render::Image clear_preview;
        for(unsigned n=0;n<5500;++n) {
            const auto frame=runtime->advance(clear_input(n));
            const auto observed=diagnostics::observe_alien_state(runtime->work_ram());
            if(n==4799) clear.initial=runtime->save();
            if(n>=4800) clear.inputs.push_back(clear_input(n));
            if(observed.bonus_cleared) {
                if(clear_frames==0) {
                    require(n==4973 && observed.bonus_room==3 && observed.score_points==448200,"Original room-3 clear result boundary and award");
                    require(!observed.bonus_round_over,"Clear is evaluated after the round-over pause");
                    clear_preview=panel.compose(observed);
                }
                ++clear_frames;
            }
            if(n>4973 && observed.bonus_room==4) {
                require(observed.ball_counter==3,"Successful practice clear preserves ball count");
                next_room=true;
            }
            if(n==5499) {
                const auto ram=runtime->work_ram();final_ram.assign(ram.begin(),ram.end());
                final_pixels.assign(frame.pixels.begin(),frame.pixels.end());
                final_audio.assign(frame.stereo.begin(),frame.stereo.end());
            }
        }
        require(clear_frames>0 && next_room,"Clear results advance to next practice room");
        runtime->restore(clear.initial);
        for(std::size_t i=0;i<clear.inputs.size();++i) {
            const auto frame=recording::advance(*runtime,clear.inputs[i]);
            if(i==173) require(panel.compose(diagnostics::observe_alien_state(runtime->work_ram())).pixels==clear_preview.pixels,"Clear presentation restores exactly");
            if(i+1==clear.inputs.size()) {
                require(std::equal(final_ram.begin(),final_ram.end(),runtime->work_ram().begin()),"Successful clear restore RAM parity");
                require(std::equal(final_pixels.begin(),final_pixels.end(),frame.pixels.begin(),frame.pixels.end()),"Successful clear restore video parity");
                require(std::equal(final_audio.begin(),final_audio.end(),frame.stereo.begin(),frame.stereo.end()),"Successful clear restore audio parity");
            }
        }
        auto clear_path=core::path_from_utf8(argv[2]);clear_path+=".clear.acrec";recording::write(clear_path,clear);
        auto preview_path=core::path_from_utf8(argv[2]);preview_path+=".clear.ppm";
        std::ofstream cleared(preview_path,std::ios::binary);cleared<<"P6\n256 96\n255\n";
        for(const auto color:clear_preview.pixels) for(const auto shift:{16,8,0}) cleared.put(static_cast<char>(color>>shift));
        require(static_cast<bool>(cleared),"Write clear preview");
        std::cout<<"Room 3 clear: "<<clear_frames<<" clear frames, next room with all three balls; restore parity passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
