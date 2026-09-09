// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "cabinet.hpp"
#include "cabinet_input.hpp"
#include "cabinet_plunger.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include <SDL3/SDL_main.h>
#include <iostream>
#include <stdexcept>
#include <optional>

using namespace alien_crush;
namespace {
struct SdlLifetime { SdlLifetime(){ if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMEPAD|SDL_INIT_JOYSTICK)) throw std::runtime_error(SDL_GetError()); } ~SdlLifetime(){ SDL_Quit(); } };
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
        bool cabinet_mode=false,stitched_mode=false,probe=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            auto value=[&](){if(++i>=argc) throw std::runtime_error("Missing value for "+arg); return std::string(argv[i]);};
            if(arg=="--rom") rom_path=core::path_from_utf8(value());
            else if(arg=="--frames") limit=std::stoull(value());
            else if(arg=="--record") record_path=core::path_from_utf8(value());
            else if(arg=="--replay") replay_path=core::path_from_utf8(value());
            else if(arg=="--data-dir") data_path=core::path_from_utf8(value());
            else if(arg=="--cabinet") cabinet_mode=true;
            else if(arg=="--input-probe") probe=true;
            else if(arg=="--stitched") {cabinet_mode=true;stitched_mode=true;}
            else if(arg=="--help") { std::cout<<"alien-crush --rom <path> [--cabinet | --stitched] [--frames N] [--record <file> | --replay <file>] [--data-dir <directory>]\nEnter start/pause; Left Shift left flipper; Right Shift/Z right flipper and launcher; X nudge; Backspace select. F5/F9 quick save/load, F11 fullscreen, Escape exit.\nCabinet: move/resize three windows; F11 toggles the focused window, R rotates the playfield, V toggles experimental stitching. --stitched starts in that view. Layout saves on normal exit.\nRaw cabinet buttons: configure cabinet-inputs.txt in the data directory (see README).\n--input-probe: inspect raw buttons and axes without a ROM; --frames limits 100ms samples.\n"; return 0; }
            else throw std::runtime_error("Unknown option "+arg);
        }
        if(!record_path.empty() && !replay_path.empty()) throw std::runtime_error("Choose recording or replay");
        SdlLifetime sdl;
        if(probe) return host::input_probe(limit);
        const auto base=core::path_from_utf8(SDL_GetBasePath());
        if(rom_path.empty()) rom_path=base/"Alien Crush.pce";
        if(data_path.empty()) data_path=base/"portable-data";
        host::CabinetInput cabinet_input(cabinet_mode && replay_path.empty()?
            host::load_cabinet_buttons(data_path/"cabinet-inputs.txt"):host::CabinetButtons{});
        host::CabinetPlunger cabinet_plunger(cabinet_mode && replay_path.empty()?
            host::load_cabinet_plunger(data_path/"cabinet-plunger.txt"):input::PlungerBinding{});
        auto runtime=make_reference_runtime(); runtime->load(rom_path);
        const bool table_raster=cabinet_mode && runtime->supports_display_observation();
        if(table_raster) runtime->observe_display(true);
        std::optional<recording::Recording> playback;
        if(!replay_path.empty()) { playback=recording::read(replay_path); runtime->restore(playback->initial); }
        recording::Recording recorded{runtime->save(),{}};
        host::Presentation display(cabinet_mode?"Alien Crush - Playfield":"Alien Crush",cabinet_mode?540:960,720,cabinet_mode);
        std::unique_ptr<host::Cabinet> cabinet;
        if(cabinet_mode) cabinet=std::make_unique<host::Cabinet>(display,data_path,core::read_file(rom_path));
        std::unique_ptr<render::TablePlayfield> table;
        std::optional<diagnostics::ReferenceSnapshot> previous_video;
        render::TableTransitionHold transition;
        if(cabinet_mode) table=std::make_unique<render::TablePlayfield>(core::read_file(rom_path));
        Audio audio;
        const SDL_AudioSpec spec{SDL_AUDIO_S16,2,runtime->sample_rate()};
        audio.stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
        if(!audio.stream) throw std::runtime_error(SDL_GetError());
        if(!SDL_ResumeAudioStreamDevice(audio.stream)) throw std::runtime_error(SDL_GetError());
        PadDevice gamepad; open_pad(gamepad);
        bool running=true,focused=true,display_layout_changed=false;
        const auto frame_ns=static_cast<std::uint64_t>(1e9/runtime->frames_per_second());
        auto deadline=SDL_GetTicksNS();
        std::uint64_t count=0;
        while(running && (!limit || count<limit) && (!playback || count<playback->inputs.size())) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) running=false;
                if(cabinet && event.type>=SDL_EVENT_DISPLAY_FIRST && event.type<=SDL_EVENT_DISPLAY_LAST) {
                    display_layout_changed=true;
                    SDL_ClearAudioStream(audio.stream);cabinet_input.rearm();cabinet_plunger.interrupt();
                }
                if(event.type==SDL_EVENT_JOYSTICK_ADDED || event.type==SDL_EVENT_JOYSTICK_REMOVED) {cabinet_input.refresh();cabinet_plunger.refresh();}
                if(event.type==SDL_EVENT_GAMEPAD_ADDED) open_pad(gamepad);
                if(event.type==SDL_EVENT_GAMEPAD_REMOVED && gamepad.pad && SDL_GetGamepadID(gamepad.pad)==event.gdevice.which) { SDL_CloseGamepad(gamepad.pad); gamepad.pad=nullptr; open_pad(gamepad); }
                if(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if(cabinet && cabinet->handle_key(event.key)) continue;
                    if(event.key.scancode==SDL_SCANCODE_ESCAPE) running=false;
                    if(event.key.scancode==SDL_SCANCODE_F11) {
                        auto* window=cabinet?SDL_GetWindowFromID(event.key.windowID):display.window();
                        if(window && !SDL_SetWindowFullscreen(window,!(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN))) throw std::runtime_error(SDL_GetError());
                    }
                    if(cabinet && event.key.scancode==SDL_SCANCODE_V) {stitched_mode=!stitched_mode;previous_video.reset();transition.reset();}
                    if(event.key.scancode==SDL_SCANCODE_F5 || event.key.scancode==SDL_SCANCODE_F9) {
                        try {
                            if(playback || !record_path.empty()) throw std::runtime_error("Quick states are disabled during recording/replay to keep the input timeline reproducible");
                            const auto path=data_path/"alien-crush.reference.state";
                            if(event.key.scancode==SDL_SCANCODE_F5) {
                                if(cabinet_plunger.owns_draw()) throw std::runtime_error("Release the plunger before saving a quick state");
                                core::write_file_atomic(path,state::encode(runtime->save()));
                            } else { auto decoded=state::decode(core::read_file(path)); if(!decoded.decoded) throw std::runtime_error(decoded.message); runtime->restore(decoded.container); previous_video.reset(); transition.reset(); cabinet_input.rearm(); cabinet_plunger.restored(); SDL_ClearAudioStream(audio.stream); }
                            SDL_SetWindowTitle(display.window(),"Alien Crush");
                        } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; SDL_SetWindowTitle(display.window(),e.what()); }
                    }
                }
            }
            if(!running) break;
            if(display_layout_changed) {
                if(!cabinet->recover_displays()) {SDL_Delay(10);continue;}
                display_layout_changed=false;deadline=SDL_GetTicksNS();
            }
            const auto* keyboard=SDL_GetKeyboardFocus();
            const bool now_focused=cabinet?cabinet->owns(SDL_GetKeyboardFocus()):keyboard==display.window();
            if(focused && !now_focused) {SDL_ClearAudioStream(audio.stream);cabinet_input.rearm();cabinet_plunger.interrupt();}
            if(!focused && now_focused) deadline=SDL_GetTicksNS();
            focused=now_focused;
            if(!running) break;
            if(!focused && !limit) { SDL_Delay(10); continue; }
            // A mapped raw cabinet board must not also trigger SDL's default gamepad buttons.
            auto* standard_pad=gamepad.pad && SDL_GetGamepadID(gamepad.pad)!=cabinet_input.id() &&
                SDL_GetGamepadID(gamepad.pad)!=cabinet_plunger.id()?gamepad.pad:nullptr;
            auto input=playback?playback->inputs[static_cast<std::size_t>(count)]:read_input(standard_pad);
            if(!playback && focused) input.player_one|=cabinet_input.read().player_one;
            const auto command=playback?playback->plunger_at(static_cast<std::size_t>(count)):cabinet_plunger.apply(*runtime,input,focused);
            const auto frame=playback?recording::advance(*runtime,input,command):runtime->advance(input);
            if(!record_path.empty()) {
                if(command && recorded.plunger.empty()) recorded.plunger.resize(recorded.inputs.size(),0);
                recorded.inputs.push_back(input);
                if(command || !recorded.plunger.empty()) recorded.plunger.push_back(command);
            }
            if(!SDL_PutAudioStreamData(audio.stream,frame.stereo.data(),static_cast<int>(frame.stereo.size_bytes()))) throw std::runtime_error(SDL_GetError());
            // A stalled output device must not accumulate minutes of queued sound.
            if(SDL_GetAudioStreamQueued(audio.stream)>runtime->sample_rate()*4/4) SDL_ClearAudioStream(audio.stream);
            if(stitched_mode) {
                // Save invalidates borrowed runtime views. Audio is already queued;
                // own the fallback pixels before inspecting the video snapshot.
                const std::vector<std::uint32_t> original(frame.pixels.begin(),frame.pixels.end());
                const auto ram=runtime->work_ram();
                const auto half=std::to_integer<unsigned>(ram[0x79]);
                const auto camera=std::to_integer<unsigned>(ram[0x16])|(std::to_integer<unsigned>(ram[0x17])<<8);
                const auto snapshot=diagnostics::decode_reference_snapshot(runtime->save());
                const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                std::optional<render::TableBallBackdrop> backdrop;
                auto stitched=previous_video?table->compose(snapshot,*previous_video,half,camera,runtime->work_ram(),
                    table_raster?&raster:nullptr,&backdrop):std::nullopt;
                const auto ball=stitched?std::nullopt:table->transition_ball(half,runtime->work_ram());
                stitched=transition.present(std::move(stitched),snapshot.frame,half,camera,runtime->work_ram(),
                    backdrop?&*backdrop:nullptr,ball?&*ball:nullptr);
                if(stitched) display.show({frame.number,stitched->width,stitched->height,stitched->pixels,{}},20,29);
                else display.show({frame.number,frame.width,frame.height,original,{}});
                previous_video=snapshot;
            } else display.show(frame);
            if(cabinet) cabinet->show(runtime->work_ram());
            ++count;
            deadline+=frame_ns;
            const auto now=SDL_GetTicksNS();
            if(deadline>now) SDL_DelayPrecise(deadline-now);
            else if(now-deadline>frame_ns*4) deadline=now;
        }
        if(!record_path.empty()) recording::write(record_path,recorded);
        // Keep the last usable layout if the user exits during a display outage.
        if(cabinet && !display_layout_changed) cabinet->save();
        return 0;
    } catch(const std::exception& e) { std::cerr<<"Alien Crush: "<<e.what()<<'\n'; return 1; }
}
