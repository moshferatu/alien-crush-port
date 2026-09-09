// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/render/title_logo.hpp"
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include <algorithm>
#include <stdexcept>
namespace alien_crush::render {
Image title_logo(std::span<const std::byte> rom) {
    const assets::ActorAssets actors(rom); // Validates the supported ROM first.
    // 00:EC58 -> 12:400A/4025: title sprite stream to VRAM 4000.
    const auto patterns=assets::decode_word_rle(rom.subspan(0x24125),0x4000);
    std::array<std::uint16_t,32768> vram{};
    std::copy(patterns.words.begin(),patterns.words.end(),vram.begin()+0x4000);
    std::array<std::uint16_t,512> palette{};
    // 1C:4089 copies 512 colors from 1C:4BD0, before palette animation.
    for(unsigned i=0;i<512;++i) {
        palette[i]=static_cast<std::uint16_t>(std::to_integer<unsigned>(rom[0x38bd0+i*2])|
            (std::to_integer<unsigned>(rom[0x38bd1+i*2])<<8));
        if(palette[i]>511) throw std::runtime_error("Invalid title palette");
    }
    palette[0]=0; // Cabinet background, replacing the title's scrolling BAT.
    std::array<std::uint16_t,256> sat{};
    const auto pieces=actors.pieces(0xb9);
    if(pieces.size()!=13) throw std::runtime_error("Unexpected title logo descriptor");
    for(std::size_t i=0;i<pieces.size();++i) {
        const auto& p=pieces[i];
        sat[i*4]=static_cast<std::uint16_t>(78+p.y); // Settled Y, 1C:4309.
        sat[i*4+1]=static_cast<std::uint16_t>(128+p.x); // 1C:42DD.
        sat[i*4+2]=static_cast<std::uint16_t>(0x200+p.pattern);
        sat[i*4+3]=p.flags;
    }
    const auto canvas=compose(vram,palette,sat,{256,240,0,0,0,0x40});
    // Bounds of B9's thirteen pieces: 32..223, 14..157, including its top crest.
    Image result{192,144,std::vector<std::uint32_t>(192*144)};
    for(int y=0;y<144;++y) std::copy_n(canvas.pixels.begin()+(y+14)*256+32,192,result.pixels.begin()+y*192);
    return result;
}
}
