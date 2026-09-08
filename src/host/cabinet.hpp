// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "presentation.hpp"
#include <array>
namespace alien_crush::host {
// Uses the Devil's Crush host pattern: named displays, bounded window geometry,
// auxiliary renderers and one shared game clock. No game-specific state is reused.
class Cabinet {
    Presentation backglass_{"Alien Crush - Backglass",640,360,true};
    Presentation score_{"Alien Crush - Score",640,200,true};
    std::array<Presentation*,3> windows_;
    std::filesystem::path path_;
public:
    Cabinet(Presentation& playfield,const std::filesystem::path& data);
    void show(std::span<const std::byte> ram);
    bool owns(SDL_Window*) const;
    void save() const;
};
}
