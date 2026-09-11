// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_input.hpp"
#include "cabinet_settings.hpp"
#include "alien_crush/core/files.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace alien_crush::host {
namespace {
constexpr std::array masks{Pad::left,Pad::right,Pad::button_ii,Pad::run,
    Pad::select,Pad::up,Pad::down,Pad::right,Pad::button_i};
bool name_matches(const char* name,const std::string& needle) {
    return name && SDL_strcasestr(name,needle.c_str());
}
using Joystick=std::unique_ptr<SDL_Joystick,decltype(&SDL_CloseJoystick)>;
}
CabinetButtons parse_cabinet_buttons(std::string_view text) {
    if(text.size()>4096) throw std::runtime_error("Cabinet input configuration exceeds 4096 bytes");
    std::istringstream stream{std::string(text)};
    CabinetButtons result;
    std::string header;
    if(!(stream>>header) || (header!="ACINPUTS1" && header!="ACINPUTS2") || !(stream>>std::quoted(result.device)) ||
        result.device.empty() || result.device.size()>1024 ||
        result.device.find_first_of("\r\n\0",0,3)!=std::string::npos)
        throw std::runtime_error("Invalid cabinet input header/device; name a specific controller");
    for(unsigned i=0;i<(header=="ACINPUTS2"?9u:8u);++i)
        if(!(stream>>result.buttons[i]) || result.buttons[i] < -1 || result.buttons[i]>255)
            throw std::runtime_error("Cabinet button indices must be -1 (disabled) or 0..255");
    stream>>std::ws;
    if(!stream.eof()) throw std::runtime_error("Unexpected trailing cabinet input configuration");
    return result;
}
CabinetButtons load_cabinet_buttons(const std::filesystem::path& path) {
    const auto settings=read_cabinet_settings(path.parent_path()/"cabinet.ini");
    if(const auto found=settings.find("input.buttons");found!=settings.end())
        return found->second.empty()?CabinetButtons{}:parse_cabinet_buttons(found->second);
    if(!std::filesystem::exists(path)) return {};
    const auto bytes=core::read_file(path,4096);
    return parse_cabinet_buttons({reinterpret_cast<const char*>(bytes.data()),bytes.size()});
}
CabinetInput::CabinetInput(CabinetButtons binding):binding_(std::move(binding)) { refresh(); }
CabinetInput::~CabinetInput() { SDL_CloseJoystick(joystick_); }
SDL_JoystickID CabinetInput::id() const { return joystick_?SDL_GetJoystickID(joystick_):0; }
void CabinetInput::refresh() {
    if(joystick_ && SDL_JoystickConnected(joystick_)) return;
    SDL_CloseJoystick(joystick_); joystick_=nullptr; rearm();
    const auto highest=*std::max_element(binding_.buttons.begin(),binding_.buttons.end());
    if(binding_.device.empty() || highest<0) return;
    int count{};
    auto* ids=SDL_GetJoysticks(&count);
    for(int i=0;i<count;++i) {
        if(!name_matches(SDL_GetJoystickNameForID(ids[i]),binding_.device)) continue;
        auto* candidate=SDL_OpenJoystick(ids[i]);
        if(!candidate) continue;
        if(SDL_GetNumJoystickButtons(candidate)<=highest) {SDL_CloseJoystick(candidate);continue;}
        joystick_=candidate;
        std::cout<<"Cabinet buttons: "<<SDL_GetJoystickName(joystick_)<<'\n';
        break;
    }
    SDL_free(ids);
}
PadInput CabinetInput::read() {
    PadInput result;
    if(!joystick_ || !SDL_JoystickConnected(joystick_)) {rearm();return result;}
    for(std::size_t i=0;i<masks.size();++i)
        if(binding_.buttons[i]>=0 && SDL_GetJoystickButton(joystick_,binding_.buttons[i]))
            result.player_one|=masks[i];
    // Held start/flippers must not act immediately after reconnect or focus return.
    if(!armed_) {armed_=result.player_one==0;return {};}
    return result;
}
int input_probe(std::uint64_t samples) {
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("Alien Crush - Input probe (Escape to close)",960,640,SDL_WINDOW_RESIZABLE),SDL_DestroyWindow);
    if(!window) throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateRenderer(window.get(),nullptr),SDL_DestroyRenderer);
    if(!renderer) throw std::runtime_error(SDL_GetError());
    SDL_SetRenderLogicalPresentation(renderer.get(),960,640,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    std::vector<Joystick> devices;
    auto discover=[&] {
        devices.clear();
        int count{}; auto* ids=SDL_GetJoysticks(&count);
        for(int i=0;i<count;++i) if(auto* joystick=SDL_OpenJoystick(ids[i])) devices.emplace_back(joystick,SDL_CloseJoystick);
        SDL_free(ids);
    };
    discover();
    std::string previous;
    for(std::uint64_t sample=0;!samples || sample<samples;++sample) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (event.type==SDL_EVENT_KEY_DOWN && event.key.scancode==SDL_SCANCODE_ESCAPE)) return 0;
            if(event.type==SDL_EVENT_JOYSTICK_ADDED || event.type==SDL_EVENT_JOYSTICK_REMOVED) discover();
        }
        std::ostringstream report;
        report<<"Raw joystick probe - button/axis indices start at 0\n"
              <<"Press buttons; pull/release the rod. Record its axis, rest and full-pull values.\n"
              <<"Set input.plunger.* in the main cabinet.ini (see README).\n\n";
        if(devices.empty()) report<<"No joysticks detected. Connect the cabinet controller.\n";
        for(const auto& device:devices) {
            const auto* name=SDL_GetJoystickName(device.get());
            report<<std::quoted(name?name:"unnamed")<<" (id "<<SDL_GetJoystickID(device.get())<<")\nAxes:";
            for(int a=0;a<SDL_GetNumJoystickAxes(device.get());++a) report<<' '<<a<<'='<<SDL_GetJoystickAxis(device.get(),a);
            report<<"\nPressed buttons ("<<SDL_GetNumJoystickButtons(device.get())<<" total):";
            for(int b=0;b<SDL_GetNumJoystickButtons(device.get());++b) if(SDL_GetJoystickButton(device.get(),b)) report<<' '<<b;
            report<<"\n\n";
        }
        const auto text=report.str();
        if(text!=previous) {std::cout<<text<<std::flush;previous=text;}
        SDL_SetRenderDrawColor(renderer.get(),0,0,0,255); SDL_RenderClear(renderer.get());
        SDL_SetRenderDrawColor(renderer.get(),255,180,80,255);
        SDL_RenderDebugText(renderer.get(),12,12,text.c_str()); SDL_RenderPresent(renderer.get());
        SDL_Delay(100);
    }
    return 0;
}
}
