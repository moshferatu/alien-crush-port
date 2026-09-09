// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <array>
#include <algorithm>
extern "C" void ac_handoff_counts(unsigned long long*,unsigned long long*);
extern "C" void ac_set_handoff_audit(bool);
extern "C" bool ac_handoff_audit_result(unsigned long long*);
extern "C" void ac_set_handoff_boundary(void (*)(unsigned,const unsigned char*));
using namespace alien_crush;
namespace {
struct Boundary {unsigned pc;std::array<unsigned char,8192> ram;};
std::vector<Boundary> boundaries;
void boundary(unsigned pc,const unsigned char* ram) {
    Boundary value{pc,{}};std::copy_n(ram,8192,value.ram.begin());boundaries.push_back(value);
}
void compare_boundaries(const std::vector<Boundary>& native,const std::vector<Boundary>& original,unsigned offset=0) {
    if(native.size()<4 || original.size()<offset+4) throw std::runtime_error("Missing semantic handoff boundaries");
    for(unsigned i=0;i<4;++i) {
        const auto& a=native[i];const auto& b=original[offset+i];
        if(a.pc!=b.pc) throw std::runtime_error("Different handoff operation order");
        for(const auto [start,end]:{std::pair{0x42U,0x46U},std::pair{0x200U,0xa00U},std::pair{0x1500U,0x1d00U}})
            for(unsigned p=start;p<end;++p) if(a.ram[p]!=b.ram[p]) {
                std::cerr<<"Boundary "<<std::hex<<a.pc<<" RAM "<<p<<" native "<<unsigned(a.ram[p])<<" original "<<unsigned(b.ram[p])<<'\n';
                throw std::runtime_error("Semantic handoff state changed");
            }
        for(unsigned p:{0x53U,0x54U,0x68U,0x79U,0x7aU,0x7bU,0x7cU})
            if(a.ram[p]!=b.ram[p]) throw std::runtime_error("Handoff flags changed");
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("Expected ROM and recording paths");
        const auto path=core::path_from_utf8(argv[1]);
        auto runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(path);
        recording::Recording recording{runtime->save(),{}};
        recording::Recording crossing;
        std::vector<std::int16_t> crossing_audio;
        ac_set_handoff_boundary(boundary);
        std::ofstream trace(std::filesystem::path(argv[2]).string()+".csv");
        trace<<"frame,half,x,y,pc,packets,waits,audio,ms\n";
        unsigned crossings=0,last_half=0;
        std::uint64_t samples=0;
        for(unsigned n=0;n<1800;++n) {
            const auto start=std::chrono::steady_clock::now();
            const auto input=recording::canonical_input(n);recording.inputs.push_back(input);
            const auto frame=runtime->advance(input);
            if(n>=579 && n<699) crossing_audio.insert(crossing_audio.end(),frame.stereo.begin(),frame.stereo.end());
            if(n>=579 && n<699) crossing.inputs.push_back(input);
            if(frame.stereo.empty()) throw std::runtime_error("Missing audio batch");
            samples+=frame.stereo.size()/2;
            const auto ram=runtime->work_ram();
            const auto value=[&](unsigned i){return std::to_integer<unsigned>(ram[i]);};
            const auto half=value(0x79);
            if(n>360 && half<=1 && half!=last_half) ++crossings;
            last_half=half;
            unsigned long long packets{},waits{};ac_handoff_counts(&packets,&waits);
            trace<<n+1<<','<<half<<','<<(value(0x15c0)|(value(0x1600)<<8))<<','
                <<(value(0x1680)|(value(0x16c0)<<8))<<','<<runtime->video_snapshot().pc<<','
                <<packets<<','<<waits<<','<<frame.stereo.size()<<','
                <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
            if(n==578) crossing.initial=runtime->save();
        }
        unsigned long long packets{},waits{};ac_handoff_counts(&packets,&waits);
        if(crossings<2 || packets==0 || waits<4) throw std::runtime_error("Native handoffs were not exercised");
        const auto expected=1800.0*runtime->sample_rate()/runtime->frames_per_second();
        if(samples<expected-2000 || samples>expected+2000) throw std::runtime_error("Audio timeline changed");
        recording::write(core::path_from_utf8(argv[2]),recording);
        recording::write(core::path_from_utf8(std::string(argv[2])+".crossing"),crossing);
        std::ofstream wav(std::filesystem::path(argv[2]).string()+".wav",std::ios::binary);
        wav.exceptions(std::ios::failbit|std::ios::badbit);
        const auto u16=[&](unsigned v){wav.put(static_cast<char>(v));wav.put(static_cast<char>(v>>8));};
        const auto u32=[&](unsigned v){u16(v&65535);u16(v>>16);};
        const auto bytes=static_cast<unsigned>(crossing_audio.size()*2);
        wav.write("RIFF",4);u32(36+bytes);wav.write("WAVEfmt ",8);u32(16);u16(1);u16(2);
        u32(static_cast<unsigned>(runtime->sample_rate()));u32(static_cast<unsigned>(runtime->sample_rate())*4);
        u16(4);u16(16);wav.write("data",4);u32(bytes);
        wav.write(reinterpret_cast<const char*>(crossing_audio.data()),bytes);
        const auto native=runtime->save();runtime.reset();
        const auto native_boundaries=std::move(boundaries);boundaries.clear();
        runtime=make_reference_runtime();runtime->load(path);
        bool rejected=false;
        try {runtime->restore(native);} catch(const std::runtime_error&) {rejected=true;}
        if(!rejected) throw std::runtime_error("Native state accepted by original runtime");
        ac_set_handoff_audit(true);
        std::vector<state::SaveStateContainer> entries;
        constexpr unsigned entry_frames[]{606,843,1169,1197,1380,1414,1522,1540,1557,1578};
        for(unsigned n=0;n<1800;++n) {
            runtime->advance(recording::canonical_input(n));
            if(std::find(std::begin(entry_frames),std::end(entry_frames),n+1)!=std::end(entry_frames)) entries.push_back(runtime->save());
        }
        unsigned long long checked{};
        const bool matched=ac_handoff_audit_result(&checked);
        ac_set_handoff_audit(false);
        ac_set_handoff_boundary(nullptr);
        if(!matched || checked<9000) throw std::runtime_error("Original CPU transfer contracts did not match");
        // The first crossing has identical input history. Compare the entire
        // active/inactive actor banks and collision pointers at semantic boundaries.
        compare_boundaries(native_boundaries,boundaries);
        const auto original_boundaries=std::move(boundaries);boundaries.clear();
        runtime.reset();runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(path);
        ac_set_handoff_boundary(boundary);
        for(unsigned i=0;i<entries.size();++i) {
            // Explicit diagnostic import of a genuine pre-handoff reference
            // checkpoint. Production restore never changes a state's identity.
            auto entry=entries[i];entry.core_revision=runtime->revision();runtime->restore(entry);
            boundaries.clear();
            for(unsigned n=static_cast<unsigned>(entry.frame_number);n<entry.frame_number+20;++n) {
                runtime->advance(recording::canonical_input(n));
                const auto snapshot=runtime->video_snapshot();
                const auto ram=runtime->work_ram();
                const auto byte=[&](unsigned p){return std::to_integer<unsigned>(ram[p]);};
                if(byte(0x1fe)==0xa0 && byte(0x1ff)==0x40 &&
                    ((snapshot.pc>=0xe3dd && snapshot.pc<=0xe4a0) || (snapshot.pc>=0x46fd && snapshot.pc<=0x4740))) {
                    std::cerr<<"Entry "<<i<<" frame "<<n+1<<" PC "<<std::hex<<snapshot.pc<<" caller "<<(byte(0x1fc)|(byte(0x1fd)<<8))<<'\n';
                    for(unsigned p:{0x76U,0x79U,0x7cU,0x16U,0x17U,0x53U,0x54U,0x1500U}) std::cerr<<p<<'='<<byte(p)<<' ';
                    std::cerr<<'\n';
                    throw std::runtime_error("A loading operation consumed a visible frame");
                }
            }
            compare_boundaries(boundaries,original_boundaries,4*i);
        }
        ac_set_handoff_boundary(nullptr);
        std::cout<<"PASS: all ten handoff entries preserve collision pointers, flags and both actor banks, without loading frames\n";
        std::cout<<"PASS: "<<checked<<" original CPU packet/actor contracts matched native predictions\n";
        std::cout<<"PASS: "<<crossings<<" crossings, "<<packets<<" packets, "<<waits<<" display waits removed; "<<samples<<" stereo sample frames\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
