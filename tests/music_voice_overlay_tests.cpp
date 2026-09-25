// SPDX-License-Identifier: GPL-3.0-or-later
// The music voice overlay's model, and, given the ROM, the overlay in the
// running game: it adds the music's lost voices and changes nothing else.
#include "music_voice_overlay.hpp"
#ifdef AC_OVERLAY_ROM_TESTS
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
using Overlay=alien_crush::audio::MusicVoiceOverlay;
using Writes=std::vector<Overlay::PsgWrite>;
Writes step(Overlay& overlay,std::uint16_t pc,std::uint8_t a=0,std::uint8_t x=0,std::uint8_t y=0) {
    const auto writes=overlay.step(pc,a,x,y);
    return {writes.begin(),writes.end()};
}
// What a push programs on one channel, from its select to the next.
Writes channel_writes(const Writes& writes,std::uint8_t channel) {
    Writes out;
    bool in=false;
    for(const auto& w:writes) {
        if(w.address==0) in=w.value==channel;
        else if(in) out.push_back(w);
    }
    return out;
}
// The commit of one voice, as 19:A6C3 stores it.
void commit(Overlay& overlay,std::uint8_t slot,std::uint8_t channel,std::uint8_t amplitude,std::uint8_t waveform=5) {
    (void)step(overlay,0xA73E,0x40,slot,channel);
    (void)step(overlay,0xA744,0x01,slot,channel);
    (void)step(overlay,0xA74A,waveform,slot,channel);
    (void)step(overlay,0xA750,amplitude,slot,channel);
    (void)step(overlay,0xA756,0xEE,slot,channel);
}
bool silent(const Writes& writes) {return writes.size()==1 && writes[0].address==4 && writes[0].value==0;}

void check_model() {
    std::vector<std::byte> waveforms(Overlay::kWaveformTableSize);
    for(std::size_t i=0;i<waveforms.size();++i) waveforms[i]=static_cast<std::byte>(i&31);
    Overlay overlay(waveforms);
    require(step(overlay,0xA000).empty() && step(overlay,0xA7D3).empty(),"Only the driver's own steps count");

    // Music on channel 3 from slot 0; the flipper effect in slot 7 takes it.
    (void)step(overlay,0xA6AE);
    commit(overlay,0,3,0x1C);
    commit(overlay,1,4,0x10);
    commit(overlay,7,3,0x1F,9);
    require(overlay.displaced()==0x08,"An effect's commit takes its channel");
    auto writes=step(overlay,0xA139);
    const auto three=channel_writes(writes,3);
    require(three.size()==4+2+32+1 && three[0].address==2 && three[0].value==0x40 && three[1].address==3 &&
        three[1].value==0x01 && three[2].address==5 && three[2].value==0xEE && three[3].address==7,
        "The taken channel plays the music voice's period, balance and noise");
    require(three[4].address==4 && three[4].value==0x40 && three[5].address==4 && three[5].value==0 &&
        three[6].address==6 && three[6].value==(5*32&31) && three.back().address==4 && three.back().value==(0x1C|0x80),
        "...its waveform, uploaded as the driver uploads one, and its amplitude");
    for(const std::uint8_t channel:std::array<std::uint8_t,5>{0,1,2,4,5})
        require(silent(channel_writes(writes,channel)),"The other channels are the physical PSG's");

    // The next pass: the same waveform is not uploaded again, and a channel
    // no effect takes goes back to the physical PSG alone.
    (void)step(overlay,0xA6AE);
    require(overlay.displaced()==0,"A new pass starts with every channel the music's");
    commit(overlay,0,3,0x1A);
    commit(overlay,8,3,0,9);
    writes=step(overlay,0xA139);
    require(channel_writes(writes,3).size()==5,"A waveform already loaded is not uploaded again");
    (void)step(overlay,0xA6AE);
    commit(overlay,0,3,0x1A);
    require(silent(channel_writes(step(overlay,0xA139),3)),"A channel no effect took is not doubled");

    // A silent effect -- no period, amplitude zero -- still takes the channel.
    (void)step(overlay,0xA6AE);
    commit(overlay,0,2,0x18);
    (void)step(overlay,0xA6F5,0,6,2);
    require(overlay.displaced()==0x04 && channel_writes(step(overlay,0xA139),2).back().value==(0x18|0x80),
        "The music plays where a silent effect cleared its channel");

    // The exclusive slot clears every earlier latch, the music's too.
    (void)step(overlay,0xA6AE);
    commit(overlay,0,1,0x18);
    (void)step(overlay,0xA6AE);
    commit(overlay,7,1,0x10);
    require(silent(channel_writes(step(overlay,0xA139),1)),"The exclusive slot silences the music as on hardware");

    // A music voice at rest is no voice; a restored state knows nothing.
    (void)step(overlay,0xA6AE);
    commit(overlay,0,5,0);
    commit(overlay,6,5,0x10);
    require(silent(channel_writes(step(overlay,0xA139),5)),"A resting voice stays at rest");
    overlay.reset();
    (void)step(overlay,0xA6AE);
    commit(overlay,7,0,0x10);
    require(silent(channel_writes(step(overlay,0xA139),0)),"Nothing is played from before a reset");
    (void)step(overlay,0xA6AE);
    (void)step(overlay,0xA750,0x10,6,6);
    require(overlay.displaced()==0,"A channel past the sixth is ignored");
}

