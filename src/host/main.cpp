// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "cabinet.hpp"
#include "cabinet_input.hpp"
#include "cabinet_plunger.hpp"
#include "cabinet_settings.hpp"
#include <iomanip>
#include <algorithm>
#include <sstream>
#include "game_input.hpp"
#include "high_score_autosave.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include <SDL3/SDL_main.h>
#include <iostream>
#include <stdexcept>
#include <optional>
#include <fstream>

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
    int count{};const bool* keys=SDL_GetKeyboardState(&count);
    std::array<bool,SDL_GAMEPAD_BUTTON_COUNT> buttons{};
    for(int i=0;pad && i<SDL_GAMEPAD_BUTTON_COUNT;++i) buttons[static_cast<std::size_t>(i)]=SDL_GetGamepadButton(pad,static_cast<SDL_GamepadButton>(i));
    return host::device_game_input({keys,static_cast<std::size_t>(count)},buttons);
}
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path rom_path,record_path,replay_path,data_path,timing_path,plunger_path;
        std::uint64_t limit=0;
        bool cabinet_mode=false,probe=false,probe_plunger=false,no_vsync=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            auto value=[&](){if(++i>=argc) throw std::runtime_error("Missing value for "+arg); return std::string(argv[i]);};
            if(arg=="--rom") rom_path=core::path_from_utf8(value());
            else if(arg=="--frames") limit=std::stoull(value());
            else if(arg=="--record") record_path=core::path_from_utf8(value());
            else if(arg=="--replay") replay_path=core::path_from_utf8(value());
            else if(arg=="--data-dir") data_path=core::path_from_utf8(value());
            else if(arg=="--timing-log") timing_path=core::path_from_utf8(value());
            else if(arg=="--cabinet") cabinet_mode=true;
            else if(arg=="--no-vsync") no_vsync=true;
            else if(arg=="--input-probe") probe=true;
            else if(arg=="--cabinet-plunger-probe") probe_plunger=true;
            else if(arg=="--plunger-config") plunger_path=core::path_from_utf8(value());
            else if(arg=="--help") { std::cout<<"alien-crush --rom <path> [--cabinet] [--frames N] [--record <file> | --replay <file>] [--data-dir <directory>]\nEnter launch in shooter, otherwise start/confirm/pause; Left Shift/Left left flipper; Right Shift/Right right flipper; X nudge; Backspace select. Gamepad shoulders flippers, south launch/confirm. F5/F9 quick save/load, F11 fullscreen, Escape exit.\nCabinet: move/resize three windows; F11 toggles the focused window, R rotates the full stitched playfield. Layout saves on normal exit.\nCabinet settings: windows, input.buttons and input.plunger.* share portable-data/cabinet.ini (see README).\n--cabinet-plunger-probe: guided plunger calibration (no ROM needed).\n--plunger-config <path>: reuse a Devil's Crush cabinet.ini; otherwise local cabinet.ini, then legacy cabinet-plunger.txt.\n--input-probe: inspect raw buttons and axes without a ROM; --frames limits 100ms samples.\n"; return 0; }
            else throw std::runtime_error("Unknown option "+arg);
        }
        if(!record_path.empty() && !replay_path.empty()) throw std::runtime_error("Choose recording or replay");
        SdlLifetime sdl;
        if(probe_plunger) return host::plunger_probe();
        if(probe) return host::input_probe(limit);
        // SDL cursor visibility applies to all game windows, including the
        // cabinet backglass and score display, without grabbing the mouse.
        if(!SDL_HideCursor()) throw std::runtime_error(SDL_GetError());
        const auto base=core::path_from_utf8(SDL_GetBasePath());
        if(rom_path.empty()) rom_path=base/"Alien Crush.pce";
        if(data_path.empty()) data_path=base/"portable-data";
        const bool external_plunger=!plunger_path.empty();
        if(plunger_path.empty()) {
            const auto settings=cabinet_mode && replay_path.empty()?host::read_cabinet_settings(data_path/"cabinet.ini"):host::CabinetSettings{};
            const bool has_plunger=std::any_of(settings.begin(),settings.end(),[](const auto& item){return item.first.starts_with("input.plunger.");});
            plunger_path=has_plunger?data_path/"cabinet.ini":data_path/"cabinet-plunger.txt";
        }
        else if(!std::filesystem::exists(plunger_path)) throw std::runtime_error("Plunger configuration file does not exist");
        const auto buttons=cabinet_mode && replay_path.empty()?
            host::load_cabinet_buttons(data_path/"cabinet-inputs.txt"):host::CabinetButtons{};
        const auto plunger=cabinet_mode && replay_path.empty()?
            host::load_cabinet_plunger(plunger_path):input::PlungerBinding{};
        host::CabinetInput cabinet_input(buttons);
        host::CabinetPlunger cabinet_plunger(plunger);
        std::optional<recording::Recording> playback;
        if(!replay_path.empty()) playback=recording::read(replay_path);
        const auto mode=playback?((playback->initial.core_revision.find("-handoff1")!=std::string::npos || playback->initial.core_revision.find("-h1-")!=std::string::npos)?HandoffMode::continuous:HandoffMode::original):
            (cabinet_mode?HandoffMode::continuous:HandoffMode::original);
        const auto controls=playback && !playback->initial.core_revision.ends_with("-controls1")?ControlMode::original:ControlMode::cabinet;
        auto runtime=make_reference_runtime(mode,controls); runtime->load(rom_path);
        std::optional<host::HighScoreAutosave> high_scores;
        if(!playback) high_scores.emplace(*runtime,data_path/"alien-crush.hiscore");
        const bool table_raster=cabinet_mode && runtime->supports_display_observation();
        if(table_raster) runtime->observe_display(true);
        if(playback) runtime->restore(playback->initial);
        recording::Recording recorded{runtime->save(),{}};
        std::ofstream timing;
        if(!timing_path.empty()) {
            timing.exceptions(std::ios::failbit|std::ios::badbit);timing.open(timing_path);
            timing<<"frame,start_ns,end_ns,audio_samples,queued_bytes,runtime_end_ns,compose_end_ns,playfield_end_ns,cabinet_end_ns,table_half\n";
        }
        host::Presentation display(cabinet_mode?"Alien Crush - Playfield":"Alien Crush",cabinet_mode?540:960,720,cabinet_mode);
        const bool vsync=!no_vsync && display.enable_vsync();
        if(!no_vsync && !vsync) std::cerr<<"Playfield VSync unavailable; using timer pacing: "<<SDL_GetError()<<'\n';
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
        host::GameInputRouter input_router;
        bool running=true,focused=true,display_layout_changed=false;
        const auto frame_ns=static_cast<std::uint64_t>(1e9/runtime->frames_per_second());
        auto deadline=SDL_GetTicksNS();
        std::uint64_t count=0;
        unsigned catch_up=0;
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
                    if(event.key.scancode==SDL_SCANCODE_F5 || event.key.scancode==SDL_SCANCODE_F9) {
                        try {
                            if(playback || !record_path.empty()) throw std::runtime_error("Quick states are disabled during recording/replay to keep the input timeline reproducible");
                            const auto path=data_path/(mode==HandoffMode::continuous?"alien-crush.cabinet-controls.state":"alien-crush.desktop-controls.state");
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
            // Present at the playfield's refresh rate while the game keeps its
            // own 59.826 Hz clock. Extra refreshes reuse the completed texture;
            // they must not advance actors, inputs, or audio.
            if(vsync && count && SDL_GetTicksNS()<deadline) {
                display.repeat();catch_up=0;continue;
            }
            // A mapped raw cabinet board must not also trigger SDL's default gamepad buttons.
            auto* standard_pad=gamepad.pad && SDL_GetGamepadID(gamepad.pad)!=cabinet_input.id() &&
                SDL_GetGamepadID(gamepad.pad)!=cabinet_plunger.id()?gamepad.pad:nullptr;
            auto input=playback?playback->inputs[static_cast<std::size_t>(count)]:read_input(standard_pad);
            if(!playback && focused) input.player_one|=cabinet_input.read().player_one;
            if(!playback) input=input_router.route(input,runtime->work_ram());
            const auto command=playback?playback->plunger_at(static_cast<std::size_t>(count)):cabinet_plunger.apply(*runtime,input,focused);
            const auto frame_started=SDL_GetTicksNS();
            if(table_raster) runtime->observe_display(true);
            const auto frame=playback?recording::advance(*runtime,input,command):runtime->advance(input);
            const auto runtime_ended=timing.is_open()?SDL_GetTicksNS():0;
            auto compose_ended=runtime_ended;
            // A slow refresh or isolated stall can leave multiple game frames
            // due. Catch up without waiting for VSync on each intermediate one.
            const bool present_frame=!vsync || SDL_GetTicksNS()<deadline+frame_ns || ++catch_up>=4;
            display.defer(!present_frame);
            if(!record_path.empty()) {
                if(command && recorded.plunger.empty()) recorded.plunger.resize(recorded.inputs.size(),0);
                recorded.inputs.push_back(input);
                if(command || !recorded.plunger.empty()) recorded.plunger.push_back(command);
            }
            // Snapshot inspection invalidates borrowed views. Keep this frame's
            // audio through composition. Every frame is queued on the same clock.
            const std::vector<std::int16_t> sound(frame.stereo.begin(),frame.stereo.end());
            if(cabinet_mode) {
                // Own fallback pixels before inspecting the video snapshot.
                std::vector<std::uint32_t> original(frame.pixels.begin(),frame.pixels.end());
                const auto snapshot=runtime->video_snapshot();
                const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                auto stitched=table->cabinet_frame(snapshot,previous_video?&*previous_video:nullptr,
                    runtime->work_ram(),table_raster?&raster:nullptr,transition);
                if(timing.is_open()) compose_ended=SDL_GetTicksNS();
                if(stitched) display.show({frame.number,stitched->width,stitched->height,stitched->pixels,{}},
                    render::table_aspect_width,render::table_aspect_height,true);
                else {
                    const auto scene=diagnostics::observe_alien_state(runtime->work_ram());
                    if(scene.title_transition) std::fill(original.begin(),original.end(),0xff000000);
                    display.show({frame.number,frame.width,frame.height,original,{}},4,3,
                        scene.bonus_room!=0 || scene.title_menu || scene.high_score_menu);
                }
                previous_video=snapshot;
            } else {display.show(frame);}
            const auto playfield_ended=timing.is_open()?SDL_GetTicksNS():0;
            ++count;
            if(!SDL_PutAudioStreamData(audio.stream,sound.data(),static_cast<int>(sound.size()*sizeof(std::int16_t)))) throw std::runtime_error(SDL_GetError());
            // A stalled output device must not accumulate minutes of queued sound.
            if(SDL_GetAudioStreamQueued(audio.stream)>runtime->sample_rate()*4/4) SDL_ClearAudioStream(audio.stream);
            if(cabinet) cabinet->show(runtime->work_ram());
            if(present_frame) catch_up=0;
            const auto cabinet_ended=timing.is_open()?SDL_GetTicksNS():0;
            if(high_scores) high_scores->poll(runtime->work_ram());
            if(timing.is_open()) timing<<frame.number<<','<<frame_started<<','<<SDL_GetTicksNS()<<','
                <<sound.size()<<','<<SDL_GetAudioStreamQueued(audio.stream)<<','<<runtime_ended<<','<<compose_ended<<','
                <<playfield_ended<<','<<cabinet_ended<<','<<std::to_integer<unsigned>(runtime->work_ram()[0x79])<<'\n';
            deadline+=frame_ns;
            const auto now=SDL_GetTicksNS();
            if(deadline>now && !vsync) SDL_DelayPrecise(deadline-now);
            else if(now>deadline && now-deadline>frame_ns*4) deadline=now;
        }
        if(high_scores) high_scores->poll(runtime->work_ram(),true);
        if(!record_path.empty()) recording::write(record_path,recorded);
        // Keep the last usable layout if the user exits during a display outage.
        if(cabinet && !display_layout_changed) cabinet->save();
        if(cabinet && !playback) {
            auto settings=host::read_cabinet_settings(data_path/"cabinet.ini");
            std::ostringstream mapping;
            if(!buttons.device.empty()) {
                mapping<<"ACINPUTS2 "<<std::quoted(buttons.device);
                for(const auto button:buttons.buttons) mapping<<' '<<button;
            }
            settings.try_emplace("input.buttons",mapping.str());
            if(!external_plunger) {
                std::string name;
                constexpr char hex[]="0123456789ABCDEF";
                for(const unsigned char c:plunger.device) {
                    if(c=='%' || c=='=' || c=='\r' || c=='\n') {name+='%';name+=hex[c>>4];name+=hex[c&15];}
                    else name+=static_cast<char>(c);
                }
                settings.try_emplace("input.plunger.device",name);
                settings.try_emplace("input.plunger.axis",std::to_string(plunger.axis));
                settings.try_emplace("input.plunger.rest",std::to_string(plunger.calibration.rest));
                settings.try_emplace("input.plunger.drawn",std::to_string(plunger.calibration.drawn));
                settings.try_emplace("input.plunger.deadzone",std::to_string(plunger.calibration.deadzone));
                settings.try_emplace("input.plunger.release",std::to_string(plunger.calibration.release));
            }
            host::write_cabinet_settings(data_path/"cabinet.ini",settings);
        }
        return 0;
    } catch(const std::exception& e) { std::cerr<<"Alien Crush: "<<e.what()<<'\n'; return 1; }
}
