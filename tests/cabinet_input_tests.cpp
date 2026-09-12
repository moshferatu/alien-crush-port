// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_input.hpp"
#include "cabinet_plunger.hpp"
#include "game_input.hpp"
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
SDL_JoystickID attach(const char* name,int buttons) {
    SDL_VirtualJoystickDesc desc{}; SDL_INIT_INTERFACE(&desc);
    desc.name=name;desc.nbuttons=static_cast<Uint16>(buttons);desc.naxes=3;
    auto id=SDL_AttachVirtualJoystick(&desc);require(id!=0,"virtual device attach");return id;
}
void set(SDL_Joystick* device,int button,bool down) {
    require(SDL_SetJoystickVirtualButton(device,button,down),"set virtual button");SDL_UpdateJoysticks();
}
}
int main() {
    try {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
        require(SDL_Init(SDL_INIT_JOYSTICK),"SDL initialization");
        {
            std::array<bool,SDL_SCANCODE_COUNT> keys{};
            std::array<bool,SDL_GAMEPAD_BUTTON_COUNT> buttons{};
            auto mapped=[&](bool seated) {return host::route_game_input(host::device_game_input(keys,buttons),seated).player_one;};
            keys[SDL_SCANCODE_RSHIFT]=true;
            require(mapped(true)==Pad::right,"Right flipper cannot launch");
            keys[SDL_SCANCODE_LSHIFT]=true;
            require(mapped(true)==(Pad::left|Pad::up),"Both flippers survive PCE opposite-direction filtering");
            keys={};keys[SDL_SCANCODE_RETURN]=true;
            require(mapped(true)==Pad::button_i,"Enter only launches in shooter");
            require(mapped(false)==(Pad::run|Pad::button_i),"Enter starts/pauses/confirms elsewhere");
            std::array<std::byte,8192> menu_ram{};
            constexpr std::array ranking_stack{std::byte{0xfd},std::byte{0xeb},std::byte{3},
                std::byte{2},std::byte{1},std::byte{0x7d},std::byte{0x41}};
            std::copy(ranking_stack.begin(),ranking_stack.end(),menu_ram.begin()+0x1f9);
            menu_ram[0x7a]=std::byte{2};
            host::GameInputRouter menu_router;
            require(menu_router.route(host::device_game_input(keys,buttons),menu_ram).player_one==Pad::button_i,
                "Keyboard Enter advances a high-score initial without Run");
            menu_ram={};
            require(menu_router.route({Pad::run,0},menu_ram).player_one==Pad::button_i,
                "Held final confirmation cannot become Run after menu exit");
            (void)menu_router.route({},menu_ram);
            require(menu_router.route({Pad::run,0},menu_ram).player_one==(Pad::run|Pad::button_i),
                "Fresh Enter retains Run outside name entry");
            keys={};buttons[SDL_GAMEPAD_BUTTON_SOUTH]=true;
            require(mapped(true)==Pad::button_i,"South button is independent launch/confirm");
            buttons={};buttons[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER]=true;
            require(mapped(true)==Pad::right,"Right shoulder only flips");
            const auto extended=host::parse_cabinet_buttons("ACINPUTS2 \"PinOne\" 0 1 2 3 4 5 6 7 8");
            require(extended.buttons[8]==8,"Optional separate raw launch button");
            const auto plunger=host::parse_cabinet_plunger("ACPLUNGER1 \"PinOne\" 2 -32768 32767 8 25");
            const auto defaults=host::parse_cabinet_plunger_ini("input.plunger.axis=2\n");
            require(defaults.enabled() && defaults.device.empty() && defaults.calibration.rest==0 &&
                defaults.calibration.drawn==32767 && defaults.calibration.deadzone==8 && defaults.calibration.release==25,
                "Devil's Crush axis-only PinOne configuration");
            require(!host::parse_cabinet_plunger_ini("input.plunger.axis=-1\n").enabled(),"Explicitly disabled plunger");
            require(host::parse_cabinet_plunger_ini("\xef\xbb\xbfinput.plunger.axis=2\r\n").axis==2,
                "Windows UTF-8 BOM does not hide first plunger setting");
            const auto copied=host::parse_cabinet_plunger_ini(
                "# Full Devil's Crush configuration\nversion=1\nplayfield.rotation=90\ninput.one=A\n"
                "input.plunger.device=PinOne%20Board%25\r\ninput.plunger.axis=2\n"
                "input.plunger.rest=1000\ninput.plunger.drawn=-1000\ninput.plunger.deadzone=12\ninput.plunger.release=30\n");
            require(copied.device=="PinOne Board%" && copied.axis==2 && copied.calibration.rest==1000 &&
                copied.calibration.drawn==-1000 && copied.calibration.deadzone==12 && copied.calibration.release==30,
                "Copied cabinet.ini preserves device escaping, reversed endpoints and thresholds");
            for(const auto* bad:{"input.plunger.axis=no", "input.plunger.rest=0junk", "input.plunger.deadzone=101",
                "input.plunger.release=-1", "input.plunger.device=Bad%", "input.plunger.device=Bad%GG",
                "input.plunger.device=Bad%00", "input.plunger.axis", "input.plunger.axix=2"}) {
                bool rejected=false;try{(void)host::parse_cabinet_plunger_ini(bad);}catch(const std::runtime_error&){rejected=true;}
                require(rejected,"Invalid INI plunger setting rejected");
            }
            require(plunger.axis==2 && plunger.calibration.rest==-32768 && plunger.calibration.release==25,"Plunger configuration");
            for(const auto* bad:{"", "ACPLUNGER2 \"PinOne\" 2 0 1000 8 25",
                "ACPLUNGER1 \"\" 2 0 1000 8 25", "ACPLUNGER1 \"PinOne\" -1 0 1000 8 25",
                "ACPLUNGER1 \"PinOne\" 256 0 1000 8 25", "ACPLUNGER1 \"PinOne\" 2 0 0 8 25",
                "ACPLUNGER1 \"PinOne\" 2 -32769 1000 8 25", "ACPLUNGER1 \"PinOne\" 2 0 32768 8 25",
                "ACPLUNGER1 \"PinOne\" 2 0 1000 100 25", "ACPLUNGER1 \"PinOne\" 2 0 1000 8 0",
                "ACPLUNGER1 \"PinOne\" 2 0 1000 8 101", "ACPLUNGER1 \"PinOne\" 2 0 1000 8 25 junk"}) {
                bool rejected=false;try{(void)host::parse_cabinet_plunger(bad);}catch(const std::runtime_error&){rejected=true;}
                require(rejected,"Reject invalid plunger configuration");
            }
            const auto binding=host::parse_cabinet_buttons("ACINPUTS1\n\"test pinone\"\n0 1 2 3 4 5 6 7\n");
            for(const auto* bad:{"", "ACINPUTS2 \"test\" 0 1 2 3 4 5 6 7",
                "ACINPUTS1 \"\" 0 1 2 3 4 5 6 7", "ACINPUTS1 \"test\" -2 1 2 3 4 5 6 7",
                "ACINPUTS1 \"test\" 256 1 2 3 4 5 6 7", "ACINPUTS1 \"test\" 0 1",
                "ACINPUTS1 \"test\" 0 1 2 3 4 5 6 7 junk"}) {
                bool rejected=false;try{(void)host::parse_cabinet_buttons(bad);}catch(const std::runtime_error&){rejected=true;}
                require(rejected,"reject invalid configuration");
            }
            const auto unrelated=attach("Other board",8);
            const auto short_board=attach("Test PinOne too small",2);
            const auto selected=attach("Test PinOne cabinet",8);
            auto* device=SDL_OpenJoystick(selected);require(device!=nullptr,"open virtual device");
            set(device,3,true);
            host::CabinetInput input(binding);
            require(input.id()==selected,"name and button-count selection");
            require(input.read().player_one==0,"held start on connect suppressed");
            set(device,3,false);require(input.read().player_one==0,"rest arms device");
            constexpr std::array masks{Pad::left,Pad::right,Pad::button_ii,Pad::run,Pad::select,Pad::up,Pad::down,Pad::right};
            for(int b=0;b<8;++b) {
                set(device,b,true);require(input.read().player_one==masks[static_cast<std::size_t>(b)],"mapped button");
                set(device,b,false);require(input.read().player_one==0,"button release");
            }
            set(device,0,true);set(device,1,true);
            require(host::route_game_input(input.read(),true).player_one==(Pad::left|Pad::up),"simultaneous flippers");
            input.rearm();require(input.read().player_one==0,"focus/load rearm suppresses held buttons");
            set(device,0,false);set(device,1,false);(void)input.read();
            set(device,2,true);require(input.read().player_one==Pad::button_ii,"rearmed nudge");
            require(SDL_DetachVirtualJoystick(selected),"detach");
            require(input.read().player_one==0,"disconnect clears held input");input.refresh();require(input.id()==0,"no matching device");
            SDL_CloseJoystick(device);
            const auto replacement=attach("Test PinOne cabinet",8);device=SDL_OpenJoystick(replacement);
            set(device,3,true);input.refresh();require(input.id()==replacement,"reconnect with new instance ID");
            require(input.read().player_one==0,"reconnect held start suppressed");
            set(device,3,false);(void)input.read();set(device,3,true);require(input.read().player_one==Pad::run,"start after reconnect");
            host::CabinetInput disabled({});require(disabled.id()==0,"unconfigured is inert");
            SDL_CloseJoystick(device);
            require(SDL_DetachVirtualJoystick(replacement),"detach replacement");
            require(SDL_DetachVirtualJoystick(short_board),"detach short board");
            require(SDL_DetachVirtualJoystick(unrelated),"detach unrelated board");
            const auto launch_board=attach("PinOne dedicated launch",9);
            device=SDL_OpenJoystick(launch_board);require(device!=nullptr,"Open nine-button board");
            host::CabinetInput separate(extended);
            require(separate.id()==launch_board,"Select ACINPUTS2 board");
            (void)separate.read();set(device,8,true);
            require(separate.read().player_one==Pad::button_i,"Ninth raw button launches/confirms independently");
            set(device,1,true);
            require(host::route_game_input(separate.read(),true).player_one==(Pad::button_i|Pad::right),"Raw launch and right flipper work independently together");
            SDL_CloseJoystick(device);require(SDL_DetachVirtualJoystick(launch_board),"Detach nine-button board");
        }
        SDL_Quit();std::cout<<"Cabinet raw input tests passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<": "<<SDL_GetError()<<'\n';SDL_Quit();return 1;}
}
