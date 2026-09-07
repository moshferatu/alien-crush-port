// SPDX-License-Identifier: GPL-3.0-or-later
#include "video_capture.hpp"
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/core/rom_image.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/core/recording.hpp"
#include <fstream>
#include <iostream>
namespace alien_crush::diagnostics {
namespace {
void bitmap(const std::filesystem::path& path,const render::Image& image) {
    const auto size=static_cast<std::uint32_t>(image.pixels.size()*4);
    std::vector<std::byte> bytes(54+size);
    auto u32=[&](std::size_t i,std::uint32_t v){for(unsigned s=0;s<32;s+=8) bytes[i++]=static_cast<std::byte>((v>>s)&255);};
    bytes[0]=std::byte{'B'}; bytes[1]=std::byte{'M'}; u32(2,54+size); u32(10,54); u32(14,40);
    u32(18,static_cast<std::uint32_t>(image.width)); u32(22,static_cast<std::uint32_t>(-image.height));
    bytes[26]=std::byte{1}; bytes[28]=std::byte{32}; u32(34,size);
    const auto raw=std::as_bytes(std::span(image.pixels)); std::copy(raw.begin(),raw.end(),bytes.begin()+54);
    core::write_file_atomic(path,bytes);
}
render::Image pattern_atlas(const assets::TableSprites& table,
    const std::array<std::uint16_t,512>& palette,unsigned palette_index) {
    render::Image atlas{256,256,std::vector<std::uint32_t>(256*256,0xff202020)};
    // VRAM 4000..7FFF contains 256 individual 16x16 patterns (IDs 256..511).
    // Show every palette because actor data, not pattern data, selects colors.
    for(unsigned batch=0;batch<4;++batch) {
        std::array<std::uint16_t,256> sat{};
        for(unsigned slot=0;slot<64;++slot) {
            sat[slot*4+2]=static_cast<std::uint16_t>((256+batch*64+slot)*2);
            sat[slot*4+3]=static_cast<std::uint16_t>(palette_index);
        }
        const auto decoded=render::sprites(table.vram,palette,sat,0);
        for(unsigned slot=0;slot<64;++slot) {
            const auto tile=batch*64+slot;
            for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
                const auto color=decoded[slot].image.pixels[y*16+x];
                if(color>>24) atlas.pixels[((tile/16)*16+y)*256+(tile%16)*16+x]=color;
            }
        }
    }
    return atlas;
}
}
void export_table_assets(const std::filesystem::path& directory,const std::filesystem::path& rom) {
    const auto image=core::read_file(rom);
    const auto tables=assets::decode_table_backgrounds(image);
    const auto sprite_tables=assets::decode_table_sprites(image);
    const auto versioned=directory/rom::kSupportedSha256/"table-background-v1";
    std::filesystem::create_directories(versioned);
    std::ofstream manifest; manifest.exceptions(std::ios::failbit|std::ios::badbit); manifest.open(versioned/"manifest.csv");
    manifest<<"table,pattern_words,map_words,palette_words,vram_sha256,palette_sha256\n";
    for(std::size_t i=0;i<2;++i) {
        const auto name=i==0?"lower":"upper";
        const auto& table=tables[i];
        bitmap(versioned/(std::string(name)+".bmp"),render::background(table.vram,table.palette,32,32));
        const auto vram=std::as_bytes(std::span(table.vram));
        const auto palette=std::as_bytes(std::span(table.palette));
        core::write_file_atomic(versioned/(std::string(name)+"-vram.bin"),vram);
        core::write_file_atomic(versioned/(std::string(name)+"-palette.bin"),palette);
        manifest<<name<<','<<table.pattern_words<<','<<table.map_words<<','<<table.palette_words<<','
            <<rom::compute_digests(vram).sha256<<','<<rom::compute_digests(palette).sha256<<'\n';
    }
    manifest.close();
    const auto sprite_directory=directory/rom::kSupportedSha256/"table-sprites-v1";
    std::filesystem::create_directories(sprite_directory);
    manifest.open(sprite_directory/"manifest.csv");
    manifest<<"table,stream,rom_offset,compressed_bytes,vram_word,word_count,sha256\n";
    for(std::size_t level=0;level<2;++level) {
        const std::string name=level==0?"lower":"upper";
        const auto& table=sprite_tables[level];
        core::write_file_atomic(sprite_directory/(name+"-vram.bin"),std::as_bytes(std::span(table.vram)));
        core::write_file_atomic(sprite_directory/(name+"-palette.bin"),std::as_bytes(std::span(tables[level].palette)));
        for(std::size_t index=0;index<table.streams.size();++index) {
            const auto& stream=table.streams[index];
            const auto data=std::as_bytes(std::span(table.vram).subspan(stream.vram_word,stream.word_count));
            manifest<<name<<','<<index<<','<<stream.rom_offset<<','<<stream.compressed_bytes<<','
                <<stream.vram_word<<','<<stream.word_count<<','<<rom::compute_digests(data).sha256<<'\n';
        }
        for(unsigned palette=0;palette<16;++palette)
            bitmap(sprite_directory/(name+"-palette-"+std::to_string(palette)+".bmp"),pattern_atlas(table,tables[level].palette,palette));
    }
    manifest.close();
}
void export_video(const std::filesystem::path& directory,const state::SaveStateContainer& saved) {
    const auto snapshot=decode_reference_snapshot(saved);
    std::filesystem::create_directories(directory);
    core::write_file_atomic(directory/"reference.state",state::encode(saved));
    core::write_file_atomic(directory/"vram.bin",std::as_bytes(std::span(snapshot.vram)));
    core::write_file_atomic(directory/"palette.bin",std::as_bytes(std::span(snapshot.palette)));
    core::write_file_atomic(directory/"sat.bin",std::as_bytes(std::span(snapshot.sat)));
    bitmap(directory/"background.bmp",render::background(snapshot.vram,snapshot.palette,snapshot.bat_width(),snapshot.bat_height()));
    const auto sprites=render::sprites(snapshot.vram,snapshot.palette,snapshot.sat,snapshot.memory_width);
    bitmap(directory/"sprites.bmp",render::sprite_sheet(sprites));
    std::ofstream table; table.exceptions(std::ios::failbit|std::ios::badbit); table.open(directory/"sprites.csv");
    table<<"slot,x,y,width,height,pattern,flags,foreground\n";
    for(std::size_t i=0;i<sprites.size();++i) {
        const auto& s=sprites[i];
        table<<i<<','<<s.x<<','<<s.y<<','<<s.width<<','<<s.height<<','<<s.pattern<<','<<s.flags<<','<<s.foreground<<'\n';
    }
    table.close();
    std::ofstream manifest; manifest.exceptions(std::ios::failbit|std::ios::badbit); manifest.open(directory/"manifest.txt");
    manifest<<"schema=1\nrom_sha256="<<saved.rom_sha256<<"\nbackend="<<saved.core_revision<<"\nframe="<<saved.frame_number
        <<"\npc="<<snapshot.pc<<"\ncontrol="<<snapshot.control<<"\nmemory_width="<<snapshot.memory_width
        <<"\nscroll_x="<<snapshot.scroll_x<<"\nscroll_y="<<snapshot.scroll_y<<"\nbat_width="<<snapshot.bat_width()<<"\nbat_height="<<snapshot.bat_height()<<'\n';
    for(std::size_t i=0;i<8;++i) manifest<<"mpr"<<i<<'='<<static_cast<unsigned>(snapshot.mpr[i])<<'\n';
    manifest<<"phase=post-frame; register values are not a scanline history\n";
    manifest.close();
}
void verify_discovery(const std::filesystem::path& rom,const std::filesystem::path& report) {
    const auto image=core::read_file(rom);
    const auto tables=assets::decode_table_backgrounds(image);
    const auto sprite_tables=assets::decode_table_sprites(image);
    auto runtime=make_reference_runtime(); runtime->load(rom);
    const assets::ActorAssets actor_assets(image);
    // Arithmetic edges outside the natural route, using the verified ball descriptor.
    std::array<std::byte,8192> actor_fixture{};
    actor_fixture[0x50]=std::byte{20}; actor_fixture[0x1500]=std::byte{1}; actor_fixture[0x1540]=std::byte{0x80};
    actor_fixture[0x16]=std::byte{1}; actor_fixture[0x2e]=std::byte{0xff}; actor_fixture[0x2f]=std::byte{0xff};
    actor_fixture[0x1a00]=std::byte{0xa0}; actor_fixture[0x1a80]=std::byte{0x0f}; actor_fixture[0x1980]=std::byte{0x88};
    const auto fixture=assets::assemble_table_actors(actor_assets,actor_fixture);
    if(fixture.size()!=1 || fixture[0].slot!=20 || fixture[0].words!=std::array<std::uint16_t,4>{65529,65530,31,0x888f})
        throw std::runtime_error("Actor coordinate/pattern wrapping or attribute override mismatch");
    if(!actor_assets.pieces(1,62).empty() || actor_assets.pieces(1,61).size()!=2)
        throw std::runtime_error("Actor SAT capacity boundary mismatch");
    std::vector<assets::ActorSprite> previous_actors;
    std::size_t actor_checks=0,actor_bad=0,actor_frames=0;
    std::size_t verified_actor_words=0;
    std::ofstream actor_report;
    if(!report.empty()) {
        if(std::filesystem::exists(report)) throw std::runtime_error("Discovery report requires a new directory");
        std::filesystem::create_directories(report);
        actor_report.exceptions(std::ios::failbit|std::ios::badbit);
        actor_report.open(report/"actor-comparison.csv");
        actor_report<<"ram_frame,source_sat_frame,table,actor,descriptor,slot,native_y,native_x,native_pattern,native_flags,source_y,source_x,source_pattern,source_flags,matching_words\n";
    }
    std::size_t hud_checks=0;
    for(std::uint64_t n=0;n<1800;++n) {
        (void)runtime->advance(recording::canonical_input(n));
        if(n<359) continue;
        const auto observed=observe_alien_state(runtime->work_ram());
        const auto snapshot=decode_reference_snapshot(runtime->save());
        std::vector<assets::ActorSprite> actors;
        try { if(n>=479) actors=assets::assemble_table_actors(actor_assets,runtime->work_ram()); }
        catch(const std::exception& e) { throw std::runtime_error(std::string(e.what())+" frame "+std::to_string(n+1)); }
        if(n>=480) {
            bool bad=false;
            for(const auto& sprite:previous_actors) {
                unsigned matching=0;
                for(std::size_t w=0;w<4;++w) {
                    ++actor_checks;
                    if(sprite.words[w]!=snapshot.vram[0x400+sprite.slot*4+w]) {++actor_bad;bad=true;}
                    else ++matching;
                }
                if(actor_report.is_open()) {
                    actor_report<<n<<','<<n+1<<','<<static_cast<unsigned>(observed.table_code)<<','<<static_cast<unsigned>(sprite.actor)<<','
                        <<static_cast<unsigned>(sprite.descriptor)<<','<<static_cast<unsigned>(sprite.slot);
                    for(auto word:sprite.words) actor_report<<','<<word;
                    for(std::size_t w=0;w<4;++w) actor_report<<','<<snapshot.vram[0x400+sprite.slot*4+w];
                    actor_report<<','<<matching<<'\n';
                }
            }
            if(!bad) ++actor_frames;
            // Fixed, independently inspected lower/upper samples. Other epochs
            // stay diagnostic; they are not silently excluded from an equality claim.
            if(n==599 || n==719) {
                if(bad || previous_actors.size()!=(n==599?41U:40U))
                    throw std::runtime_error("Native actor SAT mismatch at frame "+std::to_string(n+1));
                verified_actor_words+=previous_actors.size()*4;
            }
        }
        if(!report.empty() && (n==599 || n==719)) {
            const auto directory=report/("frame-"+std::to_string(n+1));
            export_video(directory,runtime->save());
            std::array<std::uint16_t,256> sat{};
            for(const auto& sprite:previous_actors) std::copy(sprite.words.begin(),sprite.words.end(),sat.begin()+sprite.slot*4);
            const auto decoded=render::sprites(sprite_tables[n==599?0:1].vram,snapshot.palette,sat,snapshot.memory_width);
            render::Image layer{256,240,std::vector<std::uint32_t>(256*240,0xff202020)};
            // Diagnostic sprite layer: lowest SAT index wins, no background or scanline limits.
            for(auto it=previous_actors.rbegin();it!=previous_actors.rend();++it) {
                const auto& s=decoded[it->slot];
                for(int y=0;y<s.height;++y) for(int x=0;x<s.width;++x) {
                    const int dx=s.x+x,dy=s.y+y;
                    const auto color=s.image.pixels[static_cast<std::size_t>(y*s.width+x)];
                    if(dx>=0 && dx<256 && dy>=0 && dy<240 && color>>24) layer.pixels[static_cast<std::size_t>(dy*256+dx)]=color;
                }
            }
            bitmap(directory/"native-actor-layer.bmp",layer);
        }
        previous_actors=actors;
        if(n==599 || n==719) {
            const auto index=n==599?0U:1U; const auto& table=tables[index];
            if(observed.table_code!=index || !std::equal(table.vram.begin()+0x800,table.vram.begin()+0x800+table.pattern_words,snapshot.vram.begin()+0x800))
                throw std::runtime_error("Native table pattern mismatch at frame "+std::to_string(n+1));
            for(const auto& stream:sprite_tables[index].streams) {
                const auto begin=sprite_tables[index].vram.begin()+stream.vram_word;
                if(!std::equal(begin,begin+stream.word_count,snapshot.vram.begin()+stream.vram_word))
                    throw std::runtime_error("Native sprite pattern mismatch at frame "+std::to_string(n+1)+" VRAM word "+std::to_string(stream.vram_word));
            }
        }
        // Check the original HUD's source SAT in VRAM. The latched SAT belongs to
        // a different DMA epoch and may legitimately show the preceding score.
        bool valid=true; std::uint32_t score=0;
        for(std::size_t digit=0;digit<9;++digit) {
            const auto base=0x400+digit*4;
            const auto pattern=(snapshot.vram[base+2]>>1)&1023;
            if(snapshot.vram[base]!=80 || snapshot.vram[base+1]!=48+10*digit || pattern<20 || pattern>29) {valid=false;break;}
            score=score*10+pattern-20;
        }
        if(valid && !observed.score_update_pending) {
            if(score!=observed.score_points) throw std::runtime_error("Mapped score differs from original HUD at frame "+std::to_string(n+1));
            const auto balls=(snapshot.vram[0x426]>>1)&1023;
            if(observed.ball_counter>=1 && observed.ball_counter<=10 && balls!=20+observed.ball_counter-1)
                throw std::runtime_error("Mapped ball counter differs from original HUD at frame "+std::to_string(n+1));
            ++hud_checks;
        }
    }
    if(hud_checks<1000) throw std::runtime_error("Insufficient discovery HUD coverage");
    if(actor_report.is_open()) {
        actor_report.close();
        std::ofstream manifest; manifest.exceptions(std::ios::failbit|std::ios::badbit); manifest.open(report/"actor-manifest.txt");
        manifest<<"schema=table-actors-v1\nrom_sha256="<<rom::kSupportedSha256<<"\nbackend="<<runtime->save().core_revision
            <<"\nroute=canonical\nram_frames=480..1799\nsource_sat_frames=481..1800\ncompared_words="<<actor_checks
            <<"\nmismatching_words="<<actor_bad<<"\nexact_frames="<<actor_frames<<"\nsampled_verified_words="<<verified_actor_words
            <<"\nverified_source_frames=600,720\nverified_ram_frames=599,719\n"
            <<"preview=previous-frame RAM; native initial patterns; current reference palette; sprite-only layer\n"
            <<"limits=main-table actor pass only; no HUD, BAT writes, later sprite pass or scanline limits; timing differences unresolved\n";
        manifest.close();
    }
    std::cout<<"Actor epoch diagnostic (previous RAM vs current source SAT): "<<actor_checks<<" words, "<<actor_bad
        <<" mismatches, "<<actor_frames<<"/1320 exact frames; full actor equivalence remains open.\n";
    std::cout<<"PASS: both ROM-decoded background and sprite pattern sets exactly match reference VRAM; "<<hud_checks<<" score/ball HUD checks.\n";
    std::cout<<"PASS: "<<verified_actor_words<<" native actor SAT words match at lower/upper source frames 600/720; arithmetic and capacity edges verified.\n";
}
}
