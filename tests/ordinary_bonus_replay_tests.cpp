// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <algorithm>
#include <fstream>
#include "cabinet_presentation_check.hpp"
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
PadInput input_at(unsigned n) {
    if(n<360 || n>=2199) return recording::canonical_input(n);
    PadInput input{};
    if(n%240<100 || n%39<19) input.player_one|=Pad::button_i;
    if((n+13)%39<19) input.player_one|=Pad::left;
    if(n%181==100) input.player_one|=Pad::button_ii;
    return input;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and output prefix");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        const auto table=std::make_unique<render::TablePlayfield>(rom);
        auto runtime=make_reference_runtime();runtime->load(path);
        const bool timed=runtime->supports_display_observation();
        if(timed) runtime->observe_display(true);
        CabinetPresentationCheck cabinet_check;
        std::optional<diagnostics::ReferenceSnapshot> previous;
        render::TableTransitionHold hold;
        recording::Recording entry,returning;
        std::vector<std::string> return_fingerprints;
        state::SaveStateContainer checkpoint;
        std::vector<std::byte> final_ram;
        std::vector<std::uint32_t> final_pixels;
        std::vector<std::int16_t> final_audio;
        std::optional<render::Image> first_return;
        unsigned bonus_frames=0,first_bonus=0,first_main=0,first_ready=0;
        std::array<unsigned,2> ready{};
        std::optional<std::array<std::byte,8192>> restored_lower;
        unsigned dormant_frames=0;
        bool lower_resumed=false;
        bool was_bonus=false;
        std::uint64_t fade_out_energy=~std::uint64_t{0},fade_in_energy=0;
        unsigned fade_out_frames=0,fade_in_frames=0;
        for(unsigned n=0;n<2800;++n) {
            const auto frame=runtime->advance(input_at(n));
            if(n>=2150 && n<2243) return_fingerprints.push_back(recording::fingerprint(frame,runtime->work_ram()));
            const std::vector<std::uint32_t> pixels(frame.pixels.begin(),frame.pixels.end());
            const std::vector<std::int16_t> audio(frame.stereo.begin(),frame.stereo.end());
            const auto ram=runtime->work_ram();
            auto b=[&](unsigned address){return std::to_integer<unsigned>(ram[address]);};
            const auto observed=diagnostics::observe_alien_state(ram);
            require(b(0x7c)==0 && !observed.demo_play,"Route uses ordinary play, not practice or demo");
            if(observed.bonus_room) {
                ++bonus_frames;was_bonus=true;
                if(!first_bonus) {first_bonus=n+1;require(observed.bonus_room==2,"Ordinary route enters room 2");}
            } else if(was_bonus && observed.table_code<2 && !first_main) {
                first_main=n+1;
                require(observed.score_points==14400 && observed.ball_counter==3,"Ordinary return retains awarded score and all three balls");
            }
            const auto saved=runtime->save();
            const auto video=diagnostics::decode_reference_snapshot(saved);
            const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            cabinet_check.check(*table,video,previous?&*previous:nullptr,ram,timed?&raster:nullptr);
            // Verified ordinary-bonus ROM fade windows. Neither half may
            // flash brighter on entry or darker during the return fade.
            if((n+1>=835 && n+1<=853) || (n+1>=2213 && n+1<=2242)) {
                const auto image=table->cabinet_frame(video,previous?&*previous:nullptr,ram,
                    timed?&raster:nullptr,cabinet_check.hold);
                require(image.has_value(),"Bonus fade lost stitched projection");
                std::uint64_t energy=0;
                for(const auto pixel:image->pixels) energy+=(pixel&255)+((pixel>>8)&255)+((pixel>>16)&255);
                if(n+1<854) {
                    require(energy<=fade_out_energy,"Bonus entry fade flashed brighter");
                    fade_out_energy=energy;++fade_out_frames;
                } else {
                    require(energy>=fade_in_energy,"Bonus return fade flashed darker");
                    fade_in_energy=energy;++fade_in_frames;
                }
            }
            const unsigned camera=b(0x16)|(b(0x17)<<8);
            const auto output=previous?table->compose(video,*previous,observed.table_code,camera,ram,timed?&raster:nullptr):std::nullopt;
            const auto presented=hold.present(output,video.frame,observed.table_code,camera,ram);
            if(observed.bonus_room) require(!output && !presented && !hold.held(),"Bonus uses original viewport and discards the old stitched table");
            if(first_main && n+1<2243) require(!output && !presented,"Original viewport remains through the main-table return fade");
            if(first_main && output) {
                ++ready.at(observed.table_code);
                if(!first_ready) {
                    first_ready=n+1;first_return=output;
                    require(observed.table_code==1,"Ordinary bonus returns to upper table first");
                    restored_lower=assets::inactive_table_actor_ram(ram);
                    // E942 loads the 19 lower-table entries at 04:B97A into
                    // slots 4..22. FF61 then exchanges them into the stored bank.
                    for(unsigned i=0;i<19;++i)
                        require((*restored_lower)[0x1504+i]==rom[0x997b+i*6],"Original lower-table actor list restored into the inactive bank");
                }
                if(timed) {
                    const unsigned offset=observed.table_code==0?render::table_half_offset:0;
                    for(unsigned y=0;y<240;++y) for(unsigned x=0;x<256;++x) {
                        if((observed.table_code==0 && y<240-render::table_half_offset) || (observed.table_code==1 && y>=render::table_half_offset)) continue;
                        if((y>=16 && y<32 && x>=16 && x<112) || (y>=32 && y<48 && x>=232 && x<248)) continue;
                        if(output->pixels[(y+offset)*256+x]!=pixels[y*256+x])
                            throw std::runtime_error("Returned stitched active pixels differ at frame "+std::to_string(n+1));
                    }
                }
            }
            if(restored_lower && !lower_resumed) {
                if(observed.table_code==0) lower_resumed=true;
                else {
                    const auto projected=assets::inactive_table_actor_ram(ram);
                    for(unsigned field=0;field<32;++field)
                        require(std::equal(projected.begin()+0x1504+field*64,projected.begin()+0x1540+field*64,
                            restored_lower->begin()+0x1504+field*64),"Restored inactive actors remain dormant until the next table handoff");
                    ++dormant_frames;
                }
            }
            if(n==799) {entry.initial=saved;checkpoint=saved;}
            if(n>=800 && n<1000) entry.inputs.push_back(input_at(n));
            if(n==2149) returning.initial=saved;
            if(n>=2150 && n<2450) returning.inputs.push_back(input_at(n));
            if(n==2799) {final_ram.assign(ram.begin(),ram.end());final_pixels=pixels;final_audio=audio;}
            previous=video;
        }
        require(first_bonus==854 && first_main==2199,"Original ordinary bonus entry and return boundaries");
        require(fade_out_frames==19 && fade_in_frames==30 && fade_out_energy==0 && fade_in_energy>1000000,
            "Bonus fade coverage must reach black and restore a visible full table");
        require(first_ready==2243 && dormant_frames>1 && lower_resumed,"Stitching resumes after the complete return fade, with a dormant lower actor bank");
        require(first_return.has_value() && ready[0] && ready[1],"Both main-table halves resume stitched play after the bonus");
        runtime->restore(checkpoint);previous.reset();hold.reset();
        for(unsigned n=800;n<2800;++n) {
            const auto frame=runtime->advance(input_at(n));
            const auto ram=runtime->work_ram();
            if(n==2799) {
                require(std::equal(final_ram.begin(),final_ram.end(),ram.begin()),"Ordinary bonus restore RAM parity");
                require(std::equal(final_pixels.begin(),final_pixels.end(),frame.pixels.begin(),frame.pixels.end()),"Ordinary bonus restore video parity");
                require(std::equal(final_audio.begin(),final_audio.end(),frame.stereo.begin(),frame.stereo.end()),"Ordinary bonus restore audio parity");
            }
            if(n+2>=first_ready && n+1<=first_ready) {
                const auto video=diagnostics::decode_reference_snapshot(runtime->save());
                if(n+1==first_ready) {
                    const auto observed=diagnostics::observe_alien_state(ram);
                    const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            cabinet_check.check(*table,video,previous?&*previous:nullptr,ram,timed?&raster:nullptr);
                    const auto output=table->compose(video,*previous,observed.table_code,0,ram,timed?&raster:nullptr);
                    require(output && output->pixels==first_return->pixels,"First restored stitched frame matches across both halves");
                }
                previous=video;
            }
        }
        auto prefix=core::path_from_utf8(argv[2]);auto output=prefix;output+=".entry.acrec";recording::write(output,entry);
        output=prefix;output+=".return.acrec";recording::write(output,returning);
        if(timed) {
            // Compare both execution modes from the same cold-restored filter
            // state, rather than cold restore against uninterrupted warm audio.
            runtime.reset();runtime=make_reference_runtime();runtime->load(path);runtime->restore(returning.initial);
            for(unsigned i=0;i<return_fingerprints.size();++i) {
                const auto frame=runtime->advance(returning.inputs[i]);
                return_fingerprints[i]=recording::fingerprint(frame,runtime->work_ram());
            }
            runtime.reset();runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(core::path_from_utf8(argv[1]));
            auto native_return=returning;
            // Explicit test import before the bonus ends; ordinary user restores
            // remain strict about runtime identity.
            native_return.initial.core_revision=runtime->revision();runtime->restore(native_return.initial);
            for(unsigned i=0;i<native_return.inputs.size();++i) {
                const auto frame=runtime->advance(native_return.inputs[i]);
                if(i<return_fingerprints.size()) {
                    const auto actual=recording::fingerprint(frame,runtime->work_ram());
                    if(actual!=return_fingerprints[i]) {
                        std::cerr<<"Bonus return expected "<<return_fingerprints[i]<<"\nactual "<<actual<<'\n';
                        throw std::runtime_error("Continuous mode preserves exact bonus return/fade RAM, video and audio");
                    }
                }
            }
            output=prefix;output+=".native-return.acrec";recording::write(output,native_return);
        }
        output=prefix;output+=".ppm";std::ofstream image(output,std::ios::binary);image<<"P6\n256 456\n255\n";
        for(const auto color:first_return->pixels) for(const auto shift:{16,8,0}) image.put(static_cast<char>(color>>shift));
        require(static_cast<bool>(image),"Write returned playfield preview");
        std::cout<<bonus_frames<<" bonus frames; returned ready lower/upper "<<ready[0]<<'/'<<ready[1]<<"; exact restore passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
