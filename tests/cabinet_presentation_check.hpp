// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>

struct CabinetPresentationCheck {
    alien_crush::render::TableTransitionHold hold;
    unsigned verified_match_frames{};
    void check(const alien_crush::render::TablePlayfield& table,
        const alien_crush::diagnostics::ReferenceSnapshot& video,
        const alien_crush::diagnostics::ReferenceSnapshot* previous,
        std::span<const std::byte> ram,const alien_crush::render::TableRaster* raster) {
        using namespace alien_crush;
        const auto scene=diagnostics::observe_alien_state(ram);
        const auto image=table.cabinet_frame(video,previous,ram,raster,hold);
        const bool main=scene.table_code<2 && !scene.title_menu && !scene.high_score_menu && !scene.title_transition;
        if(image.has_value()!=main || (image && (image->width!=256 || image->height!=456 || image->pixels.size()!=256*456)))
            throw std::runtime_error("Cabinet changed projection at frame "+std::to_string(video.frame));
        if(image && scene.match_visible) {
            // Compare the original reserved SAT digits once its latched
            // hundreds digit agrees with the current game state.
            for(unsigned slot=0;slot+2<64;++slot) {
                const auto* s=video.sat.data()+slot*4;
                if(s[0]!=176 || s[1]!=168 || s[2]!=40+scene.match_digit*2 || s[3]!=0x8f) continue;
                const auto original=render::sprites(video.vram,video.palette,video.sat,0);
                unsigned opaque=0;
                for(unsigned i=0;i<3;++i) for(unsigned row=0;row<16;++row) for(unsigned col=0;col<16;++col) {
                    const auto color=original[slot+i].image.pixels[row*16+col];
                    if(!(color>>24)) continue;
                    ++opaque;
                    const unsigned y=112+row+(scene.table_code==0?render::table_half_offset:0);
                    if(image->pixels[y*256+136+i*16+col]!=color)
                        throw std::runtime_error("Cabinet match digit differs from original SAT at frame "+std::to_string(video.frame)+
                            " digit "+std::to_string(scene.match_digit)+" piece "+std::to_string(i)+" row "+std::to_string(row)+
                            " col "+std::to_string(col)+" original "+std::to_string(color)+
                            " cabinet "+std::to_string(image->pixels[y*256+136+i*16+col]));
                }
                if(opaque<30) throw std::runtime_error("Match digits are blank");
                ++verified_match_frames;
                break;
            }
        }
        // A cold presentation (startup or quick restore) must also be full-size.
        if(main && video.frame%113==0) {
            render::TableTransitionHold cold;
            const auto restored=table.cabinet_frame(video,nullptr,ram,raster,cold);
            if(!restored || restored->height!=456) throw std::runtime_error("Cold cabinet presentation lost the full table");
        }
        for(const unsigned frame:{325,335,345,355,600,2200,2210,2220,2230,2240,2243,4792,4800,4810,4860,4900}) {
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
