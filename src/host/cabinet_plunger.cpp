// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_plunger.hpp"
#include "alien_crush/core/files.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace alien_crush::host {
input::PlungerBinding parse_cabinet_plunger(std::string_view text) {
    if(text.size()>4096) throw std::runtime_error("Plunger configuration exceeds 4096 bytes");
    std::istringstream stream{std::string(text)};
    input::PlungerBinding result;
    std::string header;
    int deadzone{},release{};
    if(!(stream>>header>>std::quoted(result.device)>>result.axis>>result.calibration.rest>>result.calibration.drawn>>deadzone>>release) ||
        header!="ACPLUNGER1" || result.device.empty() || result.device.size()>1024 ||
        result.device.find_first_of("\r\n\0",0,3)!=std::string::npos || result.axis<0 || result.axis>255 ||
        result.calibration.rest < -32768 || result.calibration.rest>32767 ||
        result.calibration.drawn < -32768 || result.calibration.drawn>32767 ||
        result.calibration.rest==result.calibration.drawn || deadzone<0 || deadzone>=100 || release<1 || release>100)
        throw std::runtime_error("Invalid ACPLUNGER1 device, axis, endpoints or thresholds");
    stream>>std::ws;
    if(!stream.eof()) throw std::runtime_error("Unexpected trailing plunger configuration");
    result.calibration.deadzone=static_cast<std::uint8_t>(deadzone);
    result.calibration.release=static_cast<std::uint8_t>(release);
    return result;
}
input::PlungerBinding load_cabinet_plunger(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path)) return {};
    const auto bytes=core::read_file(path,4096);
    return parse_cabinet_plunger({reinterpret_cast<const char*>(bytes.data()),bytes.size()});
}
CabinetPlunger::CabinetPlunger(input::PlungerBinding binding):binding_(std::move(binding)) {
    detector_.configure(binding_.calibration);refresh();
}
CabinetPlunger::~CabinetPlunger() {SDL_CloseJoystick(joystick_);}
SDL_JoystickID CabinetPlunger::id() const {return joystick_?SDL_GetJoystickID(joystick_):0;}
void CabinetPlunger::interrupt() {interrupted_=true;detector_.rearm();}
void CabinetPlunger::restored() {owned_=false;interrupted_=false;blocked_=binding_.enabled();detector_.rearm();}
void CabinetPlunger::refresh() {
    if(joystick_ && SDL_JoystickConnected(joystick_)) return;
    if(joystick_) interrupt();
    SDL_CloseJoystick(joystick_);joystick_=nullptr;
    if(!binding_.enabled()) return;
    int count{};auto* ids=SDL_GetJoysticks(&count);
    for(int i=0;i<count;++i) {
        const auto* name=SDL_GetJoystickNameForID(ids[i]);
        if(!name || !SDL_strcasestr(name,binding_.device.c_str())) continue;
        auto* candidate=SDL_OpenJoystick(ids[i]);
        if(!candidate) continue;
        if(SDL_GetNumJoystickAxes(candidate)<=binding_.axis) {SDL_CloseJoystick(candidate);continue;}
        joystick_=candidate;detector_.rearm();
        std::cout<<"Cabinet plunger: "<<name<<", axis "<<binding_.axis<<'\n';break;
    }
    SDL_free(ids);
}
std::uint8_t CabinetPlunger::apply(ReferenceRuntime& runtime,PadInput& pad,bool focused) {
    if(!binding_.enabled()) return 0;
    refresh();
    if(!focused) interrupt();
    const auto clear_launch=[&] {pad.player_one&=static_cast<std::uint16_t>(~3U);};
    const auto cancel=[&]() -> std::uint8_t {
        const bool cancelled=owned_ && runtime.cancel_plunger_draw();
        owned_=false;blocked_=true;interrupted_=false;detector_.rearm();clear_launch();
        return cancelled?101:0;
    };
    if(interrupted_) return cancel();
    if(blocked_) {
        detector_.rearm();
        if(focused && joystick_) (void)detector_.update(SDL_GetJoystickAxis(joystick_,binding_.axis));
        if(focused && (pad.player_one&3)==0 && (!joystick_ || (detector_.armed() && detector_.draw()==0))) blocked_=false;
        clear_launch();return 0;
    }
    if(!focused || !joystick_) return 0;
    const auto sample=detector_.update(SDL_GetJoystickAxis(joystick_,binding_.axis));
    if(sample.drawn) {
        if(runtime.apply_plunger_draw(sample.draw)) {
            owned_=!sample.released;clear_launch();
            if(owned_) pad.player_one|=Pad::button_i;
            else blocked_=true;
            return sample.draw;
        }
        // Pause or a changed scene cancels an owned draw. Pulls outside the
        // seated-ball gate must return to rest before they can acquire a ball.
        if(owned_) return cancel();
        detector_.rearm();
    } else if(owned_) return cancel();
    return 0;
}
}
