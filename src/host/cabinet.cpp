// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet.hpp"
#include "window_layout.hpp"
#include "alien_crush/render/title_logo.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/data_files.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>
namespace alien_crush::host {
namespace {
void checked(bool ok) { if(!ok) throw std::runtime_error(SDL_GetError()); }
struct Monitor { SDL_DisplayID id; std::string name; SDL_Rect bounds; };
std::vector<Monitor> monitors(bool required=true) {
    int count{}; auto* ids=SDL_GetDisplays(&count);
    if(!ids || count<1) {
        SDL_free(ids);
        if(required) throw std::runtime_error("No cabinet displays available");
        return {};
    }
    std::vector<Monitor> result;
    for(int i=0;i<count;++i) {
        SDL_Rect bounds{};
        if(SDL_GetDisplayUsableBounds(ids[i],&bounds) && bounds.w>0 && bounds.h>0) {
            const auto* name=SDL_GetDisplayName(ids[i]);
            result.push_back({ids[i],name?name:"Unnamed display",bounds});
        }
    }
    SDL_free(ids);
    if(result.empty() && required) throw std::runtime_error("No usable cabinet displays available");
    return result;
}
}
Cabinet::Cabinet(Presentation& playfield,const std::array<WindowPlacement,kCabinetWindowCount>& placements,
    const std::filesystem::path& data,std::span<const std::byte> rom)
    : windows_{&playfield,&backglass_,&score_},score_panel_(render::ScorePanel::from_rom(rom)),
      backglass_image_(load_backglass(data/core::kBackglassFileName)) {
    if(!backglass_image_) backglass_animation_.emplace(rom);
    const auto displays=monitors();
    for(std::size_t i=0;i<windows_.size();++i) {
        auto* output=windows_[i]; auto* window=output->window();
        checked(SDL_SetWindowMinimumSize(window,64,64));
        const auto& p=placements[i];
        const auto& g=p.geometry;
        const bool saved=g.valid();
        auto selected=std::min(i,displays.size()-1);
        if(saved) {
            // Geometry distinguishes identical monitor names; missing monitors
            // fall back to a connected display and clamp back into view.
            for(std::size_t j=0;j<displays.size();++j) if(displays[j].name==p.display_identity) {
                selected=j;
                const auto& b=displays[j].bounds;
                if(g.x>=b.x && g.x<b.x+b.w && g.y>=b.y && g.y<b.y+b.h) break;
            }
        }
        const auto& b=displays[selected].bounds;
        int w{},h{}; checked(SDL_GetWindowSize(window,&w,&h));
        w=std::min(saved?std::max(g.width,64):w,b.w); h=std::min(saved?std::max(g.height,64):h,b.h);
        const int x=std::clamp(saved?g.x:b.x+static_cast<int>(i)*40,b.x,b.x+b.w-w);
        const int y=std::clamp(saved?g.y:b.y+static_cast<int>(i)*40,b.y,b.y+b.h-h);
        checked(SDL_SetWindowSize(window,w,h)); checked(SDL_SetWindowPosition(window,x,y));
        output->rotation(p.artwork_rotation);
        if(p.fullscreen) checked(SDL_SetWindowFullscreen(window,true));
        output->reveal();
    }
    checked(SDL_RaiseWindow(playfield.window()));
}
bool Cabinet::owns(SDL_Window* window) const {
    return std::any_of(windows_.begin(),windows_.end(),[&](const auto* output){return output->window()==window;});
}
bool Cabinet::handle_key(const SDL_KeyboardEvent& key) {
    if(key.type!=SDL_EVENT_KEY_DOWN || key.repeat) return false;
    auto* window=SDL_GetWindowFromID(key.windowID);
    const auto found=std::find_if(windows_.begin(),windows_.end(),[&](const auto* output){return output->window()==window;});
    if(found==windows_.end()) return false;
    const bool reverse=(key.mod&SDL_KMOD_SHIFT)!=0;
    if(key.scancode==SDL_SCANCODE_R) {
        auto* output=*found;
        output->rotation((output->rotation()+(reverse?270:90))%360);
    } else if(key.scancode==SDL_SCANCODE_F6) {
        const auto index=static_cast<std::size_t>(found-windows_.begin());
        checked(SDL_RaiseWindow(windows_[(index+(reverse?2:1))%windows_.size()]->window()));
    } else if(key.scancode==SDL_SCANCODE_F11) {
        checked(SDL_SetWindowFullscreen(window,!(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)));
    } else if(key.scancode==SDL_SCANCODE_F10) {
        const auto displays=monitors(false);
        if(displays.size()<2) return true;
        const auto current=SDL_GetDisplayForWindow(window);
        const auto display=std::find_if(displays.begin(),displays.end(),[&](const auto& m){return m.id==current;});
        const auto index=display==displays.end()?0:static_cast<std::size_t>(display-displays.begin());
        const auto& target=displays[(index+(reverse?displays.size()-1:1))%displays.size()];
        const bool fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
        // Leave fullscreen before changing its windowed placement; wait for the
        // platform to finish so the next request uses the new display.
        if(fullscreen) {checked(SDL_SetWindowFullscreen(window,false));checked(SDL_SyncWindow(window));}
        int w{},h{};checked(SDL_GetWindowSize(window,&w,&h));
        const auto& b=target.bounds;
        w=std::min(w,b.w);h=std::min(h,b.h);
        checked(SDL_SetWindowSize(window,w,h));
        checked(SDL_SetWindowPosition(window,b.x+(b.w-w)/2,b.y+(b.h-h)/2));
        checked(SDL_SyncWindow(window));
        if(fullscreen) checked(SDL_SetWindowFullscreen(window,true));
        checked(SDL_RaiseWindow(window));
    } else return false;
    return true;
}
bool Cabinet::recover_displays() {
    const auto displays=monitors(false);
    if(displays.empty()) return false;
    std::vector<SDL_Rect> usable,full;
    for(const auto& display:displays) {
        SDL_Rect bounds{};
        if(!SDL_GetDisplayBounds(display.id,&bounds) || bounds.w<=0 || bounds.h<=0) return false;
        usable.push_back(display.bounds);full.push_back(bounds);
    }
    for(std::size_t i=0;i<windows_.size();++i) {
        auto* window=windows_[i]->window();
        checked(SDL_SyncWindow(window));
        const bool fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
        SDL_Rect before{};
        checked(SDL_GetWindowPosition(window,&before.x,&before.y));
        checked(SDL_GetWindowSize(window,&before.w,&before.h));
        const auto after=visible_window(before,fullscreen?full:usable,i);
        if(!after) return false;
        if(SDL_RectsEqual(&before,&*after)) continue;
        if(fullscreen) {checked(SDL_SetWindowFullscreen(window,false));checked(SDL_SyncWindow(window));}
        checked(SDL_SetWindowSize(window,after->w,after->h));
        checked(SDL_SetWindowPosition(window,after->x,after->y));
        checked(SDL_SyncWindow(window));
        if(fullscreen) {checked(SDL_SetWindowFullscreen(window,true));checked(SDL_SyncWindow(window));}
        // Do not raise windows: topology changes must not take focus from another app.
    }
    return true;
}
void Cabinet::show(std::span<const std::byte> ram,bool smoothing,double fraction) {
    if(backglass_image_ || backglass_animation_) {
        const auto& art=backglass_image_?*backglass_image_:backglass_animation_->frame(backglass_tick_++);
        const render::AnimationScene animation{art,{},100};
        backglass_.show({backglass_tick_,art.width,art.height,art.pixels,{}},art.width*(backglass_animation_?5:1),art.height*(backglass_animation_?4:1),
            false,{},nullptr,nullptr,{},fraction,smoothing && backglass_animation_?&animation:nullptr);
    } else backglass_.panel("ALIEN CRUSH");
    const auto& image=panel_.update(score_panel_,diagnostics::observe_alien_state(ram),ball_);
    score_.show({0,image.width,image.height,image.pixels,{}},8,3);
}
std::array<WindowPlacement,kCabinetWindowCount> Cabinet::placements() const {
    std::array<WindowPlacement,kCabinetWindowCount> result;
    for(std::size_t i=0;i<windows_.size();++i) {
        auto* window=windows_[i]->window();
        auto& p=result[i];
        auto& g=p.geometry;
        checked(SDL_GetWindowPosition(window,&g.x,&g.y)); checked(SDL_GetWindowSize(window,&g.width,&g.height));
        const auto display=SDL_GetDisplayForWindow(window);
        const auto* name=SDL_GetDisplayName(display);
        p.display_identity=name?name:"Unnamed display";
        p.rotation=display_rotation(SDL_GetCurrentDisplayOrientation(display));
        p.fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
        p.artwork_rotation=windows_[i]->rotation();
    }
    return result;
}
}
