// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <span>
#include <string>
#include <vector>
#include "alien_crush/core/save_state.hpp"
namespace alien_crush::diagnostics {
struct StateField {
    std::string section, name;
    std::span<const std::byte> bytes;
};
// Field views borrow the supplied payload. This parser never calls the emulator.
std::vector<StateField> parse_reference_fields(std::span<const std::byte> payload);
struct ReferenceSnapshot {
    std::uint64_t frame{};
    std::uint16_t pc{};
    std::array<std::uint8_t,8> mpr{};
    std::array<std::uint16_t,32768> vram{};
    std::array<std::uint16_t,512> palette{};
    std::array<std::uint16_t,256> sat{};
    std::uint16_t control{}, memory_width{}, scroll_x{}, scroll_y{};
    int bat_width() const noexcept { const int widths[]{32,64,128,128}; return widths[(memory_width>>4)&3]; }
    int bat_height() const noexcept { return (memory_width&0x40)?64:32; }
};
ReferenceSnapshot decode_reference_snapshot(const state::SaveStateContainer& state);
}
