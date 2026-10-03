// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cabinet_config.hpp"
#include "game_input.hpp"
#include "alien_crush/core/files.hpp"
#include <fstream>

inline void check_cabinet_config() {
    using namespace alien_crush;
    auto check=[](bool ok,const char* why){if(!ok) throw std::runtime_error(why);};
    const auto decode=[](std::string_view text) {
        return host::decode_cabinet_config(std::as_bytes(std::span(text.data(),text.size())));
    };
    const auto rejected=[&](std::string_view text) {
        try {(void)decode(text);} catch(const std::runtime_error&) {return true;}
        return false;
    };
    const auto text_of=[](const std::vector<std::byte>& bytes) {
        return std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    };

    check(decode("version=1\n")==host::CabinetConfig{},"A file with only a version plays with the defaults");
    check(decode("\xEF\xBB\xBFversion=1\r\n")==host::CabinetConfig{},"A byte-order mark and CRLF lines are read");
    check(host::CabinetConfig{}.play==host::PlaySettings{} && host::PlaySettings{}.music_voice_overlay,
        "The music voice overlay is on by default");

    host::CabinetConfig chosen;
    chosen.play={"D:\\Games\\Alien Crush (USA) \xC3\xA9.pce",true,false,false,2.5,3};
    chosen.windows[host::playfield_window]={"Display = 1%",{-1920,40,1080,1920},host::DisplayRotation::portrait,true,90};
    chosen.windows[host::score_window].artwork_rotation=270;
    chosen.input[host::PadButton::run]={"Space","Keypad Enter"};
    chosen.input[host::PadButton::select]={};
    chosen.input.flippers={{host::PadButton::left,host::PadButton::down},{}};
    chosen.input.launch_keys={"Space"};
    chosen.input.nudge={false,"Clev, Soft",2,3,30,10,500};
    chosen.input.plunger.device="PinOne Board%";
    chosen.input.plunger.axis=2;
    chosen.input.plunger.calibration={1000,-1000,12,30};
    chosen.input.buttons={"PinOne",{0,1,2,3,-1,5,6,7,8}};
    const auto encoded=host::encode_cabinet_config(chosen);
    check(!encoded.empty() && decode(text_of(encoded))==chosen,"Every setting round trips through cabinet.ini");
    const auto written=text_of(encoded);
    for(const auto* line:{"version=1\n","input.nudge_multiplier=2.5\n","input.plunger.device=PinOne Board%25\n",
        "input.run=Space,Keypad Enter\n","input.select=\n","input.left_flipper=left,down\n","input.right_flipper=\n",
        "playfield.display=Display %3D 1%25\n","playfield.rotation=3\n","dmd.artwork_rotation=270\n",
        "audio.music_voice_overlay=0\n","input.buttons.select=-1\n","input.nudge.device=Clev%2C Soft\n"})
        check(written.find(line)!=std::string::npos,"Written in Devil's Crush's form");
    check(written.find("game.rom=D:\\Games\\Alien Crush %28USA%29 %C3%A9.pce\n")!=std::string::npos,"A path is escaped byte by byte");

    // Devil's Crush's own file: its keys read the same, and what it has that
    // this game does not is ignored.
    const auto shared=decode("version=1\ninput.one=Return\ninput.launch=Return,Space\ngame.bonus_stage=3\ngame.native=1\n"
        "input.plunger.axis=2\nplayfield.artwork_rotation=90\n");
    check(shared.input.launch_keys==std::vector<std::string>{"Return","Space"} && shared.input.plunger.axis==2 &&
        shared.windows[host::playfield_window].artwork_rotation==90,"A Devil's Crush cabinet.ini reads here");
    check(decode("version=1\ninput.one=A, A ,B\n").input[host::PadButton::one]==std::vector<std::string>{"A","B"},
        "A key bound twice to one input is kept once");

    for(const auto* bad:{"", "input.one=A\n", "version=2\n", "version=1\nversion=1\n", "version=1\n# comment\n",
        "version=1\ngame.cabinet=2\n", "version=1\ngame.bonus_stage=5\n", "version=1\ngame.bonus_stage=\n", "version=1\nvideo.smoothing=off\n", "version=1\naudio.music_voice_overlay=yes\n",
        "version=1\ninput.nudge_multiplier=17\n", "version=1\ninput.nudge_multiplier=2x\n",
        "version=1\nplayfield.artwork_rotation=45\n", "version=1\ninput.left_flipper=left,sideways\n",
        "version=1\ninput.nudge.typo=1\n", "version=1\ninput.nudge.axis_y=0\n", "version=1\ninput.plunger.release=101\n",
        "version=1\ninput.plunger.device=Bad%G1\n", "version=1\ninput.buttons.launch=256\n", "version=1\ninput.buttons.jump=1\n",
        "version=1\nplayfield.x=0\n", "version=1\n=value\n"})
        check(rejected(bad),"A file the game cannot read is refused, not guessed at");
    auto past=chosen;past.play.nudge_multiplier=16.5;
    check(host::encode_cabinet_config(past).empty(),"A nudge multiplier past 16 is not written");
    past=chosen;past.windows[host::backglass_window].artwork_rotation=45;
    check(host::encode_cabinet_config(past).empty(),"A rotation off the quarter turns is not written");

    // The older format, and the separate files from before it.
    const auto older=std::filesystem::temp_directory_path()/"alien-crush-config-check";
    std::filesystem::remove_all(older);
    std::filesystem::create_directories(older);
    const auto write=[&](const char* name,std::string_view text) {
        std::ofstream(older/name,std::ios::binary)<<text;
    };
    check(host::load_cabinet_config(older).status==host::ConfigLoadStatus::not_found,"No file is the defaults");
    write("cabinet.ini","# Alien Crush cabinet settings\ngame.cabinet=1\ninput.buttons=ACINPUTS2 \"PinOne\" 0 1 2 3 4 5 6 7 8\n"
        "input.nudge.threshold=30\ninput.plunger.axis=2\ninput.plunger.device=Pin%3DOne\nvideo.smoothing=0\n"
        "window.playfield=\"Left\" 0 0 540 720 1 90\nwindow.backglass=\"Right\" 1920 0 640 360 0 0\n"
        "window.score=\"Right\" 1920 400 640 200 0 180\n");
    auto legacy=host::load_cabinet_config(older);
    check(legacy.loaded() && legacy.legacy && legacy.config.play.cabinet && !legacy.config.play.smoothing &&
        legacy.config.input.buttons.buttons[8]==8 && legacy.config.input.nudge.threshold==30 &&
        legacy.config.input.plunger.device=="Pin=One" && legacy.config.windows[host::playfield_window].fullscreen &&
        legacy.config.windows[host::playfield_window].artwork_rotation==90 &&
        legacy.config.windows[host::score_window].artwork_rotation==180 &&
        legacy.config.windows[host::backglass_window].display_identity=="Right","The older cabinet.ini is read");
    host::save_cabinet_config(older/"cabinet.ini",legacy.config);
    const auto migrated=host::load_cabinet_config(older);
    check(migrated.loaded() && !migrated.legacy && migrated.config==legacy.config,"A save rewrites it in the current format");
    std::filesystem::remove(older/"cabinet.ini");
    write("cabinet-windows.txt","ACWINDOWS 1\nplayfield \"A\" 0 0 540 720 0 0\nbackglass \"A\" 0 0 640 360 0 90\nscore \"A\" 0 0 640 200 0 0\n");
    write("cabinet-inputs.txt","ACINPUTS1 \"Board\" 0 1 2 3 4 5 6 7\n");
    write("cabinet-plunger.txt","ACPLUNGER1 \"PinOne\" 2 -32768 32767 8 25\n");
    legacy=host::load_cabinet_config(older);
    check(legacy.loaded() && legacy.legacy && legacy.config.windows[host::backglass_window].artwork_rotation==90 &&
        legacy.config.input.buttons.device=="Board" && legacy.config.input.plunger.calibration.rest==-32768,
        "The older separate files are read when there is no cabinet.ini");
    write("cabinet.ini","version=1\ngame.cabinet=2\n");
    check(host::load_cabinet_config(older).status==host::ConfigLoadStatus::invalid,"A bad file is reported, not replaced");
    std::filesystem::remove_all(older);

    // The keyboard, as bound.
    std::array<bool,SDL_SCANCODE_COUNT> keys{};
    host::KeyboardInput keyboard;
    keys[SDL_SCANCODE_RETURN]=true;
    check(keyboard.read(keys,true).player_one==Pad::button_i,"A launch key sends I alone from the shooter");
    check(keyboard.read(keys,false).player_one==Pad::button_i,"A held launch stays a launch after the ball leaves");
    keys={};(void)keyboard.read(keys,false);keys[SDL_SCANCODE_RETURN]=true;
    check(keyboard.read(keys,false).player_one==(Pad::button_i|Pad::run),"A fresh press elsewhere is I and Run");
    auto bindings=host::default_input_bindings();
    bindings.launch_keys={"Space"};
    bindings[host::PadButton::two]={"Z","Not a key"};
    host::KeyboardInput custom(bindings);
    keys={};keys[SDL_SCANCODE_SPACE]=true;
    check(custom.read(keys,true).player_one==Pad::button_i && custom.read(keys,false).player_one==Pad::button_i,
        "A launch key bound to nothing else launches");
    keys={};(void)custom.read(keys,false);keys[SDL_SCANCODE_SPACE]=true;
    check(custom.read(keys,false).player_one==0,"...and does nothing outside the shooter");
    keys={};keys[SDL_SCANCODE_Z]=true;
    check(custom.read(keys,false).player_one==Pad::button_ii,"A rebound key sends its input, and a bad name is skipped");
    keys={};keys[SDL_SCANCODE_X]=true;
    check(custom.read(keys,false).player_one==0,"A key no longer bound sends nothing");

    // Flippers on their own inputs, and the runtime identity that carries them.
    const host::FlipperInputs moved{{host::PadButton::two},{host::PadButton::right,host::PadButton::up}};
    std::array<bool,SDL_GAMEPAD_BUTTON_COUNT> pad{};
    pad[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]=true;
    check(host::gamepad_game_input(pad,moved).player_one==Pad::button_ii,"A shoulder sends its flipper's first input");
    check(host::route_game_input({Pad::left|Pad::right,0},false).player_one==(Pad::left|Pad::up),"Default flippers resolve");
    check(host::route_game_input({Pad::left|Pad::right,0},false,{{host::PadButton::left,host::PadButton::down},{host::PadButton::right}}).player_one==
        (Pad::down|Pad::right),"The left flipper moves when it has the alternate");
    check(host::route_game_input({Pad::up|Pad::down,0},false).player_one==Pad::up,"Down gives way to Up");
    check(host::route_game_input({Pad::left|Pad::right|Pad::down,0},false).player_one==(Pad::left|Pad::up),
        "Down gives way to both flippers");
    check(input::flipper_revision_suffix({}).empty(),"Default flipper inputs keep the runtime identity");
    const input::FlipperMasks masks{0x02,0x00};
    check(input::recorded_flipper_masks("beetle-x-controls1"+input::flipper_revision_suffix(masks)+"-nudge=2")==masks,
        "Recorded flipper inputs round trip");
}
