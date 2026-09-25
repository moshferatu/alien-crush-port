// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet.hpp"
#include "startup_display.hpp"
#include "window_layout.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/core/recording.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}
void require_black_window(SDL_Window* window) {
    require(!(SDL_GetWindowFlags(window)&SDL_WINDOW_HIDDEN),"Startup window is visible");
    // Read the renderer directly: Presentation::capture redraws and would mask
    // a missing startup clear.
    auto* surface=SDL_RenderReadPixels(SDL_GetRenderer(window),nullptr);
    require(surface!=nullptr,"Read startup pixels");
    auto* pixels=SDL_ConvertSurface(surface,SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(surface);
    require(pixels!=nullptr,"Convert startup pixels");
    bool black=true;
    for(int y=0;y<pixels->h;++y) {
        const auto* row=reinterpret_cast<const Uint32*>(static_cast<const Uint8*>(pixels->pixels)+y*pixels->pitch);
        for(int x=0;x<pixels->w;++x) black=black && (row[x]&0xffffff)==0;
    }
    SDL_DestroySurface(pixels);
    require(black,"Startup is painted black before any game frame");
}
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
        require(argc==3 || argc==4,"Expected ROM and test data directory");
        geometry_tests();
        require(SDL_Init(SDL_INIT_VIDEO),"Initialize SDL");
        {
            host::Presentation desktop("Desktop startup",320,240);
            require_black_window(desktop.window());
            require(desktop.pictures()==0,"Startup clear does not count as a game picture");
        }
        if(argc==4 && std::string(argv[3])=="--startup-only") {
            {
                host::Presentation playfield("Cabinet startup",320,240,true);
                require(SDL_GetWindowFlags(playfield.window())&SDL_WINDOW_HIDDEN,"Cabinet starts hidden");
                require(SDL_SetWindowSize(playfield.window(),384,768),"Restore portrait size");
                playfield.reveal();
                require_black_window(playfield.window());
                require(SDL_HideWindow(playfield.window()),"Hide before fullscreen startup");
                require(SDL_SetWindowFullscreen(playfield.window(),true),"Restore fullscreen");
                playfield.reveal();
                require_black_window(playfield.window());
            }
            SDL_Quit();
            std::cout<<"PASS: desktop, portrait and fullscreen startup pixels are black\n";
            return 0;
        }
        {
            host::Presentation playfield("Bonus alignment",384,768,true);
            const std::vector<std::uint32_t> pixels(256*240,0xff80c040);
            for(const int rotation:{0,90,180,270}) {
                playfield.rotation(rotation);
                playfield.show({1,256,240,pixels,{}},4,3,true);
                auto* capture=SDL_RenderReadPixels(SDL_GetRenderer(playfield.window()),nullptr);
                require(capture!=nullptr,"Read bottom-aligned bonus viewport");
                Uint8 r{},g{},b{},a{};
                const bool bottom=SDL_ReadSurfacePixel(capture,capture->w/2,capture->h-1,&r,&g,&b,&a) && g>150;
                const bool top=SDL_ReadSurfacePixel(capture,capture->w/2,0,&r,&g,&b,&a) && r==0 && g==0 && b==0;
                if(!bottom || !top) {
                    std::cerr<<"rotation="<<rotation<<" bottom="<<bottom<<" top="<<top<<'\n';
                    SDL_SaveBMP(capture,"bonus-alignment-failure.bmp");
                }
                SDL_DestroySurface(capture);
                require(bottom && top,"Bonus viewport reaches physical bottom at every cabinet rotation");
            }
        }
        const auto data=core::path_from_utf8(argv[2]);
        std::filesystem::create_directories(data);
        {
            auto runtime=make_reference_runtime();runtime->load(core::path_from_utf8(argv[1]));
            Frame frame;
            host::StartupDisplay startup;
            host::Presentation boot("Boot frames",256,240,true);
            for(int i=0;i<240;++i) {
                frame=runtime->advance({});
                if(i<30) {
                    std::vector<std::uint32_t> pixels(frame.pixels.begin(),frame.pixels.end());
                    const auto snapshot=runtime->video_snapshot();
                    const bool blank=startup.blank(snapshot.control);
                    require(blank==(i<19),"Mask exactly the ROM's nineteen display-disabled boot frames");
                    if(blank) {
                        require(std::count_if(pixels.begin(),pixels.end(),[](auto p){return (p&0xffffff)==0xffffff;})>frame.width*frame.height/2,"Original boot output contains the white flash");
                        std::fill(pixels.begin(),pixels.end(),0xff000000);
                    }
                    boot.show({frame.number,frame.width,frame.height,pixels,{}},256,240);
                    const auto shown=boot.capture();
                    require(shown.pixels.size()==pixels.size() && std::equal(shown.pixels.begin(),shown.pixels.end(),pixels.begin(),
                        [](auto a,auto b){return (a&0xffffff)==(b&0xffffff);}),"Displayed startup pixels match black mask or original title");
                }
            }
            require(!startup.blank(0),"Later game blanking never re-arms startup masking");
            const std::vector<std::uint32_t> pixels(frame.pixels.begin(),frame.pixels.end());
            frame.pixels=pixels;
            const auto video=runtime->video_snapshot();
            const auto tile=render::compose(video.vram,video.palette,video.sat,
                {96,96,video.scroll_x,video.scroll_y,video.memory_width,0x80});
            host::Presentation title("Centered tiled title",384,768,true);
            for(const int rotation:{0,90,180,270}) {
                title.rotation(rotation);
                title.show(frame,4,3,false,{frame.number,96,96,tile.pixels,{}});
                auto* capture=SDL_RenderReadPixels(SDL_GetRenderer(title.window()),nullptr);
                require(capture!=nullptr,"Read tiled title viewport");
                for(const int y:{0,capture->h-1}) {
                    int colored=0;
                    for(int x=0;x<capture->w;++x) {
                        Uint8 r{},g{},b{},a{};
                        require(SDL_ReadSurfacePixel(capture,x,y,&r,&g,&b,&a),"Read title edge pixel");
                        if(r || g || b) ++colored;
                    }
                    require(colored>capture->w/4,"Scrolling title background fills both physical window edges at every rotation");
                }
                if(rotation==0) SDL_SaveBMP(capture,(data/"centered-title.bmp").string().c_str());
                SDL_DestroySurface(capture);
                title.repeat();
            }
            title.rotation(0);
            title.show(frame);
            auto* capture=SDL_RenderReadPixels(SDL_GetRenderer(title.window()),nullptr);
            require(capture!=nullptr,"Read title exit viewport");
            Uint8 r{},g{},b{},a{};
            require(SDL_ReadSurfacePixel(capture,0,0,&r,&g,&b,&a) && r==0 && g==0 && b==0,
                "Title background clears when the next scene does not request tiling");
            SDL_DestroySurface(capture);
        }
        {
            auto runtime=make_reference_runtime();runtime->load(core::path_from_utf8(argv[1]));
            Frame frame;
            for(unsigned n=0;n<800;++n) frame=runtime->advance(n<600?recording::canonical_input(n):
                (n==600?PadInput{Pad::run,0}:(n==650?PadInput{Pad::select,0}:PadInput{})));
            const std::vector<std::uint32_t> pixels(frame.pixels.begin(),frame.pixels.end());frame.pixels=pixels;
            host::Presentation preview("High scores",384,768,true);
            preview.show(frame);
            auto* capture=SDL_RenderReadPixels(SDL_GetRenderer(preview.window()),nullptr);
            require(capture!=nullptr,"Read centered high-score artwork");
            require(SDL_SaveBMP(capture,(data/"centered-high-scores.bmp").string().c_str()),"Save high-score preview");
            SDL_DestroySurface(capture);
            // Verify centered artwork and plain black margins at exact pixel scale.
            host::Presentation exact("High-score edges",256,480,true);
            for(int rotation:{0,180}) {
                exact.rotation(rotation);
                exact.show(frame,256,240);
                auto* image=SDL_RenderReadPixels(SDL_GetRenderer(exact.window()),nullptr);
                require(image!=nullptr,"Read centered high-score screen");
                require(image->w==256 && image->h==480 && frame.width==256 && frame.height==240,"Exact high-score test dimensions");
                for(int y=0;y<480;++y) for(int x=0;x<256;++x) {
                    const int upright_y=rotation==0?y:479-y;
                    const int source_x=rotation==0?x:255-x;
                    const int source_y=upright_y-120;
                    const auto expected=source_y>=0 && source_y<240?pixels[source_y*256+source_x]&0xffffff:0;
                    Uint8 r{},g{},b{},a{};
                    require(SDL_ReadSurfacePixel(image,x,y,&r,&g,&b,&a),"Read high-score pixel");
                    require(((static_cast<unsigned>(r)<<16)|(static_cast<unsigned>(g)<<8)|b)==
                        expected,"Centered scores remain pixel-exact with black margins above and below");
                }
                SDL_DestroySurface(image);
            }
            require(SDL_SetWindowSize(exact.window(),640,240),"Resize to landscape");
            exact.rotation(0);
            exact.show(frame,256,240);
            capture=SDL_RenderReadPixels(SDL_GetRenderer(exact.window()),nullptr);
            require(capture!=nullptr,"Read landscape high-score view");
            Uint8 r{},g{},b{},a{};
            require(SDL_ReadSurfacePixel(capture,0,capture->h/2,&r,&g,&b,&a) && r==0 && g==0 && b==0,
                "High-score decoration never tiles horizontally");
            SDL_DestroySurface(capture);
        }
        // A known fixture makes repeated test runs independent of saved geometry.
        host::CabinetConfig fixture;
        fixture.input.plunger.axis=2;
        for(auto& window:fixture.windows) window.display_identity="Test",window.geometry={0,0,320,240};
        host::save_cabinet_config(data/"cabinet.ini",fixture);
        {
            const auto rom=core::read_file(core::path_from_utf8(argv[1]));
            host::Presentation playfield("Test playfield",320,240,true);
            host::Cabinet cabinet(playfield,host::load_cabinet_config(data).config.windows,data,rom);
            int count{};auto** all=SDL_GetWindows(&count);
            require(all && count==3,"Three cabinet windows");
            std::array<SDL_WindowID,3> ids{};
            ids[0]=SDL_GetWindowID(playfield.window());
            for(int i=0;i<count;++i) {
                require_black_window(all[i]);
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
            auto saved=host::load_cabinet_config(data).config;
            saved.windows=cabinet.placements();
            host::save_cabinet_config(data/"cabinet.ini",saved);
        }
        {
            const auto loaded=host::load_cabinet_config(data);
            require(loaded.loaded() && !loaded.legacy,"Read the saved cabinet.ini");
            require(loaded.config.input.plunger.axis==2,"Window saves preserve shared inputs");
            const std::array<int,3> expected{90,90,270};
            for(std::size_t i=0;i<expected.size();++i) {
                const auto& window=loaded.config.windows[i];
                require(window.artwork_rotation==expected[i] && !window.fullscreen && window.geometry.valid(),
                    "Save each role's rotation and fullscreen");
            }
            const auto rom=core::read_file(core::path_from_utf8(argv[1]));
            host::Presentation playfield("Restored playfield",320,240,true);
            host::Cabinet cabinet(playfield,loaded.config.windows,data,rom);
            require(playfield.rotation()==90,"Restore configured rotation");
        }
        SDL_Quit();
        std::cout<<"PASS: cabinet window selection, rotation, display recovery, fullscreen and persistence\n";
        return 0;
    } catch(const std::exception& e) {SDL_Quit();std::cerr<<e.what()<<'\n';return 1;}
}
