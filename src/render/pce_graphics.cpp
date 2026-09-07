// SPDX-License-Identifier: GPL-3.0-or-later
// Generic background bitplane layout adapted from Devil's Crush pce_background.cpp.
// Sprite addressing follows pinned Beetle HuC6270 FetchSpriteData.
#include "alien_crush/render/pce_graphics.hpp"
#include <stdexcept>
namespace alien_crush::render {
std::uint32_t pce_color(std::uint16_t raw) noexcept {
    const auto r=(36U*((raw>>3)&7))>>3;
    const auto g=(36U*((raw>>6)&7))>>2;
    const auto b=(36U*(raw&7))>>3;
    return 0xff000000U | (((r<<3)|(r>>2))<<16) | (((g<<2)|(g>>4))<<8) | (b<<3)|(b>>2);
}
Image background(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,int width_tiles,int height_tiles) {
    if(vram.size()!=32768 || palette.size()<256 || (width_tiles!=32 && width_tiles!=64 && width_tiles!=128) || (height_tiles!=32 && height_tiles!=64))
        throw std::runtime_error("Invalid PCE background dimensions or memory");
    Image result{width_tiles*8,height_tiles*8,{}};
    result.pixels.resize(static_cast<std::size_t>(result.width)*result.height);
    for(int y=0;y<result.height;++y) for(int x=0;x<result.width;++x) {
        const auto bat=vram[static_cast<std::size_t>(y/8)*width_tiles+x/8];
        const auto tile=static_cast<std::size_t>(bat&0xfff)*16;
        const unsigned shift=7U-static_cast<unsigned>(x%8);
        // Pinned core leaves patterns above physical VRAM transparent.
        const auto low=tile+15<vram.size()?vram[tile+y%8]:0;
        const auto high=tile+15<vram.size()?vram[tile+8+y%8]:0;
        const auto pixel=((low>>shift)&1)|(((low>>(shift+8))&1)<<1)|(((high>>shift)&1)<<2)|(((high>>(shift+8))&1)<<3);
        const auto index=pixel?((bat>>12)*16+pixel):0;
        result.pixels[static_cast<std::size_t>(y)*result.width+x]=pce_color(palette[index]);
    }
    return result;
}
std::array<Sprite,64> sprites(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,std::uint16_t memory_width) {
    if(vram.size()!=32768 || palette.size()!=512 || sat.size()!=256) throw std::runtime_error("Invalid PCE sprite memory");
    std::array<Sprite,64> result;
    constexpr int heights[]{16,32,64,64};
    constexpr unsigned masks[]{~0U,~2U,~6U,~6U};
    for(std::size_t i=0;i<64;++i) {
        auto& sprite=result[i]; const auto flags=sat[i*4+3];
        sprite.x=(sat[i*4+1]&1023)-32; sprite.y=(sat[i*4]&1023)-64;
        sprite.width=(flags&0x100)?32:16; sprite.height=heights[(flags>>12)&3];
        sprite.pattern=(sat[i*4+2]>>1)&1023; sprite.flags=flags; sprite.foreground=(flags&0x80)!=0;
        sprite.image={sprite.width,sprite.height,{}};
        sprite.image.pixels.resize(static_cast<std::size_t>(sprite.width)*sprite.height);
        for(int y=0;y<sprite.height;++y) for(int x=0;x<sprite.width;++x) {
            const auto sx=static_cast<unsigned>((flags&0x800)?sprite.width-1-x:x);
            const auto sy=static_cast<unsigned>((flags&0x8000)?sprite.height-1-y:y);
            auto pattern=static_cast<unsigned>(sprite.pattern)&masks[(flags>>12)&3];
            if(sprite.width==32) pattern&=~1U;
            pattern|=((sy&0x30)>>3)|(sx>>4);
            const auto offset=static_cast<std::size_t>(pattern)*64+(sy&15);
            unsigned pixel=0;
            const bool two_bit=(memory_width&0xc)==4;
            const unsigned start=two_bit && (sat[i*4+2]&1)?32:0;
            for(unsigned plane=0;plane<(two_bit?2U:4U);++plane) {
                const auto address=offset+start+plane*16;
                if(address<vram.size()) pixel|=((vram[address]>>(15-(sx&15)))&1)<<plane;
            }
            if(pixel) sprite.image.pixels[static_cast<std::size_t>(y)*sprite.width+x]=pce_color(palette[256+(flags&15)*16+pixel]);
        }
    }
    return result;
}
Image sprite_sheet(const std::array<Sprite,64>& sprites) {
    Image result{8*32,8*64,std::vector<std::uint32_t>(8*32*8*64,0xff202020)};
    for(std::size_t i=0;i<sprites.size();++i) {
        const auto& image=sprites[i].image;
        for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
            const auto color=image.pixels[static_cast<std::size_t>(y)*image.width+x];
            if(color>>24) result.pixels[((i/8)*64+y)*result.width+(i%8)*32+x]=color;
        }
    }
    return result;
}
}
