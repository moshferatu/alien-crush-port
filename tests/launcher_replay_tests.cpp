// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include <array>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
int main(int argc,char** argv) {
    try {
        if(argc<2 || argc>3) throw std::runtime_error("Expected ROM path and optional recording output");
        auto runtime=make_reference_runtime();
        runtime->load(core::path_from_utf8(argv[1]));
        for(unsigned n=0;n<480;++n) (void)runtime->advance(recording::canonical_input(n));
        const auto ready=runtime->save();
        auto byte=[&](unsigned offset) {return std::to_integer<unsigned>(runtime->work_ram()[offset]);};
        auto word=[&](unsigned lo,unsigned hi) {return byte(lo)|(byte(hi)<<8);};
        auto require=[](bool valid,const char* message) {if(!valid) throw std::runtime_error(message);};
        // Branch only through original controller input, never RAM writes. Slot 6
        // is a canonical-route assertion, not a general launcher identity rule.
        for(const unsigned held:{9U,17U,33U,100U,177U}) {
            runtime->restore(ready);
            for(unsigned n=0;n<held;++n) (void)runtime->advance({Pad::button_i,0});
            const auto travel=word(0x1c06,0x1c46);
            require(byte(0x1506)==6 && byte(0x67)==1,"Launcher identity changed");
            require(travel==(held<33?(held-1)*64:held==177?1536:2048),"Launcher draw ramp changed");
            if(held==177) require(byte(0x1846)==3,"Long hold must release from return phase");
            require(word(0xa6,0xa7)==256,"Canonical release factor changed");
            (void)runtime->advance({});
            require(byte(0x1846)==4,"Release did not enter return phase");
            require(word(0x1786,0x17c6)==((65536-travel)&65535),"Release velocity changed");
            require(word(0x1b86,0x1bc6)==0xff00,"Release return step changed");
        }
        runtime->restore(ready);
        bool plateau=false,returning=false,rest=false;
        for(unsigned n=0;n<210;++n) {
            (void)runtime->advance({Pad::button_i,0});
            const auto phase=byte(0x1846);
            plateau|=phase==2;
            returning|=phase==3;
            rest|=returning && phase==0 && word(0x1c06,0x1c46)==0;
        }
        require(plateau && returning && rest,"Held launcher cycle changed");
        runtime->restore(ready);
        require(reference_launcher_seat(runtime->work_ram()).has_value(),"Natural seated ball not recognized");
        std::array<std::byte,8192> seated{};
        std::copy(runtime->work_ram().begin(),runtime->work_ram().end(),seated.begin());
        for(const auto [offset,value]:std::array<std::pair<unsigned,unsigned>,15>{{
            {0x79,1},{0x76,1},{0x53,128},{0x53,0xc0},{0x78,16},{0x68,0},
            {0xf6,2},{0x1500,28},{0x1846,4},{0x1a40,0},{0x1680,100},
            {0x1507,6},{0x1806,31},{0x1fe,0},{0x1ff,0}}}) {
            auto invalid=seated;invalid[offset]=static_cast<std::byte>(value);
            const auto before=invalid;
            require(!apply_reference_launcher_draw(invalid,50) && invalid==before,"Rejected adapter input changed RAM");
        }
        // A completed lane set leaves $54 at 1..3 and can set $53 bit 6; the
        // ROM launcher (01:4E5C) tests only $53's sign, so the ball stays seated.
        for(const auto [lanes,scene]:std::array<std::pair<unsigned,unsigned>,3>{{{1,0},{2,0x40},{3,0}}}) {
            auto lane=seated;lane[0x54]=static_cast<std::byte>(lanes);lane[0x53]=static_cast<std::byte>(scene);
            require(reference_launcher_active(lane) && reference_launcher_seat(lane).has_value() &&
                apply_reference_launcher_draw(lane,50),"Completed lane set hid the seated ball");
        }
        for(const auto percent:{0U,101U}) {
            auto invalid=seated;
            require(!apply_reference_launcher_draw(invalid,static_cast<std::uint8_t>(percent)) && invalid==seated,
                "Invalid plunger percentage changed RAM");
        }
        (void)runtime->advance({Pad::run,0});
        (void)runtime->advance({});
        require((byte(0xf6)&2)!=0 && !runtime->apply_plunger_draw(50),"Paused launcher accepted adapter writes");
        (void)runtime->advance({Pad::run,0});
        unsigned resume_wait=0;
        bool resumed=false;
        for(unsigned n=0;n<32;++n) {
            const bool eligible=reference_launcher_seat(runtime->work_ram()).has_value();
            if(word(0x1fe,0x1ff)==0x408e) {
                ++resume_wait;
                require(!eligible && !runtime->apply_plunger_draw(50),"Resume delay accepted adapter writes");
            } else resumed|=eligible;
            (void)runtime->advance({});
        }
        require(resume_wait>=15 && resumed,"Pause/resume epoch guard was not exercised");
        // These are explicit adapter trials, separate from the unmodified-ROM
        // evidence above: draw RAM changes, original release/collisions run.
        for(const unsigned percent:{1U,25U,50U,75U,100U}) {
            runtime->restore(ready);
            for(unsigned n=0;n<300;++n) {
                const auto ball_vx=word(0x1700,0x1740),ball_vy=word(0x1780,0x17c0);
                require(runtime->apply_plunger_draw(static_cast<std::uint8_t>(percent)),"Seated adapter draw rejected");
                require(word(0x1700,0x1740)==ball_vx && word(0x1780,0x17c0)==ball_vy,
                    "Draw adapter changed ball velocity");
                (void)runtime->advance({Pad::button_i,0});
                require(word(0x1c06,0x1c46)==percent*2048/100 && byte(0x1846)==1,
                    "Physical draw drifted or timed out");
            }
            const auto charged=runtime->save();
            const auto shot=runtime->advance({});
            const auto expected=recording::fingerprint(shot,runtime->work_ram());
            require(byte(0x1846)==4 && word(0x1786,0x17c6)==((65536-percent*2048/100)&65535),
                "Original release did not use physical draw strength");
            require(!runtime->apply_plunger_draw(50),"Release phase was recaptured by adapter");
            runtime->restore(charged);
            const auto repeated=runtime->advance({});
            require(recording::fingerprint(repeated,runtime->work_ram())==expected,"Charged save/restore changed shot");
        }
        runtime->restore(ready);
        for(const unsigned percent:{25U,100U,50U,1U,75U,25U}) {
            require(runtime->apply_plunger_draw(static_cast<std::uint8_t>(percent)),"Changing rod position lost seat");
            (void)runtime->advance({Pad::button_i,0});
            require(reference_launcher_seat(runtime->work_ram()).has_value(),"Ball no longer seated after draw change");
        }
        for(const unsigned percent:{1U,25U,50U,75U,100U}) {
            // 3 cancels immediately, before the first original-game update.
            for(const unsigned pause_stage:{0U,1U,2U,3U}) {
                runtime->restore(ready);
                require(runtime->apply_plunger_draw(static_cast<std::uint8_t>(percent)),"Cancellation setup failed");
                if(pause_stage!=3) (void)runtime->advance({Pad::button_i,0});
                if(pause_stage==1 || pause_stage==2) {
                    (void)runtime->advance({Pad::button_i|Pad::run,0});
                    (void)runtime->advance({Pad::button_i,0});
                    require((byte(0xf6)&2)!=0,"Cancellation pause setup failed");
                    if(pause_stage==2) (void)runtime->advance({Pad::button_i|Pad::run,0});
                }
                const auto charged=runtime->save();
                const auto ball_vx=word(0x1700,0x1740),ball_vy=word(0x1780,0x17c0);
                require(runtime->cancel_plunger_draw(),"Pinned draw cancellation rejected");
                require(word(0x1700,0x1740)==ball_vx && word(0x1780,0x17c0)==ball_vy,
                    "Cancellation changed ball velocity");
                require(byte(0x1846)==0 && word(0x1c06,0x1c46)==0 && byte(0x1806)==23 &&
                    word(0x1786,0x17c6)==0,"Cancellation did not return launcher to rest");
                std::array<std::byte,8192> cancelled{};
                std::copy(runtime->work_ram().begin(),runtime->work_ram().end(),cancelled.begin());
                require(!runtime->cancel_plunger_draw() && std::equal(cancelled.begin(),cancelled.end(),
                    runtime->work_ram().begin()),"Repeated cancellation changed RAM");
                const auto cancelled_frame=runtime->advance({});
                const auto expected=recording::fingerprint(cancelled_frame,runtime->work_ram());
                runtime->restore(charged);
                require(runtime->cancel_plunger_draw(),"Restored cancellation rejected");
                const auto repeated=runtime->advance({});
                require(recording::fingerprint(repeated,runtime->work_ram())==expected,
                    "Cancellation changed after save/restore");
                if(pause_stage==1) (void)runtime->advance({Pad::run,0});
                for(unsigned n=0;n<240;++n) {
                    (void)runtime->advance({});
                    require(byte(0x1846)==0 && word(0x1c06,0x1c46)==0 &&
                        word(0x15c0,0x1600)==272 && word(0x1680,0x16c0)>=242 &&
                        word(0x1680,0x16c0)<=244,"Cancelled launcher fired or lost seated ball");
                }
                require(runtime->apply_plunger_draw(50),"Cancelled launcher could not draw again");
                (void)runtime->advance({Pad::button_i,0});
                (void)runtime->advance({});
                require(byte(0x1846)==4 && word(0x1786,0x17c6)==64512,"Redraw did not release normally");
                const auto fired=runtime->save();
                require(!runtime->cancel_plunger_draw(),"Cancellation accepted a fired launcher");
                require(state::encode(runtime->save())==state::encode(fired),"Rejected cancellation changed runtime");
            }
        }
        runtime->restore(ready);
        (void)runtime->advance({Pad::button_i,0});
        const auto natural=runtime->save();
        require(!runtime->cancel_plunger_draw(),"Cancellation accepted natural digital draw");
        require(state::encode(runtime->save())==state::encode(natural),"Rejected natural cancellation changed runtime");
        auto pinned=seated;
        require(apply_reference_launcher_draw(pinned,50),"Cancellation rejection setup failed");
        for(const auto [offset,value]:std::array<std::pair<unsigned,unsigned>,15>{{
            {0x79,1},{0x76,1},{0x53,128},{0x53,0xc0},{0x78,16},{0x68,0},
            {0x1500,28},{0x1846,4},{0x1a40,0},{0x1680,100},{0x1507,6},
            {0x1806,31},{0x1fe,0},{0x1b86,64},{0x1bc6,255}}}) {
            auto invalid=pinned;invalid[offset]=static_cast<std::byte>(value);
            const auto before=invalid;
            require(!cancel_reference_launcher_draw(invalid) && invalid==before,
                "Rejected cancellation changed RAM");
        }
        recording::Recording session{ready,{},{}};
        std::vector<std::string> fingerprints;
        runtime->restore(ready);
        for(unsigned n=0;n<120;++n) {
            // The midpoint checkpoint lies inside a pinned draw, before cancellation.
            const std::uint8_t command=n==0?25:n==65?101:n==80?75:n==100?100:0;
            const PadInput pad{static_cast<std::uint16_t>(n<65 || (n>=80 && n<100)?Pad::button_i:0),0};
            session.inputs.push_back(pad);session.plunger.push_back(command);
            const auto frame=recording::advance(*runtime,pad,command);
            fingerprints.push_back(recording::fingerprint(frame,runtime->work_ram()));
        }
        require(byte(0x1846)==4 || !reference_launcher_seat(runtime->work_ram()),"Recorded release did not launch");
        const auto fired=runtime->save();
        bool rejected=false;
        try {(void)recording::advance(*runtime,{},25);}catch(const std::exception&){rejected=true;}
        require(rejected && state::encode(runtime->save())==state::encode(fired),"Rejected replay command advanced or mutated runtime");
        if(argc==3) {
            const auto path=core::path_from_utf8(argv[2]);
            recording::write(path,session);
            const auto replay=recording::read(path);
            runtime->restore(replay.initial);
            for(std::size_t n=0;n<replay.inputs.size();++n) {
                const auto frame=recording::advance(*runtime,replay.inputs[n],replay.plunger_at(n));
                require(recording::fingerprint(frame,runtime->work_ram())==fingerprints[n],"File plunger replay diverged");
            }
        }
        runtime->load(core::path_from_utf8(argv[1]));
        unsigned seated_frames=0;
        for(unsigned n=0;n<1800;++n) {
            (void)runtime->advance(recording::canonical_input(n));
            if(reference_launcher_seat(runtime->work_ram())) {
                ++seated_frames;
                require(n>=360 && n<580,"Launcher gate accepted canonical menu/flight/transition state");
            }
        }
        require(seated_frames>0,"Canonical seat gate never opened");
        std::cout<<"Five natural hold durations, four strengths and held-button return cycle verified\n";
        std::cout<<"Five adapter draws hold for 300 frames and release deterministically; invalid/paused states rejected\n";
        std::cout<<"Twenty immediate/active/paused/resuming cancellations stay seated, restore exactly and redraw successfully\n";
        std::cout<<seated_frames<<" canonical frames accepted only before first release\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
