// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
// A full plunger shot, then both flippers worked on a short cycle.
PadInput play(unsigned n) {
    if(n<100) return {Pad::button_i,0};
    std::uint16_t pad=0;
    if(n%39<19) pad|=Pad::button_i;
    if((n+13)%39<19) pad|=Pad::left;
    return {pad,0};
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM");
        const auto path=core::path_from_utf8(argv[1]);
        for(const bool cabinet:{false,true}) {
            auto runtime=cabinet?make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1,{},ActorMode::cabinet):
                make_reference_runtime();
            runtime->load(path);
            for(unsigned n=0;n<120;++n) runtime->advance({});
            const std::vector<std::byte> before(runtime->work_ram().begin(),runtime->work_ram().end());
            require(!runtime->set_score(kNearMaximumScore) && std::equal(before.begin(),before.end(),runtime->work_ram().begin()),
                "A score is not given outside the shooter");
            start_with_score(*runtime,kNearMaximumScore);
            auto state=diagnostics::observe_alien_state(runtime->work_ram());
            require(state.score_points==kNearMaximumScore && state.ball_counter==3 && !state.title_menu && !state.demo_play &&
                !state.bonus_room && runtime->work_ram()[0x7c]==std::byte{0} && reference_launcher_seat(runtime->work_ram()),
                "The game starts just under the maximum score, in the shooter of an ordinary game");
            require(!runtime->set_score(kMaximumScore+100) && !runtime->set_score(150),
                "Only a multiple of 100 up to the maximum is given");
            // Played, the score climbs to the maximum and stops there. The
            // game then ends by itself: the table blows up, CONGRATULATIONS
            // is shown, and the score is ranked first.
            unsigned reached=0,ranked=0;
            bool stored_explosions=false,congratulations=false;
            std::uint32_t last=state.score_points;
            for(unsigned n=0;n<6000 && !ranked;++n) {
                runtime->advance(play(n));
                state=diagnostics::observe_alien_state(runtime->work_ram());
                // The stored half's actors (work RAM $2200) blow up with the
                // ball's half's in cabinet play; the cartridge leaves them.
                const auto ram=runtime->work_ram();
                if(reached && (ram[0x9d]&std::byte{0x40})==std::byte{})
                    stored_explosions|=std::find(ram.begin()+0x204,ram.begin()+0x240,std::byte{50})!=ram.begin()+0x240;
                if(state.ending_screen) {
                    congratulations=true;
                    require(std::all_of(ram.begin()+0x204,ram.begin()+0x240,[](std::byte type) {return type==std::byte{};}) || !cabinet,
                        "The stored half is empty by the congratulations screen");
                }
                if(state.high_scores[0]==kMaximumScore) {ranked=n+1;break;}
                require(state.score_points>=last && state.score_points<=kMaximumScore,"The score only climbs, and never past the maximum");
                last=state.score_points;
                if(state.score_points==kMaximumScore && !reached) {
                    reached=n+1;
                    require((runtime->work_ram()[0x9d]&std::byte{0x80})!=std::byte{0},"Reaching the maximum sets the game's ending flag");
                }
            }
            require(reached && ranked>reached && state.ball_counter==3 && !state.game_over,
                "Play reaches the maximum score, and the game's ending ranks it first with balls still to play");
            require(congratulations && state.high_score_menu && !state.ending_screen,
                "The congratulations screen and the ranking after it are recognized");
            require(stored_explosions==cabinet,"The stored half blows up in cabinet play only");
            std::cout<<(cabinet?"Cabinet":"Desktop")<<": the maximum score after "<<reached<<" frames of play, ranked first after "<<ranked<<'\n';
            // With a bonus stage as well, the score goes into the stage.
            runtime->load(path);
            start_with_score(*runtime,kNearMaximumScore);
            start_in_bonus_room(*runtime,2);
            state=diagnostics::observe_alien_state(runtime->work_ram());
            require(state.bonus_room==2 && state.score_points==kNearMaximumScore,"A bonus stage starts with the same score");
            runtime.reset();
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
