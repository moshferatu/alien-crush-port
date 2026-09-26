// SPDX-License-Identifier: GPL-3.0-or-later
// Only the two main-table handoff call trees, from live play or attract mode. No persistent execution
// phase exists outside the serialized CPU/RAM state; IRQs fail the stack guard.
        {
        // Cabinet aliens (src/gameplay/cabinet_aliens.cpp): the actor dispatcher
        // starting at 01:45A8, or entering an alien routine with its return to
        // the dispatcher (45C7) on top of the stack.
        if(ac_cabinet_aliens && (PC==0x45a8 || PC==0x5da5 || PC==0x5e70 || PC==0x5f82 || PC==0x60e1 ||
            PC==0x619d || PC==0x630c || PC==0x63cd || PC==0x6552 || PC==0x65ab) &&
            MPR[1]==0xf8 && MPR[2]==1 && MPR[3]==2 && MPR[4]==3 && MPR[7]==0) {
            const unsigned ac_caller=RdMem(0x2100+((S+1)&255))|(RdMem(0x2100+((S+2)&255))<<8);
            if(const unsigned ac_next=ac_cabinet_aliens(PC,X,ac_caller)) PC=ac_next;
        }
        // 01:4443 is the return after the original signed half-impulse was
        // added to ball Y velocity. The early-return branches also land here,
        // so repeat their ball/action gates before reading the scratch impulse.
        // Preserve the nudge vector, screen shake, input cooldown and tilt count.
        if(ac_nudge_motion && PC==0x4443 && MPR[1]==0xf8 && MPR[2]==1 &&
            !RdMem(0x2076) && RdMem(0x3500)==1 && !(RdMem(0x3840)&0x21)) {
            const unsigned short motion=RdMem(0x3780)|(RdMem(0x37c0)<<8);
            const unsigned short impulse=RdMem(0x2004)|(RdMem(0x2005)<<8);
            const unsigned short scaled=ac_nudge_motion(motion,impulse);
            WrMem(0x3780,scaled&255);WrMem(0x37c0,scaled>>8);
        }
        // The sound driver in bank $19, for the music voice overlay
        // (src/runtime/music_voice_overlay.hpp): each instruction from the push
        // to the end of the commit, with the registers its store will use.
        if(ac_music_overlay_step && MPR[5]==0x19 && PC>=0xa139 && PC<0xa7d3) ac_music_overlay_step(PC,A,X,Y);
        // Original flipper routines, after initialization and before their
        // ordinary motion/collision code. Only live, initialized flipper actors
        // in table scenes may use these masks or bypass the shooter/ball gate.
        if(ac_cabinet_controls && (PC==0x4b7b || PC==0x4b95 || PC==0x4ba9) &&
            MPR[1]==0xf8 && MPR[2]==1 && X>=4 && X<64 &&
            RdMem(0x2079)<=2 && !RdMem(0x2076) && (RdMem(0x2068)&1) &&
            (RdMem(0x3540+X)&0x80) && (RdMem(0x3500+X)==2 || RdMem(0x3500+X)==3)) {
            // Each side's pad-input mask (input.left_flipper/right_flipper); zero
            // leaves the cartridge's own.
            if(PC==0x4b7b) {
                const unsigned char mask=RdMem(0x3500+X)==2?ac_left_flipper_mask:ac_right_flipper_mask;
                if(mask) WrMem(0x3900+X,mask);
            }
            // Keep the original pause/tilt gates at 4BA1..4BA7. Remove the
            // lower-table launch-state restriction and the ball-present gate.
            if(PC==0x4b95) PC=0x4ba1;
            if(PC==0x4ba9) PC=0x4bae;
        }
        // 01:409E (live) and 01:4038 (demo) both call 03:894B.
        const bool ac_table_handoff_caller=(ac_continuous_handoff || ac_handoff_audit.enabled || ac_handoff_boundary) &&
            (PC==0xe3f7 || PC==0xe43c || PC==0x4714 || (PC>=0x89b3 && PC<=0x8a41)) &&
            MPR[1]==0xf8 && RdMem(0x21ff)==0x40 &&
            RdMem(0x21fe)==(RdMem(0x2076)?0x3a:0xa0);
        if(ac_handoff_boundary && MPR[1]==0xf8 && MPR[4]==3 && MPR[2]==1 && S==0xfd &&
            ac_table_handoff_caller &&
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
            ac_table_handoff_caller &&
            RdMem(0x2079)<=1 && !RdMem(0x207c) &&
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
        }
