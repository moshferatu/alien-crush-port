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
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM");
        const auto path=core::path_from_utf8(argv[1]);
        for(const bool cabinet:{false,true}) {
            auto runtime=cabinet?make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1,{},ActorMode::cabinet):
                make_reference_runtime();
            for(unsigned room=1;room<=kBonusRooms;++room) {
                runtime->load(path);
                for(unsigned n=0;n<120;++n) runtime->advance({});
                const std::vector<std::byte> before(runtime->work_ram().begin(),runtime->work_ram().end());
                require(!runtime->request_bonus_room(room) && std::equal(before.begin(),before.end(),runtime->work_ram().begin()),
                    "A bonus stage is not requested outside the shooter");
                start_in_bonus_room(*runtime,room);
                auto state=diagnostics::observe_alien_state(runtime->work_ram());
                require(state.bonus_room==room && !state.title_menu && !state.demo_play && runtime->work_ram()[0x7c]==std::byte{0},
                    "The requested bonus stage begins in an ordinary game, not practice");
                {
                    // The room fades in from black, with no bright frame first.
                    std::uint64_t last=0;
                    for(unsigned n=0;n<30;++n) {
                        const auto frame=runtime->advance({});
                        std::uint64_t light=0;
                        for(const auto pixel:frame.pixels) light+=(pixel&255)+((pixel>>8)&255)+((pixel>>16)&255);
                        require(light>=last && (n || light<100000),"The bonus stage opens on its fade-in without a flash");
                        last=light;
                    }
                    require(last>1000000,"The bonus stage fades in");
                }
                require(!runtime->request_bonus_room(0) && !runtime->request_bonus_room(kBonusRooms+1) && !runtime->request_bonus_room(room),
                    "Only rooms 1 to 4 are requested, and only from the table");
                // Left alone the ball is lost, and the game's ordinary return
                // puts it back on the table with every ball still to play.
                unsigned returned=0,earned=0;
                for(unsigned n=0;n<3000 && !returned;++n) {
                    runtime->advance({});
                    state=diagnostics::observe_alien_state(runtime->work_ram());
                    if(state.bonus_room) earned=state.score_points;
                    else if(state.table_code<2) returned=n+1;
                }
                require(returned && state.ball_counter==3 && state.score_points>=earned && !state.game_over,
                    "The bonus stage returns to the table with its score and all three balls");
                unsigned seated=0;
                for(unsigned n=0;n<3000 && !seated;++n) {
                    runtime->advance({});
                    if(reference_launcher_active(runtime->work_ram())) seated=n+1;
                }
                require(seated,"Play goes on after the bonus stage");
                std::cout<<(cabinet?"Cabinet":"Desktop")<<" bonus stage "<<room<<": on the table after "<<returned<<" frames with "
                    <<state.score_points<<" points\n";
            }
            runtime.reset();
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
