// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "handoff_packets.hpp"
#include <vector>
#include <utility>
#include <cstdio>
extern "C" unsigned ac_handoff_video_peek(unsigned);
namespace alien_crush::handoff {
// Diagnostic oracle: predict a packet, then let the original CPU execute it.
// Compare at the matching instruction boundary, including original ADC flags.
struct Audit {
    bool enabled{},pending{},failed{};
    unsigned long long checked{};
    Registers expected{};
    unsigned stack{},start{},increment{},entry{};
    std::vector<std::pair<unsigned,std::uint8_t>> writes;
    std::vector<std::uint16_t> words;
    template<class Read>
    void finish(const Registers& actual,unsigned s,Read read) {
        if(!pending || actual.pc!=expected.pc || s!=stack) return;
        bool match=actual.a==expected.a && actual.x==expected.x && actual.y==expected.y && actual.p==expected.p;
        for(const auto& [address,value]:writes) if(read(address)!=value) {
            if(!failed) std::fprintf(stderr,"RAM %04X %02X/%02X\n",address,read(address),value);
            match=false;
        }
        for(unsigned i=0;i<words.size();++i)
            if(ac_handoff_video_peek((start+i*increment)&32767)!=words[i]) {
                if(!failed) std::fprintf(stderr,"VRAM %04X %04X/%04X\n",(start+i*increment)&32767,ac_handoff_video_peek((start+i*increment)&32767),words[i]);
                match=false;
            }
        if(!words.empty() && ac_handoff_video_peek(32768)!=((start+words.size()*increment)&65535)) {
            if(!failed) std::fprintf(stderr,"MAWR %04X/%04X\n",ac_handoff_video_peek(32768),static_cast<unsigned>((start+words.size()*increment)&65535));
            match=false;
        }
        if(!match && !failed) std::fprintf(stderr,"Handoff contract mismatch %04X -> %04X: A %02X/%02X X %02X/%02X Y %02X/%02X P %02X/%02X\n",
            entry,actual.pc,actual.a,expected.a,actual.x,expected.x,actual.y,expected.y,actual.p,expected.p);
        failed|=!match;++checked;pending=false;
    }
    template<class Read>
    void begin(Registers regs,unsigned s,Read read) {
        if(pending || failed) return;
        writes.clear();words.clear();stack=s;entry=regs.pc;
        const auto shadow_read=[&](unsigned address) {
            for(const auto& [a,v]:writes) if(a==address) return v;
            return read(address);
        };
        const auto shadow_write=[&](unsigned address,std::uint8_t value) {
            for(auto& [a,v]:writes) if(a==address) {v=value;return;}
            writes.emplace_back(address,value);
        };
        const auto video=[&](const std::uint16_t* data,unsigned count) {
            start=ac_handoff_video_peek(32768);
            constexpr unsigned increments[]{1,32,64,128};
            increment=increments[(ac_handoff_video_peek(32769)>>11)&3];
            words.assign(data,data+count);return true;
        };
        if(!packet(regs,shadow_read,shadow_write,video,[&](std::uint8_t value){shadow_write(0x2100|stack,value);--stack;})) return;
        if(regs.pc==0xe42b) next_video_packet(regs,shadow_read,shadow_write);
        expected=regs;pending=true;
    }
};
}
