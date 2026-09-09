// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/render/title_logo.hpp"
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
namespace alien_crush::render {
namespace {
Image logo(std::span<const std::byte> rom,std::span<const std::uint16_t> colors={}) {
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
    if(!colors.empty()) std::copy(colors.begin(),colors.end(),palette.begin()+256);
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
Image title_logo(std::span<const std::byte> rom) { return logo(rom); }
TitleLogoAnimation::TitleLogoAnimation(std::span<const std::byte> rom) {
    // Validate before accessing palette offsets.
    images_.push_back(title_logo(rom));ends_.push_back(120);
    auto palette=[&](unsigned bank) {
        std::array<std::uint16_t,16> result{};
        for(unsigned i=0;i<16;++i) {
            const auto at=0x38bd0+bank*32+i*2;
            result[i]=static_cast<std::uint16_t>(std::to_integer<unsigned>(rom[at])|
                (std::to_integer<unsigned>(rom[at+1])<<8));
        }
        return result;
    };
    auto current=palette(0);
    auto append=[&](unsigned ticks) {
        auto image=logo(rom,current);
        if(image.pixels==images_.back().pixels) ends_.back()+=ticks;
        else {images_.push_back(std::move(image));ends_.push_back(ends_.back()+ticks);}
    };
    auto step_toward=[&](unsigned target) {
        const auto goal=palette(target);
        // 1C:4193..429B moves one RGB component one level toward the
        // target per update: largest distance, ties red then green then blue.
        for(unsigned i=0;i<16;++i) {
            int best=0;unsigned channel=3;
            for(const unsigned shift:{3u,6u,0u}) {
                const int delta=static_cast<int>((goal[i]>>shift)&7)-static_cast<int>((current[i]>>shift)&7);
                if(std::abs(delta)>std::abs(best)) {best=delta;channel=shift;}
            }
            if(best) current[i]=static_cast<std::uint16_t>(static_cast<int>(current[i])+(best>0?1:-1)*(1<<channel));
        }
    };
    // Observed 1C:40E8..4192 alternates targets 11 and 8 on successive
    // updates, including during the reveal (it does not finish each palette
    // before switching). Step 0010 gives sixteen ticks per subsequent update.
    // Preserve those intermediate colors; the full bright target palettes are
    // never reached by the cartridge's two-frame glow cycle.
    for(unsigned step=0;step<48;++step) {
        step_toward(step%2?8:11);
        append(16);
    }
    // The cartridge eventually enters attract play. The backglass instead
    // returns to its blue starting artwork, keeping a continuous silent loop.
    const auto forward=images_.size();
    for(std::size_t i=forward-1;i>0;--i) {
        images_.push_back(images_[i-1]);ends_.push_back(ends_.back()+16);
    }
}
const Image& TitleLogoAnimation::frame(std::uint64_t tick) const {
    const auto found=std::upper_bound(ends_.begin(),ends_.end(),tick%duration());
    return images_[static_cast<std::size_t>(found-ends_.begin())];
}
}
