// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "presentation.hpp"
#include "backglass.hpp"
#include "cabinet_config.hpp"
#include "game_input.hpp"
#include "alien_crush/render/score_panel.hpp"
#include "alien_crush/render/title_logo.hpp"
#include <array>
namespace alien_crush::host {
// Uses the Devil's Crush host pattern: named displays, bounded window geometry,
// auxiliary renderers and one shared game clock. No game-specific state is reused.
//
// The score and backglass windows float: they never take the keyboard and stay
// above a fullscreen playfield, which keeps it. So the keys act on a selected
// window rather than the focused one, chosen with next-window.
class Cabinet {
    Presentation score_{"Alien Crush - Score",640,200,true,true};
    Presentation backglass_{"Alien Crush - Backglass",640,360,true,true};
    std::array<Presentation*,3> windows_;
    HostKeys keys_;
    std::size_t selected_{};
    bool fullscreen_beside(SDL_Window*,SDL_DisplayID) const;
    render::ScorePanel score_panel_;
    render::BallNumber ball_;
    render::PanelHold panel_;
    std::optional<render::Image> backglass_image_;
    std::optional<render::TitleLogoAnimation> backglass_animation_;
    std::uint64_t backglass_tick_{};
public:
    // Opens each window where `placements` (alien-crush.ini) left it; one never
    // placed opens on the monitor of its turn. `data` holds backglass.bmp.
    // `borderless` takes the title bar and border off every window.
    Cabinet(Presentation& playfield,const std::array<WindowPlacement,kCabinetWindowCount>& placements,
        const std::filesystem::path& data,std::span<const std::byte> rom,
        const HostKeys& keys=resolve_host_keys(default_input_bindings()),bool borderless=false);
    void show(std::span<const std::byte> ram,bool smoothing=false,double fraction=1);
    void repeat(double fraction) {backglass_.repeat(fraction);}
    // After a quick state: the ball number starts again from the balls left.
    void restored() {ball_.reset();panel_.reset();}
    bool owns(SDL_Window*) const;
    // R rotates the selected window; the window keys select, fullscreen and
    // move it. True when the key was one of those.
    bool handle_key(const SDL_KeyboardEvent&);
    SDL_Window* selected() const {return windows_[selected_]->window();}
    // False means the display topology is temporarily unavailable; retry later.
    bool recover_displays();
    // Where each window is now, in the game's order, for alien-crush.ini.
    std::array<WindowPlacement,kCabinetWindowCount> placements() const;
};
}
