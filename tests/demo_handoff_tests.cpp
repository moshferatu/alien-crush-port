// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
extern "C" void ac_handoff_counts(unsigned long long*,unsigned long long*);
extern "C" void ac_set_handoff_audit(bool);
extern "C" bool ac_handoff_audit_result(unsigned long long*);
extern "C" void ac_set_handoff_boundary(void (*)(unsigned,const unsigned char*));
namespace {
struct Boundary {unsigned pc;std::array<unsigned char,8192> ram;};
std::vector<Boundary> boundaries;
void boundary(unsigned pc,const unsigned char* ram) {
    Boundary value{pc,{}};std::copy_n(ram,8192,value.ram.begin());boundaries.push_back(value);
}
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and demo recording paths");
        const auto path=core::path_from_utf8(argv[1]);
        auto runtime=make_reference_runtime();runtime->load(path);
        ac_set_handoff_boundary(boundary);ac_set_handoff_audit(true);
        std::vector<state::SaveStateContainer> entries;
        std::vector<std::vector<Boundary>> expected;
        bool upper=false,lower=false;
        for(unsigned n=0;n<5500 && !(upper && lower);++n) {
            const auto scene=diagnostics::observe_alien_state(runtime->work_ram());
            const bool watch=scene.demo_play && !scene.title_menu &&
                ((scene.table_code==0 && !upper) || (scene.table_code==1 && !lower));
            const auto checkpoint=watch?runtime->save():state::SaveStateContainer{};
            const auto before=boundaries.size();runtime->advance({});
            if(watch && boundaries.size()>before && (boundaries[before].pc==0x89cd || boundaries[before].pc==0x8a28)) {
                entries.push_back(checkpoint);
                while(boundaries.size()<before+4) runtime->advance({});
                expected.emplace_back(boundaries.begin()+before,boundaries.begin()+before+4);
                if(scene.table_code==0) upper=true;else lower=true;
            }
        }
        unsigned long long audited{};
        require(ac_handoff_audit_result(&audited) && audited>0,"Original demo transfer contracts match native predictions");
        ac_set_handoff_audit(false);
        require(upper && lower && entries.size()==2,"Natural demo exercises both seam directions");
        runtime.reset();runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(path);
        for(unsigned i=0;i<entries.size();++i) {
            // Diagnostic import only; production restore still checks identity.
            auto entry=entries[i];entry.core_revision=runtime->revision();runtime->restore(entry);
            boundaries.clear();
            for(unsigned n=0;n<120;++n) {
                const auto frame=runtime->advance({});
                require(frame.stereo.size()>1000,"Every demo frame retains its audio batch");
                const auto video=runtime->video_snapshot();const auto ram=runtime->work_ram();
                const auto b=[&](unsigned p){return std::to_integer<unsigned>(ram[p]);};
                require(!(b(0x1fe)==0x3a && b(0x1ff)==0x40 &&
                    ((video.pc>=0xe3dd && video.pc<=0xe4a0) || (video.pc>=0x46fd && video.pc<=0x4740))),
                    "Demo seam loader cannot consume a visible frame");
            }
            require(boundaries.size()>=4,"Accelerated demo crosses seam");
            for(unsigned j=0;j<4;++j) {
                require(boundaries[j].pc==expected[i][j].pc,"Demo handoff operation order preserved");
                for(const auto [start,end]:{std::pair{0x42U,0x46U},std::pair{0x200U,0xa00U},std::pair{0x1500U,0x1d00U}})
                    require(std::equal(boundaries[j].ram.begin()+start,boundaries[j].ram.begin()+end,expected[i][j].ram.begin()+start),
                        "Demo collision pointers and both actor banks preserved");
            }
            unsigned long long packets{},waits{};ac_handoff_counts(&packets,&waits);
            // The checkpoint can follow the entry wait; the exit wait must
            // still be removed, alongside the loader and actor transfers.
            require(packets>0 && waits>=1,"Demo transfer and display waits accelerated");
        }
        ac_set_handoff_boundary(nullptr);
        runtime.reset();runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet);runtime->load(path);
        bool demo_seen=false,title_returned=false;
        std::optional<state::SaveStateContainer> demo;
        recording::Recording replay;
        std::vector<std::string> fingerprints;
        std::uint64_t samples=0;
        unsigned frames=0;
        for(;frames<6500;++frames) {
            const auto frame=runtime->advance({});
            samples+=frame.stereo.size()/2;
            if(demo && fingerprints.size()<400) {
                replay.inputs.push_back({});
                fingerprints.push_back(recording::fingerprint(frame,runtime->work_ram()));
            }
            const auto scene=diagnostics::observe_alien_state(runtime->work_ram());
            if(scene.demo_play && !scene.title_menu) {
                demo_seen=true;
                if(!demo) {demo=runtime->save();replay.initial=*demo;}
            }
            if(demo_seen && scene.title_menu && !scene.demo_play) {title_returned=true;++frames;break;}
        }
        require(title_returned,"Continuous cabinet demo returns naturally to title");
        const auto expected_samples=frames*runtime->sample_rate()/runtime->frames_per_second();
        require(samples>expected_samples-2000 && samples<expected_samples+2000,"Full demo retains audio timeline");
        unsigned long long packets{},waits{};ac_handoff_counts(&packets,&waits);
        require(packets>0 && waits>=4,"Natural cabinet demo accelerates multiple seam crossings");
        runtime->restore(*demo);
        for(const auto& expected_frame:fingerprints) {
            const auto frame=runtime->advance({});
            require(recording::fingerprint(frame,runtime->work_ram())==expected_frame,"Native demo restores exact RAM, pixels and audio across seam");
        }
        recording::write(core::path_from_utf8(argv[2]),replay);
        runtime->restore(*demo);
        bool interrupted=false;
        for(unsigned n=0;n<120;++n) {
            runtime->advance({static_cast<std::uint16_t>(n>=60 && n<64?Pad::run:0),0});
            const auto scene=diagnostics::observe_alien_state(runtime->work_ram());
            interrupted|=scene.title_menu && !scene.demo_play;
        }
        require(interrupted,"Run still exits continuous cabinet demo");
        std::cout<<"PASS: both demo seam directions, original transfer audit, no loading frames, continuous audio and exact replay\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
