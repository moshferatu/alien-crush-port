// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "startup_display.hpp"
#include "alien_crush/render/viewport_actors.hpp"
#include "cabinet.hpp"
#include "cabinet_input.hpp"
#include "cabinet_nudge.hpp"
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
#include "alien_crush/input/nudge_multiplier.hpp"
#include <SDL3/SDL_main.h>
#include <iostream>
#include <stdexcept>
#include <optional>
#include <fstream>
#include <charconv>
#include <cmath>

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
        std::optional<double> requested_nudge_multiplier;
        double refresh_override=0;
        bool smoothing=true;
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
            else if(arg=="--no-smooth") smoothing=false;
            else if(arg=="--smooth") smoothing=true;
            else if(arg=="--refresh") {
                const auto text=value();
                const auto parsed=std::from_chars(text.data(),text.data()+text.size(),refresh_override);
                if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() ||
                    !std::isfinite(refresh_override) || refresh_override<1 || refresh_override>1000)
                    throw std::runtime_error("--refresh requires a display rate from 1 to 1000 Hz");
            }
            else if(arg=="--nudge-multiplier") {
                requested_nudge_multiplier=input::parse_nudge_multiplier(value());
                if(!requested_nudge_multiplier) throw std::runtime_error("--nudge-multiplier requires a number from 0 to 16");
            }
            else if(arg=="--input-probe") probe=true;
            else if(arg=="--cabinet-plunger-probe") probe_plunger=true;
            else if(arg=="--plunger-config") plunger_path=core::path_from_utf8(value());
            else if(arg=="--help") { std::cout<<"alien-crush --rom <path> [--cabinet] [--frames N] [--record <file> | --replay <file>] [--data-dir <directory>]\nEnter launch in shooter, otherwise start/confirm/pause; Left Shift/Left left flipper; Right Shift/Right right flipper; X nudge; Backspace select. Gamepad shoulders flippers, south launch/confirm. F5/F9 quick save/load, F11 fullscreen, Escape exit.\nCabinet: move/resize three windows; F11 toggles the focused window, R rotates the full stitched playfield. Layout saves on normal exit.\nCabinet settings: windows, input.buttons, input.nudge.* and input.plunger.* share portable-data/cabinet.ini (see README).\n--smooth / --no-smooth: high-refresh cabinet animation smoothing (default on); F7 toggles.\n--refresh <Hz>: override the detected playfield refresh rate; --no-vsync uses timer pacing.\n--nudge-multiplier <factor>: nudge force from 0 to 16 (default 1); applies to keyboard, buttons and physical nudges.\n--cabinet-plunger-probe: guided plunger calibration (no ROM needed).\n--plunger-config <path>: reuse a Devil's Crush cabinet.ini; otherwise local cabinet.ini, then legacy cabinet-plunger.txt.\n--input-probe: inspect raw buttons and axes without a ROM; --frames limits 100ms samples.\n"; return 0; }
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
        auto nudge=host::parse_cabinet_nudge(cabinet_mode && replay_path.empty()?host::read_cabinet_settings(data_path/"cabinet.ini"):host::CabinetSettings{});
        nudge.enabled=nudge.enabled && cabinet_mode && replay_path.empty();
        host::CabinetNudge cabinet_nudge(nudge);
        host::CabinetInput cabinet_input(buttons);
        host::CabinetPlunger cabinet_plunger(plunger);
        std::optional<recording::Recording> playback;
        if(!replay_path.empty()) playback=recording::read(replay_path);
        const auto mode=playback?((playback->initial.core_revision.find("-handoff1")!=std::string::npos || playback->initial.core_revision.find("-h1-")!=std::string::npos)?HandoffMode::continuous:HandoffMode::original):
            (cabinet_mode?HandoffMode::continuous:HandoffMode::original);
        const auto controls=playback && playback->initial.core_revision.find("-controls1")==std::string::npos?ControlMode::original:ControlMode::cabinet;
        const auto multiplier=playback?input::recorded_nudge_multiplier(playback->initial.core_revision):requested_nudge_multiplier.value_or(1);
        if(playback && requested_nudge_multiplier && *requested_nudge_multiplier!=multiplier)
            throw std::runtime_error("Nudge multiplier differs from the recorded session");
        auto runtime=make_reference_runtime(mode,controls,multiplier); runtime->load(rom_path);
        if(multiplier!=1) std::cout<<"Nudge force multiplier: "<<multiplier<<"x\n";
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
        std::array<std::byte,8192> previous_actor_ram{};
        std::unique_ptr<assets::ActorAssets> viewport_actors;
        if(cabinet_mode) viewport_actors=std::make_unique<assets::ActorAssets>(core::read_file(rom_path));
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
        auto next_picture=deadline;
        auto refresh_check=deadline;
        double refresh_hz=refresh_override?refresh_override:display.refresh_rate();
        bool smooth_active=cabinet_mode && smoothing && refresh_hz>runtime->frames_per_second()*1.1;
        std::cout<<"Cabinet animation smoothing: "<<(smooth_active?"on":"off")<<" ("<<refresh_hz<<" Hz; F7 toggles)\n";
        std::uint64_t count=0,stitched_frames=0;
        host::StartupDisplay startup_display;
        unsigned catch_up=0;
        while(running && (!limit || count<limit) && (!playback || count<playback->inputs.size())) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_EVENT_QUIT || event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) running=false;
                if(cabinet && event.type>=SDL_EVENT_DISPLAY_FIRST && event.type<=SDL_EVENT_DISPLAY_LAST) {
                    display_layout_changed=true;
                    SDL_ClearAudioStream(audio.stream);cabinet_input.rearm();cabinet_nudge.rearm();cabinet_plunger.interrupt();
                }
                if(event.type==SDL_EVENT_JOYSTICK_ADDED || event.type==SDL_EVENT_JOYSTICK_REMOVED) {cabinet_input.refresh();cabinet_plunger.refresh();cabinet_nudge.refresh();}
                if(event.type==SDL_EVENT_GAMEPAD_ADDED) open_pad(gamepad);
                if(event.type==SDL_EVENT_GAMEPAD_REMOVED && gamepad.pad && SDL_GetGamepadID(gamepad.pad)==event.gdevice.which) { SDL_CloseGamepad(gamepad.pad); gamepad.pad=nullptr; open_pad(gamepad); }
                if(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if(event.key.scancode==SDL_SCANCODE_F7) {
                        smoothing=!smoothing;display.reset_smoothing();
                        std::cout<<"Cabinet animation smoothing: "<<(smoothing?"enabled for high-refresh displays":"off")<<'\n';
                    }
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
                            } else { auto decoded=state::decode(core::read_file(path)); if(!decoded.decoded) throw std::runtime_error(decoded.message); runtime->restore(decoded.container); previous_video.reset(); transition.reset(); display.reset_smoothing(); cabinet_input.rearm();cabinet_nudge.rearm(); cabinet_plunger.restored(); SDL_ClearAudioStream(audio.stream); }
                            SDL_SetWindowTitle(display.window(),"Alien Crush");
                        } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; SDL_SetWindowTitle(display.window(),e.what()); }
                    }
                }
            }
            if(!running) break;
            if(display_layout_changed) {
                if(!cabinet->recover_displays()) {SDL_Delay(10);continue;}
                display_layout_changed=false;deadline=SDL_GetTicksNS();display.reset_smoothing();refresh_check=0;
            }
            const auto* keyboard=SDL_GetKeyboardFocus();
            const bool now_focused=cabinet?cabinet->owns(SDL_GetKeyboardFocus()):keyboard==display.window();
            if(focused && !now_focused) {SDL_ClearAudioStream(audio.stream);cabinet_input.rearm();cabinet_nudge.rearm();cabinet_plunger.interrupt();}
            if(!focused && now_focused) {deadline=SDL_GetTicksNS();display.reset_smoothing();}
            focused=now_focused;
            if(!running) break;
            if(!focused && !limit) { SDL_Delay(10); continue; }
            const auto picture_now=SDL_GetTicksNS();
            if(picture_now>=refresh_check) {
                refresh_hz=refresh_override?refresh_override:display.refresh_rate();
                refresh_check=picture_now+1000000000;
            }
            const bool smooth_wanted=cabinet_mode && smoothing && refresh_hz>runtime->frames_per_second()*1.1;
            if(smooth_wanted!=smooth_active) {display.reset_smoothing();smooth_active=smooth_wanted;}
            const auto picture_ns=static_cast<std::uint64_t>(1e9/std::max(1.0,refresh_hz));
            // Refresh the GPU layers between simulation ticks. Never run the
            // emulator, animation timelines, input or audio on extra refreshes.
            if(count && picture_now<deadline) {
                if(vsync || smooth_active) {
                    if(!vsync && picture_now<next_picture) {
                        SDL_DelayPrecise(std::min(next_picture,deadline)-picture_now);continue;
                    }
                    // Sample for the upcoming scanout, not for the start of a
                    // variable-cost render. Otherwise simulation work makes
                    // each first subframe move farther than the cached ones.
                    const auto target=vsync?std::max(picture_now,next_picture):picture_now;
                    const double fraction=std::clamp(1.0+(static_cast<double>(target)-static_cast<double>(deadline))/frame_ns,0.0,1.0);
                    display.repeat(smooth_active?fraction:1);
                    if(cabinet && smooth_active) cabinet->repeat(fraction);
                    next_picture=vsync?SDL_GetTicksNS()+picture_ns:std::max(next_picture+picture_ns,picture_now+1);
                    catch_up=0;continue;
                }
                SDL_DelayPrecise(deadline-picture_now);continue;
            }
            // A mapped raw cabinet board must not also trigger SDL's default gamepad buttons.
            auto* standard_pad=gamepad.pad && SDL_GetGamepadID(gamepad.pad)!=cabinet_input.id() &&
                SDL_GetGamepadID(gamepad.pad)!=cabinet_plunger.id()?gamepad.pad:nullptr;
            auto input=playback?playback->inputs[static_cast<std::size_t>(count)]:read_input(standard_pad);
            if(!playback && focused) input.player_one|=cabinet_input.read().player_one;
            if(cabinet_mode && !playback && cabinet_nudge.read(SDL_GetTicks(),focused && host::nudge_play_active(runtime->work_ram())))
                input.player_one|=Pad::button_ii;
            if(!playback) input=input_router.route(input,runtime->work_ram());
            const auto command=playback?playback->plunger_at(static_cast<std::size_t>(count)):cabinet_plunger.apply(*runtime,input,focused);
            const auto frame_started=SDL_GetTicksNS();
            if(table_raster) runtime->observe_display(true);
            auto frame=playback?recording::advance(*runtime,input,command):runtime->advance(input);
            const auto runtime_ended=timing.is_open()?SDL_GetTicksNS():0;
            auto compose_ended=runtime_ended;
            // A slow refresh or isolated stall can leave multiple game frames
            // due. Catch up without waiting for VSync on each intermediate one.
            const bool present_frame=vsync?(SDL_GetTicksNS()<deadline+frame_ns || ++catch_up>=4):
                (!smooth_active || SDL_GetTicksNS()>=next_picture);
            display.defer(!present_frame);
            if(!record_path.empty()) {
                if(command && recorded.plunger.empty()) recorded.plunger.resize(recorded.inputs.size(),0);
                recorded.inputs.push_back(input);
                if(command || !recorded.plunger.empty()) recorded.plunger.push_back(command);
            }
            // Snapshot inspection invalidates borrowed views. Keep this frame's
            // audio through composition. Every frame is queued on the same clock.
            const std::vector<std::int16_t> sound(frame.stereo.begin(),frame.stereo.end());
            std::vector<std::uint32_t> startup_pixels;
            bool startup_blank=false;
            if(!startup_display.ready()) {
                // Snapshot inspection invalidates the borrowed frame, including
                // the first visible title frame. Preserve it before inspection.
                startup_pixels.assign(frame.pixels.begin(),frame.pixels.end());
                startup_blank=startup_display.blank(runtime->video_snapshot().control);
                if(startup_blank) std::fill(startup_pixels.begin(),startup_pixels.end(),0xff000000);
                frame.pixels=startup_pixels;
            }
            if(startup_blank) display.show(frame);
            else if(cabinet_mode) {
                // Own fallback pixels before inspecting the video snapshot.
                std::vector<std::uint32_t> original(frame.pixels.begin(),frame.pixels.end());
                const auto snapshot=runtime->video_snapshot();
                const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                table->capture_animation(smooth_active);
                auto stitched=table->cabinet_frame(snapshot,previous_video?&*previous_video:nullptr,
                    runtime->work_ram(),table_raster?&raster:nullptr,transition);
                if(timing.is_open()) compose_ended=SDL_GetTicksNS();
                if(stitched) {
                    render::MotionPosition position;
                    const auto ram=runtime->work_ram();
                    const auto scene=diagnostics::observe_alien_state(ram);
                    const auto ball=smooth_active && !(std::to_integer<unsigned>(ram[0xf6])&2)?
                        table->transition_ball(std::to_integer<unsigned>(ram[0x79]),ram,&position):std::nullopt;
                    const auto* backdrop=ball?transition.ball_backdrop():nullptr;
                    const auto now=vsync?std::max(SDL_GetTicksNS(),next_picture):SDL_GetTicksNS();
                    const auto fraction=now>deadline?std::min(1.0,static_cast<double>(now-deadline)/frame_ns):0.0;
                    std::optional<render::AnimationScene> animation;
                    if(smooth_active && !(std::to_integer<unsigned>(ram[0xf6])&2) && table->animation_scene()) {
                        animation=*table->animation_scene();
                        if(ball && backdrop) animation->layers.push_back({0xffffffffffffffffull,position,ball->image});
                        std::stable_partition(animation->layers.begin(),animation->layers.end(),[](const auto& layer){return !layer.foreground;});
                    }
                    display.show({frame.number,stitched->width,stitched->height,stitched->pixels,{}},
                        render::table_aspect_width,render::table_aspect_height,true,{},backdrop,ball?&*ball:nullptr,position,fraction,animation?&*animation:nullptr);
                    ++stitched_frames;
                }
                else {
                    const auto scene=diagnostics::observe_alien_state(runtime->work_ram());
                    const bool blank=render::cabinet_viewport_blank(snapshot,previous_video?&*previous_video:nullptr,runtime->work_ram());
                    if(blank)
                        std::fill(original.begin(),original.end(),0xff000000);
                    // The title's scrolling BAT pattern repeats every 96 pixels.
                    // Its top-left repeat contains no menu lettering; omit sprites
                    // so only the background extends outside the original image.
                    render::Image title_background;
                    if(scene.title_menu && !scene.title_transition)
                        title_background=render::compose(snapshot.vram,snapshot.palette,snapshot.sat,
                            {96,96,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,
                                static_cast<std::uint16_t>(snapshot.control & 0x80)});
                    std::optional<render::AnimationScene> animation;
                    if(smooth_active && !blank && !(std::to_integer<unsigned>(runtime->work_ram()[0xf6])&2)) {
                        const auto& sat=previous_video?previous_video->sat:snapshot.sat;
                        animation=render::viewport_animation({frame.width,frame.height,original},snapshot.vram,snapshot.palette,sat,
                            {frame.width,frame.height,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,snapshot.control},
                            scene.title_menu?2:scene.high_score_menu?3:scene.bonus_room?10+scene.bonus_room:4);
                        if(scene.bonus_room && !scene.title_menu && !scene.high_score_menu)
                            render::identify_viewport_actors(*animation,*viewport_actors,
                                previous_video?std::span<const std::byte>(previous_actor_ram):runtime->work_ram(),sat,
                                previous_video?previous_video->frame:snapshot.frame);
                        if(scene.title_menu) {
                            animation->scroll={double(snapshot.scroll_x%96),double(snapshot.scroll_y%96)};
                            animation->scroll_period=96;
                        }
                    }
                    const auto picture_time=vsync?std::max(SDL_GetTicksNS(),next_picture):SDL_GetTicksNS();
                    const auto fraction=picture_time>deadline?std::min(1.0,static_cast<double>(picture_time-deadline)/frame_ns):0.0;
                    display.show({frame.number,frame.width,frame.height,original,{}},4,3,
                        !scene.title_menu && !scene.high_score_menu && scene.bonus_room!=0,
                        {frame.number,title_background.width,title_background.height,title_background.pixels,{}},
                        nullptr,nullptr,{},fraction,animation?&*animation:nullptr);
                }
                previous_video=snapshot;
                std::copy(runtime->work_ram().begin(),runtime->work_ram().end(),previous_actor_ram.begin());
            } else {display.show(frame);}
            const auto playfield_ended=SDL_GetTicksNS();
            if(present_frame) next_picture=vsync?playfield_ended+picture_ns:std::max(next_picture+picture_ns,playfield_ended+1);
            ++count;
            if(!SDL_PutAudioStreamData(audio.stream,sound.data(),static_cast<int>(sound.size()*sizeof(std::int16_t)))) throw std::runtime_error(SDL_GetError());
            // A stalled output device must not accumulate minutes of queued sound.
            if(SDL_GetAudioStreamQueued(audio.stream)>runtime->sample_rate()*4/4) SDL_ClearAudioStream(audio.stream);
            if(cabinet) cabinet->show(runtime->work_ram(),smooth_active,
                std::clamp((static_cast<double>(SDL_GetTicksNS())-static_cast<double>(deadline))/frame_ns,0.0,1.0));
            if(present_frame) catch_up=0;
            const auto cabinet_ended=timing.is_open()?SDL_GetTicksNS():0;
            if(high_scores) high_scores->poll(runtime->work_ram());
            if(timing.is_open()) timing<<frame.number<<','<<frame_started<<','<<SDL_GetTicksNS()<<','
                <<sound.size()<<','<<SDL_GetAudioStreamQueued(audio.stream)<<','<<runtime_ended<<','<<compose_ended<<','
                <<playfield_ended<<','<<cabinet_ended<<','<<std::to_integer<unsigned>(runtime->work_ram()[0x79])<<'\n';
            deadline+=frame_ns;
            const auto now=SDL_GetTicksNS();
            if(now>deadline && now-deadline>frame_ns*4) {deadline=now;display.reset_smoothing();}
        }
        if(high_scores) high_scores->poll(runtime->work_ram(),true);
        std::cout<<"Playfield presentation: "<<display.pictures()<<" pictures, "<<display.smoothed_pictures()
            <<" with animation smoothing; "<<count<<" simulation frames\n";
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
        // Count simulated session frames, not VSync repeats or restored frame
        // numbers. Cabinet presentation is native; the reference backend has
        // no translated gameplay routines or execution-cycle ownership counters.
        std::cout<<"Native coverage on exit ("<<count<<" session frames):\n"
            <<"  Native CPU translation: 0 translated gameplay routines; game logic uses the reference interpreter.\n"
            <<"  Native execution outside wait/stalls: unavailable (backend has no cycle ownership accounting).\n"
            <<"  Native stitched playfield: "<<stitched_frames<<" of "<<count<<" frames";
        if(count) std::cout<<" ("<<std::fixed<<std::setprecision(2)
            <<100.0*static_cast<double>(stitched_frames)/static_cast<double>(count)<<"%)"<<std::defaultfloat;
        std::cout<<"\n  Reference viewport: "<<count-stitched_frames<<" frames\n"
            <<"  Presentation counts are not CPU execution coverage.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"Alien Crush: "<<e.what()<<'\n'; return 1; }
}
