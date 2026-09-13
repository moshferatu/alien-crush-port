// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM path");
        const auto rom=core::read_file(core::path_from_utf8(argv[1]));
        const auto backgrounds=std::make_unique<std::array<assets::TableBackground,2>>(assets::decode_table_backgrounds(rom));
        const auto patterns=std::make_unique<std::array<assets::TableSprites,2>>(assets::decode_table_sprites(rom));
        const assets::ActorAssets actors(rom);
        auto table=std::make_unique<render::TablePlayfield>(rom);
        auto video=std::make_unique<diagnostics::ReferenceSnapshot>();
        video->vram=(*backgrounds)[1].vram;
        std::copy((*patterns)[1].vram.begin()+0x4000,(*patterns)[1].vram.end(),video->vram.begin()+0x4000);
        video->palette=(*backgrounds)[1].palette;video->control=0xc0;
        unsigned covered=0,uncovered=0,decorative=0;
        // Real mouth forms, followed by the same artwork assigned to an eye
        // actor: shape/colour alone must never grant permission to hide the ball.
        for(const auto [type,descriptor]:{std::pair{11u,135u},{12u,71u},{13u,91u},{34u,127u},{47u,135u}}) {
            unsigned mouth_covered=0;
            for(const bool hidden:{false,true}) {
                std::array<std::byte,8192> ram{};
                ram[0x79]=std::byte{1};ram[0x2f]=std::byte{2};ram[0x50]=std::byte{10};
                ram[0x1504]=static_cast<std::byte>(type);ram[0x1544]=hidden?std::byte{0xa8}:std::byte{0x88};
                ram[0x15c4]=std::byte{160};ram[0x1684]=std::byte{192};
                ram[0x1804]=static_cast<std::byte>(descriptor);
                std::array<std::uint16_t,256> sat{};
                for(const auto& piece:assets::assemble_table_actors(actors,ram))
                    std::copy(piece.words.begin(),piece.words.end(),sat.begin()+piece.slot*4);
                const auto mouth=render::sprites(video->vram,video->palette,sat,0);
                std::vector<bool> opaque(256*240);
                for(const auto& sprite:mouth) for(int y=0;y<sprite.height;++y) for(int x=0;x<sprite.width;++x) {
                    const int dx=sprite.x+x,dy=sprite.y+y;
                    if(dx>=0 && dx<256 && dy>=0 && dy<240 && (sprite.image.pixels[y*sprite.width+x]>>24))
                        opaque[static_cast<std::size_t>(dy)*256+dx]=true;
                }
                render::TableTransitionHold hold;
                video->frame+=2;
                const auto clean=table->cabinet_frame(*video,nullptr,ram,nullptr,hold);
                require(clean.has_value(),"Mouth backdrop available");
                for(unsigned y=104;y<=152;y+=8) for(unsigned x=104;x<=152;x+=8) {
                    ram[0x1500]=std::byte{1};ram[0x1540]=std::byte{0x80};
                    ram[0x15c0]=static_cast<std::byte>(x+32);ram[0x1680]=static_cast<std::byte>(y+64);
                    video->frame+=2;
                    const auto image=table->cabinet_frame(*video,nullptr,ram,nullptr,hold);
                    const auto ball=table->transition_ball(1,ram);
                    require(image && ball,"Visible cabinet ball fixture");
                    for(int by=0;by<ball->height;++by) for(int bx=0;bx<ball->width;++bx) {
                        const auto color=ball->image.pixels[by*ball->width+bx];
                        if(!(color>>24)) continue;
                        const auto p=static_cast<std::size_t>(ball->y+by)*256+ball->x+bx;
                        const bool blocked=type!=47 && opaque[p];
                        require(image->pixels[p]==(blocked?clean->pixels[p]:color),
                            "Only opaque, visible mouth pixels may cover the cabinet ball");
                        if(blocked) {++covered;++mouth_covered;} else ++uncovered;
                        if(type==47 && opaque[p]) ++decorative;
                    }
                }
            }
            if(type!=47) require(mouth_covered>0,"Every swallowing mouth must exercise ball occlusion");
        }
        require(covered>100 && uncovered>100 && decorative>100,"Opaque, transparent, hidden and decorative overlap coverage");
        std::cout<<"PASS: "<<covered<<" mouth-covered pixels, "<<uncovered<<" visible ball pixels, "
            <<decorative<<" decorative overlaps; all four mouth actors\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
