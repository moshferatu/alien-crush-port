// SPDX-License-Identifier: GPL-3.0-or-later
#include "high_score_autosave.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {void require(bool ok,const char* message){if(!ok) throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and scratch directory");
        const auto rom=core::path_from_utf8(argv[1]),dir=core::path_from_utf8(argv[2]);
        std::filesystem::create_directories(dir);
        const auto path=dir/"alien-crush.hiscore";
        std::filesystem::remove(path);
        auto runtime=make_reference_runtime();runtime->load(rom);
        host::HighScoreAutosave saving(*runtime,path);
        require(!diagnostics::observe_alien_state(runtime->work_ram()).title_menu,"Restore occurs before title begins");
        const auto defaults=*core::read_high_scores(runtime->work_ram());
        auto earned=defaults;
        earned[0]=std::byte{0x20};earned[5]=std::byte{0x4e}; // 2,000,000 points
        earned[15]=std::byte{0x41};earned[20]=std::byte{0x42};earned[25]=std::byte{0x43}; // ABC
        require(core::valid_high_scores(earned),"Valid earned table");
        const std::vector<std::byte> before(runtime->work_ram().begin(),runtime->work_ram().end());
        require(runtime->restore_high_scores(earned),"Install earned-score fixture");
        for(unsigned p=0;p<8192;++p) {
            const bool ranked=std::any_of(core::score_rows.begin(),core::score_rows.end(),[&](auto row){return p>=row && p<row+5;});
            if(!ranked) require(before[p]==runtime->work_ram()[p],"Persistence changes only ranked columns, not current player or gameplay");
        }
        for(unsigned n=0;n<89;++n) saving.poll(runtime->work_ram());
        require(!std::filesystem::exists(path),"Wait for settled scores before writing");
        saving.poll(runtime->work_ram());
        require(core::decode_high_scores(core::read_file(path))==earned,"Autosave retains score and all initials");
        const auto stamp=std::filesystem::last_write_time(path);
        for(unsigned n=0;n<120;++n) saving.poll(runtime->work_ram());
        require(std::filesystem::last_write_time(path)==stamp,"Unchanged table is not rewritten");
        earned[25]=std::byte{0x44};runtime->restore_high_scores(earned);
        saving.poll(runtime->work_ram(),true);
        require(core::decode_high_scores(core::read_file(path))==earned,"Exit captures a change before the next poll");
        runtime.reset();
        runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet);runtime->load(rom);
        host::HighScoreAutosave restored(*runtime,path);
        require(core::read_high_scores(runtime->work_ram())==earned,"Fresh cabinet runtime restores saved scores and initials");
        recording::Recording recording;recording.initial=runtime->save();
        std::vector<std::string> frames;
        std::vector<std::string> visuals;
        for(unsigned n=0;n<180;++n) {
            recording.inputs.push_back({});
            const auto frame=runtime->advance({});frames.push_back(recording::fingerprint(frame,runtime->work_ram()));
            auto silent=frame;silent.stereo={};visuals.push_back(recording::fingerprint(silent,runtime->work_ram()));
        }
        runtime.reset();runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet);runtime->load(rom);
        runtime->restore(recording.initial);
        unsigned replay_frame=0;
        for(const auto& expected:frames) {
            const auto frame=runtime->advance({});
            auto silent=frame;silent.stereo={};
            require(recording::fingerprint(silent,runtime->work_ram())==visuals[replay_frame],"Restored scores replay RAM/video");
            if(recording::fingerprint(frame,runtime->work_ram())!=expected) throw std::runtime_error("Replay audio differs at "+std::to_string(replay_frame)+" checkpoint "+std::to_string(recording.initial.frame_number));
            ++replay_frame;
        }
        auto bytes=core::encode_high_scores(earned);
        for(unsigned i=0;i<bytes.size();++i) {
            bytes[i]^=std::byte{1};require(!core::decode_high_scores(bytes),"Corrupt score file rejected");bytes[i]^=std::byte{1};
        }
        require(!core::decode_high_scores(std::span(bytes).first(39)),"Truncated file rejected");
        auto invalid=earned;invalid[15]=std::byte{};
        require(!runtime->restore_high_scores(invalid),"Invalid names cannot be restored");
        require(core::read_high_scores(runtime->work_ram())==earned,"Rejected restore leaves scores intact");
        bytes[7]^=std::byte{1};core::write_file_atomic(path,bytes);
        runtime.reset();runtime=make_reference_runtime();runtime->load(rom);
        host::HighScoreAutosave corrupt(*runtime,path);
        require(core::read_high_scores(runtime->work_ram())==defaults,"Corrupt file falls back to initialized cartridge defaults");
        corrupt.poll(runtime->work_ram(),true);
        const auto preserved=core::read_file(path);
        require(std::equal(preserved.begin(),preserved.end(),bytes.begin(),bytes.end()),"Unreadable file is not overwritten merely by launching");
        std::cout<<"PASS: settled autosave, shutdown flush, restart restoration, ranked-only writes, corruption rejection and exact replay\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
