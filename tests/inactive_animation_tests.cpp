// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        assets::ActorAssets actors(rom);
        // Type 33 initializes six frames at +$0020 (01:5B15..5B25).
        // Validate both FD35 reflection endpoints and the recovered sequence.
        std::array<std::byte,8192> projected{};
        const unsigned slot=4,type=33;
        const auto sequence=std::to_integer<unsigned>(rom[0x9df7+type-1])|
            (std::to_integer<unsigned>(rom[0x9e51+type-1])<<8);
        projected[0x1500+slot]=std::byte{type};projected[0x1540+slot]=std::byte{0x88};
        projected[0x1840+slot]=std::byte{1};projected[0x1a40+slot]=std::byte{6};
        projected[0x1b80+slot]=std::byte{0x20};projected[0x1800+slot]=rom[sequence-0x2000];
        for(unsigned i=0;i<8;++i) actors.advance_animation(projected);
        require(projected[0x1800+slot]==rom[sequence-0x2000+1],"ROM animation did not advance at its original rate");
        for(unsigned i=8;i<48;++i) actors.advance_animation(projected);
        require(projected[0x1c40+slot]==std::byte{5} && projected[0x1bc0+slot]==std::byte{255},"Upper animation reflection differs from FD35");
        for(unsigned i=0;i<40;++i) actors.advance_animation(projected);
        require(projected[0x1c40+slot]==std::byte{0} && projected[0x1bc0+slot]==std::byte{0},"Lower animation reflection differs from FD35");
        projected[0x1840+slot]=std::byte{0};const auto idle=projected;
        actors.advance_animation(projected);require(projected==idle,"Animation woke a collision-triggered idle actor");
        for(unsigned half=0;half<2;++half) for(unsigned flipper=2;flipper<=3;++flipper) for(unsigned pose=0;pose<4;++pose) {
            std::array<std::byte,8192> live{},other{};
            live[0x79]=static_cast<std::byte>(half);
            live[0x1504]=other[0x1504]=static_cast<std::byte>(flipper);
            live[0x1804]=rom[0x9eaf+(flipper-2)*8+half*4+pose];
            actors.mirror_flippers(other,live,1-half);
            require(other[0x1804]==rom[0x9eaf+(flipper-2)*8+(1-half)*4+pose],"Inactive flipper used the wrong table pose");
        }

        auto table=std::make_unique<render::TablePlayfield>(rom);
        auto runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(path);runtime->observe_display(true);
        const auto initial=runtime->save();
        std::vector<std::string> fingerprints;
        std::optional<diagnostics::ReferenceSnapshot> previous;
        std::optional<render::Image> last;
        std::array<std::byte,8192> last_ram{};
        std::array<unsigned,2> animated{};
        render::TableTransitionHold hold;
        for(unsigned n=0;n<1800;++n) {
            const auto frame=runtime->advance(recording::canonical_input(n));
            fingerprints.push_back(recording::fingerprint(frame,runtime->work_ram()));
            const auto video=runtime->video_snapshot();const auto ram=runtime->work_ram();
            const auto before=runtime->save();
            const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            const auto image=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
            const auto repeated=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
            require(!image || (repeated && repeated->pixels==image->pixels),"Rendering twice advanced inactive animation");
            require(runtime->save().core_state==before.core_state,"Presentation changed runtime state");
            if(n==999) {
                auto paused_table=std::make_unique<render::TablePlayfield>(rom);
                std::array<std::byte,8192> paused_ram;
                std::copy(ram.begin(),ram.end(),paused_ram.begin());paused_ram[0x53]|=std::byte{0x80};
                auto paused_video=video,paused_previous=video;--paused_previous.frame;
                render::TableTransitionHold paused_hold;
                std::optional<render::Image> paused_image;
                for(unsigned i=0;i<16;++i) {
                    const auto next=paused_table->cabinet_frame(paused_video,&paused_previous,paused_ram,&raster,paused_hold);
                    require(next.has_value(),"Pause lost cabinet presentation");
                    if(paused_image) require(next->pixels==paused_image->pixels,"Inactive animation advanced during pause");
                    paused_image=next;paused_previous=paused_video;++paused_video.frame;
                }
                auto cold_table=std::make_unique<render::TablePlayfield>(rom);
                render::TableTransitionHold cold_hold;
                const auto cold=cold_table->cabinet_frame(video,nullptr,ram,&raster,cold_hold);
                const auto reset=paused_table->cabinet_frame(video,nullptr,ram,&raster,paused_hold);
                require(cold && reset && cold->pixels==reset->pixels,"Cold view retained stale animation history");
            }
            if(image && (n==399 || n==419 || n==999 || n==1019)) {
                std::filesystem::create_directories("inactive-previews");
                std::ofstream out("inactive-previews/"+std::to_string(n+1)+".bmp",std::ios::binary);
                const auto word=[&](unsigned value,unsigned bytes) {
                    for(unsigned i=0;i<bytes;++i) out.put(static_cast<char>(value>>(8*i)));
                };
                out<<"BM";word(54+256*456*4,4);word(0,4);word(54,4);word(40,4);
                word(256,4);word(456,4);word(1,2);word(32,2);word(0,4);word(256*456*4,4);
                for(unsigned i=0;i<4;++i) word(0,4);
                for(int y=455;y>=0;--y) for(int x=0;x<256;++x) word(image->pixels[y*256+x],4);
                require(bool(out),"Cannot write animation preview");
            }
            const auto half=std::to_integer<unsigned>(ram[0x79]);
            if(n>360 && image && last && half<2 && ram[0x79]==last_ram[0x79] &&
                std::equal(ram.begin()+0x204,ram.begin()+0xa00,last_ram.begin()+0x204)) {
                const unsigned start=half==0?0:240*256,end=half==0?216*256:456*256;
                if(!std::equal(image->pixels.begin()+start,image->pixels.begin()+end,last->pixels.begin()+start)) ++animated[1-half];
            }
            last=image;std::copy(ram.begin(),ram.end(),last_ram.begin());previous=video;
        }
        require(animated[0]>50 && animated[1]>50,"Both dormant halves must animate while their game-state bank stays frozen");
        runtime->restore(initial);runtime->observe_display(false);
        for(unsigned n=0;n<1800;++n) {
            const auto frame=runtime->advance(recording::canonical_input(n));
            require(recording::fingerprint(frame,runtime->work_ram())==fingerprints[n],"Animation changed RAM, video or audio replay");
        }
        std::cout<<"PASS: inactive lower/upper changed on "<<animated[0]<<'/'<<animated[1]
            <<" frozen-bank frames; 1800 exact RAM/video/audio frames; repeated rendering is idempotent\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
