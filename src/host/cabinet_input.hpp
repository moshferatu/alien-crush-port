// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include "pad_bindings.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace alien_crush::host {
struct CabinetButtons {
    std::string device;
    // left flipper, right flipper, nudge, start/launch, select, up, down,
    // right/menu, dedicated launch/confirm (ninth entry in ACINPUTS2). cabinet.ini has them as
    // input.buttons.*; the older ACINPUTS1/2 line is still read.
    std::array<int,9> buttons{-1,-1,-1,-1,-1,-1,-1,-1,-1};
    bool operator==(const CabinetButtons&) const=default;
};
CabinetButtons parse_cabinet_buttons(std::string_view text);
CabinetButtons load_cabinet_buttons(const std::filesystem::path& path);
class CabinetInput {
    CabinetButtons binding_;
    std::array<std::uint16_t,9> masks_;
    SDL_Joystick* joystick_{};
    bool armed_{};
public:
    // The flipper buttons send what the flippers' input lists name first.
    explicit CabinetInput(CabinetButtons binding,const FlipperInputs& flippers={});
    ~CabinetInput();
    CabinetInput(const CabinetInput&)=delete;
    CabinetInput& operator=(const CabinetInput&)=delete;
    void refresh();
    void rearm() { armed_=false; }
    SDL_JoystickID id() const;
    PadInput read();
};
// Standalone hardware inspection; never loads a ROM or changes configuration.
int input_probe(std::uint64_t samples);
}
