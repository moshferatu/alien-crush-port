// SPDX-License-Identifier: GPL-3.0-or-later
// The brain's flyers in the cabinet: the port gives one that reaches the seam
// to the lower half (gameplay::CabinetActors::carry_flyers), so it flies on
// down the whole table instead of ending at the upper half's bottom edge.
#include "alien_crush/core/files.hpp"
#include "alien_crush/gameplay/cabinet_actors.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
using namespace alien_crush;
namespace {
using Ram=std::array<std::byte,8192>;
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
unsigned get(std::span<const std::byte> ram,std::size_t at) {return std::to_integer<unsigned>(ram[at]);}
// Actor field ($3500+field) of slot x in the bank at `bank`.
unsigned field(std::span<const std::byte> ram,std::size_t bank,unsigned address,unsigned x) {return get(ram,bank+address-0x3500+x);}
int position(std::span<const std::byte> ram,std::size_t bank,unsigned low,unsigned x) {
    return static_cast<std::int16_t>(field(ram,bank,low,x)|(field(ram,bank,low+0x40,x)<<8));
}
std::vector<unsigned> flyers(std::span<const std::byte> ram,std::size_t bank) {
    std::vector<unsigned> result;
    for(unsigned x=4;x<64;++x) if(field(ram,bank,0x3500,x)==55) result.push_back(x);
    return result;
}
void preview(const render::Image& image,const std::string& name) {
    std::filesystem::create_directories("flyer-previews");
    std::ofstream out("flyer-previews/"+name+".bmp",std::ios::binary);
    const auto word=[&](unsigned value,unsigned bytes) {
        for(unsigned i=0;i<bytes;++i) out.put(static_cast<char>(value>>(8*i)));
    };
    out<<"BM";word(54+256*456*4,4);word(0,4);word(54,4);word(40,4);
    word(256,4);word(456,4);word(1,2);word(32,2);word(0,4);word(256*456*4,4);
    for(unsigned i=0;i<4;++i) word(0,4);
    for(int y=455;y>=0;--y) for(int x=0;x<256;++x) word(image.pixels[static_cast<std::size_t>(y)*256+x],4);
}
// A game in play with the ball held in a half, and the brain set off as its
// tenth touch does (01:5061): open, it lets out four flyers (50F0).
std::unique_ptr<ReferenceRuntime> opened(const std::filesystem::path& path,ActorMode mode,unsigned ball_half,double x,double y) {
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1,{},mode);
    runtime->load(path);runtime->observe_display(true);
    for(unsigned n=0;n<120;++n) runtime->advance({});
    start_with_score(*runtime,0);
    for(unsigned n=0;n<120;++n) {runtime->hold_ball(x,y);runtime->advance({});}
    require(get(runtime->work_ram(),0x79)==ball_half,"The ball is not held in its half");
    const std::size_t upper=ball_half?0x1500:0x200;
    auto state=runtime->save();
    Ram ram;std::copy(runtime->work_ram().begin(),runtime->work_ram().end(),ram.begin());
    const auto found=std::search(state.core_state.begin(),state.core_state.end(),ram.begin(),ram.end());
    require(found!=state.core_state.end(),"Work RAM is not in the saved state");
    unsigned brain=0;
    for(unsigned slot=4;slot<64;++slot) if(ram[upper+slot]==std::byte{7}) brain=slot;
    require(brain!=0,"The upper table has no brain");
    const auto put=[&](unsigned address,unsigned value) {ram[upper+address-0x3500+brain]=static_cast<std::byte>(value);};
    put(0x3840,2);put(0x3a40,4);put(0x3b80,0x20);put(0x3bc0,0);put(0x3c00,0);put(0x3c40,0);
    std::copy(ram.begin(),ram.end(),found);
    runtime->restore(state);
    return runtime;
}
// The stored half alone, on RAM with nothing else in it.
void synthetic(std::span<const std::byte> rom) {
    const auto flyer=[](Ram& ram,std::size_t bank,unsigned x,unsigned y) {
        const auto put=[&](unsigned address,unsigned value) {ram[bank+address-0x3500+x]=static_cast<std::byte>(value&255);};
        put(0x3500,55);put(0x3540,0x84);put(0x3a00,3);put(0x3a40,4);put(0x3b80,0x20);put(0x3800,0x76);
        put(0x35c0,0x58);put(0x3680,y);put(0x36c0,y>>8);put(0x3780,0x80); // Half a pixel down each update.
    };
    for(const unsigned ball_half:{0u,1u}) {
        const std::size_t upper=ball_half?0x1500:0x200,lower=0x1700-upper;
        gameplay::CabinetActors actors(rom,gameplay::CabinetActors::Scope::table);
        Ram ram{};
        ram[0x79]=static_cast<std::byte>(ball_half);
        flyer(ram,upper,40,271);
        actors.update_inactive(ram,1);
        require(flyers(ram,lower).empty(),"A flyer above the seam was given to the lower half");
        // The slots in the way are taken: it goes to the first free one.
        for(unsigned x=4;x<9;++x) ram[lower+x]=std::byte{45};
        ram[upper+0x3680-0x3500+40]=std::byte{272&255};ram[upper+0x36c0-0x3500+40]=std::byte{272>>8};
        ram[upper+0x3640-0x3500+40]=std::byte{};
        const int across=position(ram,upper,0x35c0,40);
        actors.update_inactive(ram,2);
        require(flyers(ram,lower)==std::vector<unsigned>{9},"A flyer at the seam is given to the lower half's first free slot");
        require(position(ram,lower,0x35c0,9)==across && field(ram,lower,0x3780,9)==0x80 && (field(ram,lower,0x3540,9)&0xa5)==0x84 &&
            std::abs(position(ram,lower,0x3680,9)-(272-216))<=1,"The lower half's flyer is the upper's, 216 rows up");
        actors.update_inactive(ram,3);
        require(flyers(ram,lower).size()==1,"A flyer is given once");
        // No room in the lower half: the flyer stays the upper's.
        gameplay::CabinetActors crowded(rom,gameplay::CabinetActors::Scope::table);
        Ram full{};
        full[0x79]=static_cast<std::byte>(ball_half);
        flyer(full,upper,40,276);
        for(unsigned x=4;x<0x3c;++x) full[lower+x]=std::byte{45};
        crowded.update_inactive(full,1);
        require(flyers(full,lower).empty() && flyers(full,upper).size()==1 && field(full,upper,0x38c0,40)==0,
            "A flyer with no slot to go to stays in the upper half");
        // Runtimes up to -actors2 left it there always.
        gameplay::CabinetActors before(rom,gameplay::CabinetActors::Scope::table,true,true,false);
        Ram old{};
        old[0x79]=static_cast<std::byte>(ball_half);
        flyer(old,upper,40,276);
        before.update_inactive(old,1);
        require(flyers(old,lower).empty(),"-actors2 flyers stay in the upper half");
    }
}
}
int main(int argc,char** argv) {
    try {
        require(argc>=2,"Expected ROM");
        const bool previews=argc>2;
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        synthetic(rom);
        for(const unsigned ball_half:{0u,1u}) {
            const double hold_x=ball_half?60:150,hold_y=ball_half?150:420;
            const std::size_t upper=ball_half?0x1500:0x200,lower=0x1700-upper;
            auto runtime=opened(path,ActorMode::cabinet,ball_half,hold_x,hold_y);
            require(runtime->revision().find(kCabinetActorsRevision)!=std::string_view::npos &&
                recorded_actor_mode(runtime->revision())==ActorMode::cabinet,"The runtime's revision lacks its marker");
            auto table=std::make_unique<render::TablePlayfield>(rom),bare=std::make_unique<render::TablePlayfield>(rom);
            render::TableTransitionHold hold,bare_hold;
            std::optional<diagnostics::ReferenceSnapshot> previous;
            unsigned released=0,given=0,straddling=0,lowest=0,last=0,pictured=0;
            for(unsigned n=0;n<1100;++n) {
                runtime->hold_ball(hold_x,hold_y);
                runtime->advance({});
                const auto ram=runtime->work_ram();
                const auto video=runtime->video_snapshot();
                const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                const auto image=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
                // The same frame with no flyers, to see what they add to it.
                Ram without;std::copy(ram.begin(),ram.end(),without.begin());
                for(const std::size_t bank:{upper,lower}) for(const unsigned x:flyers(ram,bank)) without[bank+x]=without[bank+0x40+x]=std::byte{};
                const auto plain=bare->cabinet_frame(video,previous?&*previous:nullptr,without,&raster,bare_hold);
                previous=video;
                const auto above=flyers(ram,upper),below=flyers(ram,lower);
                released=std::max(released,static_cast<unsigned>(above.size()));
                if(!above.empty() || !below.empty()) last=n;
                for(const unsigned x:above) {
                    const int y=position(ram,upper,0x3680,x);
                    require(y<281,"A flyer went on in the upper half past the seam");
                    // At 272 its last row is the upper picture's last, 239; the
                    // next update takes it over the edge, with the lower half's.
                    if(y<273) continue;
                    // Both halves hold it across the seam: the same flyer, 216 rows apart.
                    const auto twin=std::find_if(below.begin(),below.end(),[&](unsigned slot) {
                        return position(ram,lower,0x35c0,slot)==position(ram,upper,0x35c0,x) &&
                            position(ram,lower,0x3680,slot)==y-216 && field(ram,lower,0x3640,slot)==field(ram,upper,0x3640,x) &&
                            field(ram,lower,0x3800,slot)==field(ram,upper,0x3800,x);
                    });
                    require(twin!=below.end(),"A flyer at the seam is not in the lower half too");
                    ++straddling;
                    // Pose 120 fills all 32 rows of the sprite. Rows 212..243 of
                    // the picture at the least: neither half's edge (240 and
                    // 216) cuts it.
                    if(field(ram,upper,0x3800,x)!=120 || !image || !plain) continue;
                    const int top=y-64,left=position(ram,upper,0x35c0,x)-32;
                    int first=456,final=0;
                    for(int row=std::max(0,top-4);row<std::min(456,top+36);++row) for(int column=std::max(0,left);column<std::min(256,left+32);++column)
                        if(image->pixels[static_cast<std::size_t>(row)*256+column]!=plain->pixels[static_cast<std::size_t>(row)*256+column]) {
                            first=std::min(first,row);final=std::max(final,row);
                        }
                    if(first!=top || final!=top+31) std::cerr<<"Flyer at Y "<<y<<" drawn on rows "<<first<<".."<<final<<'\n';
                    require(first==top && final==top+31,"A flyer across the seam is not drawn whole");
                    if(previews && !pictured) preview(*image,"seam-"+std::to_string(ball_half));
                    ++pictured;
                }
                given=std::max(given,static_cast<unsigned>(below.size()));
                for(const unsigned x:below) lowest=std::max(lowest,static_cast<unsigned>(position(ram,lower,0x3680,x)));
                if(previews && image && n%120==0) preview(*image,"half"+std::to_string(ball_half)+"-"+std::to_string(1000+n));
            }
            std::cout<<"Ball in half "<<ball_half<<": "<<released<<" flyers, "<<given<<" in the lower half at once, "<<straddling<<
                " frames across the seam ("<<pictured<<" pictured), lowest Y "<<lowest<<", last seen in frame "<<last<<'\n';
            require(released==4,"The brain did not release four flyers");
            require(given>=2 && straddling>=16 && pictured>=2,"No flyer crossed the seam");
            // The lower half's own bottom edge, Y $140: off the table's picture.
            require(lowest>=0x13e,"The flyers did not reach the bottom of the table");
            require(last<1090 && flyers(runtime->work_ram(),upper).empty() && flyers(runtime->work_ram(),lower).empty(),
                "The flyers were not removed at the table's edges");
            runtime.reset();
        }
        for(const unsigned half:{1u,0u}) {
            // The ball bursts one in the lower half as it does in the upper.
            const double hold_x=half?60:150,hold_y=half?150:420;
            const int origin=half?0:224;
            auto runtime=opened(path,ActorMode::cabinet,half,hold_x,hold_y);
            unsigned slot=0;
            for(unsigned n=0;n<700 && !slot;++n) {
                runtime->hold_ball(hold_x,hold_y);runtime->advance({});
                for(const unsigned x:flyers(runtime->work_ram(),0x1500))
                    if(position(runtime->work_ram(),0x1500,0x3680,x)>=(half?200:110)) slot=x;
            }
            require(slot!=0,"No flyer came down the table");
            // Thrown at it from above: a ball held still touches nothing.
            bool burst=false;
            for(unsigned attempt=0;attempt<6 && !burst;++attempt) for(unsigned n=0;n<40 && !burst;++n) {
                const auto ram=runtime->work_ram();
                burst=field(ram,0x1500,0x3500,slot)==59;
                require(burst || field(ram,0x1500,0x3500,slot)==55,"The flyer left before the ball reached it");
                if(n<8) runtime->hold_ball(position(ram,0x1500,0x35c0,slot)+16,position(ram,0x1500,0x3680,slot)+origin-16+3.0*std::max(0,int(n)-3));
                runtime->advance({});
            }
            require(burst,half?"The ball did not burst a flyer in the upper half":"The ball did not burst a flyer in the lower half");
            runtime.reset();
        }
        {
            // As runtimes marked -actors2 played: the flyers end in the upper half.
            auto runtime=opened(path,ActorMode::cabinet_actors2,0,150,420);
            require(runtime->revision().find(kCabinetActors2Revision)!=std::string_view::npos &&
                recorded_actor_mode(runtime->revision())==ActorMode::cabinet_actors2,"The -actors2 runtime's revision lacks its marker");
            unsigned released=0,deepest=0;
            for(unsigned n=0;n<700;++n) {
                runtime->hold_ball(150,420);runtime->advance({});
                const auto ram=runtime->work_ram();
                require(flyers(ram,0x1500).empty(),"An -actors2 flyer was given to the lower half");
                const auto above=flyers(ram,0x200);
                released=std::max(released,static_cast<unsigned>(above.size()));
                for(const unsigned x:above) deepest=std::max(deepest,static_cast<unsigned>(position(ram,0x200,0x3680,x)));
            }
            require(released==4 && deepest>=300 && deepest<0x140,"-actors2 flyers did not end at the upper half's edge");
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
