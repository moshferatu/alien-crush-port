// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/rom_image.hpp"
#include "video_capture.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/diagnostics/reference_snapshot.hpp"
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
    const recording::Recording* playback=nullptr, bool restore_initial=false,
    std::uint64_t video_every=0,bool raw=true,bool compare_video=false) {
    auto runtime=make_reference_runtime(); runtime->load(rom_path);
    if(playback) runtime->restore(playback->initial);
    else if(restore_initial) runtime->restore(runtime->save());
    std::vector<std::string> hashes;
    std::optional<state::SaveStateContainer> checkpoint;
    std::optional<std::array<std::uint16_t,256>> previous_sat;
    recording::Recording recording{runtime->save(),{}};
    std::ofstream census;
    std::ofstream ram_stream;
    std::ofstream pixel_stream;
    std::ofstream observations;
    std::ofstream composition;
    std::size_t current_exact=0,previous_exact=0,previous_compared=0;
    if(!output.empty()) {
        std::filesystem::create_directories(output);
        census.exceptions(std::ios::failbit|std::ios::badbit);
        ram_stream.exceptions(std::ios::failbit|std::ios::badbit);
        pixel_stream.exceptions(std::ios::failbit|std::ios::badbit);
        census.open(output/"census.csv");
        if(compare_video) {
            composition.exceptions(std::ios::failbit|std::ios::badbit);
            composition.open(output/"composite-epochs.csv");
            composition<<"frame,compared_pixels,current_sat_different_pixels,previous_sat_different_pixels\n";
        }
        observations.exceptions(std::ios::failbit|std::ios::badbit);
        observations.open(output/"game-observations.csv");
        observations<<"frame,score,table_code,ball_counter,score_pending,high_score_1,high_score_2,high_score_3,high_score_4,high_score_5,pulse_flags,pulse_green,pulse_red,pulse_blue\n";
        if(raw) {
            ram_stream.open(output/"work-ram.bin",std::ios::binary);
            pixel_stream.open(output/"pixels.argb",std::ios::binary);
        }
        census<<"frame,width,height,ram_sha256,pixels_sha256,audio_samples,audio_sha256\n";
    }
    for(std::uint64_t n=0;n<frames;++n) {
        auto input=playback?playback->inputs.at(static_cast<std::size_t>(n)):recording::canonical_input(n);
        recording.inputs.push_back(input);
        const auto frame=runtime->advance(input);
        if(frame.pixels.empty() || frame.stereo.empty()) throw std::runtime_error("Missing reference frame/audio");
        const bool export_frame=!output.empty() && video_every && ((n+1)%video_every==0 || n+1==frames);
        std::optional<render::Image> reference_image;
        // Runtime frame views expire at the next runtime operation, including save.
        if(export_frame || compare_video) reference_image=render::Image{frame.width,frame.height,{frame.pixels.begin(),frame.pixels.end()}};
        hashes.push_back(recording::fingerprint(frame,runtime->work_ram()));
        if(!output.empty()) census<<hashes.back()<<'\n';
        if(!output.empty()) {
            const auto observed=diagnostics::observe_alien_state(runtime->work_ram());
            observations<<frame.number<<','<<observed.score_points<<','<<static_cast<unsigned>(observed.table_code)<<','
                <<static_cast<unsigned>(observed.ball_counter)<<','<<observed.score_update_pending;
            for(auto high:observed.high_scores) observations<<','<<high;
            for(unsigned p=0x8a;p<=0x8d;++p) observations<<','<<std::to_integer<unsigned>(runtime->work_ram()[p]);
            observations<<'\n';
        }
        if(!output.empty() && raw) {
            const auto ram=runtime->work_ram();
            ram_stream.write(reinterpret_cast<const char*>(ram.data()),static_cast<std::streamsize>(ram.size()));
        }
        if(!output.empty() && raw) pixel_stream.write(reinterpret_cast<const char*>(frame.pixels.data()),static_cast<std::streamsize>(frame.pixels.size_bytes()));
        if(n+1==frames/2) checkpoint=runtime->save();
        if(!output.empty() && (n%120==119 || n+1==frames)) {
            bitmap(output/("frame-"+std::to_string(n+1)+".bmp"),frame);
            core::write_file_atomic(output/("ram-"+std::to_string(n+1)+".bin"),runtime->work_ram());
        }
        if(export_frame)
            diagnostics::export_video(output/("video-"+std::to_string(frame.number)),runtime->save(),&*reference_image,
                previous_sat?&*previous_sat:nullptr);
        if(compare_video) {
            const auto snapshot=diagnostics::decode_reference_snapshot(runtime->save());
            const auto compare=[&](const std::array<std::uint16_t,256>& sat) {
                const auto candidate=render::compose(snapshot.vram,snapshot.palette,sat,
                    {reference_image->width,reference_image->height,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,snapshot.control});
                std::size_t different=0;
                for(std::size_t p=0;p<candidate.pixels.size();++p)
                    if(candidate.pixels[p]!=reference_image->pixels[p]) ++different;
                return different;
            };
            const auto current=compare(snapshot.sat);
            current_exact+=current==0;
            composition<<frame.number<<','<<reference_image->pixels.size()<<','<<current<<',';
            if(previous_sat) {
                const auto previous=compare(*previous_sat);
                previous_exact+=previous==0; ++previous_compared;
                composition<<previous;
            }
            composition<<'\n';
            previous_sat=snapshot.sat;
        }
        // Capture only the immediately preceding frame for each requested sample.
        if(!compare_video && !output.empty() && video_every && n+1<frames && ((n+2)%video_every==0 || n+2==frames))
            previous_sat=diagnostics::decode_reference_snapshot(runtime->save()).sat;
    }
    if(!output.empty()) {
        census.close(); observations.close();
        if(compare_video) {
            composition.close();
            std::cout<<"Diagnostic composition: current SAT exact "<<current_exact<<'/'<<frames
                <<", previous SAT exact "<<previous_exact<<'/'<<previous_compared<<" frames.\n";
        }
        if(raw) {ram_stream.close(); pixel_stream.close();}
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
        std::filesystem::path rom_path="Alien Crush.pce",output,replay_path,asset_path,actor_trace,background_trace,palette_trace,pulse_trace;
        std::uint64_t frames=1800,video_every=0;
        bool validate=false,verify=false,cold=false,raw=true,discovery=false,compare_video=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            auto value=[&](){ if(++i>=argc) throw std::runtime_error("Missing value for "+arg); return std::string(argv[i]); };
            if(arg=="--rom") rom_path=core::path_from_utf8(value());
            else if(arg=="--frames") frames=std::stoull(value());
            else if(arg=="--capture") output=core::path_from_utf8(value());
            else if(arg=="--replay") replay_path=core::path_from_utf8(value());
            else if(arg=="--video-every") { video_every=std::stoull(value()); if(!video_every) throw std::runtime_error("Video interval must be positive"); }
            else if(arg=="--compact") raw=false;
            else if(arg=="--compare-video") compare_video=true;
            else if(arg=="--extract-assets") asset_path=core::path_from_utf8(value());
            else if(arg=="--verify-discovery") discovery=true;
            else if(arg=="--verify-actor-trace") actor_trace=core::path_from_utf8(value());
            else if(arg=="--verify-background-trace") background_trace=core::path_from_utf8(value());
            else if(arg=="--verify-pattern-trace") background_trace=core::path_from_utf8(value());
            else if(arg=="--verify-palette-trace") palette_trace=core::path_from_utf8(value());
            else if(arg=="--verify-pulse-trace") pulse_trace=core::path_from_utf8(value());
            else if(arg=="--validate-rom") validate=true;
            else if(arg=="--verify-replay") verify=true;
            else if(arg=="--verify-cold") cold=true;
            else if(arg=="--help") {
                std::cout<<"alien-crush-tools --rom <path> [--validate-rom | --verify-replay | --verify-cold | --verify-discovery]\n"
                    <<"  [--frames N] [--replay <file>] [--capture <directory>] [--video-every N] [--compact]\n"
                    <<"  --extract-assets <directory>: decode both table backgrounds and sprite patterns without running the game.\n"
                    <<"Replay consumes the entire recording; --frames applies to the canonical route.\n"
                    <<"--video-every exports native/reference images, exact pixel differences, and diagnostic video state every N frames.\n"
                    <<"--compare-video requires --capture; writes all-frame current/previous SAT pixel differences without extra images.\n"
                    <<"--compact omits large per-frame raw RAM/video streams.\n";
                std::cout<<"--verify-discovery --capture <directory> writes actor comparison evidence and sprite-layer previews.\n";
                std::cout<<"--verify-actor-trace <file> compares main actor (ACAT0001) or table overlay (ACOV0001) instruction-boundary traces.\n";
                std::cout<<"--verify-background-trace <file> checks actor BAT writes against complete before/after VRAM captures.\n";
                std::cout<<"--verify-pattern-trace <file> checks dynamic sprite-pattern transfers against complete before/after VRAM captures.\n";
                std::cout<<"--verify-palette-trace <file> checks native fade-in/out steps against captured palette buffers.\n";
                std::cout<<"--verify-pulse-trace <file> checks palette-zero pulse state and complete palette output.\n";
                return 0;
            }
            else throw std::runtime_error("Unknown option "+arg);
        }
        if(frames<2 || frames>10'000'000) throw std::runtime_error("Frames must be between 2 and 10000000");
        if(video_every && output.empty()) throw std::runtime_error("--video-every requires --capture");
        if(compare_video && output.empty()) throw std::runtime_error("--compare-video requires --capture");
        const auto validation=rom::validate_file(rom_path);
        if(!validation.supported()) throw std::runtime_error(validation.message);
        std::cout<<"ROM SHA-256: "<<validation.digests.sha256<<'\n';
        if(!asset_path.empty()) {diagnostics::export_table_assets(asset_path,rom_path); std::cout<<"Extracted both table backgrounds, sprite patterns and HUD artwork without running the reference core.\n"; return 0;}
        if(discovery) {diagnostics::verify_discovery(rom_path,output); return 0;}
        if(!actor_trace.empty()) {diagnostics::verify_actor_trace(rom_path,actor_trace); return 0;}
        if(!background_trace.empty()) {diagnostics::verify_background_trace(rom_path,background_trace); return 0;}
        if(!palette_trace.empty()) {diagnostics::verify_palette_fade_trace(palette_trace); return 0;}
        if(!pulse_trace.empty()) {diagnostics::verify_palette_pulse_trace(pulse_trace); return 0;}
        if(validate) return 0;
        std::optional<recording::Recording> playback;
        if(!replay_path.empty()) {
            playback=recording::read(replay_path); frames=playback->inputs.size();
            if(frames<2) throw std::runtime_error("Replay requires at least two frames");
        }
        const auto* source=playback?&*playback:nullptr;
        const auto first=capture(rom_path,frames,output,verify,source,false,video_every,raw,compare_video);
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
