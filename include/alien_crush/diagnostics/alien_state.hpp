// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
namespace alien_crush::diagnostics {
struct AlienState {
    std::uint32_t score_points{};
    std::array<std::uint32_t,5> high_scores{};
    std::uint8_t table_code{},ball_counter{};
    bool score_update_pending{};
};
// Diagnostics only: these fields are not a scene-validity or bridge-write gate.
AlienState observe_alien_state(std::span<const std::byte> ram);
}
