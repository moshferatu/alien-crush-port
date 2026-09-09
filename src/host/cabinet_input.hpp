// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace alien_crush::host {
struct CabinetButtons {
    std::string device;
    // left flipper, right flipper, nudge, start/launch, select, up, down,
    // right/menu, dedicated launch/confirm (ninth entry in ACINPUTS2).
    std::array<int,9> buttons{-1,-1,-1,-1,-1,-1,-1,-1,-1};
};
CabinetButtons parse_cabinet_buttons(std::string_view text);
CabinetButtons load_cabinet_buttons(const std::filesystem::path& path);
class CabinetInput {
    CabinetButtons binding_;
    SDL_Joystick* joystick_{};
    bool armed_{};
public:
    explicit CabinetInput(CabinetButtons binding);
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
