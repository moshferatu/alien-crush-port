// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/input/nudge_multiplier.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include <iostream>
#include <vector>
using namespace alien_crush;
extern "C" void ac_set_nudge_motion(unsigned short (*)(unsigned short,unsigned short));
namespace {
double factor;
unsigned positive,negative;
void require(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
unsigned short audited_motion(unsigned short motion,unsigned short impulse) {
    require(impulse==128 || impulse==65408,"Original impulse is signed 128");
    if(impulse==128) ++positive;else ++negative;
    const int before=static_cast<std::int16_t>(static_cast<unsigned short>(motion-impulse));
    const auto expected=std::clamp<long long>(before+std::llround((impulse==128?128:-128)*factor),-32768,32767);
    return static_cast<unsigned short>(expected);
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and output recording");
        for(const auto text:{"", "nan", "inf", "-1", "16.01", "2x", "1e999", " 2"})
            require(!input::parse_nudge_multiplier(text),"Invalid multiplier rejected");
        for(const double value:{0.,0.5,1.,2.,16.,0.12345678901234567})
            require(input::recorded_nudge_multiplier("backend"+input::nudge_revision_suffix(value))==value,"Lossless identity");
        require(input::scale_nudge_motion(0x8000,128,2)==32767,"Positive overflow saturates");
        require(input::scale_nudge_motion(0x7fff,65408,2)==32768,"Negative overflow saturates");
        require(input::scale_nudge_motion(0x8000,128,1)==0x8000,"Default keeps original overflow");
        for(const double value:{0.,0.5,1.,2.,16.}) {
            auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,value);
            runtime->load(core::path_from_utf8(argv[1]));
            const auto initial=runtime->save();
            require(initial.core_revision.size()<=64,"Identity fits save header");
            recording::Recording capture{initial,{}};
            std::vector<std::string> hashes;
            for(unsigned n=0;n<750;++n) {
                auto pad=recording::canonical_input(n);
                if(n>=550 && n%30==0) pad.player_one|=Pad::button_ii;
                capture.inputs.push_back(pad);
                hashes.push_back(recording::fingerprint(runtime->advance(pad),runtime->work_ram()));
            }
            runtime->restore(initial);
            factor=value;positive=negative=0;
            ac_set_nudge_motion(audited_motion);
            for(std::size_t n=0;n<capture.inputs.size();++n) {
                const auto frame=runtime->advance(capture.inputs[n]);
                require(recording::fingerprint(frame,runtime->work_ram())==hashes[n],"ROM impulse oracle matches production and restore");
            }
            require(positive && negative,"Both halves of live ROM nudge exercised");
            std::cout<<value<<"x: "<<positive<<" positive and "<<negative<<" negative impulses verified\n";
            if(value==2) {
                recording::write(core::path_from_utf8(argv[2]),capture);
                const auto restored=recording::read(core::path_from_utf8(argv[2]));
                require(input::recorded_nudge_multiplier(restored.initial.core_revision)==2,"Recording retains factor");
            }
            ac_set_nudge_motion(nullptr);
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
