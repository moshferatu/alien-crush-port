// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/diagnostics/alien_state.hpp"
#include <stdexcept>
#include <algorithm>
namespace alien_crush::diagnostics {
AlienState observe_alien_state(std::span<const std::byte> ram) {
    if(ram.size()!=8192) throw std::runtime_error("Alien state requires 8192-byte work RAM");
    auto b=[&](std::size_t offset){return std::to_integer<std::uint32_t>(ram[offset]);};
    auto score=[&](std::size_t index){return 100U*(b(0x1219+index)|(b(0x121f+index)<<8)|(b(0x1225+index)<<16));};
    AlienState result;
    result.score_points=score(5);
    for(std::size_t i=0;i<5;++i) result.high_scores[i]=score(i);
    result.table_code=static_cast<std::uint8_t>(b(0x79));
    result.ball_counter=static_cast<std::uint8_t>(b(0x46));
    result.score_update_pending=(b(0x68)&4)!=0;
    // 01:4003 JSR EAEB saves return 4005; EAEB pushes MPR2..6 then
    // EB0A JSR 4000 saves EB0C. This outer frame spans the title menus and
    // their fades, and is replaced when 01:4006 continues into game setup.
    constexpr std::array<std::byte,9> title_stack{std::byte{0x0c},std::byte{0xeb},std::byte{0x18},
        std::byte{4},std::byte{3},std::byte{2},std::byte{1},std::byte{5},std::byte{0x40}};
    result.title_menu=std::equal(title_stack.begin(),title_stack.end(),ram.begin()+0x1f7);
    result.demo_play=b(0x76)!=0; // 01:4009 branches to demo setup for any nonzero value.
    // 01:41AE JSR EBE6 within paused 408C -> 4187. The wrapper pushes
    // MPR2..4 and calls 1C:4003, saving EBFD. Mode 7A stays 1 after exit,
    // so it cannot identify the score viewer without this complete outer frame.
    constexpr std::array<std::byte,9> scores_stack{std::byte{0xfd},std::byte{0xeb},std::byte{3},
        std::byte{2},std::byte{1},std::byte{0xb0},std::byte{0x41},std::byte{0x8e},std::byte{0x40}};
    result.high_score_menu=b(0x7a)==1 && !result.demo_play &&
        std::equal(scores_stack.begin(),scores_stack.end(),ram.begin()+0x1f7);
    // Verified bonus-practice game end: 01:448D -> EBE6, within 4006's
    // practice call. This spans ranking, name editing and the confirmation pause.
    constexpr std::array<std::byte,9> practice_scores_stack{std::byte{0xfd},std::byte{0xeb},std::byte{3},
        std::byte{2},std::byte{1},std::byte{0x8f},std::byte{0x44},std::byte{8},std::byte{0x40}};
    result.high_score_menu|=b(0x7a)==2 && b(0x7c)==1 && !result.demo_play &&
        std::equal(practice_scores_stack.begin(),practice_scores_stack.end(),ram.begin()+0x1f7);
    // Normal game end calls EBE6 directly at 01:417B (return 417D).
    // Unlike practice and pause, there is no enclosing JSR: the seven-byte
    // wrapper frame ends at 1FF. This includes ranking and initials entry.
    constexpr std::array<std::byte,7> game_scores_stack{std::byte{0xfd},std::byte{0xeb},std::byte{3},
        std::byte{2},std::byte{1},std::byte{0x7d},std::byte{0x41}};
    result.high_score_menu|=b(0x7a)==2 && b(0x7c)==0 && !result.demo_play &&
        std::equal(game_scores_stack.begin(),game_scores_stack.end(),ram.begin()+0x1f9);
    // Attract completion calls the same ranking screen at 01:405A (405C).
    // The demo selector stays set until the subsequent return to title.
    constexpr std::array<std::byte,7> demo_scores_stack{std::byte{0xfd},std::byte{0xeb},std::byte{3},
        std::byte{2},std::byte{1},std::byte{0x5c},std::byte{0x40}};
    result.high_score_menu|=b(0x7a)==2 && result.demo_play &&
        std::equal(demo_scores_stack.begin(),demo_scores_stack.end(),ram.begin()+0x1f9);
    // EBE6 restores table graphics through EC0A -> E705 after rankings close.
    // Game-end callers immediately reset to title; only the pause viewer
    // (mode 1) actually resumes play. Preserve the original blank transition.
    const bool ending_caller=(b(0x1fe)==0x5c && b(0x1ff)==0x40 && result.demo_play) ||
        (b(0x1fe)==0x7d && b(0x1ff)==0x41 && !result.demo_play);
    result.title_transition=b(0x7a)==2 && ending_caller && b(0x1fc)==0x0c && b(0x1fd)==0xec;
    // E0DE waits one frame (E0E3), clears RAM and VRAM (E121), then
    // enters 01:4000. These outer returns survive the zero-page/stack clear
    // and identify the gap even after the demo and table selectors are zero.
    result.title_transition|=(b(0x1fe)==0xe3 && b(0x1ff)==0xe0) ||
        (b(0x1fe)==0x21 && b(0x1ff)==0xe1);
    // FDE5 uses 83+2 to load room actors, then restores shared table code 2.
    if(result.table_code==2 && b(0x83)<4) result.bonus_room=static_cast<std::uint8_t>(b(0x83)+1);
    result.bonus_round_over=result.bonus_room!=0 && (b(0x78)&1)!=0;
    result.bonus_cleared=result.bonus_room!=0 && (b(0x85)&1)!=0;
    result.game_over=result.table_code<2 && result.ball_counter==0 && b(0x7c)==0 &&
        !result.demo_play && (b(0x78)&3)==3;
    // 03:82CC initializes the spin; 83CC resolves it; 8403 removes the
    // three reserved digit sprites on entering action 5. A winning result
    // remains visible after the ball counter is incremented.
    if(result.table_code<2 && !result.title_menu && !result.high_score_menu && !result.demo_play)
        for(unsigned x=4;x<64;++x)
            if(b(0x1500+x)==43 && (b(0x1540+x)&0x80) &&
                b(0x1840+x)>=2 && b(0x1840+x)<=4 && b(0x1c40+x)<10) {
                result.match_visible=true;
                result.match_digit=static_cast<std::uint8_t>(b(0x1c40+x));
                break;
            }
    return result;
}
}
