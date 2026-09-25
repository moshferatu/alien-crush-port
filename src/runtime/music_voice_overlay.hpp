// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace alien_crush::audio {
// The music's voices, kept while sound effects play, on a second PSG: Devil's
// Crush's music voice overlay (its src/audio/music_voice_overlay.cpp), for
// this game's older build of the same Compile sound driver.
//
// The driver lives in bank $19 at MPR5 (entry table 19:A000). Every frame
// its tick (19:A015) clears the six channels' amplitude latches (19:A6AE),
// walks slots 0 to 8 upward, and commits each slot's voice (19:A6C3) into
// the latch bank at $3D02-$3D25 on the channel `$3D6E,X` names -- X the slot,
// Y the channel, A the value at every store. Slots 0-5 are the song's voices;
// 6-8 are the sound effects. Walking upward, an effect commits after the
// music and overwrites its channel's latches, so on the hardware the music
// voice on that channel goes silent -- including for a silent effect, whose
// zero amplitude is stored all the same -- though the voice keeps advancing
// unheard. Then 19:A139 pushes the six channels' latches to the PSG.
//
// This keeps a second latch bank with the music slots' stores alone, and at
// the push programs a second PSG from it for the channels an effect took, and
// only those; the other channels are the physical PSG's, untouched. Whatever
// the music does to a channel -- rests, the exclusive slot's clear, a song
// that leaves channels unused -- the copy does too, since it is built from
// the same stores.
class MusicVoiceOverlay {
public:
    struct PsgWrite {
        std::uint8_t address,value;
    };
    // The PSG waveforms the driver uploads, 32 bytes each: ROM 19:A9DF on
    // (file offset 0x329DF), indexed by the latch's waveform number.
    static constexpr std::size_t kWaveformOffset=0x329DF;
    static constexpr std::size_t kWaveformTableSize=256*32;
    static constexpr std::uint16_t kFirstPc=0xA139,kEndPc=0xA7D3;

    explicit MusicVoiceOverlay(std::span<const std::byte> waveforms):waveforms_(waveforms.begin(),waveforms.end()) {
        waveforms_.resize(kWaveformTableSize);
        reset();
    }
    // After a power-on or a restored state: nothing is known of the latches
    // or of what the second PSG holds. It is silent until the next push.
    void reset() {
        shadow_={};
        loaded_.fill(-1);
        displaced_=0;
    }
    // One sound-driver instruction in bank $19 about to run, with its
    // registers. Returns the writes the second PSG needs now, which is only
    // ever at the push.
    std::span<const PsgWrite> step(std::uint16_t pc,std::uint8_t a,std::uint8_t x,std::uint8_t y) {
        writes_.clear();
        switch(pc) {
        case 0xA6AE:  // the amplitude latches cleared: a new pass, or the exclusive slot's
            for(auto& latch:shadow_) latch.amplitude=0;
            displaced_=0;
            break;
        case 0xA6F5: case 0xA750: store(x,y,&Latch::amplitude,a); break;
        case 0xA70B: case 0xA738: case 0xA7C2: store(x,y,&Latch::noise,a); break;
        case 0xA710: case 0xA74A: store(x,y,&Latch::waveform,a); break;
        case 0xA73E: case 0xA7C7: store(x,y,&Latch::period_low,a); break;
        case 0xA744: case 0xA7CC: store(x,y,&Latch::period_high,a); break;
        case 0xA756: store(x,y,&Latch::balance,a); break;
        case kFirstPc: push(); break;
        default: break;
        }
        return writes_;
    }
    // The channels an effect took this pass, as a bit each.
    std::uint8_t displaced() const {return displaced_;}

private:
    static constexpr std::uint8_t kMusicSlots=6,kChannels=6;
    struct Latch {
        std::uint8_t period_low=0,period_high=0,amplitude=0,balance=0,waveform=0xFF,noise=0;
    };
    void store(std::uint8_t slot,std::uint8_t channel,std::uint8_t Latch::*field,std::uint8_t value) {
        if(channel>=kChannels) return;
        if(slot<kMusicSlots) shadow_[channel].*field=value;
        else displaced_=static_cast<std::uint8_t>(displaced_|1U<<channel);
    }
    void write(std::uint8_t address,std::uint8_t value) {writes_.push_back({address,value});}
    // As 19:A139 pushes the physical latches, for the displaced channels; the
    // others are switched off.
    void push() {
        for(std::uint8_t channel=0;channel<kChannels;++channel) {
            const auto& latch=shadow_[channel];
            write(0x00,channel);
            if(!(displaced_&1U<<channel) || !latch.amplitude) {write(0x04,0);continue;}
            write(0x02,latch.period_low);
            write(0x03,(latch.period_low|latch.period_high)!=0?latch.period_high:std::uint8_t{0x0F});
            write(0x05,latch.balance);
            write(0x07,latch.noise);
            if(latch.waveform!=0xFF && latch.waveform!=loaded_[channel]) {
                write(0x04,0x40);
                write(0x04,0x00);
                for(std::size_t i=0;i<32;++i)
                    write(0x06,std::to_integer<std::uint8_t>(waveforms_[static_cast<std::size_t>(latch.waveform)*32+i]));
                loaded_[channel]=latch.waveform;
            }
            write(0x04,static_cast<std::uint8_t>(latch.amplitude|0x80));
        }
    }
    std::vector<std::byte> waveforms_;
    std::array<Latch,kChannels> shadow_{};
    std::array<int,kChannels> loaded_{};
    std::uint8_t displaced_=0;
    std::vector<PsgWrite> writes_;
};
}
