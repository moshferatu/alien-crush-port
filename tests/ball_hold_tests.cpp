// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Ball { unsigned table{}; double x{},y{}; int velocity_x{},velocity_y{}; };
// Y as hold_ball takes it: the lower half's is 224 more.
Ball ball(std::span<const std::byte> ram) {
    auto b=[&](unsigned at){return std::to_integer<unsigned>(ram[at]);};
    const unsigned table=b(0x79);
    return {table,(b(0x1580)|(b(0x15c0)<<8)|(b(0x1600)<<16))/256.0,
        (b(0x1640)|(b(0x1680)<<8)|(b(0x16c0)<<16))/256.0+(table==0?224:0),
        static_cast<std::int16_t>(b(0x1700)|(b(0x1740)<<8)),static_cast<std::int16_t>(b(0x1780)|(b(0x17c0)<<8))};
}
// Hold the ball at a place for `frames`, as the host does before each frame.
Ball hold(ReferenceRuntime& runtime,double x,double y,unsigned frames) {
    for(unsigned n=0;n<frames;++n) {
        runtime.hold_ball(x,y);
        runtime.advance({});
    }
    return ball(runtime.work_ram());
}
bool near(const Ball& at,double x,double y) {return std::abs(at.x-x)<4 && std::abs(at.y-y)<4;}
void require(bool ok,const char* message,const Ball& at) {
    if(ok) return;
    std::cerr<<"Ball in half "<<at.table<<" at "<<at.x<<','<<at.y<<" moving "<<at.velocity_x<<','<<at.velocity_y<<'\n';
    throw std::runtime_error(message);
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM");
        const auto path=core::path_from_utf8(argv[1]);
        // The whole table's drawn Y and the game's world Y, both ways.
        for(double y=160;y<=420;y+=0.25) {
            const double drawn=y-render::table_ball_seam_correction(y);
            require(std::abs(render::table_ball_world_y(drawn)-y)<1e-6,"A drawn Y maps back to its world Y");
        }
        for(const bool cabinet:{false,true}) {
            auto runtime=cabinet?make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1,{},ActorMode::cabinet):
                make_reference_runtime();
            runtime->load(path);
            for(unsigned n=0;n<120;++n) runtime->advance({});
            const std::vector<std::byte> before(runtime->work_ram().begin(),runtime->work_ram().end());
            require(!runtime->hold_ball(150,400) && std::equal(before.begin(),before.end(),runtime->work_ram().begin()),
                "There is no ball to hold at the title");
            start_with_score(*runtime,0);
            require(reference_launcher_seat(runtime->work_ram()).has_value(),"The game starts in the shooter");
            // Out of the shooter to the lower table, and held there.
            auto at=hold(*runtime,150,360,20);
            require(at.table==0 && near(at,150,360) && at.velocity_x==0,"The ball is held on the lower table",at);
            // To the upper table by the game's own crossing: at once in the
            // cabinet, after the cartridge's loading otherwise.
            unsigned frames=0;
            for(;frames<240 && !(at.table==1 && near(at,120,150));++frames) at=hold(*runtime,120,150,1);
            require(at.table==1 && near(at,120,150),"The ball is carried to the upper table",at);
            require(!cabinet || frames<=3,"The cabinet changes halves without loading");
            std::cout<<(cabinet?"Cabinet":"Desktop")<<": upper table after "<<frames<<" frames\n";
            at=hold(*runtime,120,150,20);
            require(at.table==1 && near(at,120,150),"The ball is held on the upper table",at);
            if(!cabinet) {
                // The host takes the picture's corner for a sprite at (32,64):
                // the ball's sixteen pixels are drawn where that puts them.
                const auto piece=assets::ActorAssets(core::read_file(path)).pieces(0).at(0);
                std::vector<std::uint32_t> first;
                std::vector<std::uint8_t> changed;
                int width=0,height=0;
                for(const double x:{120.0,176.0}) {
                    hold(*runtime,x,150,3);
                    runtime->hold_ball(x,150);
                    const auto frame=runtime->advance({});
                    if(first.empty()) {first.assign(frame.pixels.begin(),frame.pixels.end());width=frame.width;height=frame.height;continue;}
                    require(frame.width==width && frame.height==height,"The picture keeps its size");
                    changed.resize(first.size());
                    for(std::size_t i=0;i<first.size();++i) changed[i]=first[i]!=frame.pixels[i];
                }
                // Other things on the table move too, so compare the sixteen
                // pixels with every place near them.
                auto count=[&](int left,int top) {
                    unsigned total=0;
                    for(int py=std::max(0,top);py<std::min(height,top+16);++py) for(int px=std::max(0,left);px<std::min(width,left+16);++px)
                        total+=changed[static_cast<std::size_t>(py)*width+px];
                    return total;
                };
                for(const int x:{120,176}) {
                    const int left=x+piece.x-32,top=150+piece.y-64;
                    const auto here=count(left,top);
                    require(here>=80,"The ball is drawn where the host's pointer mapping puts it");
                    // The ball is rounder than its sixteen pixels, so a pixel
                    // or two either way holds as much of it.
                    for(int dy=-12;dy<=12;++dy) for(int dx=-12;dx<=12;++dx) {
                        const auto there=count(left+dx,top+dy);
                        if(there>here || (there==here && std::max(std::abs(dx),std::abs(dy))>2))
                            std::cerr<<"As much at "<<dx<<','<<dy<<": "<<there<<" against "<<here<<'\n';
                        require(there<=here && (there<here || std::max(std::abs(dx),std::abs(dy))<=2),"The ball is drawn nowhere else nearby");
                    }
                    std::cout<<"Desktop picture "<<width<<'x'<<height<<": ball at "<<left<<','<<top<<" for X "<<x<<", "<<here<<" pixels\n";
                }
            }
            // And back, from either side of the seam's two thresholds.
            for(frames=0;frames<240 && !(at.table==0 && near(at,150,300));++frames) at=hold(*runtime,150,300,1);
            require(at.table==0 && near(at,150,300),"The ball is carried back to the lower table",at);
            at=hold(*runtime,150,300,20);
            require(at.table==0 && near(at,150,300),"The ball stays in the lower half just below the seam",at);
            for(frames=0;frames<240 && !(at.table==1 && near(at,150,280));++frames) at=hold(*runtime,150,280,1);
            require(at.table==1 && near(at,150,280),"The ball is carried just above the seam",at);
            at=hold(*runtime,150,280,20);
            require(at.table==1 && near(at,150,280),"The ball stays in the upper half just above the seam",at);
            // Let go while moving: the ball keeps the motion.
            for(unsigned n=0;n<6;++n) at=hold(*runtime,150.0-3*n,200,1);
            require(at.velocity_x<=-0x200,"A moving hold gives the ball its velocity",at);
            // Paused, the ball is left alone.
            runtime->advance({Pad::run,0});
            for(unsigned n=0;n<30;++n) runtime->advance({});
            if(std::to_integer<unsigned>(runtime->work_ram()[0xf6])&2) {
                const std::vector<std::byte> paused(runtime->work_ram().begin(),runtime->work_ram().end());
                require(!runtime->hold_ball(100,100) && std::equal(paused.begin(),paused.end(),runtime->work_ram().begin()),
                    "A paused game's ball is not held");
            } else throw std::runtime_error("The game did not pause");
            // A bonus room's ball, in the room's own coordinates.
            runtime->load(path);
            start_in_bonus_room(*runtime,1);
            bool held=false;
            for(unsigned n=0;n<600 && !held;++n) {runtime->advance({});held=runtime->hold_ball(128,200);}
            require(held,"A bonus room's ball can be held");
            at=hold(*runtime,128,200,20);
            require(diagnostics::observe_alien_state(runtime->work_ram()).bonus_room==1 && std::abs(at.x-128)<4 &&
                std::abs(at.y-200)<4,"The ball is held in a bonus room");
            runtime.reset();
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
