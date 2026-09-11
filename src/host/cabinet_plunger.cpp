// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_plunger.hpp"
#include "alien_crush/core/files.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <charconv>
#include <algorithm>
#include <memory>
#include <vector>

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
input::PlungerBinding parse_cabinet_plunger_ini(std::string_view text) {
    if(text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    input::PlungerBinding result;
    auto trim=[](std::string_view s) {
        const auto first=s.find_first_not_of(" \t\r\n");
        return first==s.npos?std::string_view{}:s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
    };
    auto integer=[](std::string_view s) {
        int value{};const auto parsed=std::from_chars(s.data(),s.data()+s.size(),value);
        if(parsed.ec!=std::errc{} || parsed.ptr!=s.data()+s.size())
            throw std::runtime_error("Invalid input.plunger numeric value");
        return value;
    };
    std::istringstream lines{std::string(text)};
    for(std::string line;std::getline(lines,line);) {
        const auto s=trim(line);
        if(!s.starts_with("input.plunger.")) continue;
        const auto equal=s.find('=');
        if(equal==s.npos) throw std::runtime_error("Expected input.plunger.setting=value");
        const auto key=trim(s.substr(14,equal-14)),value=trim(s.substr(equal+1));
        if(key=="device") {
            result.device.clear();
            for(std::size_t i=0;i<value.size();++i) {
                unsigned code=static_cast<unsigned char>(value[i]);
                if(value[i]=='%') {
                    if(i+2>=value.size()) throw std::runtime_error("Invalid escaped plunger device name");
                    const auto parsed=std::from_chars(value.data()+i+1,value.data()+i+3,code,16);
                    if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+i+3)
                        throw std::runtime_error("Invalid escaped plunger device name");
                    i+=2;
                }
                if(code==0 || code==10 || code==13) throw std::runtime_error("Invalid plunger device name");
                result.device.push_back(static_cast<char>(code));
            }
        } else if(key=="axis") result.axis=integer(value);
        else if(key=="rest") result.calibration.rest=integer(value);
        else if(key=="drawn") result.calibration.drawn=integer(value);
        else if(key=="deadzone" || key=="release") {
            const auto percent=integer(value);
            if(percent<0 || percent>100) throw std::runtime_error("Plunger percentage outside 0..100");
            (key=="deadzone"?result.calibration.deadzone:result.calibration.release)=static_cast<std::uint8_t>(percent);
        } else throw std::runtime_error("Unknown input.plunger setting: "+std::string(key));
    }
    return result;
}
input::PlungerBinding load_cabinet_plunger(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path)) return {};
    const auto bytes=core::read_file(path,65536);
    std::string_view text{reinterpret_cast<const char*>(bytes.data()),bytes.size()};
    if(text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first!=text.npos) text.remove_prefix(first);
    return text.starts_with("ACPLUNGER") || path.filename()=="cabinet-plunger.txt"?
        parse_cabinet_plunger(text):parse_cabinet_plunger_ini(text);
}
int plunger_probe() {
    // Same two-second rest and twelve-second pull measurement as Devil's
    // Crush. Sample raw joysticks, including boards without gamepad mappings.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    using Joystick=std::unique_ptr<SDL_Joystick,decltype(&SDL_CloseJoystick)>;
    struct Axis {SDL_Joystick* joystick;std::string name;int index,rest,extreme;};
    std::vector<Joystick> devices;
    std::vector<Axis> axes;
    int count{};auto* ids=SDL_GetJoysticks(&count);
    for(int i=0;i<count;++i) if(auto* device=SDL_OpenJoystick(ids[i])) {
        devices.emplace_back(device,SDL_CloseJoystick);
        const auto* name=SDL_GetJoystickName(device);
        for(int a=0;a<SDL_GetNumJoystickAxes(device);++a)
            axes.push_back({device,name?name:"unnamed SDL joystick",a,0,0});
        std::cout<<"Controller: "<<(name?name:"unnamed")<<" ("<<SDL_GetNumJoystickAxes(device)<<" axes)\n";
    }
    SDL_free(ids);
    if(axes.empty()) {std::cerr<<"No controller with an axis is connected.\n";return 1;}
    auto pump=[] {
        SDL_Event event;
        while(SDL_PollEvent(&event)) if(event.type==SDL_EVENT_QUIT) return false;
        SDL_Delay(16);return true;
    };
    std::cout<<"Leave the plunger at rest for two seconds.\n"<<std::flush;
    const auto start=SDL_GetTicks();
    while(SDL_GetTicks()-start<2000) if(!pump()) return 1;
    for(auto& axis:axes) axis.rest=axis.extreme=SDL_GetJoystickAxis(axis.joystick,axis.index);
    std::cout<<"Now pull all the way back and release a few times (12 seconds).\n"<<std::flush;
    const auto watching=SDL_GetTicks();Uint64 reported=0;
    while(SDL_GetTicks()-watching<12000) {
        if(!pump()) return 1;
        for(auto& axis:axes) {
            if(!SDL_JoystickConnected(axis.joystick)) {std::cerr<<"Controller disconnected; run the probe again.\n";return 1;}
            const int value=SDL_GetJoystickAxis(axis.joystick,axis.index);
            if(std::abs(value-axis.rest)>std::abs(axis.extreme-axis.rest)) axis.extreme=value;
        }
        if(SDL_GetTicks()-reported>=250) {
            reported=SDL_GetTicks();
            for(const auto& axis:axes) std::cout<<axis.name<<" axis "<<axis.index<<'='
                <<SDL_GetJoystickAxis(axis.joystick,axis.index)<<"  ";
            std::cout<<'\n'<<std::flush;
        }
    }
    const auto moved=std::max_element(axes.begin(),axes.end(),[](const Axis& a,const Axis& b) {
        return std::abs(a.extreme-a.rest)<std::abs(b.extreme-b.rest);
    });
    if(std::abs(moved->extreme-moved->rest)<4096) {std::cerr<<"No axis moved far enough to identify a plunger.\n";return 1;}
    std::string escaped;
    constexpr char hex[]="0123456789ABCDEF";
    for(const unsigned char c:moved->name) {
        if(c=='%' || c=='\r' || c=='\n' || c=='=') {escaped+='%';escaped+=hex[c>>4];escaped+=hex[c&15];}
        else escaped+=static_cast<char>(c);
    }
    std::cout<<"\nPut these settings in Alien Crush's portable-data/cabinet.ini:\n\n"
        <<"input.plunger.device="<<escaped<<"\ninput.plunger.axis="<<moved->index
        <<"\ninput.plunger.rest="<<moved->rest<<"\ninput.plunger.drawn="<<moved->extreme
        <<"\ninput.plunger.deadzone=8\ninput.plunger.release=25\n";
    return 0;
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
