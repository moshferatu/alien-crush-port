// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cabinet_nudge.hpp"

inline void check_cabinet_nudge() {
    using namespace alien_crush;
    auto check=[](bool ok,const char* why){if(!ok) throw std::runtime_error(why);};
    const auto defaults=host::parse_cabinet_nudge({});
    check(defaults.enabled && defaults.device=="PinOne" && defaults.axis_x==0 && defaults.axis_y==1,
        "PinOne nudge works without configuration");
    check(!host::parse_cabinet_nudge({{"input.nudge.enabled","0"}}).enabled,"Disable physical nudge");
    for(const auto& settings:{host::CabinetSettings{{"input.nudge.threshold","0"}},
        {{"input.nudge.release","20"}},{{"input.nudge.axis_x","1"}},{{"input.nudge.axis_y","256"}},
        {{"input.nudge.enabled","2"}},{{"input.nudge.cooldown_ms","10"}},
        {{"input.nudge.device",""}},{{"input.nudge.threshold","20junk"}},{{"input.nudge.typo","1"}}}) {
        bool rejected=false;try{(void)host::parse_cabinet_nudge(settings);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"Invalid nudge settings rejected");
    }
    auto attach=[&](const char* name) {
        SDL_VirtualJoystickDesc desc{};SDL_INIT_INTERFACE(&desc);
        desc.name=name;desc.naxes=3;
        const auto id=SDL_AttachVirtualJoystick(&desc);check(id!=0,"Attach nudge device");return id;
    };
    const auto other=attach("Unrelated gamepad");
    host::CabinetNudge input(defaults);
    check(input.id()==0,"Do not interpret arbitrary joystick motion as a cabinet shake");
    auto selected=attach("Clev Soft PinOne");
    auto* device=SDL_OpenJoystick(selected);
    auto axes=[&](Sint16 x,Sint16 y,Sint16 z=0) {
        check(SDL_SetJoystickVirtualAxis(device,0,x),"Set nudge X");
        check(SDL_SetJoystickVirtualAxis(device,1,y),"Set nudge Y");
        check(SDL_SetJoystickVirtualAxis(device,2,z),"Set plunger Z");SDL_UpdateJoysticks();
    };
    axes(20000,0);input.refresh();check(input.id()==selected,"Hotplug PinOne");
    check(!input.read(0,true) && !input.read(500,true),"No shake on connect while moving");
    axes(0,0);check(!input.read(510,true) && !input.read(610,true),"Quiet startup arms without input");
    axes(1500,-1500,32767);check(!input.read(620,true),"Noise and plunger do not nudge");
    axes(10000,0);check(input.read(630,true),"Positive X triggers nudge");
    check(input.read(660,true) && !input.read(680,true),"Finite 50ms nudge pulse");
    axes(-20000,0);check(!input.read(900,true),"Shake rebound cannot retrigger without settling");
    axes(0,0);check(!input.read(910,true) && !input.read(1010,true),"Rearm after sustained quiet");
    axes(0,-32768);check(input.read(1020,true),"Negative Y extreme triggers safely");
    axes(0,0);check(input.read(1030,true) && !input.read(1130,true),"Pulse ends even at rest");
    axes(0,20000);check(!input.read(1140,true),"Cooldown suppresses early second shake");
    axes(0,0);(void)input.read(1250,true);(void)input.read(1350,true);
    axes(0,20000);check(input.read(1360,true),"Positive Y triggers after cooldown and quiet");
    check(!input.read(1370,false) && !input.read(1500,true),"Focus/pause/scene interruption cancels pulse and disarms");
    axes(0,0);(void)input.read(1510,true);(void)input.read(1610,true);
    axes(-10000,0);check(input.read(1620,true),"Negative X triggers");
    input.rearm();check(!input.read(1621,true),"Quick-load clears pending nudge");
    check(SDL_DetachVirtualJoystick(selected),"Disconnect nudge device");
    check(!input.read(1630,true),"Disconnect clears input");SDL_CloseJoystick(device);
    input.refresh();check(input.id()==0,"Release disconnected device");
    selected=attach("Clev Soft PinOne");device=SDL_OpenJoystick(selected);axes(0,20000);input.refresh();
    check(input.id()==selected && !input.read(1700,true),"Reconnect finds new ID and waits for quiet");
    axes(0,0);(void)input.read(1710,true);(void)input.read(1810,true);
    axes(0,10000);check(input.read(1820,true),"Nudge works after reconnect");
    host::NudgeBinding disabled;disabled.enabled=false;
    host::CabinetNudge inactive(disabled);check(inactive.id()==0 && !inactive.read(2000,true),"Replay/desktop disabled");
    SDL_CloseJoystick(device);check(SDL_DetachVirtualJoystick(selected),"Detach PinOne");
    check(SDL_DetachVirtualJoystick(other),"Detach other device");
    std::array<std::byte,8192> ram{};
    check(host::nudge_play_active(ram),"Live play permits shake");
    ram[0xf6]=std::byte{2};check(!host::nudge_play_active(ram),"Run pause suppresses shake");ram[0xf6]={};
    ram[0x53]=std::byte{0x80};check(!host::nudge_play_active(ram),"Pause suppresses shake");
    ram[0x53]={};ram[0x54]=std::byte{0x80};check(!host::nudge_play_active(ram),"Secondary pause suppresses shake");
    ram[0x54]={};ram[0x76]=std::byte{1};check(!host::nudge_play_active(ram),"Demo suppresses shake");
    ram[0x76]={};ram[0x78]=std::byte{3};check(!host::nudge_play_active(ram),"Game over suppresses shake");
    ram={};constexpr std::array title{0x0c,0xeb,0x18,4,3,2,1,5,0x40};
    for(std::size_t i=0;i<title.size();++i) ram[0x1f7+i]=static_cast<std::byte>(title[i]);
    check(!host::nudge_play_active(ram),"Title shake cannot change options");
    ram={};ram[0x7a]=std::byte{2};constexpr std::array scores{0xfd,0xeb,3,2,1,0x7d,0x41};
    for(std::size_t i=0;i<scores.size();++i) ram[0x1f9+i]=static_cast<std::byte>(scores[i]);
    check(!host::nudge_play_active(ram),"Initials entry ignores shakes");
}