#ifdef AC_OVERLAY_ROM_TESTS
// 16-bit stereo, for listening to what the overlay adds.
void write_wav(const std::filesystem::path& path,const std::vector<std::vector<std::int16_t>>& frames,int rate) {
    std::vector<std::int16_t> samples;
    for(const auto& frame:frames) samples.insert(samples.end(),frame.begin(),frame.end());
    const auto bytes=static_cast<std::uint32_t>(samples.size()*2);
    std::vector<std::byte> out(44+bytes);
    const auto put=[&](std::size_t at,std::uint32_t value,int size) {
        for(int i=0;i<size;++i) out[at+static_cast<std::size_t>(i)]=static_cast<std::byte>(value>>(8*i));
    };
    std::memcpy(out.data(),"RIFF",4);put(4,36+bytes,4);std::memcpy(out.data()+8,"WAVEfmt ",8);
    put(16,16,4);put(20,1,2);put(22,2,2);put(24,static_cast<std::uint32_t>(rate),4);put(28,static_cast<std::uint32_t>(rate)*4,4);
    put(32,4,2);put(34,16,2);std::memcpy(out.data()+36,"data",4);put(40,bytes,4);
    std::memcpy(out.data()+44,samples.data(),bytes);
    alien_crush::core::write_file_atomic(path,out);
}
struct Run {
    std::vector<std::vector<std::int16_t>> audio;
    std::vector<std::vector<std::byte>> ram;
    int rate=0;
};
Run play(const std::filesystem::path& rom,bool overlay,unsigned frames) {
    using namespace alien_crush;
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet);
    runtime->load(rom);
    if(overlay) runtime->music_voice_overlay(true);
    Run run;
    run.rate=runtime->sample_rate();
    for(unsigned n=0;n<frames;++n) {
        const auto frame=runtime->advance(recording::canonical_input(n));
        run.audio.emplace_back(frame.stereo.begin(),frame.stereo.end());
        const auto ram=runtime->work_ram();
        run.ram.emplace_back(ram.begin(),ram.end());
        // A restored state starts the overlay afresh, and the game runs on unchanged.
        if(n==900) runtime->restore(runtime->save());
    }
    return run;
}
void check_game(const std::filesystem::path& rom,const std::filesystem::path& listen) {
    constexpr unsigned kFrames=1500;
    const auto original=play(rom,false,kFrames),overlaid=play(rom,true,kFrames);
    unsigned differing=0,first=0;
    for(unsigned n=0;n<kFrames;++n) {
        require(original.ram[n]==overlaid.ram[n],"The overlay changes nothing in the game");
        require(original.audio[n].size()==overlaid.audio[n].size(),"The overlay keeps the sample count");
        if(original.audio[n]!=overlaid.audio[n] && !differing++) first=n;
    }
    // The table's effects take music channels, and the overlay gives them back;
    // before the first effect there is nothing to give.
    require(differing>=30,"Sound effects on the table take the music's voices, and the overlay plays them");
    require(first>=100,"The overlay adds nothing before an effect plays");
    std::cout<<"Overlay changed "<<differing<<" of "<<kFrames<<" frames' sound, from frame "<<first<<'\n';
    // What it adds, on its own: the music's voices, never a second game's worth.
    Run added=overlaid;
    double added_power=0,original_power=0;
    int added_peak=0;
    for(unsigned n=0;n<kFrames;++n) for(std::size_t i=0;i<added.audio[n].size();++i) {
        const int difference=overlaid.audio[n][i]-original.audio[n][i];
        added.audio[n][i]=static_cast<std::int16_t>(std::clamp(difference,-32768,32767));
        added_power+=double(difference)*difference;
        original_power+=double(original.audio[n][i])*original.audio[n][i];
        added_peak=std::max(added_peak,std::abs(difference));
    }
    std::cout<<"Added level: "<<std::sqrt(added_power/std::max(original_power,1.0))*100<<"% of the original's RMS, peak "<<added_peak<<'\n';
    require(added_power>0 && added_power<original_power,"The overlay adds voices, not a second game's worth of sound");
    if(!listen.empty()) {
        std::filesystem::create_directories(listen);
        write_wav(listen/"original.wav",original.audio,original.rate);
        write_wav(listen/"overlay.wav",overlaid.audio,overlaid.rate);
        write_wav(listen/"added.wav",added.audio,added.rate);
    }
}
#endif
}

int main([[maybe_unused]] int argc,[[maybe_unused]] char** argv) {
    try {
        check_model();
#ifdef AC_OVERLAY_ROM_TESTS
        require(argc==2 || argc==3,"Expected the ROM, and a folder for listening if wanted");
        check_game(alien_crush::core::path_from_utf8(argv[1]),argc==3?alien_crush::core::path_from_utf8(argv[2]):std::filesystem::path{});
#endif
        std::cout<<"PASS: music voice overlay\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
