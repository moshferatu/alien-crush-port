// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <algorithm>
#include <stdexcept>
namespace alien_crush::assets {
std::array<std::uint16_t,512> decode_table_palette(std::span<const std::byte> image,unsigned table) {
    if(table>1 || !rom::matches_supported_rom(image.size(),rom::compute_digests(image)))
        throw std::runtime_error("Unsupported ROM/table for palette extraction");
    std::array<std::uint16_t,512> result{};
    for(unsigned color=0;color<512;++color) {
        const auto offset=0x36000+table*0x400+color*2;
        result[color]=static_cast<std::uint16_t>(std::to_integer<unsigned>(image[offset])|
            (std::to_integer<unsigned>(image[offset+1])<<8));
        if(result[color]>511) throw std::runtime_error("Invalid ROM table palette");
    }
    return result;
}
LowerCounterTiles decode_lower_counter_tiles(std::span<const std::byte> image) {
    if(!rom::matches_supported_rom(image.size(),rom::compute_digests(image)))
        throw std::runtime_error("Unsupported ROM for lower counter tiles");
    LowerCounterTiles result;
    for(unsigned value=0;value<7;++value) for(unsigned tile=0;tile<6;++tile) {
        const auto offset=0x7565+value*12+tile*2;
        result[value][tile]=static_cast<std::uint16_t>(std::to_integer<unsigned>(image[offset])|
            (std::to_integer<unsigned>(image[offset+1])<<8));
    }
    return result;
}
bool restore_lower_counter(std::span<std::uint16_t> bat,const LowerCounterTiles& tiles,
    unsigned table,std::span<const std::byte> ram) {
    if(bat.size()!=1024 || ram.size()!=8192 || table>1)
        throw std::runtime_error("Invalid lower counter inputs");
    const auto value=std::to_integer<unsigned>(ram[0x7f]);
    if(table!=0 || value<1 || value>7) return false;
    for(unsigned y=0;y<2;++y) for(unsigned x=0;x<3;++x)
        bat[0x22d+y*32+x]=tiles[value-1][y*3+x];
    return true;
}
std::array<TableIndicator,29> decode_table_indicators(std::span<const std::byte> image) {
    if(!rom::matches_supported_rom(image.size(),rom::compute_digests(image)))
        throw std::runtime_error("Unsupported ROM for table indicators");
    std::array<TableIndicator,29> result{};
    for(unsigned i=0;i<result.size();++i) {
        const auto p=0x7357+i*6;
        const auto high=std::to_integer<unsigned>(image[p+1]);
        result[i]={high>>4,std::to_integer<unsigned>(image[p])|((high&15)<<8),
            std::to_integer<unsigned>(image[p+2]),std::to_integer<unsigned>(image[p+3]),
            static_cast<std::uint16_t>(std::to_integer<unsigned>(image[p+4])<<8)};
    }
    return result;
}
void restore_table_indicators(std::span<std::uint16_t> bat,
    std::span<const TableIndicator> indicators,unsigned table,std::span<const std::byte> ram) {
    if(bat.size()!=1024 || ram.size()!=8192 || table>1 || indicators.size()!=29)
        throw std::runtime_error("Invalid table indicator inputs");
    for(unsigned i=0;i<indicators.size();++i) {
        const auto& indicator=indicators[i];
        if(indicator.table>1 || !indicator.width || indicator.width>32 || !indicator.height ||
            indicator.height>32 || indicator.address>=1024 ||
            indicator.address+(indicator.height-1)*32+indicator.width>1024 ||
            (indicator.enabled_palette&0x0fff)) throw std::runtime_error("Invalid table indicator rectangle");
    }
    for(unsigned i=0;i<indicators.size();++i) {
        const auto& indicator=indicators[i];
        if(indicator.table!=table || !(std::to_integer<unsigned>(ram[0xd6+i/8])&(1u<<(i%8)))) continue;
        for(unsigned y=0;y<indicator.height;++y) for(unsigned x=0;x<indicator.width;++x) {
            auto& word=bat[indicator.address+y*32+x];
            word=static_cast<std::uint16_t>((word&0x0fff)|indicator.enabled_palette);
        }
    }
}
PalettePulseStep advance_palette_pulse(PalettePulseState state) {
    if(!(state.flags&0x80)) return {state,std::nullopt};
    bool descending=(state.flags&0x40)!=0;
    if(!descending && state.green==7) {state.flags=0xc0;descending=true;}
    const int delta=descending?-1:1;
    state.green=static_cast<std::uint8_t>(state.green+delta);
    state.red=static_cast<std::uint8_t>(state.red+delta);
    state.blue=static_cast<std::uint8_t>(state.blue+delta);
    // F684 packs the low byte with 8-bit shifts; VCE retains one high bit.
    const auto color=static_cast<std::uint16_t>(((state.green&7)<<6)|((state.red<<3)&255)|state.blue);
    if(descending && state.green==0) state.flags=0;
    return {state,color};
}
PaletteFadeOut advance_palette_fade_out(std::span<const std::uint16_t> current) {
    if(current.size()!=512) throw std::runtime_error("Palette fade-out requires 512 colors");
    PaletteFadeOut result;
    for(std::size_t i=0;i<512;++i) {
        if(current[i]>511) throw std::runtime_error("Palette fade-out requires 9-bit colors");
        if(!current[i]) ++result.already_black;
        const unsigned blue=current[i]&7,red=(current[i]>>3)&7,green=(current[i]>>6)&7;
        result.colors[i]=static_cast<std::uint16_t>((blue?blue-1:0)|((red?red-1:0)<<3)|((green?green-1:0)<<6));
    }
    return result;
}
std::array<std::uint16_t,512> advance_palette_fade(std::span<const std::uint16_t> current,
    std::span<const std::uint16_t> target,unsigned threshold) {
    if(current.size()!=512 || target.size()!=512 || threshold<1 || threshold>21)
        throw std::runtime_error("Invalid palette fade input");
    std::array<std::uint16_t,512> result{};
    for(std::size_t i=0;i<512;++i) {
        if(current[i]>511 || target[i]>511) throw std::runtime_error("Palette fade requires 9-bit colors");
        const unsigned blue=target[i]&7,red=(target[i]>>3)&7,green=(target[i]>>6)&7;
        const unsigned cb=current[i]&7,cr=(current[i]>>3)&7,cg=(current[i]>>6)&7;
        if(cb>blue || cr>red || cg>green) throw std::runtime_error("Palette fade requires monotonic channel targets");
        result[i]=current[i];
        if(blue+red+green<threshold) continue;
        const auto db=blue-cb,dr=red-cr,dg=green-cg;
        if(db+dr+dg==0) continue;
        // E553..E580: red wins ties with green, and either wins ties with blue.
        result[i]=static_cast<std::uint16_t>(current[i]+(dr>=dg && dr>=db?8:(dg>=db?64:1)));
    }
    return result;
}
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
    std::array<TableBackground,2> result;
    for(std::size_t i=0;i<2;++i) {
        auto& table=result[i];
        const auto tiles=decode_word_rle(image.subspan(patterns[i]),0x3800);
        const auto map=decode_word_rle(image.subspan(maps[i]),1024);
        if(map.words.size()!=1024) throw std::runtime_error("Table asset schema changed");
        std::copy(tiles.words.begin(),tiles.words.end(),table.vram.begin()+0x800);
        std::copy(map.words.begin(),map.words.end(),table.vram.begin());
        table.palette=decode_table_palette(image,static_cast<unsigned>(i));
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
