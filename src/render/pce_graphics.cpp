// SPDX-License-Identifier: GPL-3.0-or-later
// Generic background bitplane layout adapted from Devil's Crush pce_background.cpp.
// Sprite addressing follows pinned Beetle HuC6270 FetchSpriteData.
#include "alien_crush/render/pce_graphics.hpp"
#include <algorithm>
#include <stdexcept>
namespace alien_crush::render {
std::uint32_t pce_color(std::uint16_t raw) noexcept {
    const auto r=(36U*((raw>>3)&7))>>3;
    const auto g=(36U*((raw>>6)&7))>>2;
    const auto b=(36U*(raw&7))>>3;
    return 0xff000000U | (((r<<3)|(r>>2))<<16) | (((g<<2)|(g>>4))<<8) | (b<<3)|(b>>2);
}
namespace {
unsigned background_index(std::span<const std::uint16_t> vram,int width_tiles,int x,int y) {
    const auto bat=vram[static_cast<std::size_t>(y/8)*width_tiles+x/8];
    const auto tile=static_cast<std::size_t>(bat&0xfff)*16;
    const unsigned shift=7U-static_cast<unsigned>(x%8);
    // Pinned core leaves patterns above physical VRAM transparent.
    const auto low=tile+15<vram.size()?vram[tile+y%8]:0;
    const auto high=tile+15<vram.size()?vram[tile+8+y%8]:0;
    const auto pixel=((low>>shift)&1)|(((low>>(shift+8))&1)<<1)|(((high>>shift)&1)<<2)|(((high>>(shift+8))&1)<<3);
    return pixel?((bat>>12)*16+pixel):0;
}
Image background_layer(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,int width_tiles,int height_tiles,bool transparent_zero) {
    if(vram.size()!=32768 || palette.size()<256 || (width_tiles!=32 && width_tiles!=64 && width_tiles!=128) || (height_tiles!=32 && height_tiles!=64))
        throw std::runtime_error("Invalid PCE background dimensions or memory");
    Image result{width_tiles*8,height_tiles*8,{}};
    result.pixels.resize(static_cast<std::size_t>(result.width)*result.height);
    for(int y=0;y<result.height;++y) for(int x=0;x<result.width;++x) {
        const auto index=background_index(vram,width_tiles,x,y);
        result.pixels[static_cast<std::size_t>(y)*result.width+x]=transparent_zero && !index?0:pce_color(palette[index]);
    }
    return result;
}
}
Image background(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,int width_tiles,int height_tiles) {
    return background_layer(vram,palette,width_tiles,height_tiles,false);
}
namespace {
std::uint32_t output_color(std::span<const std::uint16_t> palette,unsigned index,bool indexed) {
    return indexed?0xff000000U|index:pce_color(palette[index]);
}
std::array<Sprite,64> sprite_layer(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,std::uint16_t memory_width,bool indexed,int first=-1024,int end=2048) {
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
        // A raster band only consumes its own rows. Decoding every sprite's
        // entire image for every VRAM-write band multiplied work during uploads.
        // Keep SAT order and dimensions intact for the hardware fetch limit.
        for(int y=std::max(0,first-sprite.y);y<std::min(sprite.height,end-sprite.y);++y) for(int x=0;x<sprite.width;++x) {
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
            if(pixel) sprite.image.pixels[static_cast<std::size_t>(y)*sprite.width+x]=output_color(palette,256+(flags&15)*16+pixel,indexed);
        }
    }
    return result;
}
}
std::array<Sprite,64> sprites(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,std::uint16_t memory_width) {
    return sprite_layer(vram,palette,sat,memory_width,false);
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
namespace {
void validate_composition(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport) {
    if(vram.size()!=32768 || palette.size()!=512 || sat.size()!=256 ||
        viewport.width<1 || viewport.width>1024 || viewport.height<1 || viewport.height>1024)
        throw std::runtime_error("Invalid PCE composition memory or viewport");
}
Image compose_rows(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport,int first,int end,bool indexed=false,
    std::span<std::uint8_t> visible={},std::span<std::uint8_t> blocked={},std::uint64_t hidden=0) {
    if(!blocked.empty() && blocked.size()!=static_cast<std::size_t>(viewport.width)*viewport.height)
        throw std::runtime_error("Invalid blocked sprite mask size");
    if(!visible.empty()) {
        if(visible.size()!=static_cast<std::size_t>(viewport.width)*viewport.height)
            throw std::runtime_error("Invalid visible sprite mask size");
        std::fill(visible.begin()+first*viewport.width,visible.begin()+end*viewport.width,std::uint8_t{0});
    }
    const bool bg_enabled=!viewport.burst_mode && (viewport.control&0x80)!=0;
    const bool sprites_enabled=!viewport.burst_mode && (viewport.control&0x40)!=0;
    const auto count=static_cast<std::size_t>(viewport.width)*(end-first);
    Image result{viewport.width,end-first,std::vector<std::uint32_t>(count,
        output_color(palette,viewport.burst_mode || (!bg_enabled && sprites_enabled)?256:0,indexed))};
    std::vector<bool> opaque(count),claimed(count);
    if(bg_enabled) {
        constexpr int widths[]{32,64,128,128};
        const auto width_tiles=widths[(viewport.memory_width>>4)&3];
        const auto height_pixels=(viewport.memory_width&0x40)?512:256;
        for(int y=first;y<end;++y) for(int x=0;x<viewport.width;++x) {
            const auto index=background_index(vram,width_tiles,(x+viewport.scroll_x)%(width_tiles*8),
                (y+viewport.scroll_y)%height_pixels);
            const auto dest=static_cast<std::size_t>(y-first)*viewport.width+x;
            opaque[dest]=index!=0;
            if(opaque[dest]) result.pixels[dest]=output_color(palette,index,indexed);
        }
    }
    std::vector<unsigned> fetched(static_cast<std::size_t>(end-first));
    unsigned slot=0;
    if(sprites_enabled) for(const auto& sprite:sprite_layer(vram,palette,sat,viewport.memory_width,indexed,first,end)) {
        const bool suppress=(hidden&(std::uint64_t{1}<<slot++))!=0;
        for(int y=0;y<sprite.height;++y) {
            const int row=sprite.y+y;
            if(row<first || row>=end) continue;
            // HuC6270 fetches at most sixteen 16-pixel pieces per row in SAT order.
            // Transparent/offscreen pieces count; a wide sprite can be cut in half.
            auto& used=fetched[static_cast<std::size_t>(row-first)];
            const auto available=16U-used;
            const auto pieces=static_cast<unsigned>(sprite.width/16);
            const auto accepted=pieces<available?pieces:available;
            used+=accepted;
            if(suppress) continue;
            for(int x=0;x<static_cast<int>(accepted)*16;++x) {
                const int dx=sprite.x+x,dy=sprite.y+y;
                if(dx<0 || dy<0 || dx>=viewport.width || dy>=viewport.height) continue;
                const auto color=sprite.image.pixels[static_cast<std::size_t>(y)*sprite.width+x];
                const auto dest=static_cast<std::size_t>(dy-first)*viewport.width+dx;
                if(!(color>>24) || claimed[dest]) continue;
                // SAT priority still applies when the background covers this sprite.
                claimed[dest]=true;
                if(sprite.foreground || !opaque[dest]) {
                    result.pixels[dest]=color;
                    if(!visible.empty()) visible[static_cast<std::size_t>(dy)*viewport.width+dx]=1;
                }
            }
        }
    }
    if(!blocked.empty()) for(int row=first;row<end;++row) for(int x=0;x<viewport.width;++x) {
        const auto local=static_cast<std::size_t>(row-first)*viewport.width+x;
        blocked[static_cast<std::size_t>(row)*viewport.width+x]=
            static_cast<std::uint8_t>(claimed[local] || fetched[row-first]>=16);
    }
    return result;
}
}
Image compose(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport,std::span<std::uint8_t> visible,
    std::span<std::uint8_t> blocked,std::uint64_t hidden) {
    validate_composition(vram,palette,sat,viewport);
    return compose_rows(vram,palette,sat,viewport,0,viewport.height,false,visible,blocked,hidden);
}
namespace {
Image raster_layer(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport,std::span<const RasterWrite> writes,bool indexed,
    std::span<std::uint8_t> visible={},std::span<std::uint8_t> blocked={},std::uint64_t hidden=0) {
    validate_composition(vram,palette,sat,viewport);
    if(writes.size()>65536) throw std::runtime_error("Too many raster writes");
    unsigned last_row=0;
    for(const auto& write:writes) {
        if(write.row>=viewport.height || write.row<last_row) throw std::runtime_error("Invalid raster write order or row");
        last_row=write.row;
        switch(write.target) {
        case RasterTarget::vram:
            if(write.address>=32768) throw std::runtime_error("Raster VRAM address out of bounds");
            break;
        case RasterTarget::palette:
            if(write.address>=512 || write.value>=512) throw std::runtime_error("Raster palette write out of bounds");
            break;
        case RasterTarget::sat:
            if(write.address>=256) throw std::runtime_error("Raster SAT address out of bounds");
            break;
        case RasterTarget::burst_mode:
            if(write.address || write.value>1) throw std::runtime_error("Invalid raster burst mode");
            break;
        case RasterTarget::control: case RasterTarget::scroll_x: case RasterTarget::scroll_y: case RasterTarget::memory_width:
            if(write.address!=0) throw std::runtime_error("Raster register address must be zero");
            break;
        default: throw std::runtime_error("Unknown raster write target");
        }
    }
    if(writes.empty()) return compose_rows(vram,palette,sat,viewport,0,viewport.height,indexed,visible,blocked,hidden);
    std::vector<std::uint16_t> memory(vram.begin(),vram.end()),colors(palette.begin(),palette.end()),attributes(sat.begin(),sat.end());
    auto view=viewport;
    Image result{view.width,view.height,std::vector<std::uint32_t>(static_cast<std::size_t>(view.width)*view.height)};
    std::size_t next=0;
    for(int row=0;row<view.height;) {
        while(next<writes.size() && writes[next].row==row) {
            const auto& write=writes[next++];
            switch(write.target) {
            case RasterTarget::vram: memory[write.address]=write.value; break;
            case RasterTarget::palette: colors[write.address]=write.value; break;
            case RasterTarget::sat: attributes[write.address]=write.value; break;
            case RasterTarget::control: view.control=write.value; break;
            case RasterTarget::scroll_x: view.scroll_x=write.value; break;
            case RasterTarget::scroll_y: view.scroll_y=write.value; break;
            case RasterTarget::memory_width: view.memory_width=write.value; break;
            case RasterTarget::burst_mode: view.burst_mode=write.value!=0; break;
            }
        }
        const int end=next<writes.size()?writes[next].row:view.height;
        const auto band=compose_rows(memory,colors,attributes,view,row,end,indexed,visible,blocked,hidden);
        std::copy(band.pixels.begin(),band.pixels.end(),result.pixels.begin()+static_cast<std::size_t>(row)*view.width);
        row=end;
    }
    return result;
}
}
Image compose_raster(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport,std::span<const RasterWrite> writes,
    std::span<std::uint8_t> visible,std::span<std::uint8_t> blocked,std::uint64_t hidden) {
    return raster_layer(vram,palette,sat,viewport,writes,false,visible,blocked,hidden);
}
Image compose_palette_raster(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport,std::span<const RasterWrite> rows,
    std::span<const PalettePixelWrite> colors,std::span<std::uint8_t> visible,std::span<std::uint8_t> blocked,std::uint64_t hidden) {
    validate_composition(vram,palette,sat,viewport);
    if(colors.size()>65536) throw std::runtime_error("Too many pixel palette writes");
    for(const auto& row:rows) if(row.target==RasterTarget::palette) throw std::runtime_error("Ambiguous row and pixel palette writes");
    std::uint32_t previous=0;
    for(const auto& color:colors) {
        if(color.pixel<previous || color.pixel>=static_cast<unsigned>(viewport.width*viewport.height) || color.address>=512 || color.value>=512)
            throw std::runtime_error("Invalid pixel palette write");
        previous=color.pixel;
    }
    auto result=raster_layer(vram,palette,sat,viewport,rows,true,visible,blocked,hidden);
    std::vector<std::uint16_t> current(palette.begin(),palette.end());
    std::size_t next=0;
    for(std::size_t p=0;p<result.pixels.size();++p) {
        while(next<colors.size() && colors[next].pixel==p) {current[colors[next].address]=colors[next].value;++next;}
        result.pixels[p]=pce_color(current[result.pixels[p]&511]);
    }
    return result;
}
}
