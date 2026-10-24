// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cabinet_config.hpp"
#include "game_input.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/data_files.hpp"
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
    chosen.play.borderless=true;
    chosen.windows[host::playfield_window]={"Display = 1%",{-1920,40,1080,1920},host::DisplayRotation::portrait,true,90};
    chosen.windows[host::score_window].artwork_rotation=270;
    chosen.input[host::PadButton::run]={"Space","Keypad Enter"};
    chosen.input[host::PadButton::select]={};
    chosen.input.smoothing_keys={"F7"};
    chosen.input.next_window_keys={"Tab","F10"};
    chosen.input.fullscreen_keys={};
    chosen.input.next_monitor_keys={"F12"};
    chosen.input.nudge={false,"Clev, Soft",2,3,30,10,500};
    chosen.input.plunger.device="PinOne Board%";
    chosen.input.plunger.axis=2;
    chosen.input.plunger.calibration={1000,-1000,12,30};
    const auto encoded=host::encode_cabinet_config(chosen);
    check(!encoded.empty() && decode(text_of(encoded))==chosen,"Every setting round trips through alien-crush.ini");
    const auto written=text_of(encoded);
    for(const auto* line:{"version=1\n","input.nudge_multiplier=2.5\n","input.plunger.device=PinOne Board%25\n",
        "input.run=Space,Keypad Enter\n","input.select=\n",
        "playfield.display=Display %3D 1%25\n","playfield.rotation=3\n","dmd.artwork_rotation=270\n",
        "audio.music_voice_overlay=0\n","input.nudge.device=Clev%2C Soft\n",
        "input.smoothing=F7\n","input.next_window=Tab,F10\n","input.fullscreen=\n","input.next_monitor=F12\n"})
        check(written.find(line)!=std::string::npos,"Written in Devil's Crush's form");
    check(written.find("input.buttons.")==std::string::npos,"No raw board buttons are written");
    check(decode("version=1\ninput.buttons.device=PinOne\ninput.buttons.launch=256\ninput.buttons.jump=1\n")==decode("version=1\n"),
        "The old raw board-button settings are ignored");
    check(written.find("game.rom=D:\\Games\\Alien Crush %28USA%29 %C3%A9.pce\n")!=std::string::npos,"A path is escaped byte by byte");

    // Devil's Crush's own file: its keys read the same, and what it has that
    // this game does not is ignored.
    const auto shared=decode("version=1\ninput.one=Return\ninput.launch=Return,Space\ngame.bonus_stage=3\ngame.native=1\n"
        "input.plunger.axis=2\nplayfield.artwork_rotation=90\n");
    check(shared.input[host::PadButton::one]==std::vector<std::string>{"Return"} && shared.input.plunger.axis==2 &&
        shared.windows[host::playfield_window].artwork_rotation==90,"A Devil's Crush alien-crush.ini reads here");
    check(decode("version=1\ngame.near_max_score=1\n").play.near_max_score && !decode("version=1\n").play.near_max_score,
        "A start near the maximum score is read, and off without it");
    check(decode("version=1\ngame.drag_ball=1\n").play.drag_ball && !decode("version=1\n").play.drag_ball,
        "Carrying the ball with the mouse is read, and off without it");
    {
        const auto defaults=decode("version=1\n").input;
        check(defaults.smoothing_keys==std::vector<std::string>{"F6"} && defaults.next_window_keys==std::vector<std::string>{"F10"} &&
            defaults.fullscreen_keys==std::vector<std::string>{"F11"} && defaults.next_monitor_keys==std::vector<std::string>{"F12"},
            "The window keys default to F6, F10, F11 and F12");
        check(decode("version=1\ninput.next_window=\n").input.next_window_keys.empty(),"An empty window key is unbound");
    }
    check(decode("version=1\ninput.one=A, A ,B\n").input[host::PadButton::one]==std::vector<std::string>{"A","B"},
        "A key bound twice to one input is kept once");

    for(const auto* bad:{"", "input.one=A\n", "version=2\n", "version=1\nversion=1\n", "version=1\n# comment\n",
        "version=1\ngame.cabinet=2\n", "version=1\ngame.bonus_stage=5\n", "version=1\ngame.bonus_stage=\n", "version=1\ngame.near_max_score=2\n", "version=1\ngame.drag_ball=yes\n", "version=1\nvideo.smoothing=off\n", "version=1\nvideo.borderless=on\n", "version=1\naudio.music_voice_overlay=yes\n",
        "version=1\ninput.nudge_multiplier=17\n", "version=1\ninput.nudge_multiplier=2x\n",
        "version=1\nplayfield.artwork_rotation=45\n",
        "version=1\ninput.nudge.typo=1\n", "version=1\ninput.nudge.axis_y=0\n", "version=1\ninput.plunger.release=101\n",
        "version=1\ninput.plunger.device=Bad%G1\n",
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
    write("alien-crush.ini","# Alien Crush cabinet settings\ngame.cabinet=1\ninput.buttons=ACINPUTS2 \"PinOne\" 0 1 2 3 4 5 6 7 8\n"
        "input.nudge.threshold=30\ninput.plunger.axis=2\ninput.plunger.device=Pin%3DOne\nvideo.smoothing=0\n"
        "window.playfield=\"Left\" 0 0 540 720 1 90\nwindow.backglass=\"Right\" 1920 0 640 360 0 0\n"
        "window.score=\"Right\" 1920 400 640 200 0 180\n");
    auto legacy=host::load_cabinet_config(older);
    check(legacy.loaded() && legacy.legacy && legacy.config.play.cabinet && !legacy.config.play.smoothing &&
        legacy.config.input.nudge.threshold==30 &&
        legacy.config.input.plunger.device=="Pin=One" && legacy.config.windows[host::playfield_window].fullscreen &&
        legacy.config.windows[host::playfield_window].artwork_rotation==90 &&
        legacy.config.windows[host::score_window].artwork_rotation==180 &&
        legacy.config.windows[host::backglass_window].display_identity=="Right","The older alien-crush.ini is read");
    host::save_cabinet_config(older/"alien-crush.ini",legacy.config);
    const auto migrated=host::load_cabinet_config(older);
    check(migrated.loaded() && !migrated.legacy && migrated.config==legacy.config,"A save rewrites it in the current format");
    std::filesystem::remove(older/"alien-crush.ini");
    write("cabinet-windows.txt","ACWINDOWS 1\nplayfield \"A\" 0 0 540 720 0 0\nbackglass \"A\" 0 0 640 360 0 90\nscore \"A\" 0 0 640 200 0 0\n");
    write("cabinet-inputs.txt","ACINPUTS1 \"Board\" 256\n");
    write("cabinet-plunger.txt","ACPLUNGER1 \"PinOne\" 2 -32768 32767 8 25\n");
    legacy=host::load_cabinet_config(older);
    check(legacy.loaded() && legacy.legacy && legacy.config.windows[host::backglass_window].artwork_rotation==90 &&
        legacy.config.input.plunger.calibration.rest==-32768,
        "The older separate files are read when there is no alien-crush.ini, except the board's buttons");
    write("alien-crush.ini","version=1\ngame.cabinet=2\n");
    check(host::load_cabinet_config(older).status==host::ConfigLoadStatus::invalid,"A bad file is reported, not replaced");
    std::filesystem::remove_all(older);

    // An earlier build's subfolder, with the settings as cabinet.ini.
    {
        namespace fs=std::filesystem;
        const auto base=fs::temp_directory_path()/"alien-crush-data-migration-check";
        fs::remove_all(base);
        const auto put=[](const fs::path& path,const char* text) {std::ofstream(path,std::ios::binary)<<text;};
        const auto get=[](const fs::path& path) {
            std::ifstream input(path,std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input),{});
        };
        fs::create_directories(base/"portable-data");
        put(base/"portable-data"/"cabinet.ini","settings");
        put(base/"portable-data"/"alien-crush.hiscore","scores");
        put(base/"portable-data"/"alien-crush.cabinet-controls.state","state");
        put(base/"portable-data"/"backglass.bmp","art");
        auto result=core::migrate_legacy_data_folder(base);
        check(result.moved.size()==4 && result.problems.empty() && get(base/"alien-crush.ini")=="settings" &&
            get(base/"alien-crush.hiscore")=="scores" && get(base/"alien-crush.cabinet-controls.state")=="state" &&
            get(base/"backglass.bmp")=="art","An earlier build's settings and saves move out of portable-data");
        check(!fs::exists(base/"portable-data"),"The emptied portable-data folder is removed");
        result=core::migrate_legacy_data_folder(base);
        check(result.moved.empty() && result.problems.empty(),"Migrating again without the folder does nothing");
        // A file at the new name was written by a build that has already
        // moved, so it wins, and the old one and its folder stay.
        fs::create_directories(base/"portable-data");
        put(base/"portable-data"/"cabinet.ini","older");
        put(base/"portable-data"/"notes.txt","mine");
        result=core::migrate_legacy_data_folder(base);
        check(result.moved.empty() && result.problems.size()==1 && get(base/"alien-crush.ini")=="settings" &&
            fs::exists(base/"portable-data"/"cabinet.ini") && fs::exists(base/"portable-data"/"notes.txt"),
            "A newer file is never replaced, and other files stay");
        fs::remove_all(base);
    }

    // The keyboard, as bound.
    std::array<bool,SDL_SCANCODE_COUNT> keys{};
    host::KeyboardInput keyboard;
    keys[SDL_SCANCODE_RETURN]=true;
    check(keyboard.read(keys,true).player_one==Pad::button_i,"A key bound to I sends I alone from the shooter");
    check(keyboard.read(keys,false).player_one==Pad::button_i,"A held launch stays a launch after the ball leaves");
    keys={};(void)keyboard.read(keys,false);keys[SDL_SCANCODE_RETURN]=true;
    check(keyboard.read(keys,false).player_one==(Pad::button_i|Pad::run),"A fresh press elsewhere is I and Run");
    auto bindings=host::default_input_bindings();
    bindings[host::PadButton::one]={"Space"};
    bindings[host::PadButton::two]={"Z","Not a key"};
    host::KeyboardInput custom(bindings);
    keys={};keys[SDL_SCANCODE_SPACE]=true;
    check(custom.read(keys,true).player_one==Pad::button_i && custom.read(keys,false).player_one==Pad::button_i,
        "A key rebound to I launches");
    keys={};keys[SDL_SCANCODE_RETURN]=true;
    check(custom.read(keys,true).player_one==Pad::run,"...and a key no longer on I does not");
    keys={};keys[SDL_SCANCODE_Z]=true;
    check(custom.read(keys,false).player_one==Pad::button_ii,"A rebound key sends its input, and a bad name is skipped");
    keys={};keys[SDL_SCANCODE_X]=true;
    check(custom.read(keys,false).player_one==0,"A key no longer bound sends nothing");
    check(decode("version=1\ninput.launch=Space\ninput.left_flipper=down\n").input==host::default_input_bindings(),"The old launch and flipper settings are ignored");

    // Controller inputs, held beside the keys as "PinOne Button 3",
    // "Pad Hat 1 Left" or "Pad Axis 5+".
    using input::JoystickInput;
    using Kind=JoystickInput::Kind;
    check(input::parse_joystick_input("PinOne Button 3")==JoystickInput{"PinOne",Kind::button,2,0},"A named controller button is read, numbered from 1");
    check(input::parse_joystick_input("  pinone   BUTTON 1 ")==JoystickInput{"pinone",Kind::button,0,0},"Controller button names ignore case and spacing");
    check(input::parse_joystick_input("Button 128")==JoystickInput{"",Kind::button,127,0},"A button with no controller name matches any controller");
    check(input::parse_joystick_input("Xbox Controller Hat 1 Left")==JoystickInput{"Xbox Controller",Kind::hat,0,input::kHatLeft} &&
        input::parse_joystick_input("hat 2 up")==JoystickInput{"",Kind::hat,1,input::kHatUp},"A hat direction is read");
    check(input::parse_joystick_input("DualSense Edge Axis 5+")==JoystickInput{"DualSense Edge",Kind::axis,4,1} &&
        input::parse_joystick_input("Axis 1-")==JoystickInput{"",Kind::axis,0,-1},"An axis direction is read");
    for(const auto& in:{JoystickInput{"PinOne",Kind::button,2,0},JoystickInput{"",Kind::button,0,0},JoystickInput{"Pad",Kind::hat,0,input::kHatDown},
        JoystickInput{"Pad",Kind::hat,3,input::kHatRight},JoystickInput{"Pad (v2)",Kind::axis,4,1},JoystickInput{"",Kind::axis,1,-1}})
        check(input::parse_joystick_input(input::format_joystick_input(in))==in,"Controller inputs are written as they are read");
    check(input::format_joystick_input({"PinOne",Kind::button,2,0})=="PinOne Button 3" &&
        input::format_joystick_input({"",Kind::hat,0,input::kHatLeft})=="Hat 1 Left" &&
        input::format_joystick_input({"Pad",Kind::axis,4,1})=="Pad Axis 5+","Controller inputs are written in the documented form");
    for(const auto* text:{"Left Shift","Return","Keypad 1","Keypad +","Keypad -","Page Up","Up","Button","Button 0","Button 129","Button3",
        "PinOneButton 3","Button -1","Hat 1","Hat Up","Hat 0 Up","Hat 1 Sideways","Axis 1","Axis+","Axis 0+","PadAxis 1+","F12",""})
        check(!input::parse_joystick_input(text),"A key name is not a controller input");
    check(input::axis_pressed(1,20000) && !input::axis_pressed(1,10000) && !input::axis_pressed(1,-32768) &&
        input::axis_pressed(-1,-20000) && !input::axis_pressed(-1,20000),"An axis is pressed past half way in its direction");
    auto with_buttons=chosen;
    with_buttons.input[host::PadButton::left]={"Left Shift","PinOne (v2), rev B Button 3"};
    with_buttons.input[host::PadButton::one]={"Return","Button 12","Xbox Controller Hat 1 Left","DualSense Axis 5+"};
    check(decode(text_of(host::encode_cabinet_config(with_buttons)))==with_buttons,"Controller inputs round trip through alien-crush.ini");
    auto board=host::default_input_bindings();
    board[host::PadButton::one]={"Return","PinOne Button 3"};
    board[host::PadButton::run]={"Return","PinOne Button 3"};
    board[host::PadButton::left]={"Left Shift","PinOne Button 1","Stick Hat 1 Left"};
    board[host::PadButton::right]={"Right Shift","Stick Axis 6+"};
    host::KeyboardInput boarded(board);
    check(boarded.uses_buttons() && !keyboard.uses_buttons(),"Only bindings that name a controller input open controllers");
    check(boarded.names_controller("Cleveland Software Design PinOne") && !boarded.names_controller("Xbox Controller"),
        "A controller named by a binding leaves the fixed gamepad layout");
    std::vector<JoystickInput> down;
    const host::ButtonHeld held=[&down](const JoystickInput& b){return std::ranges::find(down,b)!=down.end();};
    keys={};
    down={{"PinOne",Kind::button,0,0}};
    check(boarded.read(keys,false,held).player_one==Pad::left,"A bound controller button sends its input");
    down={{"Stick",Kind::hat,0,input::kHatLeft}};
    check(boarded.read(keys,false,held).player_one==Pad::left,"A bound hat direction sends its input");
    down={{"Stick",Kind::axis,5,1}};
    check(boarded.read(keys,false,held).player_one==Pad::right,"A bound axis sends its input");
    down={{"PinOne",Kind::button,2,0}};
    check(boarded.read(keys,true,held).player_one==Pad::button_i,"A controller button bound to I launches, and nothing else, from the shooter");
    down={};(void)boarded.read(keys,false,held);down={{"PinOne",Kind::button,2,0}};
    check(boarded.read(keys,false,held).player_one==(Pad::button_i|Pad::run),"...and is I and Run elsewhere");

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
