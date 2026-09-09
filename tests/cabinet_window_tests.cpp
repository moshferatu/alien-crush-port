// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet.hpp"
#include "window_layout.hpp"
#include "alien_crush/core/files.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}
void geometry_tests() {
    const std::array<SDL_Rect,3> monitors{{{-1080,0,1080,1920},{0,0,1920,1080},{1920,-200,1280,720}}};
    auto check=[&](SDL_Rect window,std::size_t fallback,SDL_Rect expected,const char* message) {
        const auto result=host::visible_window(window,monitors,fallback);
        require(result && SDL_RectsEqual(&*result,&expected),message);
    };
    check({-1000,100,500,700},2,{-1000,100,500,700},"Keep visible portrait window on negative-coordinate monitor");
    check({-100,0,500,400},0,{0,0,500,400},"Choose greatest overlap, not fallback monitor");
    check({3100,400,400,300},0,{2800,220,400,300},"Clamp to moved monitor bounds");
    check({99000,-99000,640,360},2,{2560,-200,640,360},"Recover detached window to role fallback");
    check({99000,-99000,640,360},99,{2560,-200,640,360},"Bound missing fallback index");
    check({-5000,-5000,5000,5000},0,{-1080,0,1080,1920},"Fit oversized window");
    const std::array<SDL_Rect,1> remaining{{{0,0,800,600}}};
    const auto reduced=host::visible_window({-1080,0,1080,1920},remaining,2);
    require(reduced && reduced->x==0 && reduced->y==0 && reduced->w==800 && reduced->h==600,"Unplug portrait monitor and reduce resolution");
    require(!host::visible_window({0,0,320,240},{},0),"No displays defers placement");
    require(!host::visible_window({0,0,0,240},monitors,0),"Reject unavailable window size");
    const std::array<SDL_Rect,1> invalid{{{0,0,0,600}}};
    require(!host::visible_window({0,0,320,240},invalid,0),"Reject unavailable display bounds");
    const std::array<SDL_Rect,1> extreme{{{2147483600,2147483600,1000,1000}}};
    const auto edge=host::visible_window({2147483640,2147483640,10,10},extreme,0);
    require(edge && edge->x==2147483640 && edge->y==2147483640,"Coordinate sums do not overflow");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and test data directory");
        geometry_tests();
        require(SDL_Init(SDL_INIT_VIDEO),"Initialize SDL");
        const auto data=core::path_from_utf8(argv[2]);
        std::filesystem::create_directories(data);
        // A known fixture makes repeated test runs independent of saved geometry.
        {
            std::ofstream config(data/"cabinet-windows.txt");
            config<<"ACWINDOWS 1\nplayfield \"Test\" 0 0 320 240 0 0\n"
                  <<"backglass \"Test\" 0 0 320 240 0 0\nscore \"Test\" 0 0 320 240 0 0\n";
        }
        {
            const auto rom=core::read_file(core::path_from_utf8(argv[1]));
            host::Presentation playfield("Test playfield",320,240,true);
            host::Cabinet cabinet(playfield,data,rom);
            int count{};auto** all=SDL_GetWindows(&count);
            require(all && count==3,"Three cabinet windows");
            std::array<SDL_WindowID,3> ids{};
            ids[0]=SDL_GetWindowID(playfield.window());
            for(int i=0;i<count;++i) {
                const std::string title=SDL_GetWindowTitle(all[i]);
                if(title=="Alien Crush - Backglass") ids[1]=SDL_GetWindowID(all[i]);
                if(title=="Alien Crush - Score") ids[2]=SDL_GetWindowID(all[i]);
            }
            SDL_free(all);
            require(ids[0] && ids[1] && ids[2],"Find cabinet roles");
            auto key=[&](SDL_WindowID id,SDL_Scancode code,bool reverse=false,bool repeat=false) {
                SDL_KeyboardEvent event{};event.type=SDL_EVENT_KEY_DOWN;event.windowID=id;
                event.scancode=code;event.repeat=repeat;event.mod=reverse?SDL_KMOD_SHIFT:SDL_KMOD_NONE;
                return cabinet.handle_key(event);
            };
            require(!key(0,SDL_SCANCODE_R),"Ignore unknown window");
            require(!key(ids[0],SDL_SCANCODE_R,false,true),"Ignore key repeat");
            require(!key(ids[0],SDL_SCANCODE_Z),"Leave gameplay keys alone");
            for(unsigned i=0;i<3;++i) for(unsigned turn=0;turn<=i;++turn)
                require(key(ids[i],SDL_SCANCODE_R),"Rotate selected window");
            require(playfield.rotation()==90,"Auxiliary rotation leaves playfield alone");
            require(key(ids[1],SDL_SCANCODE_R,true),"Reverse rotation");
            for(int turn=0;turn<4;++turn) require(key(ids[2],SDL_SCANCODE_R),"Rotation wraps");
            for(const auto id:ids) {
                auto* window=SDL_GetWindowFromID(id);
                require(key(id,SDL_SCANCODE_F11),"Enter fullscreen");
                require((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0,"Selected window fullscreen");
                require(cabinet.recover_displays(),"Recover connected fullscreen topology");
                require((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0,"Recovery retains fullscreen");
                require(key(id,SDL_SCANCODE_F10),"Single-display cycle");
                require((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0,"Monitor cycle retains fullscreen");
                require(key(id,SDL_SCANCODE_F11),"Leave fullscreen");
                require((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)==0,"Selected window windowed");
                require(key(id,SDL_SCANCODE_F10,true),"Reverse single-display cycle");
                require(key(id,SDL_SCANCODE_F6),"Select next cabinet role");
                require(key(id,SDL_SCANCODE_F6,true),"Select previous cabinet role");
            }
            for(const auto id:ids) {
                auto* window=SDL_GetWindowFromID(id);
                require(SDL_SetWindowPosition(window,99000,-99000),"Displace window for recovery");
                require(SDL_SetWindowSize(window,4000,3000),"Oversize window for recovery");
            }
            require(cabinet.recover_displays(),"Recover three displaced windows");
            std::array<SDL_Rect,3> recovered{};
            for(std::size_t i=0;i<ids.size();++i) {
                auto* window=SDL_GetWindowFromID(ids[i]);auto& r=recovered[i];SDL_Rect b{};
                require(SDL_GetWindowPosition(window,&r.x,&r.y) && SDL_GetWindowSize(window,&r.w,&r.h),"Read recovered geometry");
                require(SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(window),&b),"Read connected display bounds");
                require(r.x>=b.x && r.y>=b.y && r.x+r.w<=b.x+b.w && r.y+r.h<=b.y+b.h,"Recovered window fully visible");
            }
            require(cabinet.recover_displays(),"Repeat layout notification");
            for(std::size_t i=0;i<ids.size();++i) {
                auto* window=SDL_GetWindowFromID(ids[i]);SDL_Rect r{};
                require(SDL_GetWindowPosition(window,&r.x,&r.y) && SDL_GetWindowSize(window,&r.w,&r.h),"Read stable geometry");
                require(SDL_RectsEqual(&r,&recovered[i]),"Repeated notification leaves visible windows unchanged");
            }
            cabinet.save();
        }
        {
            std::ifstream config(data/"cabinet-windows.txt");
            std::string magic;int version{};config>>magic>>version;
            const std::array<int,3> expected{90,90,270};
            for(const auto rotation:expected) {
                std::string role,name;int x{},y{},w{},h{},fullscreen{},actual{};
                config>>role>>std::quoted(name)>>x>>y>>w>>h>>fullscreen>>actual;
                require(static_cast<bool>(config) && actual==rotation && fullscreen==0,"Save each role's rotation and fullscreen");
            }
            config.close();
            const auto rom=core::read_file(core::path_from_utf8(argv[1]));
            host::Presentation playfield("Restored playfield",320,240,true);
            host::Cabinet cabinet(playfield,data,rom);
            require(playfield.rotation()==90,"Restore configured rotation");
            cabinet.save();
        }
        SDL_Quit();
        std::cout<<"PASS: cabinet window selection, rotation, display recovery, fullscreen and persistence\n";
        return 0;
    } catch(const std::exception& e) {SDL_Quit();std::cerr<<e.what()<<'\n';return 1;}
}
