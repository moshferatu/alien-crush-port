// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_plunger.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {void require(bool value,const char* message){if(!value) throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM path");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
        // Only the virtual board: an axis-only binding takes the first
        // joystick with that axis, which a connected controller would be.
        for(const auto* driver:{SDL_HINT_JOYSTICK_HIDAPI,SDL_HINT_JOYSTICK_RAWINPUT,SDL_HINT_JOYSTICK_DIRECTINPUT,
            SDL_HINT_JOYSTICK_GAMEINPUT,SDL_HINT_JOYSTICK_WGI,SDL_HINT_XINPUT_ENABLED}) SDL_SetHint(driver,"0");
        require(SDL_Init(SDL_INIT_JOYSTICK),"SDL initialization");
        bool controls_supported=false;
        for(const auto controls:{ControlMode::original,ControlMode::cabinet}) {
            if(controls==ControlMode::cabinet && !controls_supported) continue;
            auto runtime=make_reference_runtime(HandoffMode::original,controls);runtime->load(core::path_from_utf8(argv[1]));
            controls_supported=runtime->supports_display_observation();
            for(unsigned n=0;n<480;++n) (void)runtime->advance(recording::canonical_input(n));
            const auto ready=runtime->save();
            auto attach=[] {
                SDL_VirtualJoystickDesc desc{};SDL_INIT_INTERFACE(&desc);
                desc.name="Virtual PinOne plunger";desc.naxes=3;
                const auto id=SDL_AttachVirtualJoystick(&desc);require(id!=0,"Attach plunger");return id;
            };
            auto id=attach();auto* device=SDL_OpenJoystick(id);require(device!=nullptr,"Open plunger");
            host::CabinetPlunger unnamed(host::parse_cabinet_plunger_ini("input.plunger.axis=2\n"));
            require(unnamed.id()==id,"Devil's Crush axis-only binding selects raw joystick without a name");
            host::CabinetPlunger rod(host::parse_cabinet_plunger_ini(
                "input.plunger.device=virtual pinone\ninput.plunger.axis=2\ninput.plunger.rest=1000\n"
                "input.plunger.drawn=-1000\ninput.plunger.deadzone=0\ninput.plunger.release=25\n"));
            require(rod.id()==id,"Select named axis-only board");
            auto axis=[&](Sint16 value) {require(SDL_SetJoystickVirtualAxis(device,2,value),"Set plunger axis");SDL_UpdateJoysticks();};
            auto byte=[&](unsigned offset){return std::to_integer<unsigned>(runtime->work_ram()[offset]);};
            auto word=[&](unsigned lo,unsigned hi){return byte(lo)|(byte(hi)<<8);};
            for(unsigned scenario=0;scenario<4;++scenario) {
                runtime->restore(ready);rod.restored();
                recording::Recording recorded{ready,{},{}};
                std::vector<std::string> hashes;
                auto step=[&](std::uint16_t buttons=0,bool focused=true) {
                    PadInput pad{buttons,0};const auto command=rod.apply(*runtime,pad,focused);
                    recorded.inputs.push_back(pad);recorded.plunger.push_back(command);
                    const auto frame=runtime->advance(pad);
                    hashes.push_back(recording::fingerprint(frame,runtime->work_ram()));
                    return command;
                };
                axis(-1000);require(step()==0 && !rod.owns_draw(),"Startup held rod is inert");
                axis(1000);require(step()==0,"Rest rearms");
                axis(0);require(step(Pad::button_i|Pad::button_ii|Pad::left)==50,"Calibrated half draw");
                require(rod.owns_draw() && recorded.inputs.back().player_one==(Pad::button_i|Pad::left),"Rod owns shared launcher buttons");
                require(word(0x1c06,0x1c46)==1024,"Half draw mapped to original travel");
                if(scenario==0) {
                    axis(900);require(step(Pad::button_i|Pad::button_ii)==50,"Release keeps preceding draw");
                    require(byte(0x1846)==4 && word(0x1786,0x17c6)==64512,"Original half-strength release");
                    axis(-1000);require(step()==0 && !rod.owns_draw(),"Spring bounce cannot reacquire");
                } else {
                    if(scenario==1) {
                        rod.interrupt();require(step(Pad::button_i|Pad::button_ii,false)==101,"Focus loss cancellation");
                    } else if(scenario==2) {
                        require(SDL_DetachVirtualJoystick(id),"Disconnect drawn rod");
                        require(step(Pad::button_i)==101,"Disconnect cancellation");
                        SDL_CloseJoystick(device);id=attach();device=SDL_OpenJoystick(id);require(device!=nullptr,"Reconnect plunger");
                        axis(-1000);rod.refresh();require(rod.id()==id,"Reconnect new instance");
                    } else {
                        require(step(Pad::run)==50,"Pause begins with owned draw");
                        require((byte(0xf6)&2)!=0,"Game paused");
                        require(step()==101,"Paused draw cancels without firing");
                        (void)step(Pad::run);
                    }
                    require(!rod.owns_draw() && byte(0x1846)==0,"Cancellation leaves launcher at rest");
                    axis(-1000);
                    for(unsigned n=0;n<25;++n) {
                        require(step(Pad::button_i|Pad::button_ii)==0,"Held buttons and rod suppressed after cancellation");
                        require(byte(0x1846)==0,"No cancelled shot fires");
                    }
                    axis(1000);require(step(Pad::button_i)==0,"Buttons must also return to rest");
                    axis(-1000);require(step()==0,"Earlier rest cannot arm a currently pulled rod");
                    axis(1000);(void)step();axis(-500);require(step()==75,"Fresh draw after cancellation");
                    axis(1000);require(step()==75 && byte(0x1846)==4,"Fresh release after cancellation");
                }
                for(unsigned n=0;n<30;++n) (void)step();
                runtime->restore(recorded.initial);
                for(std::size_t n=0;n<hashes.size();++n) {
                    const auto frame=recording::advance(*runtime,recorded.inputs[n],recorded.plunger[n]);
                    require(recording::fingerprint(frame,runtime->work_ram())==hashes[n],"Virtual hardware session replay divergence");
                }
            }
            // Quick-load discards ownership of the replaced state and requires rest.
            runtime->restore(ready);rod.restored();axis(1000);PadInput pad{};
            (void)rod.apply(*runtime,pad,true);axis(0);pad={};
            require(rod.apply(*runtime,pad,true)==50,"Quick-load setup owns draw");
            runtime->restore(ready);rod.restored();pad={};
            require(rod.apply(*runtime,pad,true)==0 && !rod.owns_draw(),"Quick-load drops stale ownership");
            require(state::encode(runtime->save())==state::encode(ready),"Quick-load rearm leaves restored runtime untouched");
            SDL_CloseJoystick(device);require(SDL_DetachVirtualJoystick(id),"Detach final device");
        }
        SDL_Quit();std::cout<<"Virtual plunger launch, focus, disconnect, pause, quick-load and exact replay passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';SDL_Quit();return 1;}
}
