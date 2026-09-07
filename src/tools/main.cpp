// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>

using namespace alien_crush;
namespace {
void bitmap(const std::filesystem::path& path,const Frame& frame) {
    const auto size=static_cast<std::uint32_t>(frame.pixels.size()*4);
    std::vector<std::byte> bytes(54+size);
    auto u32=[&](std::size_t i,std::uint32_t v){ for(unsigned s=0;s<32;s+=8) bytes[i++]=static_cast<std::byte>((v>>s)&255); };
    bytes[0]=std::byte{'B'}; bytes[1]=std::byte{'M'}; u32(2,54+size); u32(10,54); u32(14,40);
    u32(18,static_cast<std::uint32_t>(frame.width)); u32(22,static_cast<std::uint32_t>(-frame.height));
    bytes[26]=std::byte{1}; bytes[28]=std::byte{32}; u32(34,size);
    const auto raw=std::as_bytes(frame.pixels);
    std::copy(raw.begin(),raw.end(),bytes.begin()+54);
    core::write_file_atomic(path,bytes);
}
std::vector<std::string> capture(const std::filesystem::path& rom_path,std::uint64_t frames,
    const std::filesystem::path& output, bool check_restore,
    const recording::Recording* playback=nullptr, bool restore_initial=false) {
    auto runtime=make_reference_runtime(); runtime->load(rom_path);
    if(playback) runtime->restore(playback->initial);
    else if(restore_initial) runtime->restore(runtime->save());
    std::vector<std::string> hashes;
    std::optional<state::SaveStateContainer> checkpoint;
    recording::Recording recording{runtime->save(),{}};
    std::ofstream census;
    std::ofstream ram_stream;
    std::ofstream pixel_stream;
    if(!output.empty()) {
        std::filesystem::create_directories(output);
        census.exceptions(std::ios::failbit|std::ios::badbit);
        ram_stream.exceptions(std::ios::failbit|std::ios::badbit);
        pixel_stream.exceptions(std::ios::failbit|std::ios::badbit);
        census.open(output/"census.csv");
        ram_stream.open(output/"work-ram.bin",std::ios::binary);
        pixel_stream.open(output/"pixels.argb",std::ios::binary);
        census<<"frame,width,height,ram_sha256,pixels_sha256,audio_samples,audio_sha256\n";
    }
    for(std::uint64_t n=0;n<frames;++n) {
        auto input=playback?playback->inputs.at(static_cast<std::size_t>(n)):recording::canonical_input(n);
        recording.inputs.push_back(input);
        const auto frame=runtime->advance(input);
        if(frame.pixels.empty() || frame.stereo.empty()) throw std::runtime_error("Missing reference frame/audio");
        hashes.push_back(recording::fingerprint(frame,runtime->work_ram()));
        if(!output.empty()) census<<hashes.back()<<'\n';
        if(!output.empty()) {
            const auto ram=runtime->work_ram();
            ram_stream.write(reinterpret_cast<const char*>(ram.data()),static_cast<std::streamsize>(ram.size()));
        }
        if(!output.empty()) pixel_stream.write(reinterpret_cast<const char*>(frame.pixels.data()),static_cast<std::streamsize>(frame.pixels.size_bytes()));
        if(n+1==frames/2) checkpoint=runtime->save();
        if(!output.empty() && (n%120==119 || n+1==frames)) {
            bitmap(output/("frame-"+std::to_string(n+1)+".bmp"),frame);
            core::write_file_atomic(output/("ram-"+std::to_string(n+1)+".bin"),runtime->work_ram());
        }
    }
    if(!output.empty()) {
        census.close(); ram_stream.close(); pixel_stream.close();
        recording::write(output/"session.acrec",recording);
    }
    if(check_restore && checkpoint) {
        if(!output.empty()) core::write_file_atomic(output/"checkpoint.payload",checkpoint->core_state);
        runtime->restore(*checkpoint);
        if(!output.empty()) core::write_file_atomic(output/"restored.payload",runtime->save().core_state);
        for(std::uint64_t n=frames/2;n<frames;++n) {
            auto frame=runtime->advance(recording.inputs.at(static_cast<std::size_t>(n)));
            auto actual=recording::fingerprint(frame,runtime->work_ram());
            if(actual!=hashes[static_cast<std::size_t>(n)]) throw std::runtime_error("Quick-state replay diverged at frame "+std::to_string(n+1)+"\nexpected: "+hashes[static_cast<std::size_t>(n)]+"\nactual: "+actual);
        }
    }
    return hashes;
}
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path rom_path="Alien Crush.pce",output,replay_path;
        std::uint64_t frames=1800;
        bool validate=false,verify=false,cold=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            auto value=[&](){ if(++i>=argc) throw std::runtime_error("Missing value for "+arg); return std::string(argv[i]); };
            if(arg=="--rom") rom_path=core::path_from_utf8(value());
            else if(arg=="--frames") frames=std::stoull(value());
            else if(arg=="--capture") output=core::path_from_utf8(value());
            else if(arg=="--replay") replay_path=core::path_from_utf8(value());
            else if(arg=="--validate-rom") validate=true;
            else if(arg=="--verify-replay") verify=true;
            else if(arg=="--verify-cold") cold=true;
            else if(arg=="--help") { std::cout<<"alien-crush-tools --rom <path> [--validate-rom | --verify-replay | --verify-cold] [--frames N] [--replay <file>] [--capture <directory>]\nReplay consumes the entire recording; --frames applies to the canonical route.\n"; return 0; }
            else throw std::runtime_error("Unknown option "+arg);
        }
        if(frames<2 || frames>10'000'000) throw std::runtime_error("Frames must be between 2 and 10000000");
        const auto validation=rom::validate_file(rom_path);
        if(!validation.supported()) throw std::runtime_error(validation.message);
        std::cout<<"ROM SHA-256: "<<validation.digests.sha256<<'\n';
        if(validate) return 0;
        std::optional<recording::Recording> playback;
        if(!replay_path.empty()) {
            playback=recording::read(replay_path); frames=playback->inputs.size();
            if(frames<2) throw std::runtime_error("Replay requires at least two frames");
        }
        const auto* source=playback?&*playback:nullptr;
        const auto first=capture(rom_path,frames,output,verify,source);
        if(verify || cold) {
            const auto second=capture(rom_path,frames,{},false,source);
            for(std::size_t i=0;i<first.size();++i) if(first[i]!=second[i]) throw std::runtime_error("Cold-boot replay diverged at frame "+std::to_string(i+1));
            if(verify && !playback) {
                const auto initial=capture(rom_path,frames,{},false,nullptr,true);
                for(std::size_t i=0;i<first.size();++i) if(first[i]!=initial[i]) throw std::runtime_error("Initial-state replay diverged at frame "+std::to_string(i+1));
            }
            std::cout<<"PASS: "<<frames<<" repeated frames";
            if(verify) std::cout<<", "<<frames-frames/2<<" checkpoint-restored frames"<<(!playback?", and initial-state replay":"");
            std::cout<<" match RAM, pixels, and audio.\n";
        } else std::cout<<"Captured "<<frames<<" frames.\n";
        std::cout<<"Final: "<<first.back()<<'\n';
        return 0;
    } catch(const std::exception& e) { std::cerr<<"Error: "<<e.what()<<'\n'; return 1; }
}
