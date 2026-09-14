// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// ROM-derived endpoint fixtures complement the natural replay, which cannot
// hit every reaction and cross a seam during each short-lived effect.
void finite_animation_checks(const assets::ActorAssets& actors,std::span<const std::byte> rom) {
    using Bank=std::array<std::byte,8192>;
    const auto fixture=[&](unsigned type,unsigned count,unsigned action,unsigned phase,int step) {
        Bank r{};
        const auto put=[&](unsigned p,unsigned v){r[p+4]=static_cast<std::byte>(v&255);};
        put(0x1500,type);put(0x1540,0xc0);put(0x1840,action);put(0x1a40,count);
        put(0x1c00,phase);put(0x1c40,phase>>8);put(0x1b80,static_cast<unsigned>(step));
        put(0x1bc0,static_cast<unsigned>(step)>>8);
        const auto pointer=std::to_integer<unsigned>(rom[0x9df7+type-1])|
            (std::to_integer<unsigned>(rom[0x9e51+type-1])<<8);
        r[0x1804]=rom[pointer-0x2000];
        return r;
    };
    const auto run=[&](Bank& r,unsigned ticks){while(ticks--) actors.advance_animation(r);};
    const auto settled=[&](Bank& r) {
        const auto before=r;run(r,120);require(r==before,"Finite reaction restarted after settling");
    };
    for(unsigned type:{14u,30u}) {
        auto r=fixture(type,1,1,0,0);r[0x1684]=std::byte{100};
        r[0x1784]=std::byte{0x80};r[0x17c4]=std::byte{0xff};r[0x1884]=std::byte{32};
        r[type==14?0x70:0x6d]=static_cast<std::byte>(type==14?8:4);
        run(r,32);require(r[0x1684]==std::byte{84} && r[0x1844]==std::byte{2},"Guard rise endpoint");
        settled(r);r[type==14?0x70:0x6d]=std::byte{};
        run(r,33);require(r[0x1684]==std::byte{100} && r[0x1844]==std::byte{} &&
            (r[0x1544]&std::byte{0x60})==std::byte{0x60},"Guard retract endpoint/hide");settled(r);
    }
    auto hit=fixture(10,3,1,0,-64);run(hit,9);
    require(hit[0x1844]==std::byte{} && hit[0x1c44]==std::byte{2},"Hit target failed to stop");settled(hit);
    for(unsigned type:{8u,37u,38u,39u}) for(unsigned choice:{0u,2u}) {
        auto r=fixture(type,3,1,0,32);r[0x1884]=static_cast<std::byte>(choice);
        const unsigned pointer=choice?0x9f38:0x9ecf;
        run(r,24);require(r[0x1844]==std::byte{} && r[0x1804]==rom[pointer+2],"Target failed to hold selected open pose");
        settled(r);r[0x1844]=std::byte{2};run(r,16);
        require(r[0x1844]==std::byte{} && r[0x1804]==rom[pointer],"Target failed to hold closed pose");settled(r);
    }
    for(unsigned action:{1u,2u}) {
        auto r=fixture(9,4,action,action==1?0:1024,action==1?64:-64);
        r[0x1b04]=static_cast<std::byte>(action==1?0xf0:0x10);
        run(r,1000);require(r[0x1844]==std::byte{} && r[0x1c44]==std::byte{},"Rebound failed to damp to idle");settled(r);
        r=fixture(9,4,action,action==1?0:1024,0);run(r,1);require(r[0x1844]==std::byte{},"Zero-speed rebound failed to settle");
    }
    for(unsigned type:{72u,73u}) for(unsigned paired_type:{25u,33u}) {
        auto r=fixture(type,4,0,0,0);r[0x1884]=std::byte{14};r[0x18c4]=std::byte{5};
        r[0x15c4]=r[0x1684]=std::byte{100};
        r[0x1505]=static_cast<std::byte>(paired_type);r[0x1545]=std::byte{0xc0};
        run(r,14);require(r[0x1844]==std::byte{1} && r[0x1684]==std::byte{100},"Defeat wait changed position");
        run(r,11);require(r[0x1684]==static_cast<std::byte>(type==72?89:122),"Defeat motion endpoint");
        run(r,1);for(unsigned field=0;field<32;++field)
            require(r[0x1504+field*64]==std::byte{},"Defeat effect failed to clear actor");
        require(r[0x1505]==static_cast<std::byte>(type==73 && paired_type==25?0:paired_type),"Defeat removed wrong paired actor");
        r=fixture(type,4,4,0,0);r[0x1544]=std::byte{};
        run(r,26);require(r[0x1504]==std::byte{},"Newly defeated actor froze before initialization");
    }
    for(unsigned type:{52u,53u,55u,56u}) {
        auto r=fixture(type,3,type<54?1:0,0,32);
        r[0x15c4]=r[0x1684]=std::byte{100};r[0x1744]=r[0x17c4]=std::byte{1};
        run(r,10);require(r[0x15c4]==std::byte{110} && r[0x1684]==std::byte{110},"Flying effect froze");
        run(r,200);require(r[0x1504]==std::byte{},"Flying effect failed to expire at bounds");
    }
    auto travel=fixture(51,3,0,0,0);travel[0x1884]=std::byte{1};
    travel[0x15c4]=travel[0x1684]=std::byte{100};
    run(travel,17);require(travel[0x15c4]==rom[0x326b] && travel[0x1684]==rom[0x326c] &&
        travel[0x1504]==std::byte{10},"Travelling target did not reach ROM destination");
    travel[0x1804]=fixture(10,3,1,0,0)[0x1804]; // Type 51 retains its type-10 sprite.
    run(travel,14);require(travel[0x1844]==std::byte{},"Travelling target did not settle after arrival");
    // Unlike frozen-bank swap tests, the ROM may dispatch an actor on the
    // very frame the viewport changes. Retain the independently advanced pose.
    for(unsigned type:{21u,22u,15u,19u,33u,9u,10u,8u,14u,70u,71u}) {
        const unsigned count=type==33?6:type==10 || type==8?3:type==19 || type>=70?2:type==14?1:4;
        auto source_actor=fixture(type,count,1,0,32);
        source_actor[0x1684]=source_actor[0x15c4]=std::byte{120};
        source_actor[0x1784]=std::byte{0x80};source_actor[0x17c4]=std::byte{0xff};
        source_actor[0x1884]=std::byte{40};source_actor[0x1a04]=std::byte{1};
        source_actor[0x1b04]=std::byte{0xf0};source_actor[0x70]=std::byte{8};
        auto timeline=std::make_unique<render::TableAnimation>(rom);
        auto incoming=source_actor;
        const auto swap=[&](Bank& r) {
            for(unsigned field=0;field<32;++field) for(unsigned x=4;x<64;++x)
                std::swap(r[0x1500+field*64+x],r[0x200+field*64+x]);
            r[0x79]^=std::byte{1};
        };
        swap(incoming);timeline->update(actors,incoming,false);
        for(unsigned i=0;i<17;++i) timeline->update(actors,incoming,true);
        auto expected=timeline->actors(0);actors.advance_animation(expected);
        actors.advance_animation(source_actor);
        timeline->update(actors,source_actor,true);
        for(unsigned field=0;field<32;++field)
            require(timeline->actors(0)[0x1504+field*64]==expected[0x1504+field*64],
                "Resuming actor dispatch at seam reset a private animation");
        expected=timeline->actors(0);actors.advance_animation(expected);
        actors.advance_animation(source_actor);swap(source_actor);
        timeline->update(actors,source_actor,true);
        for(unsigned field=0;field<32;++field)
            require(timeline->actors(0)[0x1504+field*64]==expected[0x1504+field*64],
                "Departing actor dispatch at seam reset a private animation");
    }
    // Independent runtime RNG chooses a different ordinary wait on return.
    // This must not rewind motion, while real hits and type changes still win.
    for(unsigned type:{21u,15u,19u}) {
        auto source_actor=fixture(type,4,type==21?1:type==15?2:0,0,32);
        source_actor[0x1884]=std::byte{1};source_actor[0x1684]=std::byte{120};
        source_actor[0x1784]=std::byte{0x80};source_actor[0x17c4]=std::byte{0xff};
        auto timeline=std::make_unique<render::TableAnimation>(rom);
        timeline->update(actors,source_actor,false);
        for(unsigned i=0;i<17;++i) timeline->update(actors,source_actor,true);
        auto expected=timeline->actors(0);actors.advance_animation(expected);
        actors.advance_animation(source_actor);
        source_actor[0x1884]=static_cast<std::byte>(type==19?4:239);
        timeline->update(actors,source_actor,true);
        for(unsigned field=0;field<32;++field)
            require(timeline->actors(0)[0x1504+field*64]==expected[0x1504+field*64],
                "Runtime randomized wait rewound a cosmetic animation");
        source_actor[0x1ac4]=std::byte{0x80};timeline->update(actors,source_actor,true);
        require(timeline->actors(0)[0x1ac4]==std::byte{0x80},"Random-wait reconciliation swallowed a real hit");
        source_actor[0x1504]=std::byte{73};source_actor[0x1544]=std::byte{};
        timeline->update(actors,source_actor,true);
        require(timeline->actors(0)[0x1504]==std::byte{73},"Random-wait reconciliation swallowed a type change");
    }
    // Frozen runtime banks are replayed across repeated seams. Completed
    // effects must not resurrect, and finite phases must not restart.
    auto source=fixture(73,4,0,0,0);source[0x1884]=std::byte{14};source[0x18c4]=std::byte{5};
    source[0x1505]=std::byte{25};source[0x1545]=std::byte{0xc0};
    unsigned slot=6;
    for(unsigned type:{9u,10u,8u,14u,72u,55u}) {
        auto actor=fixture(type,type==9?4:3,type==55?0:1,0,32);
        actor[0x1884]=std::byte{12};actor[0x1784]=std::byte{0x80};
        actor[0x17c4]=std::byte{0xff};actor[0x1684]=std::byte{100};actor[0x15c4]=std::byte{100};
        actor[0x1b04]=std::byte{0xf0};
        for(unsigned field=0;field<32;++field) source[0x1500+field*64+slot]=actor[0x1504+field*64];
        ++slot;
    }
    auto fixed=std::make_unique<render::TableAnimation>(rom);
    auto crossing=std::make_unique<render::TableAnimation>(rom);
    // The half selector changes before the RAM exchange. Intermediate banks
    // can contain unrelated types and mixed fields; neither is authoritative.
    for(unsigned caller:{0x89cfu,0x89d2u,0x8a2au,0x8a2du}) {
        auto loading=source;
        loading[0x79]=static_cast<std::byte>(caller<0x8a00?1:0);
        loading[0x1fc]=static_cast<std::byte>(caller&255);loading[0x1fd]=static_cast<std::byte>(caller>>8);
        loading[0x1fe]=std::byte{0xa0};loading[0x1ff]=std::byte{0x40};
        std::fill(loading.begin()+0x200,loading.begin()+0xa00,std::byte{0xe7});
        std::fill(loading.begin()+0x1500,loading.begin()+0x1d00,std::byte{0xa5});
        fixed->reset();crossing->reset();
        fixed->update(actors,source,false);crossing->update(actors,source,false);
        for(unsigned i=0;i<35;++i) {
            fixed->update(actors,source,true);crossing->update(actors,loading,true);
            for(unsigned field=0;field<32;++field) for(unsigned x=4;x<64;++x)
                require(fixed->actors(0)[0x1500+field*64+x]==crossing->actors(0)[0x1500+field*64+x],
                    "Intermediate bank exchange replaced a retained actor");
        }
    }
    fixed->reset();crossing->reset();
    for(unsigned tick=0;tick<640;++tick) {
        auto swapped=source;
        if((tick/7)%2) {
            swapped[0x79]=std::byte{1};
            for(unsigned field=0;field<32;++field) for(unsigned x=4;x<64;++x)
                std::swap(swapped[0x1500+field*64+x],swapped[0x200+field*64+x]);
        }
        fixed->update(actors,source,tick!=0);crossing->update(actors,swapped,tick!=0);
        for(unsigned field=0;field<32;++field) for(unsigned x=4;x<64;++x)
            require(fixed->actors(0)[0x1500+field*64+x]==crossing->actors(0)[0x1500+field*64+x],
                "Seam restarted a finite reaction or resurrected an expired effect");
    }
}
