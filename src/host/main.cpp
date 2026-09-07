// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include <SDL3/SDL_main.h>
#include <iostream>
#include <stdexcept>
#include <optional>

using namespace alien_crush;
namespace {
struct SdlLifetime { SdlLifetime(){ if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMEPAD)) throw std::runtime_error(SDL_GetError()); } ~SdlLifetime(){ SDL_Quit(); } };
struct Audio { SDL_AudioStream* stream{}; ~Audio(){ SDL_DestroyAudioStream(stream); } };
struct PadDevice { SDL_Gamepad* pad{}; ~PadDevice(){ SDL_CloseGamepad(pad); } };
void open_pad(PadDevice& device) {
    if(device.pad) return;
    int count{}; auto* ids=SDL_GetGamepads(&count);
    if(count>0) device.pad=SDL_OpenGamepad(ids[0]);
    SDL_free(ids);
}
PadInput read_input(SDL_Gamepad* pad) {
    const bool* keys=SDL_GetKeyboardState(nullptr);
    auto button=[&](SDL_GamepadButton b){return pad && SDL_GetGamepadButton(pad,b);};
    PadInput input;
    if(keys[SDL_SCANCODE_LSHIFT]||keys[SDL_SCANCODE_LEFT]||button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)||button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) input.player_one|=Pad::left;
    if(keys[SDL_SCANCODE_RSHIFT]||keys[SDL_SCANCODE_Z]||button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)||button(SDL_GAMEPAD_BUTTON_SOUTH)) input.player_one|=Pad::button_i;
    if(keys[SDL_SCANCODE_X]||button(SDL_GAMEPAD_BUTTON_EAST)) input.player_one|=Pad::button_ii;
    if(keys[SDL_SCANCODE_RETURN]||button(SDL_GAMEPAD_BUTTON_START)) input.player_one|=Pad::run;
    if(keys[SDL_SCANCODE_BACKSPACE]||button(SDL_GAMEPAD_BUTTON_BACK)) input.player_one|=Pad::select;
    if(keys[SDL_SCANCODE_UP]||button(SDL_GAMEPAD_BUTTON_DPAD_UP)) input.player_one|=Pad::up;
    if(keys[SDL_SCANCODE_DOWN]||button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) input.player_one|=Pad::down;
    if(keys[SDL_SCANCODE_RIGHT]||button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) input.player_one|=Pad::right;
    return input;
}
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path rom_path,record_path,replay_path,data_path;
        std::uint64_t limit=0;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            auto value=[&](){if(++i>=argc) throw std::runtime_error("Missing value for "+arg); return std::string(argv[i]);};
            if(arg=="--rom") rom_path=core::path_from_utf8(value());
            else if(arg=="--frames") limit=std::stoull(value());
            else if(arg=="--record") record_path=core::path_from_utf8(value());
            else if(arg=="--replay") replay_path=core::path_from_utf8(value());
            else if(arg=="--data-dir") data_path=core::path_from_utf8(value());
            else if(arg=="--help") { std::cout<<"alien-crush --rom <path> [--frames N] [--record <file> | --replay <file>] [--data-dir <directory>]\nEnter start/pause; Left Shift left flipper; Right Shift/Z right flipper and launcher; X nudge; Backspace select. F5/F9 quick save/load, F11 fullscreen, Escape exit.\n"; return 0; }
            else throw std::runtime_error("Unknown option "+arg);
        }
        if(!record_path.empty() && !replay_path.empty()) throw std::runtime_error("Choose recording or replay");
        SdlLifetime sdl;
        const auto base=core::path_from_utf8(SDL_GetBasePath());
        if(rom_path.empty()) rom_path=base/"Alien Crush.pce";
        if(data_path.empty()) data_path=base/"portable-data";
        auto runtime=make_reference_runtime(); runtime->load(rom_path);
        std::optional<recording::Recording> playback;
        if(!replay_path.empty()) { playback=recording::read(replay_path); runtime->restore(playback->initial); }
        recording::Recording recorded{runtime->save(),{}};
        host::Presentation display("Alien Crush",960,720);
        Audio audio;
        const SDL_AudioSpec spec{SDL_AUDIO_S16,2,runtime->sample_rate()};
        audio.stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
        if(!audio.stream) throw std::runtime_error(SDL_GetError());
        if(!SDL_ResumeAudioStreamDevice(audio.stream)) throw std::runtime_error(SDL_GetError());
        PadDevice gamepad; open_pad(gamepad);
        bool running=true,focused=true;
        const auto frame_ns=static_cast<std::uint64_t>(1e9/runtime->frames_per_second());
        auto deadline=SDL_GetTicksNS();
        std::uint64_t count=0;
        while(running && (!limit || count<limit) && (!playback || count<playback->inputs.size())) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) running=false;
                if(event.type==SDL_EVENT_GAMEPAD_ADDED) open_pad(gamepad);
                if(event.type==SDL_EVENT_GAMEPAD_REMOVED && gamepad.pad && SDL_GetGamepadID(gamepad.pad)==event.gdevice.which) { SDL_CloseGamepad(gamepad.pad); gamepad.pad=nullptr; open_pad(gamepad); }
                if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST) { focused=false; SDL_ClearAudioStream(audio.stream); }
                if(event.type==SDL_EVENT_WINDOW_FOCUS_GAINED) { focused=true; deadline=SDL_GetTicksNS(); }
                if(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if(event.key.scancode==SDL_SCANCODE_ESCAPE) running=false;
                    if(event.key.scancode==SDL_SCANCODE_F11) SDL_SetWindowFullscreen(display.window(),!(SDL_GetWindowFlags(display.window())&SDL_WINDOW_FULLSCREEN));
                    if(event.key.scancode==SDL_SCANCODE_F5 || event.key.scancode==SDL_SCANCODE_F9) {
                        try {
                            if(playback || !record_path.empty()) throw std::runtime_error("Quick states are disabled during recording/replay to keep the input timeline reproducible");
                            const auto path=data_path/"alien-crush.reference.state";
                            if(event.key.scancode==SDL_SCANCODE_F5) core::write_file_atomic(path,state::encode(runtime->save()));
                            else { auto decoded=state::decode(core::read_file(path)); if(!decoded.decoded) throw std::runtime_error(decoded.message); runtime->restore(decoded.container); SDL_ClearAudioStream(audio.stream); }
                            SDL_SetWindowTitle(display.window(),"Alien Crush");
                        } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; SDL_SetWindowTitle(display.window(),e.what()); }
                    }
                }
            }
            if(!running) break;
            if(!focused && !limit) { SDL_Delay(10); continue; }
            const auto input=playback?playback->inputs[static_cast<std::size_t>(count)]:read_input(gamepad.pad);
            const auto frame=runtime->advance(input);
            if(!record_path.empty()) recorded.inputs.push_back(input);
            if(!SDL_PutAudioStreamData(audio.stream,frame.stereo.data(),static_cast<int>(frame.stereo.size_bytes()))) throw std::runtime_error(SDL_GetError());
            // A stalled output device must not accumulate minutes of queued sound.
            if(SDL_GetAudioStreamQueued(audio.stream)>runtime->sample_rate()*4/4) SDL_ClearAudioStream(audio.stream);
            display.show(frame); ++count;
            deadline+=frame_ns;
            const auto now=SDL_GetTicksNS();
            if(deadline>now) SDL_DelayPrecise(deadline-now);
            else if(now-deadline>frame_ns*4) deadline=now;
        }
        if(!record_path.empty()) recording::write(record_path,recorded);
        return 0;
    } catch(const std::exception& e) { std::cerr<<"Alien Crush: "<<e.what()<<'\n'; return 1; }
}
