// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace alien_crush::handoff {
// Packets stop before the ROM's pointer arithmetic and return sequence.
// Full video streams also replace pointer arithmetic with its exact ADC flags;
// stack/MPR restoration remains in the ROM. Sources must be mapped ROM.
struct Registers { std::uint8_t a,x,y,p; std::uint16_t pc; };
template<class Read,class Write>
void next_video_packet(Registers& r,Read read,Write write) {
    // 00:E42B..E438: TYA / CLC / ADC $04 / STA $04 /
    // LDA $05 / ADC #0 / STA $05 / BRA E3F6 / CLY.
    const unsigned low=read(0x2004)+r.y;
    const unsigned high=read(0x2005),sum=high+(low>>8);
    write(0x2004,static_cast<std::uint8_t>(low));
    write(0x2005,static_cast<std::uint8_t>(sum));
    r.a=static_cast<std::uint8_t>(sum);
    r.p=static_cast<std::uint8_t>((r.p&~0xc3)|(r.a&0x80)|(r.a==0?2:0)|
        (sum>255?1:0)|((~high&(high^sum)&0x80)?0x40:0));
    r.y=0;r.pc=0xe3f7;
}
template<class Read,class Write,class Video,class Push>
bool packet(Registers& r, Read read, Write write, Video video, Push push) {
    const auto source=static_cast<std::uint16_t>(read(0x2004)|(read(0x2005)<<8));
    const auto byte=[&](unsigned y) {return read(static_cast<std::uint16_t>(source+y));};
    if(r.pc==0xe3f7 && r.y==0) {
        const auto control=byte(0);
        if(!control) return false;
        const bool repeat=(control&0x80)!=0;
        const unsigned count=repeat?((control&127)?control&127:256):control;
        std::uint16_t words[256];
        for(unsigned i=0;i<count;++i) {
            const unsigned offset=repeat?1:1+2*i;
            words[i]=static_cast<std::uint16_t>(byte(offset)|(byte(offset+1)<<8));
        }
        if(!video(words,count)) return false;
        if(repeat) {write(0x200c,byte(1));write(0x200d,byte(2));}
        r.a=static_cast<std::uint8_t>(words[count-1]>>8); r.x=0;
        r.y=static_cast<std::uint8_t>(repeat?3:1+2*count);
        r.p=static_cast<std::uint8_t>((r.p&~0x82)|2); // final DEX
        r.pc=0xe42b; return true;
    }
    if(r.pc==0xe43c && r.y==0) {
        const auto control=byte(0);
        if(!control) return false;
        const bool repeat=(control&0x80)!=0;
        const unsigned count=repeat?((control&127)?control&127:256):control;
        const auto dest=static_cast<std::uint16_t>(read(0x2002)|(read(0x2003)<<8));
        // Validate the complete destination before performing any mutation.
        if(dest<0x2000 || dest+255>=0x4000) return false;
        if(repeat) {
            write(0x200c,byte(1));write(0x200d,byte(2));push(3);
        } else write(0x200c,0);
        for(unsigned i=0;i<count;++i) {
            const unsigned offset=repeat?1:1+2*i;
            const auto lo=byte(offset),hi=byte(offset+1);
            write(static_cast<std::uint16_t>(dest+((2*i)&255)),lo);
            write(static_cast<std::uint16_t>(dest+((2*i+1)&255)),hi);
            r.a=hi;
        }
        r.x=static_cast<std::uint8_t>(repeat?0:2*count);
        r.y=static_cast<std::uint8_t>(repeat?2*count:1+2*count);
        r.p=static_cast<std::uint8_t>((r.p&~0x82)|2); // DEC $0C / DEX
        r.pc=repeat?0xe485:0xe45a; return true;
    }
    if(r.pc==0x4714 && r.y==0 && read(0x2001)==60) {
        const auto dest=static_cast<std::uint16_t>(read(0x2002)|(read(0x2003)<<8));
        if(source<0x3504 || source>0x3cc4 || dest+0x1300!=source) return false;
        for(unsigned i=0;i<60;++i) {
            r.a=read(source+i); const auto other=read(dest+i);
            write(source+i,other);write(dest+i,r.a);
        }
        write(0x200a,r.a);write(0x2001,0);r.y=60;
        r.p=static_cast<std::uint8_t>((r.p&~0x82)|2);
        r.pc=0x4725; return true;
    }
    return false;
}
}
