// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/runtime/reference_launcher.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
namespace alien_crush {
namespace {
// 01:4FC2: nine (x,y) offsets. All X offsets are 14. Descriptor sequence
// 04:BEC2 is 23..31, selected by integer travel through 00:FD8F.
constexpr std::array<int,9> seat_y{3,5,9,10,11,13,19,25,27};
std::optional<ReferenceLauncherSeat> launcher_seat(std::span<const std::byte> ram,bool suspended) {
    if(ram.size()!=8192) return {};
    auto b=[&](unsigned at){return std::to_integer<unsigned>(ram[at]);};
    auto word=[&](unsigned lo,unsigned hi){return b(lo)|(b(hi)<<8);};
    // 01:4E5C refuses a launch only while $53 is negative. A completed lane
    // set (01:490B) leaves $54 at 1..3 and can set $53 bit 6, and the ball
    // still launches then.
    if(b(0x79)!=0 || b(0x67)!=1 || b(0x76)!=0 || (b(0x53)&0x80)!=0 ||
        (b(0x78)&0x13)!=0 || (b(0x68)&1)==0 || (!suspended && (b(0xf6)&2)!=0) ||
        word(0x16,0x17)!=0 || b(0x1500)!=1 || b(0x1540)!=0x80) return {};
    // The outermost JSR is the normal 01:4086 frame wait (return $4088).
    // Pause and its 16-frame resume delay instead retain the $408E caller
    // of 01:4187. This also excludes other menu/initialization wait loops.
    const auto caller=word(0x1fe,0x1ff);
    if(caller!=0x4088 && !(suspended && caller==0x408e)) return {};
    unsigned slot=0,matches=0;
    for(unsigned i=4;i<64;++i) if(b(0x1500+i)==6) {slot=i;++matches;}
    if(matches!=1 || b(0x1540+slot)!=0x88 || b(0x1840+slot)>3 ||
        b(0x1a00+slot)!=0 || b(0x1a40+slot)!=9 || b(0x1a40)!=slot) return {};
    const auto travel=word(0x1c00+slot,0x1c40+slot);
    if(travel>2048 || b(0x1800+slot)!=23+(travel>>8)) return {};
    const auto x=word(0x15c0+slot,0x1600+slot),y=word(0x1680+slot,0x16c0+slot);
    // Limit this first adapter to the observed original lower-table launcher.
    if(x!=258 || y!=240 || word(0x15c0,0x1600)!=x+14 ||
        std::abs(static_cast<int>(word(0x1680,0x16c0))-static_cast<int>(y)-seat_y[travel>>8])>1)
        return {};
    return ReferenceLauncherSeat{slot,travel,x,y};
}
}
std::optional<ReferenceLauncherSeat> reference_launcher_seat(std::span<const std::byte> ram) {
    return launcher_seat(ram,false);
}
bool reference_launcher_active(std::span<const std::byte> ram) {
    if(ram.size()!=8192) return false;
    auto b=[&](unsigned at){return std::to_integer<unsigned>(ram[at]);};
    // RAM 67 is set to 1 by the shooter collision at 01:4FDC. It remains
    // 1 through the shot's departure, whereas the exact seat/descriptor gate
    // can fail for a single frame at full draw or during return motion.
    if(b(0x79)!=0 || b(0x67)!=1 || b(0x76)!=0 || !(b(0x68)&1) ||
        (b(0x78)&0x13) || (b(0xf6)&2) || (b(0x53)&0x80) ||
        b(0x1500)!=1 || b(0x1540)!=0x80) return false;
    for(unsigned slot=4;slot<64;++slot)
        if(b(0x1500+slot)==6 && b(0x1540+slot)==0x88) return true;
    return false;
}
bool cancel_reference_launcher_draw(std::span<std::byte> ram) {
    const auto seat=launcher_seat(ram,true);
    if(!seat) return false;
    const auto slot=seat->slot;
    // Only the adapter's pinned phase is cancellable. Natural digital draws
    // have a nonzero step, and fired launchers have phase 4.
    if(ram[0x1840+slot]!=std::byte{1} || ram[0x1b80+slot]!=std::byte{0} ||
        ram[0x1bc0+slot]!=std::byte{0}) return false;
    auto put=[&](unsigned at,unsigned value){ram[at]=static_cast<std::byte>(value&255);};
    put(0x1c00+slot,0);put(0x1c40+slot,0);
    put(0x1840+slot,0);put(0x1800+slot,23);
    put(0x1780+slot,0);put(0x17c0+slot,0);
    put(0x15c0,seat->x+14);put(0x1600,(seat->x+14)>>8);
    put(0x1680,seat->y+3);put(0x16c0,(seat->y+3)>>8);
    return true;
}
bool request_reference_bonus_room(std::span<std::byte> ram,unsigned room) {
    if(room<1 || room>kBonusRooms || !reference_launcher_seat(ram) || ram[0x7c]!=std::byte{0}) return false;
    ram[0x83]=static_cast<std::byte>(room-1);
    ram[0x78]|=std::byte{4};
    return true;
}
bool set_reference_score(std::span<std::byte> ram,std::uint32_t points) {
    if(points>kMaximumScore || points%100 || !reference_launcher_seat(ram) || ram[0x7c]!=std::byte{0}) return false;
    const auto hundreds=points/100;
    ram[0x121e]=static_cast<std::byte>(hundreds&255);
    ram[0x1224]=static_cast<std::byte>((hundreds>>8)&255);
    ram[0x122a]=static_cast<std::byte>(hundreds>>16);
    ram[0x68]|=std::byte{4};
    return true;
}
bool hold_reference_ball(std::span<std::byte> ram,double x,double y) {
    if(ram.size()!=8192 || !std::isfinite(x) || !std::isfinite(y)) return false;
    auto b=[&](unsigned at){return std::to_integer<unsigned>(ram[at]);};
    const unsigned table=b(0x79);
    // The ball as the cabinet draws it: in play, shown and not swallowed.
    if(b(0x1500)!=1 || (b(0x1540)&0xa0)!=0x80 || (b(0x1a00)&0xa0)!=0 || (b(0xf6)&2)!=0 || b(0x76)!=0) return false;
    // The table's live loop at its frame wait (return $4088), which leaves
    // out a change of halves still loading; or a bonus room still in play.
    if(table<2?(b(0x1fe)|(b(0x1ff)<<8))!=0x4088:(table!=2 || b(0x83)>=4 || (b(0x78)&1)!=0)) return false;
    const int origin=table==0?224*256:0;
    const int from_x=static_cast<int>(b(0x1580)|(b(0x15c0)<<8)|(b(0x1600)<<16));
    const int from_y=static_cast<int>(b(0x1640)|(b(0x1680)<<8)|(b(0x16c0)<<16))+origin;
    const int to_x=static_cast<int>(std::lround(std::clamp(x,32.0,288.0)*256));
    const int to_y=static_cast<int>(std::lround(std::clamp(y,48.0,table==2?319.0:543.0)*256));
    // The motion since the last frame as a velocity, signed 8.8: none for
    // under half a pixel, which is the fall the game added, nor for a jump
    // across the table, and no faster than the shooter's.
    auto velocity=[](int moved) {return std::abs(moved)<0x80 || std::abs(moved)>32*256?0:std::clamp(moved,-0x800,0x800);};
    int velocity_x=velocity(to_x-from_x),velocity_y=velocity(to_y-from_y);
    int local_y=to_y-origin;
    // The halves meet at Y 288. 03:895C..8994 takes a rising ball above the
    // lower half's Y $40 ($50 between the walls) to the upper half, and
    // 03:89F7..8A07 one not rising at Y $120 to the lower. Have the change
    // made at once, with no velocity to carry the ball on through it.
    if(table<2 && (table==0)==(to_y<288*256)) {
        velocity_x=0;velocity_y=table==0?-0x100:0;
        local_y=table==0?std::max(local_y,48*256):std::min(local_y,304*256);
    } else if(table==0 && local_y<80*256) velocity_y=std::max(velocity_y,0); // Not back up from under the seam.
    auto put=[&](unsigned at,int value){ram[at]=static_cast<std::byte>(value&255);};
    put(0x1580,to_x);put(0x15c0,to_x>>8);put(0x1600,to_x>>16);
    put(0x1640,local_y);put(0x1680,local_y>>8);put(0x16c0,local_y>>16);
    put(0x1700,velocity_x);put(0x1740,velocity_x>>8);
    put(0x1780,velocity_y);put(0x17c0,velocity_y>>8);
    // The ball moves by the eighths 03:9022 took of its last velocity. Without
    // them it stays here this frame, and leaves with the new velocity after.
    for(const unsigned at:{0x1900u,0x1940u,0x1980u,0x19c0u}) put(at,0);
    return true;
}
bool apply_reference_launcher_draw(std::span<std::byte> ram,std::uint8_t percent) {
    if(percent==0 || percent>100) return false;
    const auto seat=reference_launcher_seat(ram);
    if(!seat) return false;
    const auto slot=seat->slot,travel=static_cast<unsigned>(percent)*2048/100;
    auto put=[&](unsigned at,unsigned value){ram[at]=static_cast<std::byte>(value&255);};
    auto word=[&](unsigned lo,unsigned hi,unsigned value){put(lo,value);put(hi,value>>8);};
    word(0x1c00+slot,0x1c40+slot,travel);
    // Pin phase 1 with zero step. Its original FD2B/4F8F calls update the
    // descriptor and seat while held; release still executes 01:4F51.
    put(0x1840+slot,1);
    word(0x1b80+slot,0x1bc0+slot,0);
    put(0x1800+slot,23+(travel>>8));
    word(0x15c0,0x1600,seat->x+14);
    word(0x1680,0x16c0,seat->y+static_cast<unsigned>(seat_y[travel>>8]));
    return true;
}
}
