// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include "launch_input.hpp"
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and recording path");
        auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet);
        runtime->load(core::path_from_utf8(argv[1]));
        for(unsigned n=0;n<800;++n) {
            (void)runtime->advance(n<360?recording::canonical_input(n):PadInput{});
            if(n>=360 && reference_launcher_seat(runtime->work_ram())) break;
        }
        require(reference_launcher_seat(runtime->work_ram()).has_value(),"Seated ball before first launch");
        const auto initial=runtime->save();
        unsigned transient_seats=0;
        for(const unsigned held:{40u,120u,177u,260u}) {
            runtime->restore(initial);
            host::GameInputRouter router;
            for(unsigned n=0;n<held;++n) {
                const auto ram=runtime->work_ram();
                if(!reference_launcher_seat(ram)) ++transient_seats;
                const auto pad=router.route({Pad::run,0},ram);
                require(pad.player_one==Pad::button_i,"Held Enter remains launch through every draw/return pose");
                (void)runtime->advance(pad);
                require((std::to_integer<unsigned>(runtime->work_ram()[0xf6])&2)==0,"Full charge cannot pause game");
            }
            (void)runtime->advance(router.route({},runtime->work_ram()));
            // A second press during departure must still not pause. The
            // launch gesture remains launch even if the ball leaves while held.
            bool departed=false;
            for(unsigned n=0;n<240;++n) {
                if(!reference_launcher_active(runtime->work_ram())) {departed=true;break;}
                const auto pad=router.route({Pad::run,0},runtime->work_ram());
                require(!(pad.player_one&Pad::run),"Enter cannot pause before ball leaves shooter");
                (void)runtime->advance(pad);
            }
            require(departed,"Released shot leaves shooter session");
            require(!(router.route({Pad::run,0},runtime->work_ram()).player_one&Pad::run),"Held launch cannot become pause on departure");
            (void)runtime->advance(router.route({},runtime->work_ram()));
            const auto pause=router.route({Pad::run,0},runtime->work_ram());
            require((pause.player_one&Pad::run)!=0,"Fresh Enter can pause after departure");
            (void)runtime->advance(pause);
            require((std::to_integer<unsigned>(runtime->work_ram()[0xf6])&2)!=0,"Post-launch pause still works");
        }
        require(transient_seats>0,"Regression covers transient failure of strict plunger-write seat gate");
        auto phase=[&](unsigned type) {
            const auto ram=runtime->work_ram();
            for(unsigned slot=4;slot<64;++slot) if(std::to_integer<unsigned>(ram[0x1500+slot])==type)
                return std::to_integer<unsigned>(ram[0x1c40+slot]);
            throw std::runtime_error("Missing flipper");
        };
        for(const unsigned mask:{unsigned(Pad::left),unsigned(Pad::right),unsigned(Pad::left|Pad::up)}) {
            runtime->restore(initial);
            for(unsigned n=0;n<24;++n) (void)runtime->advance({static_cast<std::uint16_t>(mask),0});
            const auto seat=reference_launcher_seat(runtime->work_ram());
            require(seat && seat->travel==0,"Flippers never draw/launch the seated ball");
            require(phase(2)==((mask&Pad::left)?3u:0u),"Left flipper responds while ball is seated");
            require(phase(3)==((mask&(Pad::right|Pad::up))?3u:0u),"Right flipper responds while ball is seated");
            for(unsigned n=0;n<24;++n) (void)runtime->advance({});
            require(phase(2)==0 && phase(3)==0,"Both flippers release normally in shooter");
        }
        runtime->restore(initial);
        for(unsigned n=0;n<40;++n) (void)runtime->advance({Pad::button_i,0});
        const auto drawn=reference_launcher_seat(runtime->work_ram());
        require(drawn && drawn->travel>0,"Dedicated launch draws shooter");
        require(phase(2)==0 && phase(3)==0,"Launch does not operate either flipper");
        for(unsigned n=0;n<24;++n) (void)runtime->advance({Pad::left|Pad::up,0});
        require(!reference_launcher_seat(runtime->work_ram()),"Releasing launch fires the ball");
        require(phase(2)==3 && phase(3)==3,"Both flippers respond during launcher departure");
        runtime->restore(initial);
        (void)runtime->advance({Pad::run,0});
        for(unsigned n=0;n<24;++n) (void)runtime->advance({Pad::left|Pad::up,0});
        require(phase(2)==0 && phase(3)==0,"Pause still freezes flippers");
        runtime->restore(initial);
        recording::Recording capture{initial,{}};
        std::vector<std::string> fingerprints;
        for(unsigned n=0;n<160;++n) {
            PadInput pad{static_cast<std::uint16_t>((n<40?Pad::button_i:0)|(n%32<16?Pad::left|Pad::up:0)),0};
            capture.inputs.push_back(pad);
            const auto frame=runtime->advance(pad);
            fingerprints.push_back(recording::fingerprint(frame,runtime->work_ram()));
        }
        recording::write(core::path_from_utf8(argv[2]),capture);
        runtime->restore(initial);
        for(unsigned n=0;n<capture.inputs.size();++n) {
            const auto frame=recording::advance(*runtime,capture.inputs[n]);
            require(recording::fingerprint(frame,runtime->work_ram())==fingerprints[n],"Controls replay exactly restores RAM/video/audio");
        }
        runtime.reset();runtime=make_reference_runtime(HandoffMode::continuous);
        runtime->load(core::path_from_utf8(argv[1]));
        bool rejected=false;try {runtime->restore(initial);}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Original runtime rejects changed-controls state");
        std::cout<<"Cabinet flippers, separate launch, pause and exact replay passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
