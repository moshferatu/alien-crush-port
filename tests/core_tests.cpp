// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/rom_image.hpp"
#include "alien_crush/core/save_state.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/render/sharp_pixel_scaler.hpp"
#include "alien_crush/input/plunger.hpp"
#include <iostream>
#include <stdexcept>
#include <Windows.h>
using namespace alien_crush;
namespace { void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);} }
int main() {
    try {
        const std::string abc="abc";
        auto hash=rom::compute_digests(std::as_bytes(std::span(abc)));
        require(hash.sha256=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 known vector");
        require(hash.md5=="900150983cd24fb0d6963f7d28e17f72","MD5 known vector");
        require(!rom::matches_supported_rom(262144,hash),"Reject wrong ROM");
        state::SaveStateContainer saved;
        saved.frame_number=123; saved.rom_sha256=rom::kSupportedSha256; saved.core_revision="test";
        saved.core_state={std::byte{4},std::byte{7}};
        auto encoded=state::encode(saved);
        auto decoded=state::decode(encoded);
        require(decoded.decoded && decoded.container.core_state==saved.core_state && decoded.container.frame_number==123,"Quick state round trip");
        encoded.back() ^= std::byte{1}; require(!state::decode(encoded).decoded,"Corrupt quick state rejected");
        encoded=state::encode(saved); encoded.resize(10); require(!state::decode(encoded).decoded,"Truncated quick state rejected");
        for(auto dims:{std::pair{1920,1080},std::pair{1080,1920},std::pair{3840,2160}}) {
            auto layout=render::calculate_scale_layout(dims.first,dims.second,256,240,4,3);
            require(layout.valid() && layout.destination.width<=dims.first && layout.destination.height<=dims.second,"Aspect containment");
            require(layout.stage_width%256==0 && layout.stage_height%240==0,"Integer prescale");
        }
        input::Plunger plunger;
        plunger.configure({.rest=0,.drawn=1000,.deadzone=0,.release=25});
        require(!plunger.update(800).drawn,"No shot until rod rests");
        (void)plunger.update(0); require(plunger.update(800).draw==80,"Pull distance");
        auto shot=plunger.update(100); require(shot.released && shot.draw==80,"Release retains previous draw");
        require(!plunger.update(400).drawn,"Spring bounce cannot relaunch");
        plunger.configure({.rest=1000,.drawn=-1000,.deadzone=0,.release=25});
        (void)plunger.update(1000);
        require(plunger.update(0).draw==50 && plunger.update(-1000).draw==100,"Reversed axis");
        const auto dir=std::filesystem::temp_directory_path()/("alien-crush-tests-"+std::to_string(GetCurrentProcessId()));
        const auto path=dir/"session.acrec";
        recording::Recording original{saved,{{1,2},{128,8},{0,0}}};
        recording::write(path,original);
        const auto replay=recording::read(path);
        require(replay.inputs==original.inputs && replay.initial.core_state==saved.core_state,"Recording round trip");
        auto bytes=core::read_file(path); bytes.back()^=std::byte{1}; core::write_file_atomic(path,bytes);
        bool rejected=false; try {recording::read(path);} catch(const std::exception&){rejected=true;}
        require(rejected,"Recording corruption rejected");
        original.inputs={{1,0},{0,0},{0,0}};
        original.plunger={25,101,75};
        recording::write(path,original);
        const auto analog=recording::read(path);
        require(analog.inputs==original.inputs && analog.plunger==original.plunger,"Analog recording round trip");
        const auto valid=core::read_file(path);
        auto reject_write=[&]() {
            bool failed=false;try {recording::write(path,original);}catch(const std::exception&){failed=true;}
            require(failed && core::read_file(path)==valid,"Invalid recording write must preserve existing file");
        };
        original.plunger.pop_back();reject_write();original.plunger.push_back(75);
        original.plunger[0]=102;reject_write();original.plunger[0]=25;
        original.inputs[1].player_one=1;reject_write();original.inputs[1].player_one=0;
        original.inputs[0].player_two=256;reject_write();original.inputs[0].player_two=0;
        // Recompute the checksum so parser validation, not integrity checking, rejects these.
        const auto state_size=state::encode(saved).size();
        for(const auto offset:{16+state_size+4,16+state_size+5,16+state_size+1}) {
            auto malformed=valid;
            malformed[offset]=std::byte{255};
            const auto digest=rom::compute_digests(std::span(malformed).first(malformed.size()-64)).sha256;
            for(std::size_t i=0;i<64;++i) malformed[malformed.size()-64+i]=static_cast<std::byte>(digest[i]);
            core::write_file_atomic(path,malformed);
            rejected=false;try {recording::read(path);}catch(const std::exception&){rejected=true;}
            require(rejected,"Checksummed invalid analog record rejected");
        }
        std::filesystem::remove(path); std::filesystem::remove(dir);
        std::cout<<"PASS: hashes, ROM rejection, save/recording integrity, scaling, plunger and atomic file replacement\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
