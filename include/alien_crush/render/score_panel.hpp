// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <optional>
namespace alien_crush::render {
// The ball in play, counted as a pinball cabinet counts it: an extra ball is
// played again under the same number. The cartridge keeps only the balls left
// ($46), which an award raises (03:919C, 9BB5), so the number is the highest
// the balls left have given this game. A restored state starts from them alone.
class BallNumber {
    unsigned ball_{};
public:
    unsigned update(const diagnostics::AlienState&);
    void reset() {ball_=0;}
};
// Fixed reserves keep digit size stable as scores gain digits. Uses original
// HUD artwork for numbers and small host-authored bitmap labels.
class ScorePanel {
    std::array<Image,10> digits_;
public:
    explicit ScorePanel(std::array<Image,10> digits);
    static ScorePanel from_rom(std::span<const std::byte>);
    // `ball` is the BallNumber; zero takes it from the balls left alone.
    // Outside play the panel is one page of the high scores: `rank` from 0,
    // the Grand Champion, and `entering` while that rank's name is typed.
    Image compose(const diagnostics::AlienState&,unsigned ball=0,unsigned rank=0,bool entering=false) const;
};
// What the DMD shows. While the game reloads for the title (title_transition)
// it keeps its last picture, as a cabinet's display would, instead of going
// dark with the screen between the rankings and the title. Outside play it
// turns through the high scores, a page every three seconds, whatever the
// screen shows; a game's end holds the page of a score that ranked.
class PanelHold {
    Image image_;
    std::uint64_t tick_{};
    std::optional<std::uint32_t> score_; // The game's score when last in play.
    unsigned settle_{};
public:
    static constexpr unsigned kPageFrames=180,kSettleFrames=8;
    const Image& update(const ScorePanel&,const diagnostics::AlienState&,BallNumber&);
    void reset() {*this={};}
};
}
