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
struct SpriteStream {
    std::size_t rom_offset{},compressed_bytes{},vram_word{},word_count{};
};
struct TableSprites {
    std::array<std::uint16_t,32768> vram{};
    // Shared, level-selected, then level-local, in original load order.
    std::array<SpriteStream,3> streams{};
};
// Initial sprite pattern memory only; actor placement and animation are separate.
std::array<TableSprites,2> decode_table_sprites(std::span<const std::byte> rom);
}
