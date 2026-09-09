// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include <vector>
#include <string>
namespace alien_crush::recording {
struct Recording {
    state::SaveStateContainer initial;
    std::vector<PadInput> inputs;
    // Empty for pad-only recordings; otherwise one command per input frame.
    // 0: no change, 1..100: draw percentage, 101: cancel. Release uses pad bits.
    std::vector<std::uint8_t> plunger;
    std::uint8_t plunger_at(std::size_t frame) const { return plunger.empty()?0:plunger.at(frame); }
};
// Apply the recorded bridge operation before advancing, failing on gate mismatch.
Frame advance(ReferenceRuntime&, PadInput, std::uint8_t plunger = 0);
void write(const std::filesystem::path&, const Recording&);
Recording read(const std::filesystem::path&);
PadInput canonical_input(std::uint64_t frame);
std::string fingerprint(const Frame&, std::span<const std::byte> ram);
}
