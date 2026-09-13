// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/core/files.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#include <iostream>
namespace alien_crush::host {
class HighScoreAutosave {
    std::filesystem::path path_;
    core::HighScores last_{};
    bool pending_{},reported_{};
    unsigned ticks_{},settled_{};
    void store(int budget) {
        if(!pending_) return;
        try {
            core::write_file_atomic(path_,core::encode_high_scores(last_),budget);
            pending_=reported_=false;
        } catch(const std::exception& e) {
            if(!reported_) std::cerr<<"Could not save high scores: "<<e.what()<<'\n';
            reported_=true;
        }
    }
public:
    HighScoreAutosave(ReferenceRuntime& runtime,const std::filesystem::path& path):path_(path) {
        std::optional<core::HighScores> stored;
        try {
            if(std::filesystem::exists(path_)) {
                stored=core::decode_high_scores(core::read_file(path_,40));
                if(!stored) throw std::runtime_error("Invalid high-score file");
            }
        } catch(const std::exception& e) {std::cerr<<"Ignoring high-score file: "<<e.what()<<'\n';}
        // Boot initializes six parallel rows at E0B5..E0DD. Wait for all
        // five valid names/ranks before restoring; the first title screen
        // has not loaded yet. Recording starts AFTER this startup restore.
        auto table=core::read_high_scores(runtime.work_ram());
        for(unsigned n=0;!table && n<180;++n) {
            runtime.advance({});table=core::read_high_scores(runtime.work_ram());
        }
        if(!table) throw std::runtime_error("High-score table was not initialized at boot");
        if(stored) {
            if(!runtime.restore_high_scores(*stored)) throw std::runtime_error("Could not restore high scores");
            table=stored;
        }
        last_=*table;
    }
    void poll(std::span<const std::byte> ram,bool flush=false) {
        if(!flush && ++ticks_%30) return;
        if(const auto table=core::read_high_scores(ram)) {
            if(*table!=last_) {last_=*table;pending_=true;settled_=0;}
            else if(pending_) ++settled_;
        }
        if(flush || settled_>=2) store(flush?5000:0);
    }
};
}
