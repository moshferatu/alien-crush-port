// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>

struct CabinetPresentationCheck {
    alien_crush::render::TableTransitionHold hold;
    void check(const alien_crush::render::TablePlayfield& table,
        const alien_crush::diagnostics::ReferenceSnapshot& video,
        const alien_crush::diagnostics::ReferenceSnapshot* previous,
        std::span<const std::byte> ram,const alien_crush::render::TableRaster* raster) {
        using namespace alien_crush;
        const auto scene=diagnostics::observe_alien_state(ram);
        const auto image=table.cabinet_frame(video,previous,ram,raster,hold);
        const bool main=scene.table_code<2 && !scene.title_menu && !scene.high_score_menu;
        if(image.has_value()!=main || (image && (image->width!=256 || image->height!=456 || image->pixels.size()!=256*456)))
            throw std::runtime_error("Cabinet changed projection at frame "+std::to_string(video.frame));
        // A cold presentation (startup or quick restore) must also be full-size.
        if(main && video.frame%113==0) {
            render::TableTransitionHold cold;
            const auto restored=table.cabinet_frame(video,nullptr,ram,raster,cold);
            if(!restored || restored->height!=456) throw std::runtime_error("Cold cabinet presentation lost the full table");
        }
        for(const unsigned frame:{325,335,345,355,600,2200,2210,2220,2230,2240,2243,4792,4800,4810}) {
            if(!image || video.frame!=frame) continue;
            std::filesystem::create_directories("cabinet-previews");
            std::ofstream out("cabinet-previews/"+std::to_string(frame)+".bmp",std::ios::binary);
            const auto word=[&](unsigned value,unsigned bytes) {
                for(unsigned i=0;i<bytes;++i) out.put(static_cast<char>(value>>(8*i)));
            };
            out<<"BM";word(54+256*456*4,4);word(0,4);word(54,4);word(40,4);
            word(256,4);word(456,4);word(1,2);word(32,2);word(0,4);word(256*456*4,4);
            for(unsigned i=0;i<4;++i) word(0,4);
            for(int y=455;y>=0;--y) for(int x=0;x<256;++x) word(image->pixels[y*256+x],4);
            if(!out) throw std::runtime_error("Cannot write cabinet preview");
        }
    }
};
