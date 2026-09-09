// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/render/score_panel.hpp"
#include <fstream>
#include "alien_crush/runtime/game_runtime.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <memory>
using namespace alien_crush;
namespace {
void verify_transition_clock(ReferenceRuntime& runtime,const render::TablePlayfield& table,
    const state::SaveStateContainer& initial,const state::SaveStateContainer& final) {
    runtime.restore(initial);
    auto previous=diagnostics::decode_reference_snapshot(initial);
    render::TableTransitionHold hold;
    render::TableTransitionClock clock;
    bool observe=true;
    unsigned omitted=0,burst=0,bursts=0;
    double burst_ms=0,max_ms=0;
    std::optional<render::Image> last_crossing_image;
    for(unsigned n=599;n<1800;++n) {
        const auto started=std::chrono::steady_clock::now();
        runtime.observe_display(observe);
        runtime.advance(recording::canonical_input(n));
        const auto current=runtime.video_snapshot();
        const auto ram=runtime.work_ram();const auto half=std::to_integer<unsigned>(ram[0x79]);
        const render::TableRaster raster{runtime.display_rows(),runtime.palette_writes(),runtime.vram_rows()};
        std::optional<render::TableBallBackdrop> backdrop;
        auto image=table.compose(current,previous,half,0,ram,&raster,&backdrop);
        const auto ball=table.transition_ball(half,ram);
        image=hold.present(std::move(image),current.frame,half,0,ram,backdrop?&*backdrop:nullptr,ball?&*ball:nullptr);
        if(n==622) last_crossing_image=image;
        if(n==623 && (!image || !last_crossing_image || image->pixels==last_crossing_image->pixels || !hold.ball_updated()))
            throw std::runtime_error("Crossing reverted to a repeated delayed ball image");
        const bool omit=clock.omit(current,ram,image.has_value());
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        if(omit) {++omitted;++burst;burst_ms+=ms;}
        else if(burst) {
            ++bursts;max_ms=std::max(max_ms,burst_ms+ms);
            std::cout<<"Seam batch ends "<<n+1<<": "<<burst<<" omitted, "<<burst_ms+ms<<" ms including next visible frame\n";
            burst=0;burst_ms=0;
        }
        observe=!omit && !clock.upload_follows();previous=current;
    }
    if(runtime.save().core_state!=final.core_state) throw std::runtime_error("Fast transition changed final emulated state");
    if(omitted!=132 || bursts!=20) throw std::runtime_error("Expected every stationary handoff wait to be omitted");
    std::cout<<"Continuous clock: "<<omitted<<" omitted in "<<bursts<<" batches; max work "<<max_ms<<" ms; exact final state\n";
    runtime.observe_display(true);
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2 && argc!=3) throw std::runtime_error("Expected ROM path and optional smoke recording path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto table_storage=std::make_unique<render::TablePlayfield>(core::read_file(path));
        const auto& table=*table_storage;
        const auto indicators=assets::decode_table_indicators(core::read_file(path));
        const auto counter=assets::decode_lower_counter_tiles(core::read_file(path));
        const auto score_panel=render::ScorePanel::from_rom(core::read_file(path));
        const assets::ActorAssets actors(core::read_file(path));
        auto runtime=make_reference_runtime(); runtime->load(path);
        const bool timed=runtime->supports_display_observation();
        if(timed) runtime->observe_display(true);
        std::optional<diagnostics::ReferenceSnapshot> previous;
        std::optional<state::SaveStateContainer> checkpoint;
        std::optional<diagnostics::ReferenceSnapshot> checkpoint_video;
        std::optional<render::Image> expected;
        std::array<std::byte,8192> departing_ram{};
        std::optional<render::Image> inactive_lower;
        std::array<unsigned,2> available{};
        unsigned corrected=0;
        unsigned counter_frames=0;
        render::TableTransitionHold transition;
        render::TableTransitionHold ball_transition;
        std::array<unsigned,2> ball_updates{};
        unsigned held_frames=0;
        std::array<unsigned,2> uploads{};
        std::array<double,2> upload_ms{};
        std::optional<render::Image> lower_background;
        unsigned first_upload=0;
        unsigned upload_x=0,upload_y=0;
        std::optional<render::Image> departing_image;
        std::optional<state::SaveStateContainer> final_state;
        for(unsigned n=0;n<1800;++n) {
            const auto started=std::chrono::steady_clock::now();
            const auto frame=runtime->advance(recording::canonical_input(n));
            const std::vector<std::uint32_t> reference(frame.pixels.begin(),frame.pixels.end());
            const auto ram=runtime->work_ram();
            const auto half=std::to_integer<unsigned>(ram[0x79]);
            const auto camera=std::to_integer<unsigned>(ram[0x16])|(std::to_integer<unsigned>(ram[0x17])<<8);
            const auto saved=runtime->save();
            if(n==1799) final_state=saved;
            const auto video=diagnostics::decode_reference_snapshot(saved);
            if(runtime->video_snapshot()!=video) throw std::runtime_error("Direct snapshot differs from serialized reference state");
            const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            std::optional<render::TableBallBackdrop> backdrop;
            const auto output=previous?table.compose(video,*previous,half,camera,runtime->work_ram(),timed?&raster:nullptr,&backdrop):std::nullopt;
            if(argc==3 && (n==599 || n==719)) {
                const auto dump=[&](const render::Image& im,const std::string& suffix) {
                    auto artifact=core::path_from_utf8(argv[2]);artifact+=std::to_string(n+1)+suffix+".ppm";
                    std::ofstream f(artifact,std::ios::binary);
                    f<<"P6\n"<<im.width<<' '<<im.height<<"\n255\n";
                    for(auto c:im.pixels) for(auto s:{16,8,0}) f.put(static_cast<char>(c>>s));
                    if(!f) throw std::runtime_error("Cannot write seam preview");
                };
                if(output) dump(*output,"-stitched");
                dump({256,240,reference},"-original");
                dump(table.inactive_view(1-half,runtime->work_ram()),"-inactive");
                dump(render::background(video.vram,video.palette,32,32),"-bg");
            }
            if(output && half==0) {
                auto restored=video.vram;
                if(!assets::restore_lower_counter(std::span(restored).first(1024),counter,half,runtime->work_ram()) || restored!=video.vram)
                    throw std::runtime_error("Lower counter differs from original BAT at frame "+std::to_string(n+1));
                ++counter_frames;
            }
            const auto ball=output?std::nullopt:table.transition_ball(half,runtime->work_ram());
            (void)ball_transition.present(output,video.frame,half,camera,runtime->work_ram(),
                backdrop?&*backdrop:nullptr,ball?&*ball:nullptr);
            if(ball_transition.ball_updated()) ++ball_updates.at(half);
            if(n==607 && (!ball || ball->x!=234 || ball->y!=213))
                throw std::runtime_error("Ball translation changed at upper handoff");
            if(n==844 && (!ball || ball->x!=112 || ball->y!=210))
                throw std::runtime_error("Ball translation changed at lower handoff");
            const auto presented=transition.present(output,video.frame,half,camera,runtime->work_ram());
            const bool upload=render::table_transition_upload(video,runtime->work_ram());
            if(upload) {
                if(!transition.held() || output || !presented) throw std::runtime_error("Upload compression requires a complete held table");
                const auto b=[&](unsigned p){return std::to_integer<unsigned>(runtime->work_ram()[p]);};
                const auto x=b(0x15c0)+256*b(0x1600),y=b(0x1680)+256*b(0x16c0);
                if(!first_upload) {first_upload=n+1;upload_x=x;upload_y=y;}
                if(x!=upload_x || y!=upload_y) throw std::runtime_error("Compressed upload advanced the ball");
                ++uploads.at(half);
                upload_ms.at(half)+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
            } else first_upload=0;
            if(n>=607 && n<=617 && !upload) throw std::runtime_error("First upper upload was not compressed");
            if(n>=844 && n<=854 && !upload) throw std::runtime_error("First lower upload was not compressed");
            if((n==619 || n==620 || n==856 || n==857) && upload) throw std::runtime_error("Resumed physics must retain normal pacing");
            held_frames+=transition.held();
            if(n==606) departing_image=presented;
            if(n>=607 && n<=621 && (!presented || !transition.held() || presented->pixels!=departing_image->pixels))
                throw std::runtime_error("First table transition lost its complete stitched image");
            if(n>=844 && n<=858 && (!presented || !transition.held()))
                throw std::runtime_error("Return transition lost its stitched image");
            if(output && transition.held()) throw std::runtime_error("Ready native image remained held");
            if(n==599 || n==719) {
                const auto writes=assets::restored_actor_backgrounds(actors,runtime->work_ram());
                auto restored_bat=video.vram;
                for(const auto& write:writes) restored_bat[write.address]=write.value;
                assets::restore_table_indicators(std::span(restored_bat).first(1024),indicators,half,runtime->work_ram());
                if(writes.size()!=(half==0?25u:100u)) throw std::runtime_error("Background actor restoration coverage changed");
                for(const auto& write:writes)
                    if(restored_bat[write.address]!=video.vram[write.address])
                        throw std::runtime_error("Restored background actor differs at frame "+std::to_string(n+1));
            }
            if(output && timed) {
                const auto frozen=table.compose(video,*previous,half,camera,runtime->work_ram());
                corrected+=frozen && frozen->pixels!=output->pixels;
                const unsigned offset=half==0?render::table_half_offset:0;
                for(unsigned y=0;y<240;++y) for(unsigned x=0;x<256;++x) {
                    // Fixed seam artwork intentionally differs from the active viewport.
                    if((half==0 && y<240-render::table_half_offset) || (half==1 && y>=render::table_half_offset)) continue;
                    // Only the deliberately removed HUD can differ from the reference.
                    if((y>=16 && y<32 && x>=16 && x<112) || (y>=32 && y<48 && x>=232 && x<248)) continue;
                    if(output->pixels[(y+offset)*256+x]!=reference[y*256+x])
                        throw std::runtime_error("Live stitched raster differs at frame "+std::to_string(n+1));
                }
            }
            if(n==606) std::copy(runtime->work_ram().begin(),runtime->work_ram().end(),departing_ram.begin());
            if(n==619) {
                const auto projected=assets::inactive_table_actor_ram(runtime->work_ram());
                for(unsigned field=0;field<32;++field)
                    if(!std::equal(projected.begin()+0x1504+field*64,projected.begin()+0x1540+field*64,
                        departing_ram.begin()+0x1504+field*64)) throw std::runtime_error("Inactive bank differs from departed actor state");
                if(projected[0x1500]!=std::byte{} || projected[0x1540]!=std::byte{}) throw std::runtime_error("Inactive bank contains the live ball");
                inactive_lower=table.inactive_view(0,runtime->work_ram());
            }
            if(n>=620 && n<720 && table.inactive_view(0,runtime->work_ram()).pixels!=inactive_lower->pixels)
                throw std::runtime_error("Inactive table advanced while the upper table was active");
            if(output) ++available.at(half);
            if(n<300 && output) throw std::runtime_error("Startup/title must use original display");
            if(n==598) {checkpoint=saved;checkpoint_video=video;}
            if(n==599) {
                lower_background=render::background(video.vram,video.palette,32,32);
                if(!output) throw std::runtime_error("Lower table unavailable");
                std::array<std::uint16_t,256> sat{};
                for(unsigned i=0;i<10;++i) {sat[i*4+2]=static_cast<std::uint16_t>((20+i)*2);sat[i*4+3]=0x8f;}
                const auto original_digits=render::sprites(video.vram,video.palette,sat,0);
                for(unsigned i=0;i<10;++i) {
                    diagnostics::AlienState digit_state;digit_state.score_points=i;
                    const auto panel=score_panel.compose(digit_state);
                    for(unsigned y=0;y<32;++y) for(unsigned x=0;x<32;++x) {
                        auto color=original_digits[i].image.pixels[(y/2)*16+x/2];
                        if(!(color>>24)) color=0xff000000;
                        if(panel.pixels[(18+y)*256+112+x]!=color)
                            throw std::runtime_error("Cabinet score digit differs from original loaded HUD artwork");
                    }
                }
                if(argc==3) {
                    auto artifact=core::path_from_utf8(argv[2]);artifact.replace_extension("score.ppm");
                    const auto panel=score_panel.compose(diagnostics::observe_alien_state(runtime->work_ram()));
                    std::ofstream stream(artifact,std::ios::binary);stream<<"P6\n256 96\n255\n";
                    for(const auto color:panel.pixels) for(const auto shift:{16,8,0}) stream.put(static_cast<char>((color>>shift)&255));
                    if(!stream) throw std::runtime_error("Cannot write score preview");
                }
                expected=output;
                if(timed) {
                    auto incomplete=raster; incomplete.rows=incomplete.rows.first(239);
                    if(table.compose(video,*previous,0,0,runtime->work_ram(),&incomplete))
                        throw std::runtime_error("Incomplete raster rows accepted");
                    incomplete=raster; incomplete.vram=incomplete.vram.first(32767);
                    if(table.compose(video,*previous,0,0,runtime->work_ram(),&incomplete))
                        throw std::runtime_error("Incomplete initial raster VRAM accepted");
                }
                if(table.compose(video,*previous,2,0,ram) || table.compose(video,*previous,0,1,ram) ||
                    table.compose(video,video,0,0,ram)) throw std::runtime_error("Unsafe scene/epoch accepted");
                auto corrupt=video; corrupt.vram[0x800]^=1;
                if(table.compose(corrupt,*previous,0,0,ram)) throw std::runtime_error("Incomplete pattern load accepted");
            }
            if(n==719) {
                const auto upper_background=render::background(video.vram,video.palette,32,32);
                unsigned old_mismatches=0;
                for(unsigned y=0;y<24;++y) for(unsigned x=0;x<256;++x) {
                    if(x>=8 && (x<232 || x>=248)) continue;
                    const auto pixel=lower_background->pixels[y*256+x];
                    if(pixel!=upper_background.pixels[(y+render::table_half_offset)*256+x])
                        throw std::runtime_error("ROM side-wall artwork does not align at seam");
                    old_mismatches+=pixel!=upper_background.pixels[(y+224)*256+x];
                }
                if(!old_mismatches) throw std::runtime_error("Art alignment test did not distinguish the old ball-coordinate offset");
                if(!output) throw std::runtime_error("Upper table unavailable");
                // Synthetic rendering coverage only: never change emulator RAM.
                std::vector<std::byte> counter_ram(runtime->work_ram().begin(),runtime->work_ram().end());
                counter_ram[0x7f]=std::byte{1};
                const auto baseline=table.inactive_view(0,counter_ram);
                for(unsigned value=2;value<=7;++value) {
                    counter_ram[0x7f]=static_cast<std::byte>(value);
                    const auto variant=table.inactive_view(0,counter_ram);
                    unsigned changed=0;
                    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<256;++x) {
                        if(variant.pixels[y*256+x]==baseline.pixels[y*256+x]) continue;
                        ++changed;
                        if(x<104 || x>=128 || y<136 || y>=152)
                            throw std::runtime_error("Inactive counter changed pixels outside its rectangle");
                    }
                    if(!changed) throw std::runtime_error("Inactive counter variant did not render");
                }
                auto restored_bat=video.vram;
                assets::restore_table_indicators(std::span(restored_bat).first(1024),indicators,1,runtime->work_ram());
                if(restored_bat!=video.vram) throw std::runtime_error("Persistent indicator colors differ from original upper table");
                unsigned lit_words=0;
                for(unsigned i=0;i<indicators.size();++i)
                    if(indicators[i].table==1 && (std::to_integer<unsigned>(runtime->work_ram()[0xd6+i/8])&(1u<<(i%8))))
                        lit_words+=indicators[i].width*indicators[i].height;
                if(lit_words!=26) throw std::runtime_error("Upper indicator checkpoint coverage changed");
            }
            previous=video;
        }
        runtime->restore(*checkpoint); runtime->advance(recording::canonical_input(599));
        const auto restored_video=diagnostics::decode_reference_snapshot(runtime->save());
        const render::TableRaster restored_raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
        const auto restored=table.compose(restored_video,*checkpoint_video,0,0,runtime->work_ram(),timed?&restored_raster:nullptr);
        if(!restored || restored->pixels!=expected->pixels) throw std::runtime_error("Restored table presentation differs");
        if(!available[0] || !available[1]) throw std::runtime_error("Both table halves must render");
        if(timed && !corrected) throw std::runtime_error("Replay did not exercise a raster correction");
        if(held_frames<30) throw std::runtime_error("Both transition directions were not covered");
        if(ball_updates[0]!=75 || ball_updates[1]!=75 || ball_updates[0]+ball_updates[1]!=held_frames)
            throw std::runtime_error("Canonical ball replacement counts: "+std::to_string(ball_updates[0])+"/"+std::to_string(ball_updates[1]));
        if(argc==3) {
            recording::Recording smoke{*checkpoint,{}};
            for(unsigned n=599;n<631;++n) smoke.inputs.push_back(recording::canonical_input(n));
            recording::write(core::path_from_utf8(argv[2]),smoke);
        }
        std::cout<<"PASS: lower="<<available[0]<<", upper="<<available[1]<<", held="<<held_frames
            <<", ball updates="<<ball_updates[0]<<'/'<<ball_updates[1]<<", raster corrections="<<corrected
            <<"; active pixels, scene guards and restored presentation match\n";
        if(counter_frames!=available[0]) throw std::runtime_error("Counter verification missed lower table frames");
        std::cout<<counter_frames<<" lower counter BAT comparisons; six synthetic inactive variants verified\n";
        if(uploads[0]!=55 || uploads[1]!=55) throw std::runtime_error("Expected eleven upload frames at each of ten crossings");
        std::cout<<"Upload frames lower/upper="<<uploads[0]<<'/'<<uploads[1]<<", mean execution ms="
            <<upload_ms[0]/uploads[0]<<'/'<<upload_ms[1]/uploads[1]<<"; no ball movement compressed; 576 ROM seam pixels match\n";
        if(timed) verify_transition_clock(*runtime,table,*checkpoint,*final_state);
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
