// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/gameplay/launcher_contact.hpp"
#include "alien_crush/core/files.hpp"
#include <bit>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
void verify_trace(const std::filesystem::path& path) {
    const auto bytes=core::read_file(path,64U*1024U*1024U);
    constexpr std::size_t record_size=10+8192*2;
    require(bytes.size()>8 && std::memcmp(bytes.data(),"ACLC0001",8)==0 &&
        (bytes.size()-8)%record_size==0,"Invalid or empty launcher contact trace");
    auto b=[&](std::size_t at) {return std::to_integer<unsigned>(bytes.at(at));};
    auto u16=[&](std::size_t at) {return b(at)|(b(at+1)<<8);};
    auto u32=[&](std::size_t at) {return u16(at)|(u16(at+2)<<16);};
    unsigned last_frame=0,applied=0;
    for(std::size_t p=8;p<bytes.size();p+=record_size) {
        const auto before=p+10,after=before+8192;
        auto word=[&](std::size_t base,unsigned low,unsigned high) {
            return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(b(base+low)|(b(base+high)<<8)));
        };
        const auto frame=u32(p),end=u32(p+4),pc=u16(p+8),slot=b(before+0x39);
        require(frame>=1 && frame>=last_frame && end==frame,"Invalid launcher trace chronology");
        last_frame=end;
        require(slot>=4 && slot<64 && b(before+0x1500+slot)==6 && b(before+0x1500)==1 &&
            (b(before+0x1c)&128)!=0,"Unexpected launcher trace actors/mode");
        require(b(before+0x1b)==0x60 && word(before,0xd0,0xd1)==256 && word(before,0xd2,0xd3)==0 &&
            word(before,0x3c,0x3d)==0 && word(before,0x1700,0x1740)==0,"Unsupported launcher contact surface/basis");
        const auto result=gameplay::vertical_launcher_contact(word(before,0x3a,0x3b),
            word(before,0x3e,0x3f),(b(before+0x1ac0+slot)&128)!=0);
        require(pc==(result.applied?(result.consume_contact?0x8be1U:0x8bf8U):
            (word(before,0x3a,0x3b)<0?0x8be4U:0x8ba1U)),"Launcher response branch mismatch");
        require(word(after,0x3a,0x3b)==result.ball_velocity_y &&
            word(after,0x1780,0x17c0)==result.ball_velocity_y && word(after,0x1700,0x1740)==0,
            "Native launcher response differs from executed ball velocity");
        if(result.consume_contact) require(b(after+0x1ac0+slot)==0,"Launcher contact not consumed");
        if(result.applied) ++applied;
    }
    std::cout<<(bytes.size()-8)/record_size<<" contact calls match; "<<applied<<" applied\n";
}
}
int main(int argc,char** argv) {
    try {
        using gameplay::vertical_launcher_contact;
        require(!vertical_launcher_contact(-3,-2048,false).applied,"Existing separating contact must be ignored");
        auto hit=vertical_launcher_contact(13,-2048,true);
        require(hit.applied && hit.consume_contact && hit.ball_velocity_y==-2049,"Small reflected velocity rounds down");
        require(vertical_launcher_contact(-2049,-2048,true).ball_velocity_y==-2113,"Repeated contact preserves cartridge damping");
        require(vertical_launcher_contact(0,-512,true).ball_velocity_y==-512,"Zero incoming velocity");
        require(vertical_launcher_contact(0,-512,false).ball_velocity_y==-512,"Upward launcher catches stationary ball");
        require(!vertical_launcher_contact(64,64,false).applied,"Equal moving velocities separate");
        hit=vertical_launcher_contact(65,64,false);
        require(hit.applied && !hit.consume_contact && hit.ball_velocity_y==-3,"Slower receding launcher response");
        require(vertical_launcher_contact(-32768,-32768,true).ball_velocity_y==31744,"Signed response wraps as original words");
        for(int i=1;i<argc;++i) verify_trace(core::path_from_utf8(argv[i]));
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
