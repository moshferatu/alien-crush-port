// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <algorithm>
#include <stdexcept>
namespace alien_crush::assets {
WordStream decode_word_rle(std::span<const std::byte> source,std::size_t max_words) {
    WordStream result;
    auto byte=[&]() {if(result.consumed==source.size()) throw std::runtime_error("Truncated Alien Crush RLE stream"); return std::to_integer<unsigned>(source[result.consumed++]);};
    auto word=[&]() {const auto low=byte(); return static_cast<std::uint16_t>(low|(byte()<<8));};
    while(true) {
        const auto packet=byte(); if(!packet) break;
        auto count=packet&127; if(!count) count=256; // DEX from zero wraps on the HuC6280.
        if(count>max_words-result.words.size()) throw std::runtime_error("Alien Crush RLE exceeds destination");
        if(packet&128) {const auto value=word(); result.words.insert(result.words.end(),count,value);}
        else for(unsigned i=0;i<count;++i) result.words.push_back(word());
    }
    return result;
}
std::array<TableBackground,2> decode_table_backgrounds(std::span<const std::byte> image) {
    if(!rom::matches_supported_rom(image.size(),rom::compute_digests(image))) throw std::runtime_error("Unsupported ROM for table asset extraction");
    // Recovered from bank 05/0A:4006 and :4088, with consecutive 8-KiB ROM banks
    // mapped at CPU 4000..DFFF. This describes data; it does not execute ROM code.
    constexpr std::size_t patterns[]{0x0a0af,0x140af};
    constexpr std::size_t maps[]{0x132b8,0x1c832};
    // EB45/EB53 transfer the raw VCE palettes from bank 1B:A000/A400.
    constexpr std::size_t palettes[]{0x36000,0x36400};
    std::array<TableBackground,2> result;
    for(std::size_t i=0;i<2;++i) {
        auto& table=result[i];
        const auto tiles=decode_word_rle(image.subspan(patterns[i]),0x3800);
        const auto map=decode_word_rle(image.subspan(maps[i]),1024);
        if(map.words.size()!=1024) throw std::runtime_error("Table asset schema changed");
        std::copy(tiles.words.begin(),tiles.words.end(),table.vram.begin()+0x800);
        std::copy(map.words.begin(),map.words.end(),table.vram.begin());
        for(std::size_t color=0;color<512;++color) {
            const auto offset=palettes[i]+color*2;
            table.palette[color]=std::to_integer<std::uint16_t>(image[offset])|(std::to_integer<std::uint16_t>(image[offset+1])<<8);
            if(table.palette[color]>511) throw std::runtime_error("Invalid ROM table palette");
        }
        table.pattern_words=tiles.words.size(); table.map_words=map.words.size(); table.palette_words=512;
    }
    return result;
}
std::array<TableSprites,2> decode_table_sprites(std::span<const std::byte> image) {
    if(!rom::matches_supported_rom(image.size(),rom::compute_digests(image))) throw std::runtime_error("Unsupported ROM for sprite asset extraction");
    // 05/0A:403F map banks 0F..11 at CPU 8000..DFFF. The upper
    // loader selects pointer 8004, whereas the lower selects 8002.
    auto pointer=[&](std::size_t entry) {
        const auto at=0x1e000+entry*2;
        const auto cpu=std::to_integer<unsigned>(image[at])|(std::to_integer<unsigned>(image[at+1])<<8);
        if(cpu<0x8000 || cpu>=0xe000) throw std::runtime_error("Invalid shared sprite pointer");
        return std::size_t{0x1e000}+cpu-0x8000;
    };
    constexpr std::size_t local[]{0x10254,0x19ef6};
    constexpr std::size_t destinations[]{0x4000,0x5800,0x6000};
    constexpr std::size_t limits[]{0x1800,0x800,0x2000};
    std::array<TableSprites,2> result;
    for(std::size_t level=0;level<2;++level) {
        const std::size_t offsets[]{pointer(0),pointer(level+1),local[level]};
        for(std::size_t stream=0;stream<3;++stream) {
            const auto decoded=decode_word_rle(image.subspan(offsets[stream]),limits[stream]);
            if(decoded.words.size()!=limits[stream]) throw std::runtime_error("Invalid sprite pattern stream size");
            auto& table=result[level];
            std::copy(decoded.words.begin(),decoded.words.end(),table.vram.begin()+destinations[stream]);
            table.streams[stream]={offsets[stream],decoded.consumed,destinations[stream],decoded.words.size()};
        }
    }
    return result;
}
HudAssets decode_hud_assets(std::span<const std::byte> image) {
    if(!rom::matches_supported_rom(image.size(),rom::compute_digests(image))) throw std::runtime_error("Unsupported ROM for HUD extraction");
    const auto source=std::to_integer<unsigned>(image[0x1e008])|(std::to_integer<unsigned>(image[0x1e009])<<8);
    if(source<0x8000 || source>=0xe000) throw std::runtime_error("HUD pointer outside mapped banks");
    const auto offset=0x1e000+source-0x8000;
    auto decoded=decode_word_rle(image.subspan(offset,0x24000-offset),768);
    if(decoded.words.size()!=768) throw std::runtime_error("Unexpected main-table HUD stream size");
    return {std::move(decoded.words),{offset,decoded.consumed,0x500,768}};
}
}
