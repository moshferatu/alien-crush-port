// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/diagnostics/alien_state.hpp"
#include <stdexcept>
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
    return result;
}
}
