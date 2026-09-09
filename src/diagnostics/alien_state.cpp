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
    // FDE5 uses 83+2 to load room actors, then restores shared table code 2.
    if(result.table_code==2 && b(0x83)<4) result.bonus_room=static_cast<std::uint8_t>(b(0x83)+1);
    result.bonus_round_over=result.bonus_room!=0 && (b(0x78)&1)!=0;
    result.bonus_cleared=result.bonus_room!=0 && (b(0x85)&1)!=0;
    result.game_over=result.table_code<2 && result.ball_counter==0 && b(0x7c)==0 &&
        !result.demo_play && (b(0x78)&3)==3;
    return result;
}
}
