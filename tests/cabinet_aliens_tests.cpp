// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/gameplay/cabinet_aliens.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace alien_crush;
namespace {
using Ram=std::array<std::byte,8192>;
void require(bool ok,const std::string& message) {if(!ok) throw std::runtime_error(message);}
unsigned get(const Ram& ram,unsigned p) {return std::to_integer<unsigned>(ram[p]);}
void set(Ram& ram,unsigned p,unsigned v) {ram[p]=static_cast<std::byte>(v&255);}
// Actor field ($3500+field) of slot x, in the active (0x1500) or stored (0x200) bank.
unsigned field(const Ram& ram,bool active,unsigned address,unsigned x) {return get(ram,(active?0x1500:0x200)+address-0x3500+x);}
void put(Ram& ram,bool active,unsigned address,unsigned x,unsigned v) {set(ram,(active?0x1500:0x200)+address-0x3500+x,v);}
unsigned random_step(Ram& ram) { // 00:F9A2
    const unsigned lo=get(ram,0x1217),state=lo|(get(ram,0x1218)<<8);
    const unsigned mixed=(state+(((state+0x503f)&65535)*2))&65535,result=(lo+(mixed>>8))&255;
    set(ram,0x1217,mixed);set(ram,0x1218,result);
    return result;
}
// Scripted play: the canonical start, then varied flipper patterns. Run
// pauses and resumes every 600 frames. The kIdle route presses nothing after
// the start, so its ball ends up waiting in the launcher, downstairs.
constexpr unsigned kIdle=100;
PadInput route(unsigned seed,unsigned n) {
    auto input=recording::canonical_input(n);
    if(n<360) return input;
    input={};
    if(seed==kIdle) return input;
    const unsigned burst=240+seed*17,period=39+seed*3;
    if(n%burst<100 || n%period<19) input.player_one|=Pad::button_i;
    if((n+13+seed*5)%period<19) input.player_one|=Pad::left;
    if(n%181==100) input.player_one|=Pad::button_ii;
    if(n%600==599) input.player_one|=Pad::run;
    return input;
}
bool table_scene(std::span<const std::byte> ram) {
    const auto scene=diagnostics::observe_alien_state(ram);
    return scene.table_code<=1 && !scene.title_menu && !scene.high_score_menu && !scene.title_transition;
}

// The ROM routines, one state at a time, including the parts the replays
// cannot reach on demand: the other half, hits, group clears and respawns.
void routine_checks(std::span<const std::byte> rom) {
    gameplay::CabinetAliens aliens(rom);
    std::uint64_t frame=100;
    const auto palette_table=[&](unsigned address) {return std::to_integer<unsigned>(rom[address-0xe000]);};
    // Orange alien, in the ball's half, from the controller's spawn fields.
    {
        Ram ram{};set(ram,0x1217,0x35);set(ram,0x1218,0x7a);
        auto expected=ram;const unsigned wait=(random_step(expected)+16)&255;
        const unsigned x=9;
        put(ram,true,0x3500,x,15);put(ram,true,0x35c0,x,214);put(ram,true,0x3680,x,90);
        put(ram,true,0x3800,x,0x9a);put(ram,true,0x3a80,x,4);
        require(aliens.update_active(ram,x,frame++)==gameplay::CabinetAliens::kReturn,"Orange alien start returned into ROM");
        require(field(ram,true,0x3540,x)==0xc0 && field(ram,true,0x3a00,x)==0x23,"Orange alien did not start hidden and still");
        require(field(ram,true,0x3880,x)==(wait?wait-1:255) && get(ram,0x1217)==get(expected,0x1217) && get(ram,0x1218)==get(expected,0x1218),
            "Orange alien's first wait is not the game RNG's");
        require(get(ram,0x1245+0x14)==8 && get(ram,0x13c5+0x14)==palette_table(0xf152+0x14),"Orange alien did not start its palette");
        unsigned ticks=1;
        while(field(ram,true,0x3840,x)!=2) {aliens.update_active(ram,x,frame++);require(++ticks<300,"Orange alien never emerged");}
        require(ticks==(wait?wait:256),"Orange alien waited "+std::to_string(ticks)+" not "+std::to_string(wait));
        for(unsigned i=0;i<47;++i) {
            aliens.update_active(ram,x,frame++);
            require(field(ram,true,0x3540,x)&0x40,"Emerging orange alien became solid early");
        }
        aliens.update_active(ram,x,frame++);
        require(field(ram,true,0x3840,x)==3 && !(field(ram,true,0x3540,x)&0x40) &&
            field(ram,true,0x35c0,x)==202 && field(ram,true,0x3680,x)==102,"Orange alien did not emerge 12 pixels on each axis and turn solid");
        require(field(ram,true,0x3880,x)>=120 && field(ram,true,0x3880,x)<240,"Orange alien's wait outside is not 120..239");
        auto retreat=ram;
        put(retreat,true,0x3880,x,1);aliens.update_active(retreat,x,frame++);
        require(field(retreat,true,0x3840,x)==4 && (field(retreat,true,0x3540,x)&0x40),"Retreating orange alien stayed solid");
        for(unsigned i=0;i<32;++i) aliens.update_active(retreat,x,frame++);
        // It first emerged 48 ticks from deep in the hole; later cycles are 32.
        require(field(retreat,true,0x3840,x)==1 && field(retreat,true,0x35c0,x)==210 && field(retreat,true,0x3680,x)==94,
            "Orange alien did not retreat 8 pixels on each axis");
        // A hit: 1,000 points, a defeat effect, and the group count.
        auto hit=ram;put(hit,true,0x3ac0,x,0x80);set(hit,0x34,2);set(hit,0x121e,0xfe);
        require(aliens.update_active(hit,x,frame++)==gameplay::CabinetAliens::kReturn,"First orange hit ran the group-cleared rules");
        require(field(hit,true,0x3500,x)==72 && field(hit,true,0x3540,x)==0 && get(hit,0x34)==1 &&
            get(hit,0x121e)==8 && get(hit,0x1224)==1 && (get(hit,0x68)&4),"Orange hit did not score 1,000 and become a defeat effect");
        hit=ram;put(hit,true,0x3ac0,x,0x80);set(hit,0x34,1);set(hit,0xb0,0xc0);
        require(aliens.update_active(hit,x,frame++)==0x917f,"Last orange hit did not continue into 03:917F");
        require(get(hit,0x34)==0 && (get(hit,0x69)&1) && (get(hit,0x98)&1) && get(hit,0xb0)==0x40 &&
            get(hit,0x1245+5)==2,"Orange group clear did not signal its rules and lights");
        hit=ram;put(hit,true,0x3ac0,x,0x80);set(hit,0x9c,1);aliens.update_active(hit,x,frame++);
        require(get(hit,0x121e)==0,"Hit scored while scoring was off");
        // Game over removes it; the hide flag lasts one frame.
        auto events=ram;set(events,0x69,0x10);aliens.update_active(events,x,frame++);
        require(field(events,true,0x3540,x)&0x20,"Hide event did not hide the alien");
        set(events,0x69,0);aliens.update_active(events,x,frame++);
        require(!(field(events,true,0x3540,x)&0x20),"Hidden alien did not reappear");
        set(events,0x69,0x20);aliens.update_active(events,x,frame++);
        require(field(events,true,0x3500,x)==0,"Game over did not remove the alien");
        // The same alien in the other half: private randomness, no scratch,
        // no $69 events, and a pending hit waits for its half.
        Ram away{};set(away,0x79,1);set(away,0x1217,0x35);set(away,0x1218,0x7a);set(away,0x69,0x30);
        put(away,false,0x3500,x,15);put(away,false,0x35c0,x,214);put(away,false,0x3680,x,90);put(away,false,0x3a80,x,4);
        aliens.update_inactive(away,frame++);
        require(field(away,false,0x3540,x)==0xc0 && get(away,0x1217)==0x35 && get(away,0x1218)==0x7a,
            "Alien away from the ball changed the game's RNG");
        for(unsigned p=0;p<0x100;++p) if(p!=0x79 && p!=0x69) require(!get(away,p),"Alien away from the ball wrote zero page");
        unsigned moves=0;
        for(unsigned i=0;i<600;++i) {
            const auto before=field(away,false,0x3680,x);aliens.update_inactive(away,frame++);
            moves+=before!=field(away,false,0x3680,x);
        }
        require(field(away,false,0x3500,x)==15 && moves>20,"Alien away from the ball did not keep moving");
        auto frozen=away;put(frozen,false,0x3840,x,3);put(frozen,false,0x3ac0,x,0x80);
        const auto pending=frozen;aliens.update_inactive(frozen,frame++);
        require(frozen==pending,"A hit was processed away from the ball");
        // One update per frame: after the inactive pass, the exchange and the
        // table's second dispatch must not run it again.
        auto crossing=away;put(crossing,false,0x3840,x,2);put(crossing,false,0x3880,x,20);
        aliens.update_inactive(crossing,frame);
        Ram swapped=crossing;
        for(unsigned f=0;f<32;++f) for(unsigned s=4;s<64;++s) std::swap(swapped[0x1500+f*64+s],swapped[0x200+f*64+s]);
        set(swapped,0x79,0);set(swapped,0x69,0);
        const auto once=swapped;aliens.update_active(swapped,x,frame);
        require(swapped==once,"An alien ran twice in the frame the ball came down");
        aliens.update_active(swapped,x,frame+1);
        require(field(swapped,true,0x3880,x)==18,"An alien skipped the frame after the ball came down");
        ++frame;
    }
    // Silver orange alien: out after 48 ticks, then still until hit.
    {
        Ram ram{};const unsigned x=5;
        put(ram,true,0x3500,x,16);put(ram,true,0x35c0,x,100);put(ram,true,0x3680,x,150);put(ram,true,0x3900,x,0x0a);
        for(unsigned i=0;i<47;++i) aliens.update_active(ram,x,frame++);
        require(field(ram,true,0x3840,x)==0 && (field(ram,true,0x3540,x)&0x40),"Silver alien emerged early");
        aliens.update_active(ram,x,frame++);
        require(field(ram,true,0x3840,x)==1 && field(ram,true,0x3540,x)==0x80 &&
            field(ram,true,0x35c0,x)==88 && field(ram,true,0x3680,x)==162,"Silver alien did not emerge from its hole");
        const auto out=ram;
        for(unsigned i=0;i<600;++i) aliens.update_active(ram,x,frame++);
        require(ram==out,"Emerged silver alien moved while waiting for a hit");
        put(ram,true,0x3ac0,x,0x80);set(ram,0x34,3);aliens.update_active(ram,x,frame++);
        require(field(ram,true,0x3500,x)==72 && field(ram,true,0x3a80,x)==0x0a && get(ram,0x121e)==0x14 &&
            get(ram,0x1245+0x1a)==2 && get(ram,0x1305+0x1a)==0x80,"Silver hit did not score 2,000 and reveal its color");
    }
    // Green alien and its silver variant.
    for(unsigned type:{21u,22u}) {
        Ram ram{};const unsigned x=7;
        set(ram,0x1217,0x11);set(ram,0x1218,0x5c);
        put(ram,true,0x3500,x,type);put(ram,true,0x3680,x,152);put(ram,true,0x3800,x,0x5f);put(ram,true,0x3a80,x,3);
        if(type==21) {aliens.update_active(ram,x,frame++);put(ram,true,0x3880,x,1);}
        for(unsigned i=0;i<(type==21?49u:48u);++i) aliens.update_active(ram,x,frame++);
        require(field(ram,true,0x3680,x)==128 && !(field(ram,true,0x3540,x)&0x40),"Green alien did not rise 24 pixels and turn solid");
        if(type==22) {
            bool animated=false;const auto pose=field(ram,true,0x3800,x);
            for(unsigned i=0;i<600;++i) {aliens.update_active(ram,x,frame++);animated|=field(ram,true,0x3800,x)!=pose;}
            require(animated && field(ram,true,0x3680,x)==128,"Silver green alien lowered or froze");
            continue;
        }
        for(bool active:{true,false}) {
            if(!active) {
                for(unsigned f=0;f<32;++f) for(unsigned s=4;s<64;++s) std::swap(ram[0x1500+f*64+s],ram[0x200+f*64+s]);
                set(ram,0x79,1);
            }
            for(unsigned cycle=0;cycle<4;++cycle) {
                const auto run=[&]{if(active) aliens.update_active(ram,x,frame++); else aliens.update_inactive(ram,frame++);};
                const auto wait=field(ram,active,0x3880,x);
                require(field(ram,active,0x3840,x)==2 && wait>=120 && wait<240,"Green raised wait outside 120..239");
                bool animated=false;const auto pose=field(ram,active,0x3800,x);
                for(unsigned i=0;i<wait;++i) {run();animated|=field(ram,active,0x3800,x)!=pose;}
                require(animated && (field(ram,active,0x3540,x)&0x40),"Raised green alien froze or stayed solid");
                for(unsigned i=0;i<48;++i) run();
                require(field(ram,active,0x3680,x)==152 && field(ram,active,0x3840,x)==0,"Green alien did not lower");
                unsigned idle=field(ram,active,0x3880,x);
                for(unsigned i=0;i<(idle?idle:256)+48;++i) run();
                require(field(ram,active,0x3680,x)==128,"Green alien did not rise again");
            }
        }
    }
    // Controllers in the stored bank: the group respawns while the ball is away.
    for(unsigned type:{67u,68u}) for(bool silver:{false,true}) {
        Ram ram{};set(ram,0x79,1);set(ram,0x02,0x2d);
        // The group fills the first free slots, after the controller and a
        // bystander, so the dispatcher order starts them in the same pass.
        const unsigned controller=4;
        put(ram,false,0x3500,5,33);put(ram,false,0x3500,controller,type);
        unsigned ticks=0;
        while(field(ram,false,0x3500,controller)==type) {
            if(silver && ticks==100) set(ram,0x82,type==67?2:8);
            aliens.update_inactive(ram,frame++);require(++ticks<400,"Controller never spawned its group");
        }
        require(ticks==(silver?102u:301u),"Group respawned after "+std::to_string(ticks)+" ticks");
        const bool orange=type==67;
        const unsigned alien=orange?(silver?16:15):(silver?22:21),count=orange?3:6;
        std::vector<unsigned> slots;
        for(unsigned s=4;s<64;++s) if(field(ram,false,0x3500,s) && s!=5) slots.push_back(s);
        require(slots.size()==count && get(ram,orange?0x34:0x36)==3,"Controller spawned the wrong number of actors");
        for(unsigned i=0;i<count;++i) {
            const unsigned s=slots[i],row=(orange?0x5f4c:0x5f5e)+i*6-0x4000+0x2000;
            const unsigned actor=!orange && !(i&1)?25:alien;
            require(field(ram,false,0x3500,s)==actor && field(ram,false,0x35c0,s)==std::to_integer<unsigned>(rom[row]) &&
                field(ram,false,0x3800,s)==std::to_integer<unsigned>(rom[row+4]),"Spawned actor differs from the ROM table");
            if(!orange && (i&1)) require(field(ram,false,0x38c0,s)==slots[i-1],"Green alien lost its base");
            require((field(ram,false,0x3540,s)&0x80)!=0,"Spawned actor was not started in the same pass");
        }
        require(get(ram,0x1245+(orange?5:2))!=0,"Group lights did not start");
    }
    // Defeat effects: a pause, then a flight off, then gone; green takes its base.
    for(unsigned type:{72u,73u}) {
        Ram ram{};const unsigned x=8,base=20;
        put(ram,true,0x3500,x,type);put(ram,true,0x35c0,x,100);put(ram,true,0x3680,x,100);put(ram,true,0x38c0,x,base);
        put(ram,true,0x3500,base,25);put(ram,true,0x3540,base,0xc0);
        require(aliens.update_active(ram,x,frame++)==0xea62 && get(ram,0x73)==0x2a,"Defeat effect did not play its sound");
        for(unsigned i=1;i<14;++i) aliens.update_active(ram,x,frame++);
        require(field(ram,true,0x3840,x)==1 && field(ram,true,0x3680,x)==100,"Defeat pause changed position");
        for(unsigned i=0;i<11;++i) aliens.update_active(ram,x,frame++);
        require(field(ram,true,0x3500,x)==type,"Defeat effect ended early");
        aliens.update_active(ram,x,frame++);
        require(field(ram,true,0x3500,x)==0 && field(ram,true,0x3500,base)==(type==73?0u:25u),"Defeat effect did not clear itself and its base");
        Ram away{};set(away,0x79,1);put(away,false,0x3500,x,type);
        aliens.update_inactive(away,frame++);
        require(!get(away,0x73) && field(away,false,0x3540,x)==0xc0,"Defeat effect away from the ball played a sound");
    }
}

// While the ball stays downstairs, the native aliens must be the original's
// to the byte: same RAM (the stack page aside, where routines the port
// replaces leave their return addresses) and the same picture. Native updates
// take fewer CPU cycles, so a frame that ends while the game is still working
// (a heavy loading frame) can catch the two at different points; only frames
// that end with both waiting for vblank (00:E2FA..E2FC) are compared.
struct Sample {Ram ram;std::vector<std::uint32_t> pixels;bool waiting{};};
Sample sample(const ReferenceRuntime& runtime,const Frame& frame) {
    Sample result;std::copy(runtime.work_ram().begin(),runtime.work_ram().end(),result.ram.begin());
    result.pixels.assign(frame.pixels.begin(),frame.pixels.end());
    const auto pc=runtime.video_snapshot().pc;result.waiting=pc==0xe2fa || pc==0xe2fc;
    return result;
}
bool upstairs(const Ram& ram) {return get(ram,0x79)==1 && table_scene(ram);}
void begin(ReferenceRuntime& runtime,const state::SaveStateContainer* start) {
    if(!start) return;
    auto state=*start;state.core_revision=runtime.revision();runtime.restore(state);
}
// The original from `start` (or power-on), until the ball goes upstairs.
std::vector<Sample> record(ReferenceRuntime& runtime,const state::SaveStateContainer* start,unsigned seed,unsigned frames) {
    begin(runtime,start);
    std::vector<Sample> result;
    const unsigned first=start?static_cast<unsigned>(start->frame_number):0;
    for(unsigned n=0;n<frames;++n) {
        result.push_back(sample(runtime,runtime.advance(route(seed,first+n))));
        if(upstairs(result.back().ram)) break;
    }
    return result;
}
unsigned compare(ReferenceRuntime& runtime,const state::SaveStateContainer* start,unsigned seed,
    const std::vector<Sample>& original,const std::string& name,unsigned& owned_updates) {
    begin(runtime,start);
    const unsigned first=start?static_cast<unsigned>(start->frame_number):0;
    Ram previous;std::copy(runtime.work_ram().begin(),runtime.work_ram().end(),previous.begin());
    unsigned compared=0;
    bool comparable=false;
    for(unsigned n=0;n<original.size();++n) {
        const auto now=sample(runtime,runtime.advance(route(seed,first+n)));
        const bool was_comparable=comparable;
        comparable=original[n].waiting && now.waiting;
        if(comparable) {
            ++compared;
            for(unsigned p=0;p<8192;++p) {
                if(p>=0x100 && p<0x200) continue;
                if(now.ram[p]!=original[n].ram[p]) throw std::runtime_error(name+" frame "+std::to_string(first+n+1)+
                    " RAM "+std::to_string(p)+": native "+std::to_string(get(now.ram,p))+" original "+std::to_string(get(original[n].ram,p)));
            }
            if(was_comparable) require(now.pixels==original[n].pixels,name+" frame "+std::to_string(first+n+1)+" picture differs");
        }
        for(unsigned x=4;x<64;++x) {
            if(!gameplay::CabinetAliens::owns(std::to_integer<std::uint8_t>(now.ram[0x1500+x]))) continue;
            bool changed=false;
            for(unsigned f=0;f<32;++f) changed|=now.ram[0x1500+f*64+x]!=previous[0x1500+f*64+x];
            owned_updates+=changed;
        }
        previous=now.ram;
    }
    require(compared*10>=original.size()*9,name+" compared only "+std::to_string(compared)+" of "+
        std::to_string(original.size())+" frames");
    return compared;
}
std::vector<unsigned> slots_of(const Ram& ram,unsigned type) {
    std::vector<unsigned> result;
    for(unsigned x=4;x<64;++x) if(get(ram,0x1500+x)==type) result.push_back(x);
    return result;
}
unsigned count_of(const Ram& ram,unsigned type) {return static_cast<unsigned>(slots_of(ram,type).size());}
// A saved state edited as the ball physics leaves a collision: bit 7 of the
// alien's $3AC0 (03:858F), with the alien out and solid.
struct Checkpoint {state::SaveStateContainer state;Ram ram;};
Checkpoint checkpoint(const ReferenceRuntime& runtime) {
    Checkpoint result{runtime.save(),{}};
    std::copy(runtime.work_ram().begin(),runtime.work_ram().end(),result.ram.begin());
    return result;
}
state::SaveStateContainer edited(const Checkpoint& start,const std::function<void(Ram&)>& edit) {
    const auto& blob=start.state.core_state;
    const auto found=std::search(blob.begin(),blob.end(),start.ram.begin(),start.ram.end());
    require(found!=blob.end() && std::search(found+1,blob.end(),start.ram.begin(),start.ram.end())==blob.end(),
        "Work RAM is not stored once in the saved state");
    auto ram=start.ram;edit(ram);
    auto result=start.state;
    std::copy(ram.begin(),ram.end(),result.core_state.begin()+(found-blob.begin()));
    return result;
}
void strike(Ram& ram,unsigned x) {
    const unsigned type=get(ram,0x1500+x);
    set(ram,0x1840+x,type==15?3:type==21?2:1);
    if(type==15 || type==21) set(ram,0x1880+x,100);
    set(ram,0x1540+x,(get(ram,0x1540+x)|0x80)&0xbf);set(ram,0x1ac0+x,0x80);
}
void clear_slot(Ram& ram,unsigned x) {for(unsigned f=0;f<32;++f) set(ram,0x1500+f*64+x,0);}
// Hits, group clears and respawns, which the scripted routes seldom reach
// before going upstairs. The route leaves the ball downstairs.
unsigned hit_parity(const std::filesystem::path& path,unsigned& owned_updates) {
    constexpr unsigned kSeed=kIdle,kStart=450;
    struct Case {std::string name;state::SaveStateContainer start;std::vector<Sample> original;};
    std::vector<Case> cases;
    {
        auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet);runtime->load(path);
        for(unsigned n=0;n<kStart;++n) runtime->advance(route(kSeed,n));
        const auto start=checkpoint(*runtime);
        require(get(start.ram,0x79)==0 && count_of(start.ram,15)==3 && count_of(start.ram,21)==3,"Hit fixtures need both full groups downstairs");
        const auto orange=slots_of(start.ram,15),green=slots_of(start.ram,21);
        const auto add=[&](const std::string& name,const state::SaveStateContainer& state,unsigned frames) {
            cases.push_back({name,state,record(*runtime,&state,kSeed,frames)});
            require(cases.back().original.size()==frames,name+" went upstairs too soon");
            return cases.back().original.back().ram;
        };
        // A state `frames` into a case, for the next step.
        const auto after=[&](const state::SaveStateContainer& from,unsigned frames) {
            begin(*runtime,&from);
            for(unsigned n=0;n<frames;++n) runtime->advance(route(kSeed,static_cast<unsigned>(from.frame_number)+n));
            return checkpoint(*runtime);
        };
        auto end=add("Orange hit",edited(start,[&](Ram& r){strike(r,orange[1]);}),120);
        require(get(end,0x1500+orange[1])!=15 && get(end,0x34)==2 && get(end,0x121e)!=get(start.ram,0x121e),"Orange hit fixture scored nothing");
        end=add("Green hit",edited(start,[&](Ram& r){strike(r,green[2]);}),120);
        require(get(end,0x36)==2,"Green hit fixture missed");
        for(bool is_orange:{true,false}) {
            // The last of a group: the group-cleared rules spawn its
            // controller, and 300 ticks later a new group comes out.
            const auto clear=edited(start,[&](Ram& r){
                if(is_orange) {clear_slot(r,orange[0]);clear_slot(r,orange[2]);set(r,0x34,1);strike(r,orange[1]);return;}
                const unsigned kept=get(r,0x18c0+green[1]);
                for(unsigned x:slots_of(r,21)) if(x!=green[1]) clear_slot(r,x);
                for(unsigned x:slots_of(r,25)) if(x!=kept) clear_slot(r,x);
                set(r,0x36,1);strike(r,green[1]);
            });
            const std::string group=is_orange?"Orange":"Green";
            end=add(group+" clear",clear,420);
            require(count_of(end,is_orange?15:21)==3 && !count_of(end,is_orange?67:68),group+" group did not respawn");
            // A lane sensor during the wait makes the group silver (5DDF).
            const auto waiting=after(clear,100);
            const auto controller=slots_of(waiting.ram,is_orange?67:68);
            require(controller.size()==1,group+" controller missing");
            const auto silver=edited(waiting,[&](Ram& r){set(r,0x1840+controller[0],2);});
            end=add("Silver "+group+" respawn",silver,120);
            const auto silvers=slots_of(end,is_orange?16:22);
            require(silvers.size()==3 && get(end,0x1840+silvers[0])==1,"Silver "+group+" group did not come out");
            const auto out=after(silver,120);
            end=add("Silver "+group+" clear",edited(out,[&](Ram& r){
                for(unsigned x:slots_of(r,is_orange?16:22)) if(x!=silvers[0]) clear_slot(r,x);
                set(r,is_orange?0x34:0x36,1);strike(r,silvers[0]);
            }),120);
            require(get(end,0x1500+silvers[0])!=(is_orange?16u:22u) && count_of(end,is_orange?67:68)==1,
                "Silver "+group+" clear did not start a new respawn");
        }
    }
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1.0,{},AlienMode::cabinet);
    runtime->load(path);
    unsigned compared=0;
    for(const auto& c:cases) compared+=compare(*runtime,&c.start,kSeed,c.original,c.name,owned_updates);
    return compared;
}
unsigned parity(const std::filesystem::path& path,unsigned seed,unsigned& owned_updates) {
    std::vector<Sample> original;
    {
        auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet);runtime->load(path);
        original=record(*runtime,nullptr,seed,4000);
    }
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1.0,{},AlienMode::cabinet);
    runtime->load(path);
    require(runtime->revision().find(kCabinetAliensRevision)!=std::string_view::npos,"Cabinet alien runtime revision lacks its marker");
    return compare(*runtime,nullptr,seed,original,"Seed "+std::to_string(seed),owned_updates);
}

