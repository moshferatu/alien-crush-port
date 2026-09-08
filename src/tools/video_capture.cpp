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
#include <deque>
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
    const auto hud=assets::decode_hud_assets(image);
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
    const auto hud_directory=directory/rom::kSupportedSha256/"table-hud-v1";
    std::filesystem::create_directories(hud_directory);
    core::write_file_atomic(hud_directory/"patterns.bin",std::as_bytes(std::span(hud.patterns)));
    std::array<std::uint16_t,32768> hud_vram{};
    std::copy(hud.patterns.begin(),hud.patterns.end(),hud_vram.begin()+0x500);
    std::array<std::uint16_t,256> hud_sat{};
    for(unsigned i=0;i<10;++i) {hud_sat[i*4+2]=static_cast<std::uint16_t>((20+i)*2);hud_sat[i*4+3]=0x8f;}
    const auto digits=render::sprites(hud_vram,tables[0].palette,hud_sat,0);
    render::Image strip{160,16,std::vector<std::uint32_t>(160*16,0xff202020)};
    for(unsigned i=0;i<10;++i) for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
        const auto color=digits[i].image.pixels[y*16+x];
        if(color>>24) strip.pixels[y*160+i*16+x]=color;
    }
    bitmap(hud_directory/"digits.bmp",strip);
    manifest.open(hud_directory/"manifest.txt");
    manifest<<"schema=table-hud-v1\nrom_sha256="<<rom::kSupportedSha256<<"\nrom_offset="<<hud.stream.rom_offset
        <<"\ncompressed_bytes="<<hud.stream.compressed_bytes<<"\nvram_word=1280\nword_count="<<hud.patterns.size()
        <<"\nsha256="<<rom::compute_digests(std::as_bytes(std::span(hud.patterns))).sha256
        <<"\npreview=digits 0..9; lower initial palette 15; dark gray background\n";
    manifest.close();
}
void export_video(const std::filesystem::path& directory,const state::SaveStateContainer& saved,const render::Image* reference,
    const std::array<std::uint16_t,256>* previous_sat) {
    if(reference && (reference->width!=256 || reference->height!=240 || reference->pixels.size()!=256*240))
        throw std::runtime_error("Video comparison requires a complete 256x240 reference frame");
    const auto snapshot=decode_reference_snapshot(saved);
    std::filesystem::create_directories(directory);
    core::write_file_atomic(directory/"reference.state",state::encode(saved));
    core::write_file_atomic(directory/"vram.bin",std::as_bytes(std::span(snapshot.vram)));
    core::write_file_atomic(directory/"palette.bin",std::as_bytes(std::span(snapshot.palette)));
    core::write_file_atomic(directory/"sat.bin",std::as_bytes(std::span(snapshot.sat)));
    bitmap(directory/"background.bmp",render::background(snapshot.vram,snapshot.palette,snapshot.bat_width(),snapshot.bat_height()));
    const auto sprites=render::sprites(snapshot.vram,snapshot.palette,snapshot.sat,snapshot.memory_width);
    bitmap(directory/"sprites.bmp",render::sprite_sheet(sprites));
    const auto composite=render::compose(snapshot.vram,snapshot.palette,snapshot.sat,
        {256,240,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,snapshot.control});
    bitmap(directory/"composite.bmp",composite);
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
    manifest<<"composite=frozen registers, 256x240, raw scroll origin, latched SAT; no scanline changes or sprite limits\n";
    if(reference) {
        bitmap(directory/"reference.bmp",*reference);
        const auto compare=[&](const render::Image& candidate,const std::string& name) {
            render::Image difference{256,240,std::vector<std::uint32_t>(256*240,0xff000000)};
            std::ofstream rows; rows.exceptions(std::ios::failbit|std::ios::badbit);
            rows.open(directory/(name+"-difference.csv"));
            rows<<"frame,y,compared_pixels,different_pixels,first_x,last_x\n";
            std::size_t total=0;
            for(int y=0;y<240;++y) {
                unsigned different=0; int first=-1,last=-1;
                for(int x=0;x<256;++x) {
                    const auto p=static_cast<std::size_t>(y)*256+x;
                    if(candidate.pixels[p]==reference->pixels[p]) continue;
                    ++different; if(first<0) first=x; last=x;
                    difference.pixels[p]=0xffff00ff;
                }
                total+=different;
                rows<<saved.frame_number<<','<<y<<",256,"<<different<<','<<first<<','<<last<<'\n';
            }
            rows.close();
            bitmap(directory/(name+"-difference.bmp"),difference);
            manifest<<name<<"_compared_pixels=61440\n"<<name<<"_different_pixels="<<total<<"\n";
            std::cout<<name<<" frame "<<saved.frame_number<<": "<<total<<" / 61440 pixels differ (diagnostic).\n";
        };
        manifest<<"comparison=exact ARGB, same frame and coordinates, complete viewport; no crop or alignment search\n"
            <<"difference_image=black matches, magenta differs\n";
        compare(composite,"composite");
        if(previous_sat) {
            const auto previous=render::compose(snapshot.vram,snapshot.palette,*previous_sat,
                {256,240,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,snapshot.control});
            core::write_file_atomic(directory/"previous-sat.bin",std::as_bytes(std::span(*previous_sat)));
            bitmap(directory/"previous-sat-composite.bmp",previous);
            manifest<<"previous_sat_frame="<<saved.frame_number-1<<"\n"
                <<"previous_sat_candidate=only latched SAT from preceding frame; all other state from current frame\n";
            compare(previous,"previous-sat-composite");
        }
    }
    manifest.close();
}
void verify_palette_pulse_trace(const std::filesystem::path& trace) {
    const auto bytes=core::read_file(trace);
    constexpr std::size_t record_size=16+2048;
    if(bytes.size()<=8 || (bytes.size()-8)%record_size ||
        std::string_view(reinterpret_cast<const char*>(bytes.data()),8)!="ACPZ0001")
        throw std::runtime_error("Invalid, empty or truncated palette pulse trace");
    auto b=[&](std::size_t p){return std::to_integer<std::uint8_t>(bytes[p]);};
    auto u32=[&](std::size_t p){std::uint32_t v=0;for(unsigned s=0;s<32;s+=8) v|=static_cast<std::uint32_t>(b(p++))<<s;return v;};
    std::size_t calls=0,active=0;
    std::uint32_t previous_end=0;
    for(std::size_t p=8;p<bytes.size();p+=record_size) {
        const auto entry=u32(p),end=u32(p+4);
        if(!entry || entry<previous_end || end<entry) throw std::runtime_error("Invalid pulse trace frame order");
        previous_end=end;
        const auto next=assets::advance_palette_pulse({b(p+8),b(p+9),b(p+10),b(p+11)});
        if(next.state.flags!=b(p+12) || next.state.green!=b(p+13) || next.state.red!=b(p+14) || next.state.blue!=b(p+15))
            throw std::runtime_error("Palette pulse state mismatch at frame "+std::to_string(entry));
        for(std::size_t i=0;i<1024;++i) {
            const auto expected=next.color_zero && i<2?static_cast<std::uint8_t>(*next.color_zero>>(i*8)):b(p+16+i);
            if(expected!=b(p+16+1024+i)) throw std::runtime_error("Palette pulse color mismatch at frame "+std::to_string(entry));
        }
        ++calls; if(next.color_zero) ++active;
    }
    std::cout<<"PASS: "<<calls<<" palette pulse calls match state and all palette words; "<<active<<" active / "<<calls-active<<" inactive.\n";
    if(!active) std::cout<<"Active pulse execution remains unverified by this trace.\n";
}
void verify_palette_fade_trace(const std::filesystem::path& trace) {
    const auto bytes=core::read_file(trace);
    if(bytes.size()<=8) throw std::runtime_error("Empty palette fade trace");
    const std::string_view schema(reinterpret_cast<const char*>(bytes.data()),8);
    const bool fade_out=schema=="ACFD0001";
    const std::size_t record_size=fade_out?10+1024*2:9+1024*3;
    if((!fade_out && schema!="ACPF0001") || (bytes.size()-8)%record_size)
        throw std::runtime_error("Invalid, empty or truncated palette fade trace");
    auto u32=[&](std::size_t p){std::uint32_t v=0;for(unsigned s=0;s<32;s+=8) v|=std::to_integer<std::uint32_t>(bytes[p++])<<s;return v;};
    auto palette=[&](std::size_t p){std::array<std::uint16_t,512> result{};for(auto& color:result) {
        color=static_cast<std::uint16_t>(std::to_integer<unsigned>(bytes[p])|(std::to_integer<unsigned>(bytes[p+1])<<8));p+=2;
    }return result;};
    std::size_t steps=0,cross_frame=0,completed=0;
    std::uint32_t previous_end=0;
    std::array<bool,22> thresholds{};
    for(std::size_t p=8;p<bytes.size();p+=record_size) {
        const auto entry=u32(p),end=u32(p+4);
        if(!entry || entry<previous_end || end<entry) throw std::runtime_error("Invalid palette fade frame order");
        previous_end=end;
        if(fade_out) {
            const auto result=assets::advance_palette_fade_out(palette(p+10));
            const auto black=std::to_integer<unsigned>(bytes[p+8])|(std::to_integer<unsigned>(bytes[p+9])<<8);
            if(result.colors!=palette(p+10+1024) || result.already_black!=black)
                throw std::runtime_error("Native fade-out colors or completion counter mismatch at frame "+std::to_string(entry));
            if(black==512) ++completed;
        } else {
            const auto threshold=std::to_integer<unsigned>(bytes[p+8]);
            const auto result=assets::advance_palette_fade(palette(p+9),palette(p+9+1024),threshold);
            if(result!=palette(p+9+2048)) throw std::runtime_error("Native palette fade mismatch at frame "+std::to_string(entry));
            thresholds[threshold]=true;
        }
        ++steps;
        if(entry!=end) ++cross_frame;
    }
    std::cout<<"PASS: "<<steps<<" native palette "<<(fade_out?"fade-out":"fade-in")<<" steps / "<<steps*512<<" color words; "
        <<cross_frame<<" steps cross frame boundaries.\n";
    if(fade_out) std::cout<<"PASS: all already-black counters match; "<<completed<<" completion sweeps.\n";
    else std::cout<<"Verified "<<std::count(thresholds.begin(),thresholds.end(),true)<<" thresholds.\n";
    std::cout<<"Palette upload timing remains separate.\n";
}
void verify_background_trace(const std::filesystem::path& rom,const std::filesystem::path& trace) {
    const assets::ActorAssets assets(core::read_file(rom));
    const auto bytes=core::read_file(trace);
    constexpr std::size_t record_size=8+1+8192+65536*2+1;
    if(bytes.size()<=8 || (bytes.size()-8)%record_size)
        throw std::runtime_error("Invalid, empty or truncated background trace");
    const std::string_view schema(reinterpret_cast<const char*>(bytes.data()),8);
    const bool pattern=schema=="ACPT0001";
    if(!pattern && schema!="ACBG0001") throw std::runtime_error("Unsupported VRAM update trace schema");
    auto u32=[&](std::size_t p){std::uint32_t v=0;for(unsigned s=0;s<32;s+=8) v|=std::to_integer<std::uint32_t>(bytes[p++])<<s;return v;};
    std::size_t passes=0,words=0,cross_frame=0;
    std::array<bool,256> descriptors{};
    std::uint32_t previous_end=0;
    for(std::size_t p=8;p<bytes.size();p+=record_size) {
        const auto entry=u32(p),end=u32(p+4);
        if(!entry || entry<previous_end || end<entry) throw std::runtime_error("Invalid background trace frame order");
        previous_end=end;
        const auto actor=std::to_integer<unsigned>(bytes[p+8]);
        if(actor>=64) throw std::runtime_error("Invalid background actor index");
        const auto ram=std::span(bytes).subspan(p+9,8192);
        const auto id=std::to_integer<std::uint8_t>(ram[0x1800+actor]);
        const auto high=(std::to_integer<unsigned>(ram[0x1a00+actor])&0x20)?std::to_integer<std::uint8_t>(ram[0x1a80+actor]):std::uint8_t{0};
        std::vector<assets::BackgroundWrite> writes;
        if(pattern) {
            if(!(std::to_integer<unsigned>(ram[0x1540+actor])&2)) throw std::runtime_error("Pattern trace missing pending flag");
            const auto base=static_cast<std::uint16_t>(std::to_integer<unsigned>(ram[0x2e])|(std::to_integer<unsigned>(ram[0x2f])<<8));
            const auto transfer=assets.pattern_transfer(std::to_integer<std::uint8_t>(ram[0x1500+actor]),
                std::to_integer<std::uint8_t>(ram[0x1a80+actor]),base);
            for(std::size_t i=0;i<transfer.words.size();++i)
                writes.push_back({static_cast<std::uint16_t>(transfer.destination+i),transfer.words[i]});
        } else writes=assets.background(id,high);
        auto expected=std::vector<std::byte>(bytes.begin()+p+9+8192,bytes.begin()+p+9+8192+65536);
        for(const auto& write:writes) {
            // Captures establish normal one-word VDC increment. Unsupported
            // upper-half VRAM addresses are rejected rather than guessed.
            if(write.address>=32768) throw std::runtime_error("Background write outside captured VRAM");
            expected[write.address*2]=static_cast<std::byte>(write.value&255);
            expected[write.address*2+1]=static_cast<std::byte>(write.value>>8);
            ++words;
        }
        const auto actual=std::span(bytes).subspan(p+9+8192+65536,65536);
        if(!std::equal(expected.begin(),expected.end(),actual.begin()))
            throw std::runtime_error("Background VRAM mismatch at entry frame "+std::to_string(entry)+" descriptor "+std::to_string(id));
        const auto flags=std::to_integer<unsigned>(ram[0x1540+actor])&(pattern?0xfd:0xef);
        if(std::to_integer<unsigned>(bytes[p+record_size-1])!=flags)
            throw std::runtime_error("Background dirty-flag acknowledgement mismatch");
        descriptors[pattern?std::to_integer<std::uint8_t>(ram[0x1a80+actor]):id]=true; ++passes;
        if(entry!=end) ++cross_frame;
    }
    std::cout<<"PASS: "<<passes<<(pattern?" dynamic pattern transfers, ":" background updates, ")<<words<<" native writes, "<<std::count(descriptors.begin(),descriptors.end(),true)
        <<(pattern?" stream selectors":" descriptors")<<"; all complete VRAM snapshots and dirty-flag acknowledgements match. "<<cross_frame<<" updates cross a frame boundary.\n";
}
void verify_actor_trace(const std::filesystem::path& rom,const std::filesystem::path& trace) {
    const assets::ActorAssets assets(core::read_file(rom));
    const auto bytes=core::read_file(trace);
    constexpr std::size_t record_size=8+8192+512+1;
    constexpr std::size_t header_size=8;
    if(bytes.size()<=header_size || (bytes.size()-header_size)%record_size)
        throw std::runtime_error("Invalid, empty or truncated actor trace");
    const std::string_view magic(reinterpret_cast<const char*>(bytes.data()),header_size);
    const bool overlay=magic=="ACOV0001";
    if(!overlay && magic!="ACAT0001") throw std::runtime_error("Unsupported actor trace schema");
    auto u32=[&](std::size_t p){std::uint32_t v=0;for(unsigned s=0;s<32;s+=8) v|=std::to_integer<std::uint32_t>(bytes[p++])<<s;return v;};
    std::size_t passes=0,words=0,cross_frame=0,cleared_words=0,skipped=0;
    std::uint32_t previous_end=0;
    for(std::size_t p=magic.size();p<bytes.size();p+=record_size) {
        const auto entry=u32(p),end=u32(p+4);
        if(!entry || entry<previous_end || end<entry) throw std::runtime_error("Invalid actor trace frame order");
        previous_end=end;
        const auto ram=std::span(bytes).subspan(p+8,8192);
        const auto first_slot=std::to_integer<unsigned>(ram[overlay?0x2d:0x50]);
        std::vector<assets::TableSprite> sprites;
        if(overlay) sprites=assets::assemble_table_overlay(ram,first_slot);
        else for(const auto& sprite:assets::assemble_table_actors(assets,ram)) sprites.push_back({sprite.slot,sprite.words});
        if(sprites.empty()) {
            if(!overlay) throw std::runtime_error("Empty actor trace candidate at frame "+std::to_string(entry));
            ++skipped;
        }
        const auto end_slot=std::to_integer<unsigned>(bytes[p+record_size-1]);
        if(end_slot!=first_slot+sprites.size())
            throw std::runtime_error("Actor trace end-slot mismatch at frame "+std::to_string(entry));
        for(const auto& sprite:sprites) for(std::size_t w=0;w<4;++w) {
            const auto offset=p+8+8192+(sprite.slot*4+w)*2;
            const auto expected=std::to_integer<unsigned>(bytes[offset])|(std::to_integer<unsigned>(bytes[offset+1])<<8);
            if(sprite.words[w]!=expected) throw std::runtime_error("Actor trace mismatch at entry frame "+std::to_string(entry)+
                " slot "+std::to_string(sprite.slot)+" word "+std::to_string(w)+" native "+std::to_string(sprite.words[w])+" source "+std::to_string(expected));
            ++words;
        }
        if(overlay) for(unsigned slot=end_slot;slot<64;++slot) for(unsigned w=0;w<4;++w) {
            const auto offset=p+8+8192+(slot*4+w)*2;
            if(bytes[offset]!=std::byte{0} || bytes[offset+1]!=std::byte{0})
                throw std::runtime_error("Uncleared trailing SAT word at frame "+std::to_string(end));
            ++cleared_words;
        }
        ++passes;
        if(entry!=end) ++cross_frame;
    }
    std::cout<<"PASS: "<<passes<<" traced "<<(overlay?"table overlay":"main actor")<<" passes, "<<words<<" sprite words and all end slots match; "
        <<cross_frame<<" passes cross a frame boundary.\n";
    if(overlay) std::cout<<"PASS: "<<cleared_words<<" trailing SAT words cleared; "<<skipped<<" guarded overlay passes emit no sprites.\n";
    std::cout<<"Only the selected pass is checked; HUD, background writes and gameplay remain outside this check.\n";
}
void verify_discovery(const std::filesystem::path& rom,const std::filesystem::path& report) {
    const auto image=core::read_file(rom);
    const auto tables=assets::decode_table_backgrounds(image);
    const auto sprite_tables=assets::decode_table_sprites(image);
    const auto hud=assets::decode_hud_assets(image);
    auto runtime=make_reference_runtime(); runtime->load(rom);
    const assets::ActorAssets actor_assets(image);
    // The 17 recovered streams all replace six 16x16 sprite patterns.
    for(std::uint8_t selection=0;selection<17;++selection) {
        const auto left=actor_assets.pattern_transfer(26,selection,0x200);
        const auto right=actor_assets.pattern_transfer(27,selection,0x200);
        if(left.destination!=0x7c80 || right.destination!=0x7e80 || left.words.size()!=384 || left.words!=right.words)
            throw std::runtime_error("Dynamic sprite stream layout mismatch");
    }
    if(actor_assets.pattern_transfer(26,0,0xffff).destination!=0x3c60)
        throw std::runtime_error("Dynamic sprite destination wrapping mismatch");
    auto rejects_pattern=[&](std::uint8_t type,std::uint8_t selection,std::uint16_t base) {
        try {(void)actor_assets.pattern_transfer(type,selection,base);} catch(const std::runtime_error&) {return;}
        throw std::runtime_error("Invalid dynamic sprite selector/address accepted");
    };
    rejects_pattern(80,0,0); rejects_pattern(26,17,0); rejects_pattern(26,0,0x400);
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
    struct ActorEpoch {
        std::uint64_t frame;
        std::uint8_t table;
        std::vector<assets::ActorSprite> sprites;
    };
    std::deque<ActorEpoch> actor_history;
    std::array<std::uint16_t,256> previous_source{};
    std::array<std::size_t,4> lag_exact{},lag_words{},lag_bad{};
    std::size_t epoch_frames=0,held_frames=0,unmatched_frames=0;
    std::size_t held_run=0,longest_hold=0,lag1_held_bad=0,lag1_changed_bad=0;
    std::array<std::size_t,4> lag1_field_bad{};
    std::size_t actor_checks=0,actor_bad=0,actor_frames=0;
    std::size_t verified_actor_words=0;
    std::ofstream actor_report;
    std::ofstream epoch_report;
    if(!report.empty()) {
        if(std::filesystem::exists(report)) throw std::runtime_error("Discovery report requires a new directory");
        std::filesystem::create_directories(report);
        actor_report.exceptions(std::ios::failbit|std::ios::badbit);
        actor_report.open(report/"actor-comparison.csv");
        actor_report<<"ram_frame,source_sat_frame,table,actor,descriptor,slot,native_y,native_x,native_pattern,native_flags,source_y,source_x,source_pattern,source_flags,matching_words\n";
        epoch_report.exceptions(std::ios::failbit|std::ios::badbit);
        epoch_report.open(report/"actor-epochs.csv");
        epoch_report<<"source_sat_frame,ram_frame,lag,source_table,ram_table,pc,pc_bank,source_sat_held,held_run,sprites,compared_words,bad_y,bad_x,bad_pattern,bad_flags,exact\n";
    }
    std::size_t hud_checks=0,native_hud_words=0;
    for(std::uint64_t n=0;n<1800;++n) {
        (void)runtime->advance(recording::canonical_input(n));
        if(n<359) continue;
        const auto observed=observe_alien_state(runtime->work_ram());
        const auto snapshot=decode_reference_snapshot(runtime->save());
        std::vector<assets::ActorSprite> actors;
        try { if(n>=479) actors=assets::assemble_table_actors(actor_assets,runtime->work_ram()); }
        catch(const std::exception& e) { throw std::runtime_error(std::string(e.what())+" frame "+std::to_string(n+1)); }
        if(n>=479) {
            actor_history.push_front({n+1,observed.table_code,actors});
            if(actor_history.size()>4) actor_history.pop_back();
            if(actor_history.size()==4) {
                ++epoch_frames;
                const bool held=std::equal(previous_source.begin(),previous_source.end(),snapshot.vram.begin()+0x400);
                if(held) ++held_frames;
                held_run=held?held_run+1:0;
                longest_hold=std::max(longest_hold,held_run);
                bool any_exact=false;
                // Report every candidate on the same interval. Choosing the best lag
                // per frame would conceal timing defects and is not an equality gate.
                for(std::size_t lag=0;lag<actor_history.size();++lag) {
                    const auto& epoch=actor_history[lag];
                    std::array<std::size_t,4> bad{};
                    for(const auto& sprite:epoch.sprites) for(std::size_t w=0;w<4;++w)
                        if(sprite.words[w]!=snapshot.vram[0x400+sprite.slot*4+w]) ++bad[w];
                    const auto total=bad[0]+bad[1]+bad[2]+bad[3];
                    const bool exact=!epoch.sprites.empty() && total==0;
                    lag_words[lag]+=epoch.sprites.size()*4; lag_bad[lag]+=total;
                    if(exact) {++lag_exact[lag];any_exact=true;}
                    if(lag==1) {
                        for(std::size_t w=0;w<4;++w) lag1_field_bad[w]+=bad[w];
                        if(!exact) {if(held) ++lag1_held_bad;else ++lag1_changed_bad;}
                    }
                    if(epoch_report.is_open()) epoch_report<<n+1<<','<<epoch.frame<<','<<lag<<','
                        <<static_cast<unsigned>(observed.table_code)<<','<<static_cast<unsigned>(epoch.table)<<','
                        <<snapshot.pc<<','<<static_cast<unsigned>(snapshot.mpr[snapshot.pc>>13])<<','<<held<<','<<held_run<<','<<epoch.sprites.size()<<','
                        <<epoch.sprites.size()*4<<','<<bad[0]<<','<<bad[1]<<','<<bad[2]<<','<<bad[3]<<','<<exact<<'\n';
                }
                if(!any_exact) ++unmatched_frames;
            }
            std::copy_n(snapshot.vram.begin()+0x400,256,previous_source.begin());
        }
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
            if(!std::equal(hud.patterns.begin(),hud.patterns.end(),snapshot.vram.begin()+0x500))
                throw std::runtime_error("Native HUD artwork mismatch at frame "+std::to_string(n+1));
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
            if(observed.ball_counter>=1 && observed.ball_counter<=10) {
                for(const auto& sprite:assets::assemble_table_hud(observed.score_points,observed.ball_counter))
                    if(!std::equal(sprite.words.begin(),sprite.words.end(),snapshot.vram.begin()+0x400+sprite.slot*4))
                        throw std::runtime_error("Native HUD sprite mismatch at frame "+std::to_string(n+1));
                native_hud_words+=40;
            }
            ++hud_checks;
        }
    }
    if(hud_checks<1000) throw std::runtime_error("Insufficient discovery HUD coverage");
    if(actor_report.is_open()) {
        actor_report.close();
        epoch_report.close();
        std::ofstream manifest; manifest.exceptions(std::ios::failbit|std::ios::badbit); manifest.open(report/"actor-manifest.txt");
        manifest<<"schema=table-actors-v1\nrom_sha256="<<rom::kSupportedSha256<<"\nbackend="<<runtime->save().core_revision
            <<"\nroute=canonical\nram_frames=480..1799\nsource_sat_frames=481..1800\ncompared_words="<<actor_checks
            <<"\nmismatching_words="<<actor_bad<<"\nexact_frames="<<actor_frames<<"\nsampled_verified_words="<<verified_actor_words
            <<"\nverified_source_frames=600,720\nverified_ram_frames=599,719\n"
            <<"preview=previous-frame RAM; native initial patterns; current reference palette; sprite-only layer\n"
            <<"limits=main-table actor pass only; no HUD, BAT writes, later sprite pass or scanline limits; timing differences unresolved\n";
        manifest.close();
        manifest.open(report/"actor-epoch-manifest.txt");
        manifest<<"schema=actor-epochs-v1\nrom_sha256="<<rom::kSupportedSha256<<"\nbackend="<<runtime->save().core_revision
            <<"\nroute=canonical\nsource_sat_frames=483..1800\nframes="<<epoch_frames<<"\nheld_source_sat_frames="<<held_frames
            <<"\nframes_without_exact_candidate="<<unmatched_frames<<'\n';
        manifest<<"longest_source_sat_hold="<<longest_hold<<"\nlag1_inexact_held_frames="<<lag1_held_bad
            <<"\nlag1_inexact_changed_frames="<<lag1_changed_bad<<"\nlag1_bad_y="<<lag1_field_bad[0]
            <<"\nlag1_bad_x="<<lag1_field_bad[1]<<"\nlag1_bad_pattern="<<lag1_field_bad[2]<<"\nlag1_bad_flags="<<lag1_field_bad[3]<<'\n';
        for(std::size_t lag=0;lag<4;++lag) manifest<<"lag"<<lag<<"_exact_frames="<<lag_exact[lag]
            <<"\nlag"<<lag<<"_compared_words="<<lag_words[lag]<<"\nlag"<<lag<<"_mismatching_words="<<lag_bad[lag]<<'\n';
        manifest<<"limits=observational lag sweep, not runtime phase detection; exact means nonempty candidate slots only; no missing/extra sprite or scene-validity claim\n";
        manifest.close();
    }
    std::cout<<"Actor epoch sweep: "<<epoch_frames<<" frames, "<<held_frames<<" held source SAT frames, "<<unmatched_frames<<" without an exact lag 0..3 candidate.\n";
    for(std::size_t lag=0;lag<4;++lag) std::cout<<"  lag "<<lag<<": "<<lag_exact[lag]<<" exact frames, "<<lag_bad[lag]<<'/'<<lag_words[lag]<<" mismatching words.\n";
    std::cout<<"Actor epoch diagnostic (previous RAM vs current source SAT): "<<actor_checks<<" words, "<<actor_bad
        <<" mismatches, "<<actor_frames<<"/1320 exact frames; full actor equivalence remains open.\n";
    std::cout<<"PASS: both ROM-decoded background and sprite pattern sets exactly match reference VRAM; "<<hud_checks<<" score/ball HUD checks.\n";
    std::cout<<"PASS: "<<verified_actor_words<<" native actor SAT words match at lower/upper source frames 600/720; arithmetic and capacity edges verified.\n";
    std::cout<<"PASS: "<<native_hud_words<<" native HUD sprite words match; all 768 HUD pattern words match at both table samples.\n";
}
}
