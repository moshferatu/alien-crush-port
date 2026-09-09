// SPDX-License-Identifier: GPL-3.0-or-later
// Only the two ordinary-table handoff call trees. No persistent execution
// phase exists outside the serialized CPU/RAM state; IRQs fail the stack guard.
        if(ac_handoff_boundary && MPR[1]==0xf8 && MPR[4]==3 && MPR[2]==1 && S==0xfd &&
            RdMem(0x21fe)==0xa0 && RdMem(0x21ff)==0x40 &&
            (PC==0x89cd || PC==0x89d0 || PC==0x89d3 || PC==0x89d6 ||
             PC==0x8a28 || PC==0x8a2b || PC==0x8a2e || PC==0x8a31)) {
            unsigned char ram[8192];
            for(unsigned i=0;i<8192;++i) ram[i]=RdMem(0x2000+i);
            ac_handoff_boundary(PC,ram);
        }
        if(ac_handoff_audit.enabled)
            ac_handoff_audit.finish({A,X,Y,P,static_cast<uint16>(PC)},S,[&](unsigned address){return RdMem(address);});
        if((ac_continuous_handoff || ac_handoff_audit.enabled) &&
            (PC==0xe3f7 || PC==0xe43c || PC==0x4714 || PC==0x89b3 || PC==0x89e9 || PC==0x8a1b || PC==0x8a41) &&
            MPR[1]==0xf8 && MPR[7]==0 &&
            RdMem(0x21fe)==0xa0 && RdMem(0x21ff)==0x40 &&
            RdMem(0x2079)<=1 && !RdMem(0x2076) && !RdMem(0x207c) &&
            !(RdMem(0x2053)&0x80) &&
            !(RdMem(0x2054)&0x80) && RdMem(0x3500)==1) {
            if(MPR[4]==3 && MPR[2]==1 && S==0xfd &&
                (PC==0x89b3 || PC==0x89e9 || PC==0x8a1b || PC==0x8a41)) {
                if(ac_continuous_handoff) {PC+=3; ++ac_handoff_waits;}
            } else {
                const auto caller=RdMem(0x21fc)|(RdMem(0x21fd)<<8);
                const bool loader=(caller==0x89cf || caller==0x8a2a);
                const bool animated=(caller==0x89df || caller==0x8a3a);
                const bool exchange=(caller==0x89d2 || caller==0x8a2d);
                const bool vram=((loader && (S==0xef || S==0xf3)) || (animated && S==0xf3)) && PC==0xe3f7;
                const bool ram=loader && PC==0xe43c && S==0xf0;
                const bool actors=exchange && PC==0x4714 && MPR[2]==1 && S==0xfb;
                if((vram || ram || actors) && !(P&(D_FLAG|T_FLAG))) {
                    alien_crush::handoff::Registers regs{A,X,Y,P,static_cast<uint16>(PC)};
                    const auto read=[&](unsigned address) {return RdMem(address);};
                    const auto write=[&](unsigned address,uint8 value) {WrMem(address,value);};
                    if(ac_handoff_audit.enabled) ac_handoff_audit.begin(regs,S,read);
                    unsigned bounded=0;
                    bool replaced=false;
                    while(ac_continuous_handoff && bounded++<32768 && alien_crush::handoff::packet(regs,read,write,ac_handoff_video,[&](uint8 value){PUSH(value);})) {
                        replaced=true;
                        ++ac_handoff_packets;
                        if(regs.pc==0xe42b) alien_crush::handoff::next_video_packet(regs,read,write);
                        else break;
                    }
                    if(replaced) {
                        A=regs.a;X=regs.x;Y=regs.y;P=regs.p;PC=regs.pc;
                    }
                }
            }
        }
