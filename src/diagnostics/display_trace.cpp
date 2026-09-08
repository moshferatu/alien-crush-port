// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/diagnostics/display_trace.hpp"
#include <stdexcept>
namespace alien_crush::diagnostics {
std::vector<render::RasterWrite> display_control_schedule(std::span<const DisplayRow> rows,int height) {
    if(height<1 || height>1024 || rows.size()!=static_cast<std::size_t>(height))
        throw std::runtime_error("Display trace requires complete row coverage");
    std::vector<render::RasterWrite> result;
    for(std::size_t i=0;i<rows.size();++i) {
        if(rows[i].row!=i) throw std::runtime_error("Display trace rows must be consecutive from zero");
        if(i==0 || rows[i-1].control!=rows[i].control)
            result.push_back({rows[i].row,render::RasterTarget::control,0,rows[i].control});
        if(i==0 || rows[i-1].burst_mode!=rows[i].burst_mode)
            result.push_back({rows[i].row,render::RasterTarget::burst_mode,0,static_cast<std::uint16_t>(rows[i].burst_mode)});
    }
    return result;
}
std::vector<render::PalettePixelWrite> palette_pixel_schedule(std::span<const std::uint16_t> initial,
    std::span<const std::uint16_t> final,std::span<const PaletteWrite> writes,int width,int height) {
    if(initial.size()!=512 || final.size()!=512 || writes.size()>65536 || width<1 || width>1024 || height<1 || height>1024)
        throw std::runtime_error("Invalid palette timing input");
    std::vector<std::uint16_t> current(initial.begin(),initial.end());
    std::vector<render::PalettePixelWrite> result;
    for(const auto& write:writes) {
        if(write.address>=512 || write.before>=512 || write.after>=512 || write.row<-263 || write.row>263 ||
            write.x<-2048 || write.x>2048 || current[write.address]!=write.before)
            throw std::runtime_error("Palette observation chain is invalid or incomplete");
        current[write.address]=write.after;
        if(write.frame_done || write.before==write.after) continue;
        // Before the visible viewport (including the preceding vertical blank),
        // writes affect pixel zero. Writes beyond the right edge affect the next row.
        unsigned pixel=0;
        if(write.row>=0 && write.row<height) {
            const auto x=write.x<0?0:(write.x>width?width:write.x);
            pixel=static_cast<unsigned>(write.row*width+x);
        }
        if(pixel>=static_cast<unsigned>(width*height)) continue;
        if(!result.empty() && pixel<result.back().pixel) throw std::runtime_error("Palette output positions are not chronological");
        result.push_back({pixel,write.address,write.after});
    }
    for(std::size_t i=0;i<512;++i)
        if(initial[i]>=512 || final[i]>=512 || current[i]!=final[i]) throw std::runtime_error("Palette trace does not reach final state");
    return result;
}
}
