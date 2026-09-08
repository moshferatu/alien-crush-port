// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace alien_crush::host {
namespace {
void checked(bool ok) { if(!ok) throw std::runtime_error(SDL_GetError()); }
struct Monitor { SDL_DisplayID id; std::string name; SDL_Rect bounds; };
std::vector<Monitor> monitors() {
    int count{}; auto* ids=SDL_GetDisplays(&count);
    if(!ids || count<1) {SDL_free(ids); throw std::runtime_error("No cabinet displays available");}
    std::vector<Monitor> result;
    for(int i=0;i<count;++i) {
        SDL_Rect bounds{};
        if(SDL_GetDisplayUsableBounds(ids[i],&bounds) && bounds.w>0 && bounds.h>0) {
            const auto* name=SDL_GetDisplayName(ids[i]);
            result.push_back({ids[i],name?name:"Unnamed display",bounds});
        }
    }
    SDL_free(ids);
    if(result.empty()) throw std::runtime_error("No usable cabinet displays available");
    return result;
}
constexpr std::array<const char*,3> roles{"playfield","backglass","score"};
struct Placement { std::string name; int x{},y{},w{},h{},fullscreen{},rotation{}; };
std::array<Placement,3> read(const std::filesystem::path& path) {
    const auto bytes=core::read_file(path,16384);
    std::istringstream stream(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
    std::string magic; int version{};
    if(!(stream>>magic>>version) || magic!="ACWINDOWS" || version!=1) throw std::runtime_error("Unsupported cabinet window configuration");
    std::array<Placement,3> placements;
    for(std::size_t i=0;i<placements.size();++i) {
        std::string role; auto& p=placements[i];
        if(!(stream>>role>>std::quoted(p.name)>>p.x>>p.y>>p.w>>p.h>>p.fullscreen>>p.rotation) || role!=roles[i] ||
            p.name.size()>1024 || p.x<-100000 || p.x>100000 || p.y<-100000 || p.y>100000 ||
            p.w<64 || p.w>32768 || p.h<64 || p.h>32768 || (p.fullscreen!=0 && p.fullscreen!=1) ||
            p.rotation<0 || p.rotation>270 || p.rotation%90!=0)
            throw std::runtime_error("Invalid cabinet window configuration");
    }
    stream>>std::ws;
    if(!stream.eof()) throw std::runtime_error("Unexpected cabinet window configuration fields");
    return placements;
}
}
Cabinet::Cabinet(Presentation& playfield,const std::filesystem::path& data)
    : windows_{&playfield,&backglass_,&score_},path_(data/"cabinet-windows.txt") {
    const auto displays=monitors();
    const bool saved=std::filesystem::exists(path_);
    const auto placements=saved?read(path_):std::array<Placement,3>{};
    for(std::size_t i=0;i<windows_.size();++i) {
        auto* output=windows_[i]; auto* window=output->window();
        checked(SDL_SetWindowMinimumSize(window,64,64));
        const auto& p=placements[i];
        auto selected=std::min(i,displays.size()-1);
        if(saved) {
            // Geometry distinguishes identical monitor names; missing monitors
            // fall back to a connected display and clamp back into view.
            for(std::size_t j=0;j<displays.size();++j) if(displays[j].name==p.name) {
                selected=j;
                const auto& b=displays[j].bounds;
                if(p.x>=b.x && p.x<b.x+b.w && p.y>=b.y && p.y<b.y+b.h) break;
            }
        }
        const auto& b=displays[selected].bounds;
        int w{},h{}; checked(SDL_GetWindowSize(window,&w,&h));
        w=std::min(saved?p.w:w,b.w); h=std::min(saved?p.h:h,b.h);
        const int x=std::clamp(saved?p.x:b.x+static_cast<int>(i)*40,b.x,b.x+b.w-w);
        const int y=std::clamp(saved?p.y:b.y+static_cast<int>(i)*40,b.y,b.y+b.h-h);
        checked(SDL_SetWindowSize(window,w,h)); checked(SDL_SetWindowPosition(window,x,y));
        output->rotation(p.rotation);
        if(p.fullscreen) checked(SDL_SetWindowFullscreen(window,true));
        checked(SDL_ShowWindow(window));
    }
    checked(SDL_RaiseWindow(playfield.window()));
}
bool Cabinet::owns(SDL_Window* window) const {
    return std::any_of(windows_.begin(),windows_.end(),[&](const auto* output){return output->window()==window;});
}
void Cabinet::show(std::span<const std::byte> ram) {
    backglass_.panel("ALIEN CRUSH");
    const auto state=diagnostics::observe_alien_state(ram);
    const auto text="SCORE  "+std::to_string(state.score_points)+"\n\nBALL   "+std::to_string(state.ball_counter);
    score_.panel(text.c_str());
}
void Cabinet::save() const {
    std::ostringstream stream; stream<<"ACWINDOWS 1\n";
    for(std::size_t i=0;i<windows_.size();++i) {
        auto* window=windows_[i]->window(); int x{},y{},w{},h{};
        checked(SDL_GetWindowPosition(window,&x,&y)); checked(SDL_GetWindowSize(window,&w,&h));
        const auto* name=SDL_GetDisplayName(SDL_GetDisplayForWindow(window));
        stream<<roles[i]<<' '<<std::quoted(name?name:"Unnamed display")<<' '<<x<<' '<<y<<' '<<w<<' '<<h<<' '
            <<((SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)?1:0)<<' '<<windows_[i]->rotation()<<'\n';
    }
    const auto text=stream.str();
    core::write_file_atomic(path_,std::as_bytes(std::span(text.data(),text.size())));
}
}
