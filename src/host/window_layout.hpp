// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SDL3/SDL_rect.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
namespace alien_crush::host {
// Keep the display containing the greatest portion of a window. Fully detached
// windows use the role's fallback display. An empty topology has no placement.
inline std::optional<SDL_Rect> visible_window(SDL_Rect window,std::span<const SDL_Rect> displays,std::size_t fallback) {
    if(displays.empty() || window.w<=0 || window.h<=0) return std::nullopt;
    auto selected=std::min(fallback,displays.size()-1);
    std::int64_t largest=0;
    for(std::size_t i=0;i<displays.size();++i) {
        const auto& b=displays[i];
        if(b.w<=0 || b.h<=0) return std::nullopt;
        const auto width=std::max<std::int64_t>(0,std::min<std::int64_t>(std::int64_t{window.x}+window.w,std::int64_t{b.x}+b.w)-std::max(window.x,b.x));
        const auto height=std::max<std::int64_t>(0,std::min<std::int64_t>(std::int64_t{window.y}+window.h,std::int64_t{b.y}+b.h)-std::max(window.y,b.y));
        if(width*height>largest) {largest=width*height;selected=i;}
    }
    const auto& b=displays[selected];
    window.w=std::min(window.w,b.w);window.h=std::min(window.h,b.h);
    auto fit=[](int position,int origin,int extent,int size) {
        const auto end=std::min<std::int64_t>(std::int64_t{origin}+extent-size,std::numeric_limits<int>::max());
        return static_cast<int>(std::clamp<std::int64_t>(position,origin,end));
    };
    window.x=fit(window.x,b.x,b.w,window.w);window.y=fit(window.y,b.y,b.h,window.h);
    return window;
}
}
