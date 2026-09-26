// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/input/nudge_multiplier.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/rom_image.hpp"
#include "video_capture.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <algorithm>
#include <stdexcept>

using namespace alien_crush;
namespace {
std::string memory_signature(std::span<const render::RasterWrite> writes) {
    std::vector<std::byte> bytes;
    bytes.reserve(writes.size()*6);
    for(const auto& write:writes) for(const auto value:{write.row,write.address,write.value}) {
        bytes.push_back(static_cast<std::byte>(value&255)); bytes.push_back(static_cast<std::byte>(value>>8));
    }
    return rom::compute_digests(bytes).sha256;
}
std::string display_signature(std::span<const diagnostics::DisplayRow> rows) {
    std::string result;
    for(const auto& row:rows) result+=std::to_string(row.row)+":"+std::to_string(row.control)+":"+(row.burst_mode?"1;":"0;");
    return result;
}
std::string palette_signature(std::span<const diagnostics::PaletteWrite> writes) {
    std::string result;
    for(const auto& write:writes) result+=std::to_string(write.row)+":"+std::to_string(write.x)+":"+std::to_string(write.address)+":"+
        std::to_string(write.before)+":"+std::to_string(write.after)+":"+(write.frame_done?"1;":"0;");
    return result;
}
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
    std::uint64_t video_every=0,bool raw=true,bool compare_video=false,bool compare_display=false,bool require_display=false,bool stitched=false) {
    auto runtime=make_reference_runtime(playback && (playback->initial.core_revision.find("-handoff1")!=std::string::npos || playback->initial.core_revision.find("-h1-")!=std::string::npos)?HandoffMode::continuous:HandoffMode::original,
        playback && playback->initial.core_revision.find("-controls1")!=std::string::npos?ControlMode::cabinet:ControlMode::original,
        playback?input::recorded_nudge_multiplier(playback->initial.core_revision):1,
        playback?input::recorded_flipper_masks(playback->initial.core_revision):input::FlipperMasks{},
        playback?recorded_actor_mode(playback->initial.core_revision):ActorMode::original); runtime->load(rom_path);
    const bool table_raster=stitched && runtime->supports_display_observation();
    if(compare_display || table_raster) runtime->observe_display(true);
    if(playback) runtime->restore(playback->initial);
    else if(restore_initial) runtime->restore(runtime->save());
    std::vector<std::string> hashes;
    std::vector<std::string> display_signatures;
    std::vector<std::string> palette_signatures;
    std::vector<std::string> memory_signatures;
    std::optional<state::SaveStateContainer> checkpoint;
    std::optional<diagnostics::ReferenceSnapshot> previous_video;
    std::unique_ptr<render::TablePlayfield> table;
    render::TableTransitionHold transition;
    std::ofstream table_csv;
    if(stitched) table=std::make_unique<render::TablePlayfield>(core::read_file(rom_path));
    recording::Recording recording{runtime->save(),{}};
    std::ofstream census;
    std::ofstream ram_stream;
    std::ofstream pixel_stream;
    std::ofstream observations;
    std::ofstream composition;
    std::ofstream display_trace,display_comparison,palette_trace;
    std::size_t display_exact=0,display_compared=0;
    std::size_t palette_exact=0;
    std::size_t memory_exact=0;
    std::size_t current_exact=0,previous_exact=0,previous_compared=0;
    if(!output.empty()) {
        std::filesystem::create_directories(output);
        census.exceptions(std::ios::failbit|std::ios::badbit);
        ram_stream.exceptions(std::ios::failbit|std::ios::badbit);
        pixel_stream.exceptions(std::ios::failbit|std::ios::badbit);
        census.open(output/"census.csv");
        if(compare_display) {
            display_trace.exceptions(std::ios::failbit|std::ios::badbit);
            display_comparison.exceptions(std::ios::failbit|std::ios::badbit);
            display_trace.open(output/"display-rows.csv"); display_comparison.open(output/"display-comparison.csv");
            palette_trace.exceptions(std::ios::failbit|std::ios::badbit); palette_trace.open(output/"palette-writes.csv");
            palette_trace<<"frame,row,x,address,before,after,frame_done\n";
            display_trace<<"frame,row,control,burst_mode\n";
            display_comparison<<"frame,observed_rows,complete,different_pixels,palette_different_pixels,vram_different_pixels\n";
        }
        if(compare_video) {
            composition.exceptions(std::ios::failbit|std::ios::badbit);
            composition.open(output/"composite-epochs.csv");
            composition<<"frame,compared_pixels,current_sat_different_pixels,previous_sat_different_pixels,previous_vram_different_pixels,previous_palette_different_pixels,previous_registers_different_pixels,previous_video_different_pixels\n";
        }
        observations.exceptions(std::ios::failbit|std::ios::badbit);
        observations.open(output/"game-observations.csv");
        if(stitched) {
            table_csv.exceptions(std::ios::failbit|std::ios::badbit);
            table_csv.open(output/"stitched.csv");
            table_csv<<"frame,table,available,pixels_sha256,held,ball_updated\n";
        }
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
        const auto command=playback?playback->plunger_at(static_cast<std::size_t>(n)):std::uint8_t{0};
        if(playback && !playback->plunger.empty()) recording.plunger.push_back(command);
        const auto frame=recording::advance(*runtime,input,static_cast<std::uint8_t>(command));
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
                previous_video?&*previous_video:nullptr);
        if(compare_video) {
            const auto snapshot=diagnostics::decode_reference_snapshot(runtime->save());
            if(compare_display) {
                const auto observed=runtime->display_rows();
                for(const auto& write:runtime->palette_writes())
                    palette_trace<<frame.number<<','<<write.row<<','<<write.x<<','<<write.address<<','<<write.before<<','<<write.after<<','<<write.frame_done<<'\n';
                if(check_restore) display_signatures.push_back(display_signature(observed));
                if(check_restore) palette_signatures.push_back(palette_signature(runtime->palette_writes()));
                if(check_restore) memory_signatures.push_back(memory_signature(runtime->vram_rows()));
                bool complete=observed.size()==static_cast<std::size_t>(reference_image->height);
                for(std::size_t i=0;i<observed.size();++i) {
                    const auto& row=observed[i];
                    display_trace<<frame.number<<','<<row.row<<','<<row.control<<','<<row.burst_mode<<'\n';
                    complete=complete && row.row==i;
                }
                display_comparison<<frame.number<<','<<observed.size()<<','<<complete<<',';
                if(complete && previous_video) {
                    const auto writes=diagnostics::display_control_schedule(observed,reference_image->height);
                    const auto candidate=render::compose_raster(snapshot.vram,snapshot.palette,previous_video->sat,
                        {reference_image->width,reference_image->height,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,snapshot.control},writes);
                    std::size_t different=0;
                    for(std::size_t p=0;p<candidate.pixels.size();++p) different+=candidate.pixels[p]!=reference_image->pixels[p];
                    display_exact+=different==0; ++display_compared;
                    display_comparison<<different;
                    const auto palette_writes=diagnostics::palette_pixel_schedule(previous_video->palette,snapshot.palette,
                        runtime->palette_writes(),reference_image->width,reference_image->height);
                    const auto timed=render::compose_palette_raster(snapshot.vram,previous_video->palette,previous_video->sat,
                        {reference_image->width,reference_image->height,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,snapshot.control},writes,palette_writes);
                    std::size_t palette_different=0;
                    for(std::size_t p=0;p<timed.pixels.size();++p) palette_different+=timed.pixels[p]!=reference_image->pixels[p];
                    palette_exact+=palette_different==0;
                    display_comparison<<','<<palette_different;
                    auto memory_writes=writes;
                    const auto memory=runtime->vram_rows();
                    memory_writes.insert(memory_writes.end(),memory.begin(),memory.end());
                    std::stable_sort(memory_writes.begin(),memory_writes.end(),[](const auto& a,const auto& b){return a.row<b.row;});
                    const auto memory_timed=render::compose_palette_raster(snapshot.vram,previous_video->palette,previous_video->sat,
                        {reference_image->width,reference_image->height,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,snapshot.control},memory_writes,palette_writes);
                    std::size_t memory_different=0;
                    for(std::size_t p=0;p<memory_timed.pixels.size();++p) memory_different+=memory_timed.pixels[p]!=reference_image->pixels[p];
                    memory_exact+=memory_different==0;
                    display_comparison<<','<<memory_different;
                    if(export_frame) {
                        const auto memory_path=output/("video-"+std::to_string(frame.number))/"vram-timed";
                        std::filesystem::create_directories(memory_path);
                        diagnostics::export_palette_comparison(memory_path,frame.number,
                            previous_video->palette,palette_writes,memory_timed,*reference_image);
                        std::ofstream memory_csv(memory_path/"vram-rows.csv");
                        memory_csv.exceptions(std::ios::failbit|std::ios::badbit);
                        memory_csv<<"row,target,address,value\n";
                        for(const auto& write:memory) memory_csv<<write.row<<",vram,"<<write.address<<','<<write.value<<'\n';
                        memory_csv.close();
                        diagnostics::export_palette_comparison(output/("video-"+std::to_string(frame.number)),frame.number,
                            previous_video->palette,palette_writes,timed,*reference_image);
                        std::ofstream schedule; schedule.exceptions(std::ios::failbit|std::ios::badbit);
                        schedule.open(output/("video-"+std::to_string(frame.number))/"display-writes.csv");
                        schedule<<"row,target,address,value\n";
                        for(std::size_t i=0;i<256;++i) schedule<<"0,sat,"<<i<<','<<previous_video->sat[i]<<'\n';
                        for(const auto& write:writes) schedule<<write.row<<','<<(write.target==render::RasterTarget::burst_mode?"burst_mode":"control")<<",0,"<<write.value<<'\n';
                        schedule.close();
                    }
                } else display_comparison<<",,";
                display_comparison<<'\n';
            }
            const auto candidates=diagnostics::compose_video_epochs(snapshot,previous_video?&*previous_video:nullptr,
                reference_image->width,reference_image->height);
            composition<<frame.number<<','<<reference_image->pixels.size();
            for(std::size_t i=0;i<candidates.size();++i) {
                const auto& candidate=candidates[i].image;
                std::size_t different=0;
                for(std::size_t p=0;p<candidate.pixels.size();++p)
                    if(candidate.pixels[p]!=reference_image->pixels[p]) ++different;
                if(i==0) current_exact+=different==0;
                if(i==1) {previous_exact+=different==0; ++previous_compared;}
                composition<<','<<different;
            }
            if(!previous_video) composition<<",,,,,";
            composition<<'\n';
            if(stitched) {
                const auto ram=runtime->work_ram();
                const auto half=std::to_integer<unsigned>(ram[0x79]);
                const auto camera=std::to_integer<unsigned>(ram[0x16])|(std::to_integer<unsigned>(ram[0x17])<<8);
                const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                std::optional<render::TableBallBackdrop> backdrop;
                auto image=previous_video?table->compose(snapshot,*previous_video,half,camera,ram,
                    table_raster?&raster:nullptr,&backdrop):std::nullopt;
                const auto ball=table->transition_ball(half,ram);
                image=transition.present(std::move(image),snapshot.frame,half,camera,ram,
                    backdrop?&*backdrop:nullptr,ball?&*ball:nullptr);
                table_csv<<frame.number<<','<<half<<','<<image.has_value()<<',';
                if(image) {
                    table_csv<<rom::compute_digests(std::as_bytes(std::span(image->pixels))).sha256;
                    if(export_frame) bitmap(output/("stitched-"+std::to_string(frame.number)+".bmp"),
                        {frame.number,image->width,image->height,image->pixels,{}});
                }
                table_csv<<','<<transition.held()<<','<<transition.ball_updated()<<'\n';
            }
            previous_video=snapshot;
        }
        // Capture only the immediately preceding frame for each requested sample.
        if(!compare_video && !output.empty() && video_every && n+1<frames && ((n+2)%video_every==0 || n+2==frames))
            previous_video=diagnostics::decode_reference_snapshot(runtime->save());
    }
    if(!output.empty()) {
        census.close(); observations.close();
        if(compare_display) {
            display_trace.close(); display_comparison.close(); palette_trace.close();
            std::ofstream manifest; manifest.exceptions(std::ios::failbit|std::ios::badbit);
            manifest.open(output/"display-manifest.txt");
            manifest<<"schema=reference-display-rows-v3\nrom_sha256="<<rom::kSupportedSha256<<"\nbackend="<<runtime->revision()
                <<"\nobserver=existing VDC HPHASE_HDW boundary; RCRCount, CR_cache, burst_mode\n"
                <<"schedule=explicit CR_cache and burst mode; burst suppresses both layers and selects palette 256\n"
                <<"candidate=current VRAM/palette/scroll/MWR; previous SAT; observed display control\n"
                <<"coverage=one consecutive observation per viewport row; otherwise unavailable\n"
                <<"palette_observer=VCE after Sync before byte write; coordinates normalized by final display rectangle\n"
                <<"palette_schedule=before/after chain validated against full initial/final palettes; pixel writes use the next output cursor\n"
                <<"vram_observer=full VRAM at first HDW row followed by changes at subsequent HDW boundaries, after pending commits\n"
                <<"limits=reference-assisted control, palette and VRAM schedule, not independent device timing\n"
                <<"vram_exact_frames="<<memory_exact<<'\n'
                <<"compared_frames="<<display_compared<<"\nexact_frames="<<display_exact<<"\npalette_exact_frames="<<palette_exact<<'\n';
            manifest.close();
            std::cout<<"Observed display control: "<<display_exact<<'/'<<display_compared<<" complete comparable frames exact.\n";
            std::cout<<"Observed display and palette: "<<palette_exact<<'/'<<display_compared<<" complete comparable frames exact.\n";
            std::cout<<"Observed display, palette and VRAM: "<<memory_exact<<'/'<<display_compared<<" complete comparable frames exact.\n";
            if(require_display && (memory_exact!=display_compared || display_compared!=frames-2 || frames<3))
                throw std::runtime_error("Display parity gate failed: requires every frame after the two startup frames to match");
        }
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
            auto frame=recording::advance(*runtime,recording.inputs.at(static_cast<std::size_t>(n)),recording.plunger_at(static_cast<std::size_t>(n)));
            if(compare_display && display_signatures.at(static_cast<std::size_t>(n))!=display_signature(runtime->display_rows()))
                throw std::runtime_error("Restored display observation diverged at frame "+std::to_string(n+1));
            if(compare_display && palette_signatures.at(static_cast<std::size_t>(n))!=palette_signature(runtime->palette_writes()))
                throw std::runtime_error("Restored palette observation diverged at frame "+std::to_string(n+1));
            if(compare_display && memory_signatures.at(static_cast<std::size_t>(n))!=memory_signature(runtime->vram_rows()))
                throw std::runtime_error("Restored VRAM observation diverged at frame "+std::to_string(n+1));
            auto actual=recording::fingerprint(frame,runtime->work_ram());
            if(actual!=hashes[static_cast<std::size_t>(n)]) throw std::runtime_error("Quick-state replay diverged at frame "+std::to_string(n+1)+"\nexpected: "+hashes[static_cast<std::size_t>(n)]+"\nactual: "+actual);
        }
        if(compare_display) std::cout<<"PASS: restored display-row, palette and VRAM observations match for "<<frames-frames/2<<" frames.\n";
    }
    return hashes;
}
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path rom_path="Alien Crush.pce",output,replay_path,asset_path,actor_trace,background_trace,palette_trace,pulse_trace;
        std::filesystem::path raster_video,raster_writes;
        std::uint64_t frames=1800,video_every=0;
        bool validate=false,verify=false,cold=false,raw=true,discovery=false,compare_video=false,compare_display=false;
        bool require_display=false,stitched=false;
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
            else if(arg=="--compare-display") {compare_display=true; compare_video=true;}
            else if(arg=="--require-display-match") {require_display=true; compare_display=true; compare_video=true;}
            else if(arg=="--stitched") {stitched=true;compare_video=true;}
            else if(arg=="--render-raster") raster_video=core::path_from_utf8(value());
            else if(arg=="--raster-writes") raster_writes=core::path_from_utf8(value());
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
                    <<"--compare-video requires --capture; compares fixed SAT/VRAM/palette/register epochs without extra images.\n"
                    <<"--compact omits large per-frame raw RAM/video streams.\n";
                std::cout<<"--verify-discovery --capture <directory> writes actor comparison evidence and sprite-layer previews.\n";
                std::cout<<"--verify-actor-trace <file> compares main actor (ACAT0001) or table overlay (ACOV0001) instruction-boundary traces.\n";
                std::cout<<"--verify-background-trace <file> checks actor BAT writes against complete before/after VRAM captures.\n";
                std::cout<<"--verify-pattern-trace <file> checks dynamic sprite-pattern transfers against complete before/after VRAM captures.\n";
                std::cout<<"--verify-palette-trace <file> checks native fade-in/out steps against captured palette buffers.\n";
                std::cout<<"--verify-pulse-trace <file> checks palette-zero pulse state and complete palette output.\n";
                std::cout<<"--render-raster <video directory> --raster-writes <CSV> --capture <new directory> compares a supplied row schedule offline.\n";
                std::cout<<"--compare-display adds observed reference display-row control to --compare-video (requires observer build).\n";
                std::cout<<"--require-display-match also fails unless every frame after startup frames 1/2 is complete and exact (canonical route only).\n";
                std::cout<<"--stitched adds experimental full-table hashes and BMPs at --video-every intervals; requires --capture.\n";
                return 0;
            }
            else throw std::runtime_error("Unknown option "+arg);
        }
        if(frames<2 || frames>10'000'000) throw std::runtime_error("Frames must be between 2 and 10000000");
        if(video_every && output.empty()) throw std::runtime_error("--video-every requires --capture");
        if(compare_video && output.empty()) throw std::runtime_error("--compare-video requires --capture");
        if(require_display && !replay_path.empty()) throw std::runtime_error("--require-display-match requires the canonical route");
        if((!raster_video.empty() || !raster_writes.empty()) && (raster_video.empty() || raster_writes.empty() || output.empty()))
            throw std::runtime_error("--render-raster requires --raster-writes and --capture");
        const auto validation=rom::validate_file(rom_path);
        if(!validation.supported()) throw std::runtime_error(validation.message);
        std::cout<<"ROM SHA-256: "<<validation.digests.sha256<<'\n';
        if(!raster_video.empty()) {diagnostics::render_raster_capture(raster_video,raster_writes,output); return 0;}
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
        const auto first=capture(rom_path,frames,output,verify,source,false,video_every,raw,compare_video,compare_display,require_display,stitched);
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
