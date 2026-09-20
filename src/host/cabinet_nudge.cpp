// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_nudge.hpp"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace alien_crush::host {
NudgeBinding parse_cabinet_nudge(const CabinetSettings& settings) {
    NudgeBinding result;
    for(const auto& [key,value]:settings) {
        if(!key.starts_with("input.nudge.")) continue;
        if(key=="input.nudge.device") {
            if(value.empty() || value.size()>1024) throw std::runtime_error("Nudge device must name a controller");
            result.device=value;continue;
        }
        int number{};
        const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
        if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size())
            throw std::runtime_error("Invalid nudge number: "+key);
        if(key=="input.nudge.enabled" && (number==0 || number==1)) result.enabled=number!=0;
        else if(key=="input.nudge.axis_x") result.axis_x=number;
        else if(key=="input.nudge.axis_y") result.axis_y=number;
        else if(key=="input.nudge.threshold") result.threshold=number;
        else if(key=="input.nudge.release") result.release=number;
        else if(key=="input.nudge.cooldown_ms") result.cooldown_ms=number;
        else throw std::runtime_error("Unknown or invalid nudge setting: "+key);
    }
    if(result.axis_x<0 || result.axis_x>255 || result.axis_y<0 || result.axis_y>255 ||
        result.axis_x==result.axis_y || result.threshold<1 || result.threshold>100 ||
        result.release<0 || result.release>=result.threshold || result.cooldown_ms<100 || result.cooldown_ms>5000)
        throw std::runtime_error("Invalid nudge axes, thresholds or cooldown");
    return result;
}
CabinetNudge::CabinetNudge(NudgeBinding binding):binding_(std::move(binding)) {refresh();}
CabinetNudge::~CabinetNudge() {SDL_CloseJoystick(joystick_);}
SDL_JoystickID CabinetNudge::id() const {return joystick_?SDL_GetJoystickID(joystick_):0;}
void CabinetNudge::rearm() {
    armed_=quiet_=false;quiet_since_=pulse_until_=cooldown_until_=0;
}
void CabinetNudge::refresh() {
    if(joystick_ && SDL_JoystickConnected(joystick_)) return;
    SDL_CloseJoystick(joystick_);joystick_=nullptr;rearm();
    if(!binding_.enabled) return;
    int count{};auto* ids=SDL_GetJoysticks(&count);
    for(int i=0;i<count;++i) {
        const auto* name=SDL_GetJoystickNameForID(ids[i]);
        if(!name || !SDL_strcasestr(name,binding_.device.c_str())) continue;
        auto* candidate=SDL_OpenJoystick(ids[i]);
        if(!candidate) continue;
        if(SDL_GetNumJoystickAxes(candidate)<=std::max(binding_.axis_x,binding_.axis_y)) {
            SDL_CloseJoystick(candidate);continue;
        }
        joystick_=candidate;
        std::cout<<"Cabinet nudge: "<<name<<", axes "<<binding_.axis_x<<'/'<<binding_.axis_y
            <<", threshold "<<binding_.threshold<<"%\n";break;
    }
    SDL_free(ids);
}
bool CabinetNudge::read(Uint64 now,bool active) {
    if(!active || !joystick_ || !SDL_JoystickConnected(joystick_)) {rearm();return false;}
    const int magnitude=std::max(std::abs(static_cast<int>(SDL_GetJoystickAxis(joystick_,binding_.axis_x))),
        std::abs(static_cast<int>(SDL_GetJoystickAxis(joystick_,binding_.axis_y))));
    if(magnitude*100<=binding_.release*32767) {
        if(!quiet_) {quiet_=true;quiet_since_=now;}
        if(now-quiet_since_>=100 && now>=cooldown_until_) armed_=true;
    } else {
        quiet_=false;
        if(armed_ && magnitude*100>=binding_.threshold*32767 && now>=cooldown_until_) {
            armed_=false;pulse_until_=now+50;cooldown_until_=now+static_cast<Uint64>(binding_.cooldown_ms);
        }
    }
    return now<pulse_until_;
}
}
