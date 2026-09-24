// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cabinet_settings.hpp"
#include "alien_crush/input/nudge_multiplier.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
namespace alien_crush::host {
// The settings a player chooses once rather than per run, kept in cabinet.ini
// beside the window, button, nudge and plunger fields. Each was a command-line
// flag first, and the flag still beats the file. The launcher
// (src/launcher/launcher.cpp) is where they are meant to be changed. They apply
// to normal play only; replays and --frames runs keep the flags' defaults.
struct PlaySettings {
    // game.rom, UTF-8. Empty: Alien Crush.pce beside the executable. A relative
    // path is taken from the executable's folder.
    std::string rom;
    // game.cabinet: three windows (--cabinet), or one.
    bool cabinet=false;
    // video.smoothing: cabinet animation smoothing on high-refresh displays
    // (--smooth / --no-smooth). F7 changes it, and a cabinet session saves it.
    bool smoothing=true;
    // input.nudge_multiplier (--nudge-multiplier), 0 to 16.
    double nudge_multiplier=1;
    bool operator==(const PlaySettings&) const=default;
};
// Absent fields keep their defaults, so a file from before them plays as it
// did. A present field that cannot be read is an error, as every other is.
inline PlaySettings parse_play_settings(const CabinetSettings& settings) {
    PlaySettings result;
    const auto field=[&](const char* key) -> const std::string* {
        const auto found=settings.find(key);
        return found==settings.end()?nullptr:&found->second;
    };
    const auto flag=[&](const char* key,bool& value) {
        if(const auto* text=field(key)) {
            if(*text!="0" && *text!="1") throw std::runtime_error(std::string("Invalid ")+key+" (expected 0 or 1)");
            value=*text=="1";
        }
    };
    if(const auto* rom=field("game.rom")) result.rom=*rom;
    flag("game.cabinet",result.cabinet);
    flag("video.smoothing",result.smoothing);
    if(const auto* text=field("input.nudge_multiplier")) {
        const auto value=input::parse_nudge_multiplier(*text);
        if(!value) throw std::runtime_error("Invalid input.nudge_multiplier (expected a number from 0 to 16)");
        result.nudge_multiplier=*value;
    }
    return result;
}
// The multiplier is written as the shortest text that reads back as the same
// double, so a 2.5 stays 2.5 rather than growing digits on every save.
inline void store_play_settings(CabinetSettings& settings,const PlaySettings& play) {
    if(!std::isfinite(play.nudge_multiplier) || play.nudge_multiplier<0 || play.nudge_multiplier>16)
        throw std::runtime_error("Nudge multiplier must be a number from 0 to 16");
    std::array<char,32> multiplier{};
    const auto written=std::to_chars(multiplier.data(),multiplier.data()+multiplier.size(),play.nudge_multiplier);
    if(written.ec!=std::errc{}) throw std::runtime_error("Nudge multiplier cannot be written");
    settings["game.rom"]=play.rom;
    settings["game.cabinet"]=play.cabinet?"1":"0";
    settings["video.smoothing"]=play.smoothing?"1":"0";
    settings["input.nudge_multiplier"]=std::string(multiplier.data(),written.ptr);
}
}
