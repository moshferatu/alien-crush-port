// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace alien_crush::assets {
struct WordStream { std::vector<std::uint16_t> words; std::size_t consumed{}; };
// Original E3DD/E43B packet format. A zero packet terminates the stream.
WordStream decode_word_rle(std::span<const std::byte> source,std::size_t max_words);
struct TableBackground {
    std::array<std::uint16_t,32768> vram{};
    std::array<std::uint16_t,512> palette{};
    std::size_t pattern_words{},map_words{},palette_words{};
};
// Lower then upper. Only the supported ROM is accepted; no emulator execution.
std::array<TableBackground,2> decode_table_backgrounds(std::span<const std::byte> rom);
}
