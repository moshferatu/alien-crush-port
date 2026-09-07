// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include <vector>
#include <string>
namespace alien_crush::recording {
struct Recording {
    state::SaveStateContainer initial;
    std::vector<PadInput> inputs;
};
void write(const std::filesystem::path&, const Recording&);
Recording read(const std::filesystem::path&);
PadInput canonical_input(std::uint64_t frame);
std::string fingerprint(const Frame&, std::span<const std::byte> ram);
}
