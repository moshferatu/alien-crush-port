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
    bool title_menu{}; // Presentation only: complete saved outer title-call signature.
    bool title_transition{}; // Score-exit graphics reload and title reset; suppress gameplay presentation.
    bool demo_play{}; // Original nonzero RAM 76 demo selector; title takes precedence.
    std::uint8_t bonus_room{}; // 1..4 for table code 2; zero outside known bonus rooms.
    bool bonus_round_over{}; // FDDF exits bonus loop on RAM 78 bit 0.
    bool bonus_cleared{}; // Results routine 1F:9736 sets RAM 85 bit 0 after evaluating targets.
    bool game_over{}; // 01:410F sets RAM 78 bit 1 after the final ordinary ball is consumed.
    bool match_visible{}; // Type 43 actions 2..4 own the last-chance digit sprites.
    std::uint8_t match_digit{}; // Hundreds digit; the two trailing digits are zero.
    bool high_score_menu{}; // Verified pause viewer and practice game-end score calls; presentation only.
};
// Diagnostics/presentation only: not a gameplay-validity or bridge-write gate.
AlienState observe_alien_state(std::span<const std::byte> ram);
}
