// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cabinet_config.hpp"
#include "launch_input.hpp"
#include "alien_crush/input/joystick_input.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <functional>
#include <iostream>
namespace alien_crush::host {
// Scancodes resolved once from the key names in alien-crush.ini, so reading the
// keyboard every frame is an array lookup. A pad input's list may also name
// controller inputs ("PinOne Button 3", "Hat 1 Left"); those are split out
// into `pad_joystick`.
struct ResolvedKeys {
    std::array<std::vector<SDL_Scancode>,kPadButtonCount> pads;
    std::vector<SDL_Scancode> launch;
    std::array<std::vector<input::JoystickInput>,kPadButtonCount> pad_joystick;
    std::vector<input::JoystickInput> launch_joystick;
};
// `controls` collects the controller inputs in the list. Without it -- the
// host keys, which are read as key presses rather than held -- a controller
// input is reported and skipped.
inline std::vector<SDL_Scancode> resolve_key_names(const std::vector<std::string>& names,std::string_view field,
    std::vector<input::JoystickInput>* controls=nullptr) {
    std::vector<SDL_Scancode> resolved;
    for(const auto& name:names) {
        if(auto control=input::parse_joystick_input(name)) {
            if(controls) controls->push_back(std::move(*control));
            else std::cerr<<"Controller input \""<<name<<"\" bound to input."<<field<<"; only pad inputs take controller inputs, so ignoring it.\n";
            continue;
        }
        const auto scancode=SDL_GetScancodeFromName(name.c_str());
        // A name a hand-edited file gets wrong is named and skipped, and the
        // rest of the input's keys still work.
        if(scancode==SDL_SCANCODE_UNKNOWN) std::cerr<<"Unknown key name \""<<name<<"\" bound to input."<<field<<"; ignoring it.\n";
        else resolved.push_back(scancode);
    }
    return resolved;
}
inline ResolvedKeys resolve_keys(const InputBindings& bindings) {
    ResolvedKeys resolved;
    for(std::size_t i=0;i<kPadButtonCount;++i)
        resolved.pads[i]=resolve_key_names(bindings.keys[i],pad_button_field(static_cast<PadButton>(i)),&resolved.pad_joystick[i]);
    resolved.launch=resolved.pads[static_cast<std::size_t>(PadButton::one)];
    resolved.launch_joystick=resolved.pad_joystick[static_cast<std::size_t>(PadButton::one)];
    return resolved;
}

// The keys the game itself answers to, outside the pad.
struct HostKeys {
    std::vector<SDL_Scancode> smoothing,next_window,fullscreen,next_monitor;
    static bool has(const std::vector<SDL_Scancode>& keys,SDL_Scancode key) {return std::ranges::find(keys,key)!=keys.end();}
};
inline HostKeys resolve_host_keys(const InputBindings& bindings) {
    return {resolve_key_names(bindings.smoothing_keys,"smoothing"),resolve_key_names(bindings.next_window_keys,"next_window"),
        resolve_key_names(bindings.fullscreen_keys,"fullscreen"),resolve_key_names(bindings.next_monitor_keys,"next_monitor")};
}

// Whether a bound controller input is held.
using ButtonHeld=std::function<bool(const input::JoystickInput&)>;

// The keyboard, as alien-crush.ini binds it, and any controller inputs bound
// beside the keys. While the ball is seated in the shooter a key bound to I
// sends I and nothing else -- its other inputs are suppressed, so a key that
// is I and Run launches rather than pausing -- and a press that began as a
// launch stays one until it is let go, so the ball leaving the shooter cannot
// turn the held key into a pause. A controller input bound to I is a launch
// key on the same terms.
class KeyboardInput {
    ResolvedKeys keys_;
    bool launching_{};
public:
    KeyboardInput():KeyboardInput(default_input_bindings()) {}
    explicit KeyboardInput(const InputBindings& bindings):keys_(resolve_keys(bindings)) {}
    bool uses_buttons() const {return std::ranges::any_of(keys_.pad_joystick,[](const auto& b){return !b.empty();});}
    // Whether a binding names this controller. Such a controller is read only
    // through its bindings, never the fixed gamepad layout: a cabinet board or
    // an arcade stick can be a mapped gamepad too, and its inputs would
    // otherwise send both.
    bool names_controller(const char* name) const {
        return name && std::ranges::any_of(keys_.pad_joystick,[name](const auto& list){
            return std::ranges::any_of(list,[name](const input::JoystickInput& b){
                return !b.device.empty() && SDL_strcasestr(name,b.device.c_str());});});
    }
    PadInput read(std::span<const bool> keys,bool in_shooter,const ButtonHeld& button_held={}) {
        const auto held=[&](SDL_Scancode k){return static_cast<std::size_t>(k)<keys.size() && keys[k];};
        const auto button=[&](const input::JoystickInput& b){return button_held && button_held(b);};
        const bool launch_held=std::ranges::any_of(keys_.launch,held) || std::ranges::any_of(keys_.launch_joystick,button);
        launching_=launch_held && (launching_ || in_shooter);
        const auto suppressed=[&](SDL_Scancode k){return launching_ && std::ranges::find(keys_.launch,k)!=keys_.launch.end();};
        const auto button_suppressed=[&](const input::JoystickInput& b){
            return launching_ && std::ranges::find(keys_.launch_joystick,b)!=keys_.launch_joystick.end();};
        PadInput input;
        for(std::size_t i=0;i<kPadButtonCount;++i)
            if(std::ranges::any_of(keys_.pads[i],[&](SDL_Scancode k){return held(k) && !suppressed(k);})
                || std::ranges::any_of(keys_.pad_joystick[i],[&](const input::JoystickInput& b){return button(b) && !button_suppressed(b);}))
                input.player_one|=pad_button_bit(static_cast<PadButton>(i));
        if(launching_) input.player_one|=Pad::button_i;
        return input;
    }
};

// The controllers whose buttons, hats and axes a pad input may be bound to.
// Read through SDL's joystick layer rather than its gamepad layer, as the
// plunger and nudge are: a pinball board in its own controller mode is an
// anonymous HID device that no gamepad mapping covers. Every connected
// controller is held open, but only while some pad input names a controller
// input.
class JoystickInputs {
    struct Device {SDL_Joystick* joystick{};SDL_JoystickID id{};std::string name;};
    std::vector<Device> devices_;
    bool wanted_{};
public:
    explicit JoystickInputs(bool wanted):wanted_(wanted) {refresh();}
    ~JoystickInputs() {for(auto& d:devices_) SDL_CloseJoystick(d.joystick);}
    JoystickInputs(const JoystickInputs&)=delete;
    JoystickInputs& operator=(const JoystickInputs&)=delete;
    void refresh() {
        if(!wanted_) return;
        int count{};
        auto* ids=SDL_GetJoysticks(&count);
        if(!ids) return;
        for(int i=0;i<count;++i) {
            const auto id=ids[i];
            if(std::ranges::any_of(devices_,[id](const Device& d){return d.id==id;})) continue;
            auto* joystick=SDL_OpenJoystick(id);
            if(!joystick) {std::cerr<<"SDL_OpenJoystick failed: "<<SDL_GetError()<<'\n';continue;}
            const char* name=SDL_GetJoystickName(joystick);
            devices_.push_back({joystick,id,name?name:""});
            std::cout<<"Controller inputs: "<<devices_.back().name<<" ("<<SDL_GetNumJoystickButtons(joystick)<<" buttons, "
                <<SDL_GetNumJoystickHats(joystick)<<" hats, "<<SDL_GetNumJoystickAxes(joystick)<<" axes)\n";
        }
        SDL_free(ids);
    }
    void remove(SDL_JoystickID id) {
        const auto found=std::ranges::find_if(devices_,[id](const Device& d){return d.id==id;});
        if(found==devices_.end()) return;
        SDL_CloseJoystick(found->joystick);
        devices_.erase(found);
    }
    bool pressed(const input::JoystickInput& b) const {
        return std::ranges::any_of(devices_,[&](const Device& d){
            if(!b.device.empty() && !SDL_strcasestr(d.name.c_str(),b.device.c_str())) return false;
            switch(b.kind) {
            case input::JoystickInput::Kind::button:
                return b.index<SDL_GetNumJoystickButtons(d.joystick) && SDL_GetJoystickButton(d.joystick,b.index);
            case input::JoystickInput::Kind::hat:
                return b.index<SDL_GetNumJoystickHats(d.joystick) && (SDL_GetJoystickHat(d.joystick,b.index)&b.direction)!=0;
            case input::JoystickInput::Kind::axis:
                return b.index<SDL_GetNumJoystickAxes(d.joystick) && input::axis_pressed(b.direction,SDL_GetJoystickAxis(d.joystick,b.index));
            }
            return false;});
    }
};

// The first SDL gamepad: shoulders or the d-pad's sides work the flippers,
// South is I, East is II (nudge), Start is Run and Back is Select.
inline PadInput gamepad_game_input(std::span<const bool> buttons,const FlipperInputs& flippers={}) {
    auto button=[&](SDL_GamepadButton b){return static_cast<std::size_t>(b)<buttons.size() && buttons[b];};
    PadInput input;
    if(button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) input.player_one|=flipper_pad(flippers.left,false);
    if(button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) input.player_one|=flipper_pad(flippers.right,true);
    if(button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) input.player_one|=Pad::left;
    if(button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) input.player_one|=Pad::right;
    if(button(SDL_GAMEPAD_BUTTON_DPAD_UP)) input.player_one|=Pad::up;
    if(button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) input.player_one|=Pad::down;
    if(button(SDL_GAMEPAD_BUTTON_SOUTH)) input.player_one|=Pad::button_i;
    if(button(SDL_GAMEPAD_BUTTON_EAST)) input.player_one|=Pad::button_ii;
    if(button(SDL_GAMEPAD_BUTTON_START)) input.player_one|=Pad::run;
    if(button(SDL_GAMEPAD_BUTTON_BACK)) input.player_one|=Pad::select;
    return input;
}
// The default keys and the gamepad together, outside the shooter.
inline PadInput device_game_input(std::span<const bool> keys,std::span<const bool> buttons) {
    KeyboardInput keyboard;
    auto input=keyboard.read(keys,false);
    input.player_one|=gamepad_game_input(buttons).player_one;
    return input;
}
}
