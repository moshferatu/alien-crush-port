// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cabinet_settings.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <SDL3/SDL.h>

namespace alien_crush::host {
struct NudgeBinding {
    // Off until the player turns it on: most players have no nudge board.
    bool enabled=false;
    std::string device="PinOne";
    int axis_x=0,axis_y=1;
    int threshold=20,release=8,cooldown_ms=250;
    bool operator==(const NudgeBinding&) const=default;
};
// The older settings format's nudge lines; see cabinet_config.hpp for the current file.
NudgeBinding parse_cabinet_nudge(const CabinetSettings& settings);
// Throws when the axes, thresholds or cooldown cannot work together.
void validate_cabinet_nudge(const NudgeBinding&);
// The original game has one digital nudge action, regardless of direction.
// Only accept physical motion during live, unpaused play.
inline bool nudge_play_active(std::span<const std::byte> ram) {
    if(ram.size()!=8192) return false;
    const auto state=diagnostics::observe_alien_state(ram);
    return !state.title_menu && !state.title_transition && !state.high_score_menu &&
        !state.demo_play && !state.game_over && (std::to_integer<unsigned>(ram[0xf6])&2)==0 &&
        ((std::to_integer<unsigned>(ram[0x53])|std::to_integer<unsigned>(ram[0x54]))&0x80)==0;
}
class CabinetNudge {
    NudgeBinding binding_;
    SDL_Joystick* joystick_{};
    bool armed_{},quiet_{};
    Uint64 quiet_since_{},pulse_until_{},cooldown_until_{};
public:
    explicit CabinetNudge(NudgeBinding binding);
    ~CabinetNudge();
    CabinetNudge(const CabinetNudge&)=delete;
    CabinetNudge& operator=(const CabinetNudge&)=delete;
    void refresh();
    void rearm();
    SDL_JoystickID id() const;
    // Caller supplies monotonic wall time; catch-up frames cannot shorten debounce.
    bool read(Uint64 now,bool active);
};
}
