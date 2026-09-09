// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <optional>
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
std::array<std::uint16_t,512> decode_table_palette(std::span<const std::byte> rom,unsigned table);
struct TableIndicator {
    unsigned table{},address{},width{},height{};
    std::uint16_t enabled_palette{};
};
// 03:9357: the 29 persistent indicator rectangles restored by 03:9415.
std::array<TableIndicator,29> decode_table_indicators(std::span<const std::byte> rom);
// Reapply set bits in RAM D6..D9, preserving tile numbers. This restores the
// persistent lit state, not the transient blinking phase of the active table.
void restore_table_indicators(std::span<std::uint16_t> bat,
    std::span<const TableIndicator> indicators,unsigned table,std::span<const std::byte> ram);
using LowerCounterTiles=std::array<std::array<std::uint16_t,6>,7>;
// 03:94F6..9564 draws a 3x2 rectangle at BAT 022D using RAM 7F (1..7).
LowerCounterTiles decode_lower_counter_tiles(std::span<const std::byte> rom);
// Presentation restoration only. Unknown values leave the BAT untouched.
bool restore_lower_counter(std::span<std::uint16_t> bat,const LowerCounterTiles&,
    unsigned table,std::span<const std::byte> ram);
struct SpriteStream {
    std::size_t rom_offset{},compressed_bytes{},vram_word{},word_count{};
};
struct TableSprites {
    std::array<std::uint16_t,32768> vram{};
    // Shared, level-selected, then level-local, in original load order.
    std::array<SpriteStream,3> streams{};
};
struct HudAssets {
    std::vector<std::uint16_t> patterns;
    SpriteStream stream;
};
// Standard main-table digits, selected by 00:EDD2 when RAM $76 is zero.
HudAssets decode_hud_assets(std::span<const std::byte> rom);
// One monotonic fade-in step from 00:E4B7..E5CC. Threshold descends 21..1.
std::array<std::uint16_t,512> advance_palette_fade(std::span<const std::uint16_t> current,
    std::span<const std::uint16_t> target,unsigned threshold);
struct PaletteFadeOut {
    std::array<std::uint16_t,512> colors{};
    unsigned already_black{};
};
// 00:E605..E6AA counts colors already black before decrementing all channels.
PaletteFadeOut advance_palette_fade_out(std::span<const std::uint16_t> current);
struct PalettePulseState { std::uint8_t flags{},green{},red{},blue{}; };
struct PalettePulseStep {
    PalettePulseState state;
    std::optional<std::uint16_t> color_zero;
};
// One 00:F64B invocation. Preserves original byte arithmetic and packed VCE bits.
PalettePulseStep advance_palette_pulse(PalettePulseState state);
// Initial sprite pattern memory only; actor placement and animation are separate.
std::array<TableSprites,2> decode_table_sprites(std::span<const std::byte> rom);
}
