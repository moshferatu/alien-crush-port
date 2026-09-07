// SPDX-License-Identifier: GPL-3.0-or-later
#include "video_capture.hpp"
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/assets/table_assets.hpp"
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
}
void export_table_assets(const std::filesystem::path& directory,const std::filesystem::path& rom) {
    const auto tables=assets::decode_table_backgrounds(core::read_file(rom));
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
void verify_discovery(const std::filesystem::path& rom) {
    const auto tables=assets::decode_table_backgrounds(core::read_file(rom));
    auto runtime=make_reference_runtime(); runtime->load(rom);
    std::size_t hud_checks=0;
    for(std::uint64_t n=0;n<1800;++n) {
        (void)runtime->advance(recording::canonical_input(n));
        if(n<359) continue;
        const auto observed=observe_alien_state(runtime->work_ram());
        const auto snapshot=decode_reference_snapshot(runtime->save());
        if(n==599 || n==719) {
            const auto index=n==599?0U:1U; const auto& table=tables[index];
            if(observed.table_code!=index || !std::equal(table.vram.begin()+0x800,table.vram.begin()+0x800+table.pattern_words,snapshot.vram.begin()+0x800))
                throw std::runtime_error("Native table pattern mismatch at frame "+std::to_string(n+1));
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
    std::cout<<"PASS: both ROM-decoded table pattern sets exactly match reference VRAM; "<<hud_checks<<" score/ball HUD checks.\n";
}
}
