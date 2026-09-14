// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace alien_crush;
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected ROM path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        assets::ActorAssets assets(rom);
        for(unsigned type:{21u,15u}) for(bool forced:{false,true}) {
            std::array<std::byte,8192> source{};
            source[0x1504]=static_cast<std::byte>(type);source[0x1544]=std::byte{0xc0};
            source[0x1884]=std::byte{100};source[0x1684]=std::byte{120};
            auto clock=std::make_unique<render::TableAnimation>(rom);
            clock->update(assets,source,false);
            for(unsigned i=0;i<20;++i) clock->update(assets,source,true);
            auto expected=clock->actors(0);assets.advance_animation(expected);
            if(forced) source[0x1884]=std::byte{1};
            assets.advance_animation(source);
            // The enable can also arrive AFTER this frame's ordinary actor
            // update; in that case the normal wait must still be recognized.
            source[0xb0]=static_cast<std::byte>(type==21?0x40:0x80);
            clock->update(assets,source,true);
            for(unsigned field=0;field<32;++field)
                if(clock->actors(0)[0x1504+field*64]!=expected[0x1504+field*64])
                    throw std::runtime_error("Group enable rewound cosmetic state");
        }
        auto timeline=std::make_unique<render::TableAnimation>(rom);
        auto runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(path);
        unsigned checked[2]{};
        bool valid=false,was_exchanging=false;
        for(unsigned n=0;n<12000;++n) {
            auto input=recording::canonical_input(n);
            if(n>=360) {
                input={};
                if(n%240<100 || n%39<19) input.player_one|=Pad::button_i;
                if((n+13)%39<19) input.player_one|=Pad::left;
                if(n%181==100) input.player_one|=Pad::button_ii;
                if(n%600==599) input.player_one|=Pad::run;
            }
            runtime->advance(input);
            const auto ram=runtime->work_ram();
            const auto b=[&](unsigned p){return std::to_integer<unsigned>(ram[p]);};
            const auto scene=diagnostics::observe_alien_state(ram);
            if(scene.table_code>1 || scene.title_menu || scene.high_score_menu || scene.title_transition) {
                timeline->reset();valid=false;was_exchanging=false;continue;
            }
            if(b(0xf6)&2) continue;
            const unsigned caller=b(0x1fc)|(b(0x1fd)<<8);
            const bool exchanging=b(0x1ff)==0x40 && b(0x1fe)==(b(0x76)?0x3a:0xa0) &&
                (caller==0x89cf || caller==0x89d2 || caller==0x8a2a || caller==0x8a2d);
            const bool advance=valid && !(b(0x53)&0x80) && !(b(0x54)&0x80);
            auto expected=timeline->actors(0);
            if(advance) assets.advance_animation(expected);
            timeline->update(assets,ram,advance);
            if(valid && (exchanging || was_exchanging)) {
                for(unsigned x=4;x<64;++x) if(expected[0x1500+x]==std::byte{21}) {
                    for(unsigned field=0;field<32;++field) {
                        const unsigned p=0x1500+field*64+x;
                        if(timeline->actors(0)[p]!=expected[p]) throw std::runtime_error(
                            "Green bank changed during handoff at frame "+std::to_string(n+1)+
                            " slot "+std::to_string(x)+" field "+std::to_string(field)+
                            " caller "+std::to_string(caller)+" expected "+std::to_string(std::to_integer<unsigned>(expected[p]))+
                            " got "+std::to_string(std::to_integer<unsigned>(timeline->actors(0)[p])));
                    }
                    ++checked[scene.table_code];
                }
            }
            if(!exchanging) valid=true;
            was_exchanging=exchanging;
        }
        if(checked[0]<3 || checked[1]<3) throw std::runtime_error("Missing both real bank-transfer directions");
        std::cout<<"PASS: 12000 runtime frames; "<<checked[0]<<" lower / "<<checked[1]
            <<" upper handoff green-actor continuity checks\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
