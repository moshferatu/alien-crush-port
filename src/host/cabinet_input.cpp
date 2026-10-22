// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_input.hpp"
#include <SDL3/SDL.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace alien_crush::host {
namespace {
using Joystick=std::unique_ptr<SDL_Joystick,decltype(&SDL_CloseJoystick)>;
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
              <<"Set input.plunger.* in the main alien-crush.ini (see README).\n\n";
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
