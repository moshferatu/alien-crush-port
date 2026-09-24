// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "play_settings.hpp"

inline void check_play_settings() {
    using namespace alien_crush;
    auto check=[](bool ok,const char* why){if(!ok) throw std::runtime_error(why);};
    check(host::parse_play_settings({})==host::PlaySettings{},"A file without play settings plays with the flags' defaults");
    const host::PlaySettings chosen{"D:\\Games\\Alien Crush (USA).pce",true,false,2.5};
    host::CabinetSettings settings{{"window.playfield","\"Display\" 0 0 540 720 0 0"}};
    host::store_play_settings(settings,chosen);
    check(host::parse_play_settings(settings)==chosen,"Play settings round trip through cabinet.ini");
    check(settings.at("input.nudge_multiplier")=="2.5","A nudge multiplier is written as it was typed");
    check(settings.contains("window.playfield"),"Storing play settings keeps the other fields");
    for(const auto& bad:{host::CabinetSettings{{"game.cabinet","2"}},{{"video.smoothing","off"}},
        {{"input.nudge_multiplier","17"}},{{"input.nudge_multiplier","2x"}}}) {
        bool rejected=false;try{(void)host::parse_play_settings(bad);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"A play setting that cannot be read is rejected");
    }
    bool refused=false;
    try{host::CabinetSettings out;host::store_play_settings(out,{"",false,true,16.5});}catch(const std::runtime_error&){refused=true;}
    check(refused,"A nudge multiplier past 16 is not written");
}