// Longer native play: what the cabinet draws for these aliens is always the
// game's own state, in both halves; the away half keeps going; a quick state
// replays the same, private randomness included.
void native_play(std::span<const std::byte> rom,const std::filesystem::path& path) {
    assets::ActorAssets assets(rom);
    auto timeline=std::make_unique<render::TableAnimation>(rom);
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1.0,{},AlienMode::cabinet);
    runtime->load(path);
    bool valid=false;
    unsigned drawn_checks=0,away_moves=0,hits=0,away_spawns=0,crossings=0;
    Ram last{};
    std::optional<state::SaveStateContainer> saved;
    std::optional<Checkpoint> upstairs_state;
    std::vector<Ram> after_checkpoint;
    constexpr unsigned kFrames=12000,kCheckpoint=5000,kReplay=1500;
    for(unsigned n=0;n<kFrames;++n) {
        runtime->advance(route(0,n));
        const auto ram=runtime->work_ram();
        Ram now;std::copy(ram.begin(),ram.end(),now.begin());
        if(n==kCheckpoint) saved=runtime->save();
        if(n>kCheckpoint && n<=kCheckpoint+kReplay) after_checkpoint.push_back(now);
        if(!table_scene(ram)) {timeline->reset();valid=false;last=now;continue;}
        if(get(now,0xf6)&2) {last=now;continue;}
        const bool advance=valid && !(get(now,0x53)&0x80) && !(get(now,0x54)&0x80);
        timeline->update(assets,ram,advance);
        valid=true;
        const unsigned table=get(now,0x79);
        for(unsigned half=0;half<2;++half) {
            const bool active=half==table;
            const auto& drawn=timeline->actors(half);
            for(unsigned x=4;x<64;++x) {
                const unsigned type=field(now,active,0x3500,x);
                if(!gameplay::CabinetAliens::owns(static_cast<std::uint8_t>(type)) &&
                    !gameplay::CabinetAliens::owns(std::to_integer<std::uint8_t>(drawn[0x1500+x]))) continue;
                // During the bank exchange the timeline holds the last coherent banks.
                const unsigned caller=get(now,0x1fc)|(get(now,0x1fd)<<8);
                if(caller==0x89cf || caller==0x89d2 || caller==0x8a2a || caller==0x8a2d) continue;
                for(unsigned f=0;f<32;++f)
                    require(get(drawn,0x1500+f*64+x)==field(now,active,0x3500+f*64,x),"Frame "+std::to_string(n+1)+
                        " slot "+std::to_string(x)+" field "+std::to_string(f)+": drawn alien differs from the game's");
                ++drawn_checks;
            }
        }
        if(table==get(last,0x79) && table==1) {
            if(!upstairs_state && n>kCheckpoint) upstairs_state=checkpoint(*runtime);
            // The lower bank is stored while the ball is upstairs.
            for(unsigned x=4;x<64;++x) {
                const unsigned type=field(now,false,0x3500,x),before=field(last,false,0x3500,x);
                if(gameplay::CabinetAliens::owns(static_cast<std::uint8_t>(type)) && type==before &&
                    (field(now,false,0x3680,x)!=field(last,false,0x3680,x) || field(now,false,0x3880,x)!=field(last,false,0x3880,x))) ++away_moves;
                if((before==67 || before==68) && type!=before) ++away_spawns;
            }
        }
        if(table==0 && get(last,0x79)==0) for(unsigned x=4;x<64;++x) {
            const unsigned before=field(last,true,0x3500,x),type=field(now,true,0x3500,x);
            if(((before==15 || before==16) && type==72) || ((before==21 || before==22) && type==73)) ++hits;
        }
        if(table!=get(last,0x79) && get(last,0x79)<2) ++crossings;
        last=now;
    }
    runtime->restore(*saved);
    for(unsigned i=0;i<kReplay;++i) {
        runtime->advance(route(0,kCheckpoint+1+i));
        const auto ram=runtime->work_ram();
        require(std::equal(ram.begin(),ram.end(),after_checkpoint[i].begin()),"Quick state replay diverged at frame "+
            std::to_string(kCheckpoint+2+i));
    }
    // A group cleared just before the ball went up respawns while it is up,
    // and the cabinet draws the new group there.
    require(upstairs_state.has_value(),"Native play never stayed upstairs");
    const auto respawn=edited(*upstairs_state,[&](Ram& r) {
        for(unsigned x=4;x<64;++x) if(field(r,false,0x3500,x)==15) for(unsigned f=0;f<32;++f) put(r,false,0x3500+f*64,x,0);
        unsigned x=4;
        while(field(r,false,0x3500,x)) ++x;
        put(r,false,0x3500,x,67);put(r,false,0x3540,x,0x80);put(r,false,0x3880,x,5);put(r,false,0x3840,x,0);
        set(r,0x34,0);
    });
    begin(*runtime,&respawn);
    timeline->reset();
    bool drawn_upstairs=false;
    for(unsigned n=0;n<30;++n) {
        runtime->advance(route(0,static_cast<unsigned>(respawn.frame_number)+n));
        const auto ram=runtime->work_ram();
        Ram now;std::copy(ram.begin(),ram.end(),now.begin());
        timeline->update(assets,ram,n!=0);
        if(get(now,0x79)!=1) break;
        const auto& drawn=timeline->actors(0);
        unsigned shown=0;
        for(unsigned x=4;x<64;++x) shown+=field(now,false,0x3500,x)==15 && get(drawn,0x1500+x)==15 &&
            get(drawn,0x1680+x)==field(now,false,0x3680,x) && (get(drawn,0x1540+x)&0xa0)==0x80;
        drawn_upstairs|=shown==3 && get(now,0x34)==3;
    }
    require(drawn_upstairs,"A cleared orange group did not respawn and show while the ball was upstairs");
    std::cout<<"Native play: "<<drawn_checks<<" drawn/game alien checks, "<<away_moves<<" upstairs alien updates, "
        <<away_spawns<<" upstairs respawns, "<<hits<<" hits, "<<crossings<<" crossings\n";
    require(crossings>=6 && drawn_checks>20000 && away_moves>500,"Native play lacks coverage");
    require(hits>=1,"Native play hit no aliens");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        routine_checks(rom);
        unsigned compared=0,owned_updates=0;
        for(unsigned seed=0;seed<6;++seed) compared+=parity(path,seed,owned_updates);
        std::cout<<"Parity: "<<compared<<" frames match the original before the ball first goes upstairs ("
            <<owned_updates<<" alien field changes)\n";
        require(compared>1500 && owned_updates>500,"Parity routes lack coverage");
        unsigned hit_updates=0;
        const unsigned hit_frames=hit_parity(path,hit_updates);
        std::cout<<"Hit parity: "<<hit_frames<<" frames of hits, group clears and silver respawns match the original ("
            <<hit_updates<<" alien field changes)\n";
        native_play(rom,path);
        std::cout<<"PASS: cabinet aliens\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
