// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
#include "finite_animation_checks.hpp"
void preview(const render::Image& image,const std::string& name) {
    std::filesystem::create_directories("inactive-previews");
    std::ofstream out("inactive-previews/"+name+".bmp",std::ios::binary);
    const auto word=[&](unsigned value,unsigned bytes) {
        for(unsigned i=0;i<bytes;++i) out.put(static_cast<char>(value>>(8*i)));
    };
    out<<"BM";word(54+256*456*4,4);word(0,4);word(54,4);word(40,4);
    word(256,4);word(456,4);word(1,2);word(32,2);word(0,4);word(256*456*4,4);
    for(unsigned i=0;i<4;++i) word(0,4);
    for(int y=455;y>=0;--y) for(int x=0;x<256;++x) word(image.pixels[y*256+x],4);
    require(bool(out),"Cannot write animation preview");
}
void seam_continuity(std::span<const std::byte> rom,const diagnostics::ReferenceSnapshot& video,std::span<const std::byte> ram) {
    // Run identical visual time with and without repeated bank
    // swaps. Neither actor phase nor the tentacle palette may jump
    // when the active viewport changes. Exercise cabinet rendering,
    // not just the oscillator in isolation.
    auto fixed=std::make_unique<render::TablePlayfield>(rom);
    auto crossing=std::make_unique<render::TablePlayfield>(rom);
    render::TableTransitionHold fixed_hold,crossing_hold;
    struct Fixture {
        std::array<assets::TableBackground,2> backgrounds;
        std::array<assets::TableSprites,2> sprite_assets;
        diagnostics::ReferenceSnapshot fixed_video,crossing_video;
        std::optional<diagnostics::ReferenceSnapshot> fixed_previous,crossing_previous;
        Fixture(std::span<const std::byte> rom,const diagnostics::ReferenceSnapshot& video)
            :backgrounds(assets::decode_table_backgrounds(rom)),sprite_assets(assets::decode_table_sprites(rom)),
             fixed_video(video),crossing_video(video) {}
    };
    auto fixture=std::make_unique<Fixture>(rom,video);
    const auto& backgrounds=fixture->backgrounds;
    const auto& sprite_assets=fixture->sprite_assets;
    std::array<std::byte,8192> seam_ram{};
    std::copy(ram.begin(),ram.end(),seam_ram.begin());
    std::fill(seam_ram.begin()+0x1500,seam_ram.begin()+0x1d00,std::byte{});
    std::fill(seam_ram.begin()+0x200,seam_ram.begin()+0xa00,std::byte{});
    std::fill(seam_ram.begin()+0x1245,seam_ram.begin()+0x1445,std::byte{});
    seam_ram[0x79]=std::byte{1};seam_ram[0x70]=std::byte{};
    seam_ram[0x53]=seam_ram[0x54]=seam_ram[0xf6]=std::byte{};
    // Upper-right tentacles, type 34, in their ordinary waving loop.
    constexpr unsigned tentacle=26;
    for(unsigned field=0;field<32;++field)
        seam_ram[0x1500+field*64+tentacle]=ram[0x1500+field*64+tentacle];
    require(seam_ram[0x1500+tentacle]==std::byte{34},"Missing real tentacle fixture");
    // A lower-table oscillator must survive the same crossings independently.
    seam_ram[0x204]=std::byte{33};seam_ram[0x244]=std::byte{0x88};
    seam_ram[0x2c4]=std::byte{228};seam_ram[0x384]=std::byte{168};
    seam_ram[0x504]=std::byte{143};seam_ram[0x544]=std::byte{1};
    seam_ram[0x744]=std::byte{6};seam_ram[0x884]=std::byte{0x20};
    // Lower green alien, including a complete movement cycle across swaps.
    seam_ram[0x205]=std::byte{21};seam_ram[0x245]=std::byte{0xc0};
    seam_ram[0x2c5]=std::byte{48};seam_ram[0x385]=std::byte{152};
    seam_ram[0x505]=std::byte{96};seam_ram[0x585]=std::byte{1};
    seam_ram[0x705]=std::byte{0x21};seam_ram[0x745]=std::byte{4};
    seam_ram[0x785]=std::byte{3};
    seam_ram[0x885]=std::byte{0x10};
    for(unsigned field=0;field<8;++field)
        seam_ram[0x1245+field*64+51]=ram[0x1245+field*64+51];
    for(unsigned slot:{34u,35u}) for(unsigned field=0;field<8;++field)
        seam_ram[0x1245+field*64+slot]=ram[0x1245+field*64+slot];
    require(seam_ram[0x1245+34]==std::byte{6} && seam_ram[0x1245+35]==std::byte{6},
        "Missing fleshy-border palette scripts");
    require(seam_ram[0x1278]==std::byte{1},"Missing tentacle palette script");
    auto& fixed_video=fixture->fixed_video;auto& crossing_video=fixture->crossing_video;
    auto& fixed_previous=fixture->fixed_previous;auto& crossing_previous=fixture->crossing_previous;
    std::optional<render::Image> first_tentacles;
    unsigned changed_tentacles=0,changed_flesh=0,changed_green=0;
    for(unsigned tick=0;tick<640;++tick) {
        auto swapped=seam_ram;
        const unsigned half=(tick/17)%2;
        if(!half) for(unsigned field=0;field<32;++field) for(unsigned x=4;x<64;++x)
            std::swap(swapped[0x1500+field*64+x],swapped[0x200+field*64+x]);
        swapped[0x79]=static_cast<std::byte>(half);
        const auto prepare=[&](auto& snapshot,unsigned h) {
            snapshot.frame=2000+tick;snapshot.vram=backgrounds[h].vram;
            std::copy(sprite_assets[h].vram.begin()+0x4000,sprite_assets[h].vram.end(),snapshot.vram.begin()+0x4000);
            snapshot.palette=backgrounds[h].palette;snapshot.sat.fill(0);
            snapshot.control=0xc0;snapshot.memory_width=0;snapshot.scroll_x=snapshot.scroll_y=0;
        };
        prepare(fixed_video,1);prepare(crossing_video,half);
        const auto stationary=fixed->cabinet_frame(fixed_video,fixed_previous?&*fixed_previous:nullptr,seam_ram,nullptr,fixed_hold);
        const auto moving=crossing->cabinet_frame(crossing_video,crossing_previous?&*crossing_previous:nullptr,swapped,nullptr,crossing_hold);
        require(stationary && moving,"Bank swap lost cabinet image");
        if(stationary->pixels!=moving->pixels) {
            preview(*stationary,"seam-expected");preview(*moving,"seam-actual");
            throw std::runtime_error("Bank swap reset animation at tick "+std::to_string(tick));
        }
        if(!first_tentacles) {first_tentacles=moving;preview(*moving,"tentacles-start");}
        else if(first_tentacles->pixels!=moving->pixels) ++changed_tentacles;
        // The left border must visibly cycle with the upper half inactive;
        // tentacle movement elsewhere must not satisfy this regression.
        if(!half) {
            bool changed=false;
            for(unsigned y=100;y<180;++y) for(unsigned x=0;x<40;++x)
                changed|=first_tentacles->pixels[y*256+x]!=moving->pixels[y*256+x];
            changed_flesh+=changed;
        }
        if(half && tick) {
            bool changed=false;
            for(unsigned y=264;y<328;++y) for(unsigned x=0;x<32;++x)
                changed|=first_tentacles->pixels[y*256+x]!=moving->pixels[y*256+x];
            changed_green+=changed;
        }
        if(tick==48) preview(*moving,"green-raised-offscreen");
        if(tick==8) preview(*moving,"flesh-offscreen-later");
        if(tick==24) preview(*moving,"tentacles-later");
        fixed_previous=fixed_video;crossing_previous=crossing_video;
    }
    require(changed_tentacles>100,"Tentacle geometry/palette froze during bank swaps");
    require(changed_flesh>40,"Upper-left fleshy border froze while upper table was inactive");
    require(changed_green>100,"Green alien pixels froze while lower table was inactive");
    assets::ActorAssets actor_assets(rom);
    // Palette scripts can also execute on the first frame after a bank swap.
    // Exercise real tentacle/flesh scripts with a resumed runtime update,
    // rather than only swapping two permanently frozen source banks.
    {
        auto held_storage=std::make_unique<std::array<std::byte,8192>>(seam_ram);
        auto& held=*held_storage;held[0x79]=std::byte{};
        auto retained=std::make_unique<render::TableAnimation>(rom);
        auto control=std::make_unique<render::TableAnimation>(rom);
        auto original_tick=std::make_unique<render::TableAnimation>(rom);
        retained->update(actor_assets,held,false);control->update(actor_assets,held,false);
        for(unsigned i=0;i<17;++i) {retained->update(actor_assets,held,true);control->update(actor_assets,held,true);}
        original_tick->update(actor_assets,seam_ram,false);original_tick->update(actor_assets,seam_ram,true);
        auto resumed_storage=std::make_unique<std::array<std::byte,8192>>(held);
        auto& resumed=*resumed_storage;resumed[0x79]=std::byte{1};
        for(unsigned slot:{34u,35u,51u}) for(unsigned field=0;field<8;++field) {
            const unsigned p=0x1245+field*64+slot;resumed[p]=original_tick->actors(1)[p];
        }
        retained->update(actor_assets,resumed,true);control->update(actor_assets,held,true);
        for(unsigned slot:{34u,35u,51u}) for(unsigned field=0;field<8;++field) {
            const unsigned p=0x1245+field*64+slot;
            require(retained->actors(1)[p]==control->actors(1)[p],"Resuming palette dispatch at seam reset its phase");
        }
    }
    auto timeline=std::make_unique<render::TableAnimation>(rom);
    timeline->update(actor_assets,seam_ram,false);
    for(unsigned i=0;i<11;++i) timeline->update(actor_assets,seam_ram,true);
    auto expected_lower=timeline->actors(0);actor_assets.advance_animation(expected_lower);
    auto hit=seam_ram;hit[0x1840+tentacle]=std::byte{3};
    timeline->update(actor_assets,hit,true);
    require(timeline->actors(1)[0x1840+tentacle]==std::byte{3},"Live hit failed to supersede cosmetic state");
    for(unsigned field=0;field<32;++field)
        require(timeline->actors(0)[0x1504+field*64]==expected_lower[0x1504+field*64],
            "Unrelated actor event reset lower animation");
    hit[0x1500+tentacle]=hit[0x1540+tentacle]=std::byte{};
    timeline->update(actor_assets,hit,true);
    require(timeline->actors(1)[0x1500+tentacle]==std::byte{},"Retained actor survived authoritative removal");
}
void cocoon_rendering(std::span<const std::byte> rom,const diagnostics::ReferenceSnapshot& video,
    std::span<const std::byte> ram) {
    require(ram[0x79]==std::byte{1},"Cocoon rendering requires ball in upper half");
    std::array<std::byte,8192> source{};
    std::copy(ram.begin(),ram.end(),source.begin());
    std::fill(source.begin()+0x1500,source.begin()+0x1d00,std::byte{});
    std::fill(source.begin()+0x1245,source.begin()+0x1445,std::byte{});
    source[0x53]=source[0x54]=source[0xf6]=std::byte{};
    unsigned found=0;
    for(unsigned x=4;x<64;++x) {
        const auto type=std::to_integer<unsigned>(source[0x200+x]);
        if(type!=26 && type!=27) {
            for(unsigned field=0;field<32;++field) source[0x200+x+64*field]=std::byte{};
            continue;
        }
        ++found;source[0x240+x]=std::byte{0xe8};source[0x540+x]=std::byte{3};
        source[0x580+x]=std::byte{180};source[0x5c0+x]=std::byte{1};source[0x740+x]=std::byte{17};
        source[0x880+x]=std::byte{0xc0};source[0x8c0+x]=std::byte{255};
        source[0x900+x]=std::byte{};source[0x940+x]=std::byte{16};
        const auto pointer=std::to_integer<unsigned>(rom[0x9df7+type-1])|
            (std::to_integer<unsigned>(rom[0x9e51+type-1])<<8);
        source[0x780+x]=rom[pointer-0x2000+16];
    }
    require(found==2,"Expected both original cocoon actors");
    const auto untouched=source;
    auto table=std::make_unique<render::TablePlayfield>(rom);
    auto current=std::make_unique<diagnostics::ReferenceSnapshot>(video);
    auto previous=std::make_unique<diagnostics::ReferenceSnapshot>(video);
    render::TableTransitionHold hold;
    std::optional<render::Image> hidden,restored;
    for(unsigned tick=0;tick<=185;++tick) {
        current->frame=6000+tick;
        auto image=table->cabinet_frame(*current,tick?previous.get():nullptr,source,nullptr,hold);
        require(image.has_value(),"Cocoon fixture lost cabinet view");
        if(tick==180) {hidden=image;preview(*image,"cocoons-hidden-upstairs");}
        if(tick==181) {restored=image;preview(*image,"cocoons-restored-upstairs");}
        if(tick>181) require(image->pixels==restored->pixels,"Restored cocoons must stay still without hits");
        *previous=*current;
    }
    unsigned left=0,right=0;
    for(unsigned y=240;y<456;++y) for(unsigned x=0;x<256;++x)
        if(hidden->pixels[y*256+x]!=restored->pixels[y*256+x]) ++(x<128?left:right);
    require(left>20 && right>20,"Both cocoons must visibly return while lower source bank remains frozen");
    require(source==untouched,"Cocoon presentation modified source bank");
}
void slingshot_rendering(std::span<const std::byte> rom,const diagnostics::ReferenceSnapshot& video,
    std::span<const std::byte> ram) {
    struct Artwork {
        std::array<assets::TableBackground,2> backgrounds;
        std::array<assets::TableSprites,2> patterns;
        explicit Artwork(std::span<const std::byte> image)
            :backgrounds(assets::decode_table_backgrounds(image)),patterns(assets::decode_table_sprites(image)) {}
    };
    const auto artwork=std::make_unique<Artwork>(rom);
    for(unsigned type:{52u,53u}) {
        auto fixed=std::make_unique<render::TablePlayfield>(rom),crossing=std::make_unique<render::TablePlayfield>(rom);
        render::TableTransitionHold fixed_hold,crossing_hold;
        auto a=std::make_unique<diagnostics::ReferenceSnapshot>(video),b=std::make_unique<diagnostics::ReferenceSnapshot>(video);
        auto previous_a=std::make_unique<diagnostics::ReferenceSnapshot>(video),previous_b=std::make_unique<diagnostics::ReferenceSnapshot>(video);
        std::array<std::byte,8192> source{};
        std::copy(ram.begin(),ram.end(),source.begin());
        std::fill(source.begin()+0x1500,source.begin()+0x1d00,std::byte{});
        std::fill(source.begin()+0x200,source.begin()+0xa00,std::byte{});
        std::fill(source.begin()+0x1245,source.begin()+0x1445,std::byte{});
        source[0x79]=source[0x53]=source[0x54]=source[0xf6]=std::byte{};
        source[0x1504]=static_cast<std::byte>(type);source[0x1544]=std::byte{0x84};
        source[0x15c4]=std::byte{160};source[0x1684]=std::byte{80};source[0x17c4]=std::byte{255};
        source[0x1844]=std::byte{1};source[0x1a44]=std::byte{3};source[0x1b84]=std::byte{32};
        const unsigned pointer=std::to_integer<unsigned>(rom[0x9df7+type-1])|
            (std::to_integer<unsigned>(rom[0x9e51+type-1])<<8);
        source[0x1804]=rom[pointer-0x2000];
        unsigned visible_above=0;
        for(unsigned tick=0;tick<180;++tick) {
            auto swapped=source;
            const unsigned half=(tick/13)%2;
            if(half) for(unsigned field=0;field<32;++field) for(unsigned x=4;x<64;++x)
                std::swap(swapped[0x1500+field*64+x],swapped[0x200+field*64+x]);
            swapped[0x79]=static_cast<std::byte>(half);
            const auto prepare=[&](auto& snapshot,unsigned h) {
                snapshot.frame=4000+tick;snapshot.vram=artwork->backgrounds[h].vram;
                std::copy(artwork->patterns[h].vram.begin()+0x4000,artwork->patterns[h].vram.end(),snapshot.vram.begin()+0x4000);
                snapshot.palette=artwork->backgrounds[h].palette;snapshot.sat.fill(0);snapshot.control=0xc0;
                snapshot.memory_width=0;snapshot.scroll_x=snapshot.scroll_y=0;
            };
            prepare(*a,0);prepare(*b,half);
            const auto stationary=fixed->cabinet_frame(*a,tick?previous_a.get():nullptr,source,nullptr,fixed_hold);
            const auto moving=crossing->cabinet_frame(*b,tick?previous_b.get():nullptr,swapped,nullptr,crossing_hold);
            require(stationary && moving && stationary->pixels==moving->pixels,
                "Slingshot rendered flight changed when ball crossed seam");
            if(tick==80 || tick==140) {
                auto empty=source;empty[0x1504]=empty[0x1544]=std::byte{};
                auto blank=std::make_unique<render::TablePlayfield>(rom);render::TableTransitionHold blank_hold;
                const auto background=blank->cabinet_frame(*a,nullptr,empty,nullptr,blank_hold);
                require(background.has_value(),"Missing slingshot background");
                unsigned changed=0;
                for(unsigned p=0;p<256*render::table_half_offset;++p)
                    changed+=stationary->pixels[p]!=background->pixels[p];
                require(changed>20,"Flying alien disappeared above seam");++visible_above;
                if(type==52 && tick==140) preview(*stationary,"slingshot-above-seam");
            }
            *previous_a=*a;*previous_b=*b;
        }
        require(visible_above==2,"Missing slingshot visibility checks");
    }
}
// A respawned silver orange alien (type 16, 02:60E1..6162) emerges
// 12 pixels on each axis over 48 ticks, then stays out without
// animating. A bank left mid-emergence must still finish it.
void check_silver_orange_alien(const assets::ActorAssets& actors,std::span<const std::byte> rom) {
    std::array<std::byte,8192> silver{};
    silver[0x1504]=std::byte{16};silver[0x1544]=std::byte{0xc0};
    silver[0x15c4]=std::byte{100};silver[0x1684]=std::byte{150};
    silver[0x1804]=std::byte{96};silver[0x1a04]=std::byte{0x23};
    silver[0x1a44]=std::byte{4};silver[0x1b84]=std::byte{0x10};
    silver[0x1704]=std::byte{0xc0};silver[0x1744]=std::byte{0xff};silver[0x1584]=std::byte{0xc0};
    silver[0x1784]=std::byte{0x40};silver[0x1884]=std::byte{48};
    auto emerging=silver;
    for(unsigned i=0;i<47;++i) actors.advance_animation(emerging);
    require(emerging[0x1844]==std::byte{} && emerging[0x1544]==std::byte{0xc0},"Silver alien emerged early");
    actors.advance_animation(emerging);
    require(emerging[0x1844]==std::byte{1} && emerging[0x1544]==std::byte{0x80} &&
        emerging[0x15c4]==std::byte{88} && emerging[0x1684]==std::byte{162},"Silver alien did not emerge from its hole");
    const auto out=emerging;
    for(unsigned i=0;i<600;++i) actors.advance_animation(emerging);
    require(emerging==out,"Emerged silver alien moved or animated while waiting for a hit");
    auto timeline=std::make_unique<render::TableAnimation>(rom);
    silver[0x1884]=std::byte{20};
    timeline->update(actors,silver,false);
    for(unsigned i=0;i<20;++i) timeline->update(actors,silver,true);
    require(timeline->actors(0)[0x1844]==std::byte{1} && timeline->actors(0)[0x1544]==std::byte{0x80},
        "Silver alien left mid-emergence stayed in its hole");
    silver[0x1544]=std::byte{};const auto uninitialized=silver;
    actors.advance_animation(silver);
    require(silver==uninitialized,"Uninitialized silver alien advanced");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        assets::ActorAssets actors(rom);
        finite_animation_checks(actors,rom);
        // ROM green-alien rise/wait/lower cycle: exactly 24 pixels each way,
        // randomized waits in the ROM ranges, no accumulated position drift.
        for(unsigned green_type:{21u,22u}) {
            std::array<std::byte,8192> green{};
            green[0x1504]=static_cast<std::byte>(green_type);
            green[0x1544]=std::byte{0xc0};green[0x1684]=std::byte{152};
            green[0x1804]=std::byte{96};green[0x1a44]=std::byte{4};
            green[0x1b84]=std::byte{0x10};green[0x1884]=std::byte{1};
            if(green_type==22) {
                green[0x1884]=std::byte{48};green[0x1784]=std::byte{0x80};green[0x17c4]=std::byte{0xff};
            } else actors.advance_animation(green);
            for(unsigned i=0;i<48;++i) actors.advance_animation(green);
            require(green[0x1684]==std::byte{128} && green[0x1644]==std::byte{} &&
                green[0x1544]==std::byte{0x80},"Green alien did not rise 24 pixels");
            if(green_type==22) {
                for(unsigned i=0;i<600;++i) actors.advance_animation(green);
                require(green[0x1684]==std::byte{128} && green[0x1844]==std::byte{1},
                    "Rise-and-stay green alien incorrectly lowered");
            } else {
                for(unsigned cycle=0;cycle<5;++cycle) {
                    const auto wait=std::to_integer<unsigned>(green[0x1884]);
                    require(wait>=120 && wait<240,"Green raised wait outside ROM distribution");
                    const auto descriptor=green[0x1804];bool changed=false;
                    for(unsigned i=0;i<wait;++i) {actors.advance_animation(green);changed|=green[0x1804]!=descriptor;}
                    require(changed,"Raised green alien sprite froze");
                    for(unsigned i=0;i<48;++i) actors.advance_animation(green);
                    require(green[0x1684]==std::byte{152} && green[0x1644]==std::byte{} &&
                        green[0x1844]==std::byte{},"Green alien cycle drifted or failed to lower");
                    auto idle_wait=std::to_integer<unsigned>(green[0x1884]);
                    if(!idle_wait) idle_wait=256; // DEC wraps zero, as in the ROM.
                    for(unsigned i=0;i<idle_wait+48;++i) actors.advance_animation(green);
                    require(green[0x1684]==std::byte{128},"Green alien failed to repeat its rise");
                }
            }
            green[0x1544]=std::byte{};const auto uninitialized=green;
            actors.advance_animation(green);
            require(green==uninitialized,"Uninitialized green actor advanced");
        }
        check_silver_orange_alien(actors,rom);
        // Type 33 initializes six frames at +$0020 (01:5B15..5B25).
        // Validate both FD35 reflection endpoints and the recovered sequence.
        std::array<std::byte,8192> projected{};
        const unsigned slot=4,type=33;
        const auto sequence=std::to_integer<unsigned>(rom[0x9df7+type-1])|
            (std::to_integer<unsigned>(rom[0x9e51+type-1])<<8);
        projected[0x1500+slot]=std::byte{type};projected[0x1540+slot]=std::byte{0x88};
        projected[0x1840+slot]=std::byte{1};projected[0x1a40+slot]=std::byte{6};
        projected[0x1b80+slot]=std::byte{0x20};projected[0x1800+slot]=rom[sequence-0x2000];
        for(unsigned i=0;i<8;++i) actors.advance_animation(projected);
        require(projected[0x1800+slot]==rom[sequence-0x2000+1],"ROM animation did not advance at its original rate");
        for(unsigned i=8;i<48;++i) actors.advance_animation(projected);
        require(projected[0x1c40+slot]==std::byte{5} && projected[0x1bc0+slot]==std::byte{255},"Upper animation reflection differs from FD35");
        for(unsigned i=0;i<40;++i) actors.advance_animation(projected);
        require(projected[0x1c40+slot]==std::byte{0} && projected[0x1bc0+slot]==std::byte{0},"Lower animation reflection differs from FD35");
        projected[0x1840+slot]=std::byte{0};const auto idle=projected;
        actors.advance_animation(projected);require(projected==idle,"Animation woke a collision-triggered idle actor");
        // Exercise every slot-specific red-eye sequence plus both related
        // indicator types. Opening must end in a stable hold, not endless flashes.
        for(unsigned indicator_type:{42u,45u,46u,47u}) for(unsigned eye=55;eye<64;++eye) {
            std::array<std::byte,8192> indicator{};
            const unsigned pointer=indicator_type==47?0xbf56+(eye-55)*3:
                std::to_integer<unsigned>(rom[0x9df7+indicator_type-1])|
                (std::to_integer<unsigned>(rom[0x9e51+indicator_type-1])<<8);
            const unsigned enabled=indicator_type==42?0x4a:(indicator_type==45?0x4d:(indicator_type==46?0x4c:0x4e));
            const unsigned release=indicator_type==42?0x57:(indicator_type==45?0x60:(indicator_type==46?0x5d:0x63));
            const unsigned target=indicator_type==42?0x59:release+1;
            indicator[0x1500+eye]=static_cast<std::byte>(indicator_type);
            indicator[0x1540+eye]=std::byte{0xc0};indicator[0x1a40+eye]=std::byte{3};
            indicator[0x1bc0+eye]=std::byte{1};indicator[0x1840+eye]=std::byte{1};
            indicator[0x1800+eye]=rom[pointer-0x2000];indicator[enabled]=std::byte{1};
            for(unsigned tick=0;tick<4;++tick) {
                actors.advance_animation(indicator);
                require(indicator[0x1800+eye]==rom[pointer-0x2000+std::min(tick+1,2u)],
                    "Indicator opening uses another eye's descriptor or wrong phase");
            }
            require(indicator[0x1840+eye]==std::byte{2},"Indicator opening did not enter hold");
            const auto held=indicator;
            for(unsigned tick=0;tick<240;++tick) actors.advance_animation(indicator);
            require(indicator==held,"Inactive indicator flashes instead of holding its lit pose");
            indicator[release]=std::byte{0x80};indicator[target]=static_cast<std::byte>(eye-1);
            actors.advance_animation(indicator);
            require(indicator[0x1840+eye]==std::byte{2},"Another indicator's release closed this eye");
            indicator[target]=static_cast<std::byte>(eye);
            const auto event=indicator[release];actors.advance_animation(indicator);
            require(indicator[0x1840+eye]==std::byte{3} && indicator[release]==event,
                "Indicator release was ignored or consumed gameplay signal");
            for(unsigned tick=0;tick<3;++tick) actors.advance_animation(indicator);
            require(indicator[0x1840+eye]==std::byte{} && indicator[0x1800+eye]==rom[pointer-0x2000],
                "Inactive indicator did not close to its own resting descriptor");
            const auto resting=indicator;
            for(unsigned tick=0;tick<240;++tick) actors.advance_animation(indicator);
            require(indicator==resting,"Inactive resting eye restarted its flash");
            indicator=held;indicator[enabled]=std::byte{};actors.advance_animation(indicator);
            require(indicator[0x1840+eye]==std::byte{3},"Expired indicator group did not release held eye");
        }
        for(unsigned orb_type:{70u,71u}) {
            std::array<std::byte,8192> orb{};
            orb[0x1504]=static_cast<std::byte>(orb_type);orb[0x1544]=std::byte{0xe0};
            orb[0x1884]=std::byte{120};orb[0x1684]=std::byte{160};
            orb[0x1b44]=std::byte{0xe0};orb[0x19c4]=std::byte{1};orb[0x1804]=std::byte{42};
            for(unsigned i=0;i<119;++i) actors.advance_animation(orb);
            require((orb[0x1544]&std::byte{0x20})!=std::byte{},"Orb respawn timer ended early");
            actors.advance_animation(orb);
            require(orb[0x1844]==std::byte{1} && orb[0x1544]==std::byte{0x80},"Hidden orb never respawned");
            unsigned moved=0;
            for(unsigned i=0;i<600;++i) {
                actors.advance_animation(orb);
                const auto y=std::to_integer<unsigned>(orb[0x1684]);
                require(y>=150 && y<=160 && orb[0x16c4]==std::byte{},"Cosmetic orb drifted out of its bobbing range");
                if(y!=160) ++moved;
            }
            require(moved>100,"Respawned orb did not bob");
        }
        std::array<std::byte,8192> brain_orb{};
        brain_orb[0x1504]=std::byte{19};brain_orb[0x1544]=std::byte{0x80};brain_orb[0x1a04]=std::byte{0x16};
        brain_orb[0x1884]=std::byte{1};brain_orb[0x15c4]=std::byte{50};brain_orb[0x1684]=std::byte{100};
        unsigned brain_moves=0;
        for(unsigned i=0;i<600;++i) {
            const auto old=brain_orb;actors.advance_animation(brain_orb);
            if(old[0x1584]!=brain_orb[0x1584]) ++brain_moves;
            const auto x=std::to_integer<unsigned>(brain_orb[0x15c4]);
            require(x>=48 && x<=52 && brain_orb[0x1604]==std::byte{} && brain_orb[0x1684]==std::byte{100},
                "Brain orb wobble drifted away from its anchor");
        }
        require(brain_moves>30 && brain_moves<300,"Brain orb must alternate wobble bursts with substantial rests");
        brain_orb[0x1844]=std::byte{2};brain_orb[0x1a04]=std::byte{0x5f};
        brain_orb[0x1b44]=std::byte{0xe0};brain_orb[0x19c4]=std::byte{1};
        for(unsigned i=0;i<32;++i) actors.advance_animation(brain_orb);
        require(brain_orb[0x1844]==std::byte{} && brain_orb[0x1684]==std::byte{100} &&
            brain_orb[0x1a04]==std::byte{0x16},"Saved brain orb hop did not land and resume horizontal motion");
        std::array<std::byte,8192> orange{};
        orange[0x1504]=std::byte{15};orange[0x1544]=std::byte{0xc0};orange[0x1844]=std::byte{1};
        orange[0x1884]=std::byte{1};orange[0x15c4]=std::byte{210};orange[0x1684]=std::byte{100};
        actors.advance_animation(orange);
        for(unsigned i=0;i<32;++i) actors.advance_animation(orange);
        require(orange[0x15c4]==std::byte{202} && orange[0x1684]==std::byte{108},"Orange alien did not emerge diagonally");
        for(unsigned i=0;i<212;++i) actors.advance_animation(orange);
        require(orange[0x15c4]==std::byte{210} && orange[0x1684]==std::byte{100} && orange[0x1844]==std::byte{1},"Orange alien did not return to its starting position");
        std::array<std::byte,8192> explosion{};
        explosion[0x1504]=std::byte{60};explosion[0x1544]=std::byte{0xc4};
        explosion[0x1a44]=std::byte{3};explosion[0x1b84]=std::byte{0x40};explosion[0x1804]=std::byte{158};
        for(unsigned i=0;i<20;++i) actors.advance_animation(explosion);
        require(explosion[0x1504]==std::byte{} && explosion[0x1544]==std::byte{},"Saved orb explosion did not finish");
        for(unsigned half=0;half<2;++half) for(unsigned flipper=2;flipper<=3;++flipper) for(unsigned pose=0;pose<4;++pose) {
            std::array<std::byte,8192> live{},other{};
            live[0x79]=static_cast<std::byte>(half);
            live[0x1504]=other[0x1504]=static_cast<std::byte>(flipper);
            live[0x1804]=rom[0x9eaf+(flipper-2)*8+half*4+pose];
            actors.mirror_flippers(other,live,1-half);
            require(other[0x1804]==rom[0x9eaf+(flipper-2)*8+(1-half)*4+pose],"Inactive flipper used the wrong table pose");
        }

        auto table=std::make_unique<render::TablePlayfield>(rom);
        auto palette_probe=std::make_unique<render::TableAnimation>(rom);
        auto runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(path);runtime->observe_display(true);
        const auto initial=runtime->save();
        std::vector<std::string> fingerprints;
        std::optional<diagnostics::ReferenceSnapshot> previous;
        std::optional<render::Image> last;
        std::array<std::byte,8192> last_ram{};
        std::array<unsigned,2> animated{};
        render::TableTransitionHold hold;
        unsigned ball_pixels=0,mouth_pixels=0,orb_frames=0,brain_frames=0,palette_frames=0,flesh_frames=0,green_frames=0;
        for(unsigned n=0;n<1800;++n) {
            const auto frame=runtime->advance(recording::canonical_input(n));
            fingerprints.push_back(recording::fingerprint(frame,runtime->work_ram()));
            const auto video=runtime->video_snapshot();const auto ram=runtime->work_ram();
            const auto before=runtime->save();
            if(n && ram[0x79]==std::byte{} && last_ram[0x79]==std::byte{}) {
                auto predicted=last_ram;actors.advance_animation(predicted);
                for(unsigned x=4;x<64;++x) {
                    if(last_ram[0x1500+x]!=std::byte{21} || ram[0x1500+x]!=std::byte{21} ||
                        (last_ram[0x1540+x]&std::byte{0x80})==std::byte{} ||
                        (last_ram[0x1840+x]!=std::byte{1} && last_ram[0x1840+x]!=std::byte{3})) continue;
                    // Render frames can run without dispatching actors (e.g.
                    // transition waits). Compare actual movement updates only.
                    if(ram[0x1880+x]==last_ram[0x1880+x]) continue;
                    for(unsigned p:{0x1640u,0x1680u,0x16c0u,0x1840u})
                        if(predicted[p+x]!=ram[p+x]) throw std::runtime_error("Green ROM mismatch frame "+
                            std::to_string(n+1)+" slot "+std::to_string(x)+" field "+std::to_string(p)+
                            " expected "+std::to_string(std::to_integer<unsigned>(predicted[p+x]))+
                            " actual "+std::to_string(std::to_integer<unsigned>(ram[p+x])));
                    if(last_ram[0x1880+x]!=std::byte{1})
                        require(predicted[0x1880+x]==ram[0x1880+x],"Green movement timer differs from ROM");
                    ++green_frames;
                }
            }
            if(n && ram[0x79]==std::byte{1} && last_ram[0x79]==std::byte{1}) {
                for(unsigned palette_slot:{34u,35u}) {
                    if(ram[0x1245+palette_slot]!=std::byte{6} || last_ram[0x1245+palette_slot]!=std::byte{6} ||
                        ram[0x1285+palette_slot]!=std::byte{} || last_ram[0x1285+palette_slot]!=std::byte{} ||
                        ram[0x1385+palette_slot]==last_ram[0x1385+palette_slot]) continue;
                    auto offscreen=last_ram;offscreen[0x79]=std::byte{};
                    palette_probe->reset();palette_probe->update(actors,offscreen,false);
                    palette_probe->update(actors,offscreen,true);
                    for(unsigned field=0;field<8;++field) {
                        const unsigned p=0x1245+field*64+palette_slot;
                        require(palette_probe->actors(1)[p]==ram[p],"Fleshy-border palette differs from original F603 update");
                    }
                    ++flesh_frames;
                }
            }
            if(n && ram[0x79]==std::byte{1} && last_ram[0x79]==std::byte{1} &&
                ram[0x1278]==std::byte{1} && last_ram[0x1278]==std::byte{1} &&
                ram[0x1285+51]==std::byte{} && last_ram[0x1285+51]==std::byte{} &&
                ram[0x1385+51]!=last_ram[0x1385+51]) {
                auto offscreen=last_ram;offscreen[0x79]=std::byte{};
                palette_probe->reset();palette_probe->update(actors,offscreen,false);
                palette_probe->update(actors,offscreen,true);
                for(unsigned field=0;field<8;++field) {
                    const unsigned p=0x1245+field*64+51;
                    if(palette_probe->actors(1)[p]!=ram[p]) throw std::runtime_error("Tentacle palette mismatch frame "+
                        std::to_string(n+1)+" field "+std::to_string(field)+" predicted "+
                        std::to_string(std::to_integer<unsigned>(palette_probe->actors(1)[p]))+" actual "+
                        std::to_string(std::to_integer<unsigned>(ram[p]))+" prior phase "+
                        std::to_string(std::to_integer<unsigned>(last_ram[0x13c5+51])*256+std::to_integer<unsigned>(last_ram[0x1385+51]))+
                        " step "+std::to_string(std::to_integer<unsigned>(last_ram[0x1345+51])*256+std::to_integer<unsigned>(last_ram[0x1305+51])));
                }
                ++palette_frames;
            }
            if(n==399) {
                auto dormant=assets::inactive_table_actor_ram(ram);
                const auto original=dormant;
                // A visible skull saved when the ball falls to the lower half
                // must consume the pending reset visually, before revisiting it.
                auto skull=original;
                unsigned skull_slot=64;
                for(unsigned a=4;a<64;++a) if(skull[0x1500+a]==std::byte{14}) skull_slot=a;
                require(skull_slot<64,"Upper table fixture lacks skull guard");
                skull[0x1540+skull_slot]=std::byte{0x88};
                skull[0x1840+skull_slot]=std::byte{2};
                auto event_ram=last_ram;std::copy(ram.begin(),ram.end(),event_ram.begin());
                event_ram[0x70]&=std::byte{0xfd};
                const auto visible_skull=skull;
                const auto visible_pieces=assets::assemble_table_actors(actors,skull);
                require(std::any_of(visible_pieces.begin(),visible_pieces.end(),[&](const auto& p){return p.actor==skull_slot;}),
                    "Skull reset fixture never displayed the skull");
                assets::apply_inactive_table_events(skull,event_ram,1);
                require(skull==visible_skull,"Skull disappeared without a drain reset");
                event_ram[0x70]|=std::byte{2};const auto event_before=event_ram;
                assets::apply_inactive_table_events(skull,event_ram,0);
                require(skull==visible_skull,"Upper skull reset affected lower projection");
                assets::apply_inactive_table_events(skull,event_ram,1);
                require(skull[0x1840+skull_slot]==std::byte{} && skull[0x1540+skull_slot]==std::byte{0xe8},
                    "Drained skull remained visible in dormant upper bank");
                require(event_ram==event_before,"Presentation consumed real pending skull reset");
                const auto pieces=assets::assemble_table_actors(actors,skull);
                require(std::none_of(pieces.begin(),pieces.end(),[&](const auto& p){return p.actor==skull_slot;}),
                    "Reset skull still emits sprites");
                for(unsigned field=0;field<32;++field)
                    std::copy_n(visible_skull.begin()+0x1504+field*64,60,event_ram.begin()+0x204+field*64);
                event_ram[0x70]&=std::byte{0xfd};
                const auto before_drain=table->inactive_view(1,event_ram);
                event_ram[0x70]|=std::byte{2};
                const auto after_drain=table->inactive_view(1,event_ram);
                require(before_drain.pixels!=after_drain.pixels,"Inactive skull reset did not reach rendered table");
                require(ram[0x79]==std::byte{},"Initial orb check requires lower half");
                require(std::count(dormant.begin()+0x1504,dormant.begin()+0x1540,std::byte{19})==0,"Initial fixture already has brain orbs");
                actors.seed_brain_orbs(dormant,0);require(dormant==original,"Lower half seeded upper orbs");
                actors.seed_brain_orbs(dormant,1);
                unsigned count=0;
                for(unsigned a=4;a<64;++a) if(dormant[0x1500+a]==std::byte{19}) {
                    constexpr unsigned coordinates[]{0x15c0,0x1600,0x1680,0x16c0};
                    for(unsigned f=0;f<4;++f) require(dormant[coordinates[f]+a]==rom[0x56b6+count*4+f],"Initial brain orb is not at its ROM position");
                    require(dormant[0x1540+a]==std::byte{0x80},"Initial brain orb is invisible");
                    ++count;
                }
                require(count==5,"Initial cabinet view lacks the five brain orbs");
                // Startup creates the persistent timeline before the dormant
                // controller arrives. It must seed on readiness, not just reset.
                auto startup=std::make_unique<render::TableAnimation>(rom);
                auto loading=event_ram;
                std::fill(loading.begin()+0x204,loading.begin()+0xa00,std::byte{});
                startup->update(actors,loading,false);
                require(std::count(startup->actors(1).begin()+0x1504,startup->actors(1).begin()+0x1540,std::byte{19})==0,
                    "Brain orbs appeared before the initial controller was ready");
                startup->update(actors,ram,true);
                require(std::count(startup->actors(1).begin()+0x1504,startup->actors(1).begin()+0x1540,std::byte{19})==5,
                    "Persistent startup timeline missed the initial brain-orb group");
                auto expected_orbs=startup->actors(1);
                for(unsigned tick=0;tick<120;++tick) {
                    actors.advance_animation(expected_orbs);startup->update(actors,ram,true);
                    for(unsigned a=4;a<64;++a) if(expected_orbs[0x1500+a]==std::byte{19})
                        for(unsigned field=0;field<32;++field)
                            require(startup->actors(1)[0x1500+field*64+a]==expected_orbs[0x1500+field*64+a],
                                "Initial brain-orb animation was reseeded while waiting for first visit");
                }
                const auto seeded=dormant;actors.seed_brain_orbs(dormant,1);
                require(dormant==seeded,"Initial brain orbs were duplicated");
                unsigned mixed_frames=0,rest_frames=0;
                for(unsigned tick=0;tick<1200;++tick) {
                    actors.advance_animation(dormant);
                    unsigned moving=0;
                    for(unsigned a=4;a<64;++a) if(dormant[0x1500+a]==std::byte{19} && dormant[0x1840+a]!=std::byte{}) ++moving;
                    if(moving>0 && moving<5) ++mixed_frames;
                    if(!moving) ++rest_frames;
                }
                require(mixed_frames>300 && rest_frames>20,"Brain orb group shakes continuously instead of taking separate rests");
                auto waiting=original;
                for(unsigned a=4;a<64;++a) if(waiting[0x1500+a]==std::byte{66}) {
                    waiting[0x1880+a]=std::byte{44};waiting[0x18c0+a]=std::byte{1};
                }
                const auto respawning=waiting;actors.seed_brain_orbs(waiting,1);
                require(waiting==respawning,"Later cleared orb group was recreated early");
            }
            const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            const auto image=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
            if(n==363) {
                require(ram[0x1245+18]==std::byte{1} && ram[0x13c5+18]==ram[0x1405+18],
                    "Missing red-eye palette startup sentinel");
                palette_probe->reset();palette_probe->update(actors,ram,false);
                auto colors=video.palette;
                palette_probe->apply_palette(0,colors);
                for(unsigned color=288;color<304;++color) {
                    require(!palette_probe->owns_color(0,color),"Startup sentinel was treated as a drawable eye palette");
                    require(colors[color]==video.palette[color],"Startup sentinel replaced the loaded eye colors");
                }
                palette_probe->update(actors,ram,true);
                palette_probe->apply_palette(0,colors);
                require(palette_probe->actors(0)[0x13c5+18]==std::byte{7},"Descending palette did not enter its last valid frame");
                const unsigned pointer=std::to_integer<unsigned>(rom[0x1192+18])|
                    (std::to_integer<unsigned>(rom[0x11d2+18])<<8);
                const unsigned first=std::to_integer<unsigned>(rom[0x1212+18]);
                const unsigned bytes=std::to_integer<unsigned>(rom[0x1252+18])-first+2;
                for(unsigned i=0;i<bytes;i+=2) {
                    const unsigned offset=0x36000+pointer-0xa000+7*bytes+i;
                    const auto expected=std::to_integer<unsigned>(rom[offset])|((std::to_integer<unsigned>(rom[offset+1])&1)<<8);
                    require(colors[288+(first+i)/2]==expected,"First valid eye colors differ from ROM");
                }
                require(image.has_value(),"Startup eye regression lacks cabinet image");
                unsigned white=0;
                for(unsigned y=260;y<310;++y) for(unsigned x=95;x<175;++x)
                    white+=image->pixels[y*256+x]==0xffffffff;
                require(white<30,"Red eyes flashed white on the first palette activation frame");
                preview(*image,"startup-eyes-fixed");
            }
            const auto repeated=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
            require(!image || (repeated && repeated->pixels==image->pixels),"Rendering twice advanced inactive animation");
            require(runtime->save().core_state==before.core_state,"Presentation changed runtime state");
            if(image && hold.ball_updated()) {
                const auto ball=table->transition_ball(std::to_integer<unsigned>(ram[0x79]),ram);
                require(ball.has_value(),"Updated ball is missing");
                auto mouth_ram=std::array<std::byte,8192>{};
                std::copy(ram.begin(),ram.end(),mouth_ram.begin());
                mouth_ram[0x16]=mouth_ram[0x17]=std::byte{};
                mouth_ram[0x2e]=std::byte{};mouth_ram[0x2f]=std::byte{2};mouth_ram[0x50]=std::byte{10};
                std::array<std::uint16_t,256> mouth_sat{};
                for(const auto& piece:assets::assemble_table_actors(actors,mouth_ram)) {
                    const auto type=mouth_ram[0x1500+piece.actor];
                    if(type==std::byte{11} || type==std::byte{12} || type==std::byte{13} || type==std::byte{34})
                        std::copy(piece.words.begin(),piece.words.end(),mouth_sat.begin()+piece.slot*4);
                }
                const auto mouths=render::sprites(video.vram,video.palette,mouth_sat,0);
                for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
                    const int dx=ball->x+x,dy=ball->y+y;
                    const auto color=ball->image.pixels[y*16+x];
                    if(!(color>>24) || dx<0 || dx>=256 || dy<0 || dy>=456) continue;
                    if(image->pixels[dy*256+dx]!=color) {
                        const int local_y=dy-(ram[0x79]==std::byte{}?render::table_half_offset:0);
                        const bool mouth=std::any_of(mouths.begin(),mouths.end(),[&](const auto& sprite){
                            const int mx=dx-sprite.x,my=local_y-sprite.y;
                            return mx>=0 && my>=0 && mx<sprite.width && my<sprite.height &&
                                (sprite.image.pixels[my*sprite.width+mx]>>24);
                        });
                        require(mouth,"Only original mouth artwork may obscure the ball");
                        if(!mouth_pixels) preview(*image,"mouth-occlusion-fixed");
                        ++mouth_pixels;
                    }
                    ++ball_pixels;
                }
            }
            if(n>360 && ram[0x79]==last_ram[0x79]) {
                auto predicted=last_ram;actors.advance_animation(predicted);
                for(unsigned a=4;a<64;++a) {
                    const auto t=last_ram[0x1500+a];
                    if(t==std::byte{19} && ram[0x1500+a]==t && last_ram[0x1840+a]!=std::byte{} &&
                        (last_ram[0x1ac0+a]&std::byte{0x80})==std::byte{} &&
                        (last_ram[0x1580+a]!=ram[0x1580+a] || last_ram[0x1640+a]!=ram[0x1640+a])) {
                        for(unsigned field:{0x1580u,0x15c0u,0x1600u,0x1640u,0x1680u,0x16c0u,0x1700u,0x1740u,0x1780u,0x17c0u})
                            require(predicted[field+a]==ram[field+a],"Brain orb movement differs from original actor update");
                        ++brain_frames;
                    }
                    if((t!=std::byte{70} && t!=std::byte{71}) || ram[0x1500+a]!=t ||
                        last_ram[0x1840+a]!=std::byte{1} || (last_ram[0x1ac0+a]&std::byte{0x80})!=std::byte{} ||
                        ram[0x1840+a]==std::byte{}) continue;
                    if(last_ram[0x1640+a]==ram[0x1640+a] && last_ram[0x1680+a]==ram[0x1680+a] &&
                        last_ram[0x1780+a]==ram[0x1780+a] && last_ram[0x17c0+a]==ram[0x17c0+a]) continue;
                    for(unsigned field:{0x1640u,0x1680u,0x16c0u,0x1780u,0x17c0u,0x1b40u,0x1800u,0x1840u})
                        if(predicted[field+a]!=ram[field+a]) throw std::runtime_error("Orb visual motion differs: frame "+std::to_string(n+1)+
                            " slot "+std::to_string(a)+" field "+std::to_string(field)+" predicted "+std::to_string(std::to_integer<unsigned>(predicted[field+a]))+
                            " actual "+std::to_string(std::to_integer<unsigned>(ram[field+a])));
                    ++orb_frames;
                }
            }
            if(n==999) {
                seam_continuity(rom,video,ram);
                cocoon_rendering(rom,video,ram);
                slingshot_rendering(rom,video,ram);
                require(ram[0x79]==std::byte{1} && image,"Eye seam fixture requires active upper table");
                auto eyes=assets::inactive_table_actor_ram(ram);
                auto eye_ram=last_ram;std::copy(ram.begin(),ram.end(),eye_ram.begin());
                unsigned eye_count=0;
                for(unsigned a=4;a<64;++a) {
                    if(eyes[0x1500+a]!=std::byte{47}) {
                        eyes[0x1500+a]=std::byte{};eyes[0x1540+a]=std::byte{};continue;
                    }
                    ++eye_count;eyes[0x1840+a]=std::byte{1};eyes[0x1a40+a]=std::byte{3};
                    eyes[0x1b80+a]=std::byte{};eyes[0x1bc0+a]=std::byte{1};
                    eyes[0x1c00+a]=eyes[0x1c40+a]=std::byte{};
                    eyes[0x1800+a]=rom[0x9f56+(a-55)*3];
                }
                require(eye_count==9,"Dormant lower bank lacks the nine red eyes");
                eyes[0x4e]=std::byte{1};
                const auto render_eyes=[&]() {
                    for(unsigned field=0;field<32;++field)
                        std::copy_n(eyes.begin()+0x1504+field*64,60,eye_ram.begin()+0x204+field*64);
                    return table->inactive_view(0,eye_ram);
                };
                const auto resting_eyes=render_eyes();
                for(unsigned tick=0;tick<4;++tick) actors.advance_animation(eyes);
                const auto lit_eyes=render_eyes();
                require(lit_eyes.pixels!=resting_eyes.pixels,"Eye fixture never visibly opened");
                for(unsigned tick=0;tick<240;++tick) {
                    actors.advance_animation(eyes);
                    require(render_eyes().pixels==lit_eyes.pixels,"Dormant red eyes flickered during hold");
                }
                eyes[0x4e]=std::byte{};
                for(unsigned tick=0;tick<4;++tick) actors.advance_animation(eyes);
                require(render_eyes().pixels==resting_eyes.pixels,"Dormant red eyes did not visually return to normal");
                const render::Image upper{256,240,{image->pixels.begin(),image->pixels.begin()+256*240}};
                preview(render::stitch_table_views(resting_eyes,upper,1),"eyes-resting");
                preview(render::stitch_table_views(lit_eyes,upper,1),"eyes-held");
                // Move only a private ball copy across the reported occluders.
                // This includes artwork on the other side of the stitch.
                const auto original_ball=table->transition_ball(std::to_integer<unsigned>(ram[0x79]),ram);
                require(original_ball.has_value(),"Missing ball for overlap checks");
                const std::array targets{std::pair{108,250},std::pair{25,280},std::pair{113,343}};
                for(unsigned target=0;target<targets.size();++target) {
                    auto overlap_ram=last_ram;std::copy(ram.begin(),ram.end(),overlap_ram.begin());
                    for(unsigned axis=0;axis<2;++axis) {
                        const unsigned p=axis?0x1680:0x15c0;
                        const int value=std::to_integer<int>(ram[p])+(std::to_integer<int>(ram[p+64])<<8)+
                            (axis?targets[target].second-original_ball->y:targets[target].first-original_ball->x);
                        overlap_ram[p]=static_cast<std::byte>(value&255);overlap_ram[p+64]=static_cast<std::byte>((value>>8)&255);
                    }
                    auto overlap_table=std::make_unique<render::TablePlayfield>(rom);
                    render::TableTransitionHold overlap_hold;
                    const auto overlap=overlap_table->cabinet_frame(video,&*previous,overlap_ram,&raster,overlap_hold);
                    const auto test_ball=overlap_table->transition_ball(std::to_integer<unsigned>(ram[0x79]),overlap_ram);
                    require(overlap && test_ball && overlap_hold.ball_updated(),"Overlap view did not replace delayed ball");
                    for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
                        const auto color=test_ball->image.pixels[y*16+x];
                        if(color>>24) require(overlap->pixels[(test_ball->y+y)*256+test_ball->x+x]==color,"Reported table decoration still obscures ball");
                    }
                    preview(*overlap,"overlap-"+std::to_string(target));
                }
                auto paused_table=std::make_unique<render::TablePlayfield>(rom);
                std::array<std::byte,8192> paused_ram;
                std::copy(ram.begin(),ram.end(),paused_ram.begin());paused_ram[0xf6]|=std::byte{2};
                auto paused_video_storage=std::make_unique<diagnostics::ReferenceSnapshot>(video);
                auto paused_previous_storage=std::make_unique<diagnostics::ReferenceSnapshot>(video);
                auto& paused_video=*paused_video_storage;auto& paused_previous=*paused_previous_storage;
                --paused_previous.frame;
                render::TableTransitionHold paused_hold;
                std::optional<render::Image> paused_image;
                for(unsigned i=0;i<16;++i) {
                    const auto next=paused_table->cabinet_frame(paused_video,&paused_previous,paused_ram,&raster,paused_hold);
                    require(next.has_value(),"Pause lost cabinet presentation");
                    if(paused_image) require(next->pixels==paused_image->pixels,"Inactive animation advanced during pause");
                    paused_image=next;paused_previous=paused_video;++paused_video.frame;
                }
                auto cold_table=std::make_unique<render::TablePlayfield>(rom);
                render::TableTransitionHold cold_hold;
                const auto cold=cold_table->cabinet_frame(video,nullptr,ram,&raster,cold_hold);
                const auto reset=paused_table->cabinet_frame(video,nullptr,ram,&raster,paused_hold);
                require(cold && reset && cold->pixels==reset->pixels,"Cold view retained stale animation history");
                // Exercise the real Run pause path, including original active
                // actor scheduling, rather than relying only on a synthetic flag.
                const auto checkpoint=runtime->save();
                auto real_pause_table=std::make_unique<render::TablePlayfield>(rom);
                render::TableTransitionHold real_pause_hold;
                auto real_previous_storage=std::make_unique<diagnostics::ReferenceSnapshot>(video);
                auto& real_previous=*real_previous_storage;
                std::optional<render::Image> frozen;
                std::array<std::byte,8192> frozen_ram{};
                for(unsigned tick=0;tick<40;++tick) {
                    runtime->advance({static_cast<std::uint16_t>(tick==0?Pad::run:0),0});
                    const auto paused=runtime->video_snapshot();const auto state=runtime->work_ram();
                    const render::TableRaster paused_raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                    const auto rendered=real_pause_table->cabinet_frame(paused,&real_previous,state,&paused_raster,real_pause_hold);
                    require((state[0xf6]&std::byte{2})!=std::byte{},"Real Run did not pause");
                    require(rendered.has_value(),"Real pause lost stitched table");
                    if(tick>=4) {
                        if(frozen) {
                            require(std::equal(state.begin()+0x1500,state.begin()+0x1d00,frozen_ram.begin()+0x1500),
                                "Original game actors animate during pause");
                            if(rendered->pixels!=frozen->pixels) {
                                preview(*frozen,"pause-before");preview(*rendered,"pause-after");
                                throw std::runtime_error("Table pixels changed during real pause at tick "+std::to_string(tick));
                            }
                        }
                        frozen=rendered;std::copy(state.begin(),state.end(),frozen_ram.begin());
                    }
                    real_previous=paused;
                }
                bool resumed_motion=false;
                for(unsigned tick=0;tick<60;++tick) {
                    runtime->advance({static_cast<std::uint16_t>(tick==0?Pad::run:0),0});
                    const auto resumed=runtime->video_snapshot();
                    const render::TableRaster resumed_raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                    const auto rendered=real_pause_table->cabinet_frame(resumed,&real_previous,runtime->work_ram(),&resumed_raster,real_pause_hold);
                    if(rendered && rendered->pixels!=frozen->pixels) resumed_motion=true;
                    real_previous=resumed;
                }
                require(resumed_motion && (runtime->work_ram()[0xf6]&std::byte{2})==std::byte{},
                    "Unpause did not resume table animation");
                runtime->restore(checkpoint);
            }
            if(image && (n==399 || n==419 || n==999 || n==1019 || (n>=1399 && n<=1406))) {
                preview(*image,std::to_string(n+1));
            }
            const auto half=std::to_integer<unsigned>(ram[0x79]);
            if(n>360 && image && last && half<2 && ram[0x79]==last_ram[0x79] &&
                std::equal(ram.begin()+0x204,ram.begin()+0xa00,last_ram.begin()+0x204)) {
                const unsigned start=half==0?0:240*256,end=half==0?216*256:456*256;
                if(!std::equal(image->pixels.begin()+start,image->pixels.begin()+end,last->pixels.begin()+start)) ++animated[1-half];
            }
            last=image;std::copy(ram.begin(),ram.end(),last_ram.begin());previous=video;
        }
        require(animated[0]>50 && animated[1]>50,"Both dormant halves must animate while their game-state bank stays frozen");
        require(ball_pixels>10000 && orb_frames>100 && brain_frames>100,"Insufficient ball layering or original orb-motion coverage");
        require(mouth_pixels>0,"Natural replay must exercise mouth occlusion");
        require(palette_frames>100,"Insufficient original tentacle palette comparisons");
        require(flesh_frames>200,"Insufficient original fleshy-border palette comparisons");
        require(green_frames>100,"Insufficient original green-alien movement comparisons");
        runtime->restore(initial);runtime->observe_display(false);
        for(unsigned n=0;n<1800;++n) {
            const auto frame=runtime->advance(recording::canonical_input(n));
            require(recording::fingerprint(frame,runtime->work_ram())==fingerprints[n],"Animation changed RAM, video or audio replay");
        }
        std::cout<<"PASS: inactive lower/upper changed on "<<animated[0]<<'/'<<animated[1]
            <<" frozen-bank frames; "<<ball_pixels<<" unobscured ball pixels; "<<orb_frames
            <<" original orb-motion comparisons; "<<brain_frames<<" brain orb comparisons; "<<green_frames
            <<" green movement comparisons; 1800 exact RAM/video/audio frames; repeated rendering is idempotent\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
