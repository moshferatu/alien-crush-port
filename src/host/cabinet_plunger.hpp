// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/input/plunger.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#include <SDL3/SDL.h>
#include <string_view>

namespace alien_crush::host {
input::PlungerBinding parse_cabinet_plunger(std::string_view);
input::PlungerBinding parse_cabinet_plunger_ini(std::string_view);
input::PlungerBinding load_cabinet_plunger(const std::filesystem::path&);
int plunger_probe();
class CabinetPlunger {
    input::PlungerBinding binding_;
    input::Plunger detector_;
    SDL_Joystick* joystick_{};
    bool owned_{}, interrupted_{}, blocked_{};
public:
    explicit CabinetPlunger(input::PlungerBinding);
    ~CabinetPlunger();
    CabinetPlunger(const CabinetPlunger&)=delete;
    CabinetPlunger& operator=(const CabinetPlunger&)=delete;
    SDL_JoystickID id() const;
    bool owns_draw() const {return owned_;}
    void refresh();
    // Cancellation is applied before the next emulated frame, including recording.
    void interrupt();
    // A restored state replaces the old draw; never cancel using stale ownership.
    void restored();
    // Mutates the runtime and pad; returns the accepted command for recording.
    std::uint8_t apply(ReferenceRuntime&, PadInput&, bool focused);
};
}
