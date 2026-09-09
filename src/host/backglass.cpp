// SPDX-License-Identifier: GPL-3.0-or-later
#include "backglass.hpp"
#include "alien_crush/core/files.hpp"
#include <bit>
#include <stdexcept>
namespace alien_crush::host {
std::optional<render::Image> load_backglass(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path)) return std::nullopt;
    const auto data=core::read_file(path,64*1024*1024);
    const auto invalid=[&]() -> void {
        throw std::runtime_error("Invalid backglass BMP: "+path.string()+
            " (expected uncompressed 24/32-bit Windows BMP, at most 4096x4096 and 64 MiB)");
    };
    if(data.size()<54) { invalid(); }
    const auto byte=[&](std::size_t p) {return std::to_integer<std::uint32_t>(data[p]);};
    const auto word=[&](std::size_t p) {return byte(p)|(byte(p+1)<<8);};
    const auto dword=[&](std::size_t p) {return word(p)|(word(p+2)<<16);};
    const auto offset=dword(10),header=dword(14);
    const auto width=std::bit_cast<std::int32_t>(dword(18));
    const auto signed_height=std::bit_cast<std::int32_t>(dword(22));
    const auto bits=word(28);
    if(byte(0)!='B' || byte(1)!='M' || header<40 || header>data.size()-14 ||
        offset<14+header || offset>data.size() || word(26)!=1 || dword(30)!=0 ||
        (bits!=24 && bits!=32) || width<1 || width>4096 ||
        signed_height==0 || signed_height < -4096 || signed_height>4096) { invalid(); }
    const bool top_down=(dword(22)&0x80000000U)!=0;
    const auto height=top_down?-signed_height:signed_height;
    const auto stride=(static_cast<std::size_t>(width)*(bits/8)+3)&~std::size_t{3};
    if(static_cast<std::size_t>(height)>(data.size()-offset)/stride) { invalid(); }
    render::Image image{width,height,
        std::vector<std::uint32_t>(static_cast<std::size_t>(width)*height)};
    for(int y=0;y<height;++y) {
        const auto row=offset+stride*static_cast<std::size_t>(top_down?y:height-1-y);
        for(int x=0;x<width;++x) {
            const auto p=row+static_cast<std::size_t>(x)*(bits/8);
            // BI_RGB's fourth byte is reserved, not alpha: output is opaque.
            image.pixels[static_cast<std::size_t>(y)*width+x]=
                0xFF000000U|(byte(p+2)<<16)|(byte(p+1)<<8)|byte(p);
        }
    }
    return image;
}
}
