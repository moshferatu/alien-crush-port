// SPDX-License-Identifier: GPL-3.0-or-later
// The cocoons' aliens in the cabinet: one that flies up to the seam becomes
// the upper half's (gameplay::CabinetActors::carry_flyers), so the ball can
// burst it there as it does below, and the picture draws it from the lower
// table's patterns, which the upper table does not have.
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
bool alien(unsigned type) {return type==40 || type==41 || type==52 || type==53;}
std::vector<unsigned> aliens(std::span<const std::byte> ram,std::size_t bank) {
    std::vector<unsigned> result;
    for(unsigned x=4;x<64;++x) if(alien(field(ram,bank,0x3500,x))) result.push_back(x);
    return result;
}
// Hundreds of points, as 00:FA27 keeps them.
unsigned score(std::span<const std::byte> ram) {return get(ram,0x121e)|(get(ram,0x1224)<<8)|(get(ram,0x122a)<<16);}
void preview(const render::Image& image,const std::string& name) {
    std::filesystem::create_directories("cocoon-previews");
    std::ofstream out("cocoon-previews/"+name+".bmp",std::ios::binary);
    const auto word=[&](unsigned value,unsigned bytes) {
        for(unsigned i=0;i<bytes;++i) out.put(static_cast<char>(value>>(8*i)));
    };
    out<<"BM";word(54+256*456*4,4);word(0,4);word(54,4);word(40,4);
    word(256,4);word(456,4);word(1,2);word(32,2);word(0,4);word(256*456*4,4);
    for(unsigned i=0;i<4;++i) word(0,4);
    for(int y=455;y>=0;--y) for(int x=0;x<256;++x) word(image.pixels[static_cast<std::size_t>(y)*256+x],4);
}
// A game in play with the ball held in a half, and both cocoons hatched as
// 01:556B / 5574 do it: an unfolding alien eight rows above each.
std::unique_ptr<ReferenceRuntime> hatched(const std::filesystem::path& path,ActorMode mode,unsigned ball_half,double x,double y) {
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1,{},mode);
    runtime->load(path);runtime->observe_display(true);
    for(unsigned n=0;n<120;++n) runtime->advance({});
    start_with_score(*runtime,0);
    for(unsigned n=0;n<120;++n) {runtime->hold_ball(x,y);runtime->advance({});}
    require(get(runtime->work_ram(),0x79)==ball_half,"The ball is not held in its half");
    const std::size_t lower=ball_half?0x200:0x1500;
    auto state=runtime->save();
    Ram ram;std::copy(runtime->work_ram().begin(),runtime->work_ram().end(),ram.begin());
    const auto found=std::search(state.core_state.begin(),state.core_state.end(),ram.begin(),ram.end());
    require(found!=state.core_state.end(),"Work RAM is not in the saved state");
    unsigned made=0;
    for(unsigned cocoon=4;cocoon<64;++cocoon) {
        const unsigned type=get(ram,lower+cocoon);
        if(type!=26 && type!=27) continue;
        unsigned slot=4;
        while(slot<0x3c && ram[lower+slot]!=std::byte{}) ++slot;
        require(slot<0x3c,"The lower half has no free slot");
        const auto put=[&](unsigned address,unsigned value) {ram[lower+address-0x3500+slot]=static_cast<std::byte>(value&255);};
        const int top=position(ram,lower,0x3680,cocoon)-8;
        put(0x3500,type==26?40:41);put(0x35c0,field(ram,lower,0x35c0,cocoon));put(0x3600,field(ram,lower,0x3600,cocoon));
        put(0x3680,static_cast<unsigned>(top));put(0x36c0,static_cast<unsigned>(top)>>8);
        ++made;
    }
    require(made==2,"The lower table does not have its two cocoons");
    std::copy(ram.begin(),ram.end(),found);
    runtime->restore(state);
    return runtime;
}
// The hand-over alone, on RAM with nothing else in it.
void synthetic(std::span<const std::byte> rom) {
    const auto flyer=[](Ram& ram,std::size_t bank,unsigned x,unsigned y) {
        const auto put=[&](unsigned address,unsigned value) {ram[bank+address-0x3500+x]=static_cast<std::byte>(value&255);};
        put(0x3500,52);put(0x3540,0x84);put(0x3840,1);put(0x3a00,3);put(0x3a40,3);put(0x3b80,0x20);put(0x3800,79);
        put(0x35c0,0x58);put(0x3680,y);put(0x36c0,y>>8);
    };
    for(const unsigned ball_half:{0u,1u}) {
        const std::size_t upper=ball_half?0x1500:0x200,lower=0x1700-upper;
        gameplay::CabinetActors actors(rom,gameplay::CabinetActors::Scope::table);
        Ram ram{};
        ram[0x79]=static_cast<std::byte>(ball_half);
        flyer(ram,lower,40,64);
        // The slots in the way are taken: it goes to the first free one.
        for(unsigned x=4;x<9;++x) {ram[upper+x]=std::byte{48};ram[upper+0x40+x]=std::byte{0xc0};}
        actors.update_inactive(ram,1);
        require(aliens(ram,upper).empty() && aliens(ram,lower).size()==1,"An alien the ball can still touch below was taken");
        ram[lower+0x3680-0x3500+40]=std::byte{63};ram[lower+0x3640-0x3500+40]=std::byte{};
        actors.update_inactive(ram,2);
        require(aliens(ram,lower).empty() && aliens(ram,upper)==std::vector<unsigned>{9},
            "An alien at the top of the lower half does not become the upper half's");
        require(field(ram,upper,0x3500,9)==52 && position(ram,upper,0x35c0,9)==0x58 && (field(ram,upper,0x3540,9)&0xe1)==0xa0 &&
            field(ram,upper,0x3840,9)==1 && std::abs(position(ram,upper,0x3680,9)-(63+216))<=1,
            "The upper half's alien is the lower's, 216 rows down and hidden from its table's sprites");
        // No room in the upper half: the alien stays the lower's.
        gameplay::CabinetActors crowded(rom,gameplay::CabinetActors::Scope::table);
        Ram full{};
        full[0x79]=static_cast<std::byte>(ball_half);
        flyer(full,lower,40,60);
        for(unsigned x=4;x<0x3c;++x) {full[upper+x]=std::byte{48};full[upper+0x40+x]=std::byte{0xc0};}
        crowded.update_inactive(full,1);
        require(aliens(full,lower).size()==1 && aliens(full,upper).empty(),"An alien with no slot to go to left the lower half");
        // Runtimes up to -actors2 left it there always.
        gameplay::CabinetActors before(rom,gameplay::CabinetActors::Scope::table,true,true,false);
        Ram old{};
        old[0x79]=static_cast<std::byte>(ball_half);
        flyer(old,lower,40,60);
        before.update_inactive(old,1);
        require(aliens(old,lower).size()==1 && aliens(old,upper).empty(),"An -actors2 alien left the lower half");
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
            const double hold_x=ball_half?128:150,hold_y=ball_half?100:420;
            const std::size_t upper=ball_half?0x1500:0x200,lower=0x1700-upper;
            auto runtime=hatched(path,ActorMode::cabinet,ball_half,hold_x,hold_y);
            auto table=std::make_unique<render::TablePlayfield>(rom),bare=std::make_unique<render::TablePlayfield>(rom);
            render::TableTransitionHold hold,bare_hold;
            std::optional<diagnostics::ReferenceSnapshot> previous;
            unsigned below_most=0,above_most=0,pictured=0,last=0;
            int highest=1000;
            for(unsigned n=0;n<1100;++n) {
                runtime->hold_ball(hold_x,hold_y);
                runtime->advance({});
                const auto ram=runtime->work_ram();
                const auto video=runtime->video_snapshot();
                const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                const auto image=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
                // The same frame with no aliens, to see what they add to it.
                Ram without;std::copy(ram.begin(),ram.end(),without.begin());
                for(const std::size_t bank:{upper,lower}) for(const unsigned x:aliens(ram,bank)) without[bank+x]=without[bank+0x40+x]=std::byte{};
                const auto plain=bare->cabinet_frame(video,previous?&*previous:nullptr,without,&raster,bare_hold);
                previous=video;
                const auto above=aliens(ram,upper),below=aliens(ram,lower);
                below_most=std::max(below_most,static_cast<unsigned>(below.size()));
                above_most=std::max(above_most,static_cast<unsigned>(above.size()));
                if(!above.empty() || !below.empty()) last=n;
                for(const unsigned x:below) require(position(ram,lower,0x3680,x)>=63,"An alien went on in the lower half past its top edge");
                for(const unsigned x:above) highest=std::min(highest,position(ram,upper,0x3680,x));
                // The first pose of its flight: three pieces over 48 rows, each
                // 16 further across.
                for(const auto& [bank,origin,slots]:{std::tuple{upper,0,&above},std::tuple{lower,216,&below}}) for(const unsigned x:*slots) {
                    const unsigned pose=field(ram,bank,0x3800,x);
                    if((pose!=79 && pose!=114) || !image || !plain) continue;
                    const int y=position(ram,bank,0x3680,x)+origin,top=y-64;
                    const int left=position(ram,bank,0x35c0,x)-32-(pose==114?32:0);
                    if(left<0 || left+64>256 || top<0) continue;
                    int first=456,final=0;
                    for(int row=std::max(0,top-4);row<std::min(456,top+52);++row) for(int column=left;column<left+64;++column)
                        if(image->pixels[static_cast<std::size_t>(row)*256+column]!=plain->pixels[static_cast<std::size_t>(row)*256+column]) {
                            first=std::min(first,row);final=std::max(final,row);
                        }
                    // What it passes over hides a row or two of its edges; an
                    // edge of either half's picture would take many more.
                    if(first>top+3 || final<top+42)
                        std::cerr<<"Alien at Y "<<y<<" drawn on rows "<<first<<".."<<final<<" of "<<top<<".."<<top+47<<'\n';
                    require(first<=top+3 && final>=top+42,"An alien is not drawn whole");
                    // The seam's rows, 216..239, are within it.
                    if(top<216 && top+47>=240) {
                        if(previews && !pictured) preview(*image,"seam-"+std::to_string(ball_half));
                        ++pictured;
                    }
                }
                if(previews && image && n%120==0) preview(*image,"half"+std::to_string(ball_half)+"-"+std::to_string(1000+n));
            }
            std::cout<<"Ball in half "<<ball_half<<": "<<below_most<<" aliens below, "<<above_most<<" above, "<<pictured<<
                " pictured across the seam, highest Y "<<highest<<", last seen in frame "<<last<<'\n';
            require(below_most==2 && above_most==2,"The two aliens did not cross to the upper half");
            require(pictured>=8,"No alien was pictured across the seam");
            require(highest<200,"The aliens did not fly on up the upper table");
            require(last<1090 && aliens(runtime->work_ram(),upper).empty() && aliens(runtime->work_ram(),lower).empty(),
                "The aliens were not removed at the table's edges");
            runtime.reset();
        }
        for(const unsigned half:{0u,1u}) {
            // The ball bursts one in the upper half as it does in the lower:
            // 3,000 points (01:5703 / 57B6) and the explosion.
            const double hold_x=half?128:150,hold_y=half?100:420;
            const int origin=half?0:224;
            auto runtime=hatched(path,ActorMode::cabinet,half,hold_x,hold_y);
            unsigned slot=0;
            for(unsigned n=0;n<700 && !slot;++n) {
                runtime->hold_ball(hold_x,hold_y);runtime->advance({});
                const auto ram=runtime->work_ram();
                for(const unsigned x:aliens(ram,0x1500)) {
                    const int y=position(ram,0x1500,0x3680,x);
                    if(field(ram,0x1500,0x3500,x)==52 && (half?y<250:y<150)) slot=x;
                }
            }
            require(slot!=0,half?"No alien came up into the upper half":"No alien flew up the lower half");
            require(!half || (field(runtime->work_ram(),0x1500,0x3540,slot)&0x20),"The upper half's alien is not hidden from its sprites");
            const unsigned before=score(runtime->work_ram());
            // Thrown at it from above: a ball held still touches nothing.
            bool burst=false;
            for(unsigned attempt=0;attempt<6 && !burst;++attempt) for(unsigned n=0;n<40 && !burst;++n) {
                const auto ram=runtime->work_ram();
                burst=field(ram,0x1500,0x3500,slot)==50;
                require(burst || field(ram,0x1500,0x3500,slot)==52,"The alien left before the ball reached it");
                if(n<8) runtime->hold_ball(position(ram,0x1500,0x35c0,slot)+24,position(ram,0x1500,0x3680,slot)+origin-16+3.0*std::max(0,int(n)-3));
                runtime->advance({});
            }
            require(burst,half?"The ball did not burst an alien in the upper half":"The ball did not burst an alien in the lower half");
            require((field(runtime->work_ram(),0x1500,0x3540,slot)&0x20)==0,"The explosion is hidden");
            std::cout<<"Burst in half "<<half<<": "<<(score(runtime->work_ram())-before)*100<<" points\n";
            // The thrown ball can touch something else of the table's too.
            const unsigned gained=score(runtime->work_ram())-before;
            require(gained>=30 && gained<=40,"A burst alien does not score its 3,000 points");
            runtime.reset();
        }
        {
            // As runtimes marked -actors2 played: the aliens stay the lower half's.
            auto runtime=hatched(path,ActorMode::cabinet_actors2,1,128,100);
            unsigned most=0;
            for(unsigned n=0;n<1100;++n) {
                runtime->hold_ball(128,100);runtime->advance({});
                const auto ram=runtime->work_ram();
                require(aliens(ram,0x1500).empty(),"An -actors2 alien was given to the upper half");
                most=std::max(most,static_cast<unsigned>(aliens(ram,0x200).size()));
            }
            require(most==2 && aliens(runtime->work_ram(),0x200).empty(),"-actors2 aliens did not end in the lower half");
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
