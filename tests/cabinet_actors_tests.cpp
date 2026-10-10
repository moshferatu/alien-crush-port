// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/gameplay/cabinet_actors.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace alien_crush;
namespace {
using Ram=std::array<std::byte,8192>;
using Scope=gameplay::CabinetActors::Scope;
void require(bool ok,const std::string& message) {if(!ok) throw std::runtime_error(message);}
unsigned get(const Ram& ram,unsigned p) {return std::to_integer<unsigned>(ram[p]);}
void set(Ram& ram,unsigned p,unsigned v) {ram[p]=static_cast<std::byte>(v&255);}
// Actor field ($3500+field) of slot x, in the active (0x1500) or stored (0x200) bank.
unsigned field(const Ram& ram,bool active,unsigned address,unsigned x) {return get(ram,(active?0x1500:0x200)+address-0x3500+x);}
void put(Ram& ram,bool active,unsigned address,unsigned x,unsigned v) {set(ram,(active?0x1500:0x200)+address-0x3500+x,v);}
Ram copy(std::span<const std::byte> ram) {Ram r;std::copy(ram.begin(),ram.end(),r.begin());return r;}
// Scripted play: the canonical start, then varied flipper patterns. Run
// pauses and resumes every 600 frames. The kIdle route presses nothing after
// the start.
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
struct Checkpoint {state::SaveStateContainer state;Ram ram;};
Checkpoint checkpoint(const ReferenceRuntime& runtime) {return {runtime.save(),copy(runtime.work_ram())};}
// A saved state with its work RAM edited.
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
void begin(ReferenceRuntime& runtime,const state::SaveStateContainer& start) {
    auto state=start;state.core_revision=runtime.revision();runtime.restore(state);
}
std::vector<unsigned> slots_of(const Ram& ram,unsigned type) {
    std::vector<unsigned> result;
    for(unsigned x=4;x<64;++x) if(get(ram,0x1500+x)==type) result.push_back(x);
    return result;
}
unsigned count_of(const Ram& ram,unsigned type) {return static_cast<unsigned>(slots_of(ram,type).size());}
unsigned free_slot(const Ram& ram) {for(unsigned x=4;x<60;++x) if(!get(ram,0x1500+x)) return x;throw std::runtime_error("No free actor slot");}
void clear_slot(Ram& ram,unsigned x) {for(unsigned f=0;f<32;++f) set(ram,0x1500+f*64+x,0);}
// Distinct (type, action) states the ball's half ran, for coverage.
using Coverage=std::set<std::pair<unsigned,unsigned>>;
void cover(Coverage& coverage,const gameplay::CabinetActors& owned,const Ram& before,const Ram& after) {
    if(get(before,0x79)>1 || get(before,0x79)!=get(after,0x79)) return;
    for(unsigned x=4;x<64;++x) {
        const unsigned type=get(before,0x1500+x);
        if(owned.owns(static_cast<std::uint8_t>(type))) coverage.insert({type,get(before,0x1840+x)});
    }
}

// The port's routines against the ROM's: in verification each update is
// predicted from the RAM the ROM routine starts with, and the ROM's RAM must
// equal the prediction where the port would stop (01:45C8, or where it hands
// the routine back to the ROM). Play itself is the original's.
unsigned shadow_routes(std::span<const std::byte> rom,const std::filesystem::path& path,Coverage& coverage) {
    gameplay::CabinetActors owned(rom,Scope::table);
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1.0,{},ActorMode::verify);
    runtime->load(path);
    unsigned upstairs=0;
    for(unsigned seed:{0u,1u,4u}) {
        runtime->reset();
        auto previous=copy(runtime->work_ram());
        for(unsigned n=0;n<12000;++n) {
            try {runtime->advance(route(seed,n));}
            catch(const std::exception& e) {throw std::runtime_error("Route "+std::to_string(seed)+" frame "+std::to_string(n+1)+": "+e.what());}
            const auto now=copy(runtime->work_ram());
            cover(coverage,owned,previous,now);
            upstairs+=get(now,0x79)==1;
            previous=now;
        }
    }
    require(upstairs>5000,"Shadow routes spent too little time upstairs");
    return upstairs;
}
// Edited states that set off the rarer sequences.
unsigned shadow_scenarios(std::span<const std::byte> rom,const std::filesystem::path& path,Coverage& coverage) {
    gameplay::CabinetActors owned(rom,Scope::table);
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1.0,{},ActorMode::verify);
    runtime->load(path);
    // Downstairs at frame 450 of route 1; upstairs 30 frames into its
    // longest visit there, which the same route keeps up for 1,352 frames.
    std::optional<Checkpoint> lower,upper;
    unsigned run=0,longest=0,start=0,best=0;
    for(unsigned n=0;n<13000;++n) {
        runtime->advance(route(1,n));
        const auto ram=copy(runtime->work_ram());
        if(n==450) lower=checkpoint(*runtime);
        if(get(ram,0x79)==1) {if(!run++) start=n;}
        else run=0;
        if(run>longest) {longest=run;best=start;}
    }
    require(lower && get(lower->ram,0x79)==0 && longest>1000,"Scenario checkpoints missing");
    runtime->reset();
    for(unsigned n=0;n<=best+30;++n) runtime->advance(route(1,n));
    upper=checkpoint(*runtime);
    unsigned frames=0;
    const auto scenario=[&](const std::string& name,const Checkpoint& from,unsigned seed,unsigned length,const std::function<void(Ram&)>& edit) {
        const auto state=edited(from,edit);
        begin(*runtime,state);
        auto previous=copy(runtime->work_ram());
        for(unsigned n=0;n<length;++n) {
            try {runtime->advance(route(seed,static_cast<unsigned>(state.frame_number)+n));}
            catch(const std::exception& e) {throw std::runtime_error(name+" frame "+std::to_string(n+1)+": "+e.what());}
            const auto now=copy(runtime->work_ram());
            cover(coverage,owned,previous,now);previous=now;++frames;
        }
    };
    // Upper table: the mouths, guard and riser behind their rule flags.
    scenario("Upper event flags",*upper,1,900,[](Ram& r){set(r,0x6f,get(r,0x6f)|0x2f);set(r,0x70,get(r,0x70)|8);});
    scenario("Brain opening",*upper,1,900,[](Ram& r){
        for(auto x:slots_of(r,7)) {set(r,0x1840+x,2);set(r,0x1a40+x,4);set(r,0x1b80+x,0x20);set(r,0x1bc0+x,0);}});
    scenario("Brain closing",*upper,1,300,[](Ram& r){
        for(auto x:slots_of(r,7)) {set(r,0x1840+x,4);set(r,0x1a40+x,4);set(r,0x1c40+x,3);set(r,0x1b80+x,0xe0);set(r,0x1bc0+x,0xff);}});
    scenario("Orbs hop",*upper,1,300,[](Ram& r){set(r,0x6b,get(r,0x6b)|1);});
    scenario("Orbs respawn",*upper,1,600,[](Ram& r){for(auto x:slots_of(r,19)) clear_slot(r,x);set(r,0x65,0);});
    scenario("Bumpers travel",*upper,1,300,[](Ram& r){set(r,0x82,get(r,0x82)|1);});
    scenario("Rebound",*upper,1,300,[](Ram& r){
        for(auto x:slots_of(r,9)) {set(r,0x1840+x,1);set(r,0x1b80+x,0xf0);set(r,0x1bc0+x,1);set(r,0x1b00+x,0x40);}});
    scenario("Rebound back",*upper,1,300,[](Ram& r){
        for(auto x:slots_of(r,9)) {set(r,0x1840+x,2);set(r,0x1b80+x,0x10);set(r,0x1bc0+x,0xfe);set(r,0x1b00+x,0x40);set(r,0x1c40+x,4);}});
    scenario("Timer lamp",*upper,1,900,[](Ram& r){set(r,0x6f,get(r,0x6f)|0x80);});
    scenario("Targets reset",*upper,1,300,[](Ram& r){
        set(r,0x6c,get(r,0x6c)|4);for(unsigned t:{8u,37u,38u,39u}) for(auto x:slots_of(r,t)) set(r,0x1c40+x,3);});
    scenario("Upper indicator and sensor",*upper,1,300,[](Ram& r){
        set(r,0x55,2);set(r,0x4a,1);for(auto x:slots_of(r,74)) set(r,0x1840+x,1);
        for(unsigned t:{24u,35u}) for(auto x:slots_of(r,t)) set(r,0x1840+x,1);});
    scenario("Brain flyers",*upper,1,400,[](Ram& r){
        for(unsigned i=0;i<4;++i) {const unsigned x=free_slot(r);set(r,0x1500+x,55);set(r,0x15c0+x,0x80);set(r,0x1680+x,0x60+i*8);set(r,0x18c0+x,i*2+2);}});
    scenario("Upper explosions",*upper,1,100,[](Ram& r){
        for(unsigned t:{59u,60u,50u}) {const unsigned x=free_slot(r);set(r,0x1500+x,t);set(r,0x15c0+x,0x80);set(r,0x1680+x,0x80);}});
    // Lower table.
    scenario("Lower mouth",*lower,kIdle,1200,[](Ram& r){set(r,0x6d,get(r,0x6d)|8);});
    scenario("Lower risers",*lower,kIdle,600,[](Ram& r){set(r,0x6d,get(r,0x6d)|7);});
    scenario("Cocoons hatch",*lower,kIdle,900,[](Ram& r){for(unsigned t:{26u,27u}) for(auto x:slots_of(r,t)) set(r,0x1840+x,2);});
    scenario("Mouth flyers",*lower,kIdle,400,[](Ram& r){
        for(unsigned i=0;i<3;++i) {const unsigned x=free_slot(r);set(r,0x1500+x,56);set(r,0x15c0+x,0x80);set(r,0x1680+x,0x50);}});
    scenario("Lower explosions",*lower,kIdle,100,[](Ram& r){
        for(unsigned t:{50u,59u,60u}) {const unsigned x=free_slot(r);set(r,0x1500+x,t);set(r,0x15c0+x,0x80);set(r,0x1680+x,0x80);}
        set(r,0xdb,0x80);});
    scenario("Lower indicators and sensors",*lower,kIdle,300,[](Ram& r){
        set(r,0x5f,1);set(r,0x5c,1);set(r,0x62,2);set(r,0x4d,1);set(r,0x4c,1);set(r,0x4e,1);
        for(unsigned t:{75u,76u,77u}) for(auto x:slots_of(r,t)) set(r,0x1840+x,1);});
    scenario("Bouncing orbs hidden",*lower,kIdle,200,[](Ram& r){
        for(auto x:slots_of(r,70)) {set(r,0x1840+x,0);set(r,0x1880+x,10);set(r,0x1540+x,get(r,0x1540+x)|0x60);}});
    // Hits the ball physics would leave (bit 7 of $3AC0), which the ROM
    // handles from the port's resume point.
    scenario("Alien hits",*lower,kIdle,420,[](Ram& r){
        for(unsigned t:{15u,21u}) {
            const auto s=slots_of(r,t);if(s.empty()) continue;
            const unsigned x=s[0];set(r,0x1840+x,t==15?3:2);set(r,0x1880+x,100);
            set(r,0x1540+x,(get(r,0x1540+x)|0x80)&0xbf);set(r,0x1ac0+x,0x80);
        }});
    return frames;
}

// The stored half, one routine at a time: what the port changes there and
// what it leaves alone.
void stored_half(std::span<const std::byte> rom) {
    std::uint64_t frame=100;
    const auto swap=[](Ram& ram) {for(unsigned f=0;f<32;++f) for(unsigned s=4;s<64;++s) std::swap(ram[0x1500+f*64+s],ram[0x200+f*64+s]);};
    // Everything in reach of the stored half: never the ball's bank or slots
    // 0..3, the game's RNG, sound, score or zero-page temporaries.
    const auto untouched=[](const Ram& before,const Ram& after,const std::string& what) {
        for(unsigned p=0x1500;p<0x1d00;++p) require(before[p]==after[p],what+" changed the ball's bank");
        for(unsigned f=0;f<32;++f) for(unsigned s=0;s<4;++s) require(before[0x200+f*64+s]==after[0x200+f*64+s],what+" wrote beside the stored bank");
        for(unsigned p:{0x1217u,0x1218u,0x121eu,0x1224u,0x122au,0x73u,0x06u,0x07u,0x8eu,0x8fu,0x92u})
            require(before[p]==after[p],what+" changed RNG, score, sound or a temporary");
        for(unsigned p=0;p<0x0c;++p) require(before[p]==after[p],what+" wrote a zero-page temporary");
    };
    // A real table's stored actors, from a fresh game.
    // Cocoons hatch there: an alien unfolds and flies off the slingshot, all
    // in the stored bank. (Without the hand-over at the seam, which gives it
    // to the ball's bank: cocoon_alien_tests.cpp.)
    {
        gameplay::CabinetActors actors(rom,Scope::table,true,true,false);
        Ram ram{};set(ram,0x79,1);set(ram,0x1217,0x35);set(ram,0x1218,0x7a);
        const unsigned cocoon=6;
        put(ram,false,0x3500,cocoon,26);put(ram,false,0x35c0,cocoon,0x40);put(ram,false,0x3680,cocoon,0xa0);
        actors.update_inactive(ram,frame++);
        put(ram,false,0x3840,cocoon,2);
        std::set<unsigned> seen;
        for(unsigned i=0;i<400;++i) {
            const auto before=ram;actors.update_inactive(ram,frame++);untouched(before,ram,"A hatching cocoon");
            for(unsigned x=4;x<64;++x) if(field(ram,false,0x3500,x)) seen.insert(field(ram,false,0x3500,x));
        }
        require(seen.count(40) && seen.count(52),"Stored cocoon did not hatch an alien that took off");
        require(field(ram,false,0x3840,cocoon)==0 && !(field(ram,false,0x3540,cocoon)&0x60),
            "Stored cocoon did not come back after 180 ticks");
    }
    // The lower mouth's cycle, set off by $6D bit 3: opening waits for the
    // ball's half (it scores and releases flyers); the hold, lamp and close
    // continue in the stored half.
    {
        gameplay::CabinetActors actors(rom,Scope::table);
        Ram ram{};set(ram,0x79,1);
        const unsigned mouth=10;
        put(ram,false,0x3500,mouth,20);
        actors.update_inactive(ram,frame++);
        set(ram,0x6d,8);actors.update_inactive(ram,frame++);
        require(field(ram,false,0x3840,mouth)==1,"Stored mouth did not open on $6D bit 3");
        for(unsigned i=0;i<40;++i) actors.update_inactive(ram,frame++);
        const auto open=ram;actors.update_inactive(ram,frame++);
        require(ram==open && field(ram,false,0x3840,mouth)==1,"Stored mouth scored or released flyers away from the ball");
        // As the ROM leaves it once open: holding for 600 ticks.
        put(ram,false,0x3840,mouth,2);put(ram,false,0x3880,mouth,2);put(ram,false,0x38c0,mouth,0);
        set(ram,0xd6,0x10);set(ram,0x6d,0x88);
        actors.update_inactive(ram,frame++);actors.update_inactive(ram,frame++);
        require(field(ram,false,0x3840,mouth)==4 && !(get(ram,0xd6)&0x10) && !(get(ram,0x6d)&0x80),
            "Stored mouth's hold did not end with its lamp off");
        for(unsigned i=0;i<20;++i) actors.update_inactive(ram,frame++);
        require(field(ram,false,0x3840,mouth)==0 && !(get(ram,0x6d)&8),"Stored mouth did not close");
    }
    // Rules, points and the ball wait: the lower skull guard's retreat
    // scores, and so stays up until the ball comes down; the risers' ball
    // checks find no ball.
    {
        gameplay::CabinetActors actors(rom,Scope::table);
        Ram ram{};set(ram,0x79,1);
        const unsigned guard=12,riser=13;
        put(ram,false,0x3500,guard,30);put(ram,false,0x3500,riser,29);
        actors.update_inactive(ram,frame++);
        set(ram,0x6d,5);
        for(unsigned i=0;i<60;++i) actors.update_inactive(ram,frame++);
        require(field(ram,false,0x3840,guard)==2 && field(ram,false,0x3840,riser)==2,"Stored guard and riser did not rise");
        set(ram,0x6d,0);
        const auto up=ram;
        for(unsigned i=0;i<60;++i) actors.update_inactive(ram,frame++);
        require(ram==up,"Stored guard or riser scored away from the ball");
        // The ball's half picks them up where they wait.
        swap(ram);set(ram,0x79,0);
        require(!actors.update_active(ram,guard,frame).has_value(),"The ROM does not run the ball's half");
    }
    // The upper riser drops once the ball leaves the upper table and its flag
    // ($6F bit 5) clears, with the 20 points and progress step the ROM gives
    // on the ball's return (5AC8, 03:911B); -actors1 runtimes wait.
    for(const bool lowers:{true,false}) for(const unsigned progress:{3u,9u}) {
        gameplay::CabinetActors actors(rom,Scope::table,true,lowers);
        Ram ram{};set(ram,0x79,0);set(ram,0x4a,progress);set(ram,0xd7,0x80);set(ram,0x121e,0x40);
        const unsigned riser=7;
        put(ram,false,0x3500,riser,32);put(ram,false,0x3540,riser,0x88);put(ram,false,0x3840,riser,2);
        put(ram,false,0x3a40,riser,4);put(ram,false,0x3b80,riser,0x20);put(ram,false,0x3c40,riser,3);
        put(ram,false,0x3ac0,riser,0x80); // Touched before the ball left.
        const auto up=ram;
        actors.update_inactive(ram,frame++);
        if(!lowers) {require(ram==up,"-actors1 riser did not wait for the ball");continue;}
        require(field(ram,false,0x3840,riser)==3 && field(ram,false,0x3bc0,riser)==0xff && get(ram,0x121e)==0x54,
            "Stored riser did not start down with its points");
        if(progress==9) require(get(ram,0x4a)==0 && get(ram,0x4b)==1 && (get(ram,0x6b)&0x80) && get(ram,0xd8)!=0,
            "Stored riser's ninth progress step did not light its lamp");
        else require(get(ram,0x4a)==4 && get(ram,0x55)==1,"Stored riser did not step the upper progress");
        for(unsigned i=0;i<20;++i) actors.update_inactive(ram,frame++);
        require(field(ram,false,0x3840,riser)==0 && !(get(ram,0xd7)&0x80) && !field(ram,false,0x3ac0,riser),
            "Stored riser did not come down with its lamp off");
    }
    // The upper skull guard drops as the ball leaves (03:8A26 sets $70 bit 1),
    // even when the ball touched it on the way out: 02:63DB never reads the
    // hit, it only clears it.
    {
        gameplay::CabinetActors actors(rom,Scope::table);
        Ram ram{};set(ram,0x79,0);set(ram,0x70,0x0a);
        const unsigned guard=8;
        put(ram,false,0x3500,guard,14);put(ram,false,0x3540,guard,0xa0);put(ram,false,0x3840,guard,2);
        put(ram,false,0x3ac0,guard,0x80);
        actors.update_inactive(ram,frame++);
        require(field(ram,false,0x3840,guard)==0 && (field(ram,false,0x3540,guard)&0x60)==0x60 && !(get(ram,0x70)&0x0a),
            "Stored skull guard touched on the way out did not drop");
    }
    // A pending hit is the ball's half's.
    {
        gameplay::CabinetActors actors(rom,Scope::table);
        Ram ram{};set(ram,0x79,0);
        put(ram,false,0x3500,20,70);actors.update_inactive(ram,frame++);
        put(ram,false,0x3840,20,1);put(ram,false,0x3ac0,20,0x80);
        const auto pending=ram;actors.update_inactive(ram,frame++);
        require(ram==pending,"A hit was processed away from the ball");
    }
    // Upper actors while the ball is downstairs: the brain orbs keep their
    // wobble and rests with private randomness, their controller brings a
    // group back after 300 ticks, and the bouncing orbs keep bouncing.
    {
        gameplay::CabinetActors actors(rom,Scope::table);
        Ram ram{};set(ram,0x79,0);set(ram,0x1217,0x21);set(ram,0x1218,0x43);
        put(ram,false,0x3500,4,66);put(ram,false,0x3500,5,71);put(ram,false,0x3680,5,0x90);
        unsigned orbs=0,bobs=0;
        for(unsigned i=0;i<900;++i) {
            const auto before=ram;actors.update_inactive(ram,frame++);untouched(before,ram,"The upper table");
            unsigned count=0;for(unsigned x=4;x<64;++x) count+=field(ram,false,0x3500,x)==19;
            orbs=std::max(orbs,count);
            bobs+=field(before,false,0x3680,5)!=field(ram,false,0x3680,5);
        }
        require(orbs==5 && get(ram,0x65)==5,"Stored orb controller did not bring five orbs");
        require(bobs>100,"Stored bouncing orb did not bounce");
        unsigned moves=0;
        for(unsigned i=0;i<600;++i) {
            const auto before=ram;actors.update_inactive(ram,frame++);
            for(unsigned x=4;x<64;++x) if(field(ram,false,0x3500,x)==19) moves+=field(before,false,0x35c0,x)!=field(ram,false,0x35c0,x);
        }
        require(moves>50,"Stored brain orbs did not wobble");
    }
    // One update per frame: after the inactive pass, the exchange and the
    // table's second dispatch (03:89D8) skip what already ran.
    {
        gameplay::CabinetActors actors(rom,Scope::table);
        Ram ram{};set(ram,0x79,1);
        put(ram,false,0x3500,9,70);put(ram,false,0x3540,9,0x80);put(ram,false,0x3840,9,0);put(ram,false,0x3880,9,20);
        actors.update_inactive(ram,frame);
        swap(ram);set(ram,0x79,0);
        const auto next=actors.update_active(ram,9,frame);
        require(next && next->pc==0,"An actor ran twice in the frame the ball came down");
        require(!actors.update_active(ram,9,frame+1),"The ROM did not take the actor back the next frame");
    }
    // The stored half's palette scripts step each vblank, never the ball's
    // half's, and only while the table runs them ($68 bit 0).
    {
        gameplay::CabinetActors actors(rom,Scope::table);
        Ram ram{};set(ram,0x79,1);set(ram,0x68,1);
        for(unsigned slot:{5u,40u}) {
            set(ram,0x1245+slot,1);set(ram,0x1305+slot,0x40);set(ram,0x1405+slot,4);
        }
        for(unsigned i=0;i<8;++i) actors.update_palettes(ram);
        require(get(ram,0x13c5+5)==2 && get(ram,0x13c5+40)==0,"Palette scripts stepped in the wrong half");
        set(ram,0x68,0);const auto idle=ram;actors.update_palettes(ram);
        require(ram==idle,"Palette scripts ran with the table's scheduler off");
        // Script 2, a flash, ends by clearing its slot.
        set(ram,0x68,1);set(ram,0x1245+6,2);set(ram,0x1285+6,3);set(ram,0x1305+6,0x40);set(ram,0x1405+6,2);
        for(unsigned i=0;i<3;++i) actors.update_palettes(ram);
        require(!get(ram,0x1245+6) && !get(ram,0x1405+6),"Flash did not end");
    }
    // The -aliens1 runtimes kept the lower aliens native in the ball's half.
    {
        gameplay::CabinetActors legacy(rom,Scope::aliens);
        require(legacy.owns(15) && !legacy.owns(70),"Legacy scope differs");
        Ram ram{};const unsigned x=9;
        put(ram,true,0x3500,x,15);put(ram,true,0x35c0,x,214);put(ram,true,0x3680,x,90);
        const auto next=legacy.update_active(ram,x,frame++);
        require(next && next->pc==0 && field(ram,true,0x3540,x)==0xc0,"Legacy alien did not run in the ball's half");
    }
}

// Cabinet play: the cabinet draws each bank as the game holds it, in both
// halves, and the stored half keeps moving.
void native_play(std::span<const std::byte> rom,const std::filesystem::path& path) {
    assets::ActorAssets assets(rom);
    gameplay::CabinetActors owned(rom,Scope::table);
    auto timeline=std::make_unique<render::TableAnimation>(rom);
    auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1.0,{},ActorMode::cabinet);
    runtime->load(path);
    require(runtime->revision().find(kCabinetActorsRevision)!=std::string_view::npos,"Cabinet runtime revision lacks its marker");
    unsigned drawn_checks=0,crossings=0,away_palette=0;
    std::map<unsigned,unsigned> away_updates;
    Ram last=copy(runtime->work_ram());
    std::optional<state::SaveStateContainer> saved;
    std::optional<Checkpoint> upstairs;
    std::vector<Ram> after_checkpoint;
    constexpr unsigned kFrames=12000,kCheckpoint=5000,kReplay=1500;
    for(unsigned n=0;n<kFrames;++n) {
        runtime->advance(route(0,n));
        const auto ram=runtime->work_ram();
        const auto now=copy(ram);
        if(n==kCheckpoint) saved=runtime->save();
        if(n>kCheckpoint && n<=kCheckpoint+kReplay) after_checkpoint.push_back(now);
        if(!table_scene(ram)) {timeline->reset();last=now;continue;}
        timeline->update(assets,ram);
        const unsigned table=get(now,0x79);
        const unsigned caller=get(now,0x1fc)|(get(now,0x1fd)<<8);
        const bool exchanging=caller==0x89cf || caller==0x89d2 || caller==0x8a2a || caller==0x8a2d;
        for(unsigned half=0;half<2 && !exchanging;++half) {
            const bool active=half==table;
            const auto& drawn=timeline->actors(half);
            for(unsigned x=4;x<64;++x) {
                const unsigned type=field(now,active,0x3500,x);
                if(type==2 || type==3) continue; // The stored half's flippers follow the controls.
                for(unsigned f=0;f<32;++f)
                    require(get(drawn,0x1500+f*64+x)==field(now,active,0x3500+f*64,x),"Frame "+std::to_string(n+1)+
                        " slot "+std::to_string(x)+" field "+std::to_string(f)+": drawn actor differs from the game's");
                ++drawn_checks;
            }
        }
        if(table==get(last,0x79) && table<=1) {
            if(table==1 && !upstairs && n>kCheckpoint) upstairs=checkpoint(*runtime);
            for(unsigned x=4;x<64;++x) {
                const unsigned type=field(now,false,0x3500,x);
                if(!owned.owns(static_cast<std::uint8_t>(type)) || type!=field(last,false,0x3500,x)) continue;
                bool changed=false;
                for(unsigned f=0;f<32;++f) changed|=field(now,false,0x3500+f*64,x)!=field(last,false,0x3500+f*64,x);
                away_updates[type]+=changed;
            }
            for(unsigned s=(1-table)*32;s<(2-table)*32;++s) away_palette+=get(now,0x13c5+s)!=get(last,0x13c5+s);
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
    require(upstairs.has_value(),"Cabinet play never stayed upstairs");
    const auto respawn=edited(*upstairs,[&](Ram& r) {
        for(unsigned x=4;x<64;++x) if(field(r,false,0x3500,x)==15) for(unsigned f=0;f<32;++f) put(r,false,0x3500+f*64,x,0);
        unsigned x=4;
        while(field(r,false,0x3500,x)) ++x;
        put(r,false,0x3500,x,67);put(r,false,0x3540,x,0x80);put(r,false,0x3880,x,5);put(r,false,0x3840,x,0);
        set(r,0x34,0);
    });
    begin(*runtime,respawn);
    timeline->reset();
    bool drawn_upstairs=false;
    for(unsigned n=0;n<30;++n) {
        runtime->advance(route(0,static_cast<unsigned>(respawn.frame_number)+n));
        const auto now=copy(runtime->work_ram());
        timeline->update(assets,runtime->work_ram());
        if(get(now,0x79)!=1) break;
        const auto& drawn=timeline->actors(0);
        unsigned shown=0;
        for(unsigned x=4;x<64;++x) shown+=field(now,false,0x3500,x)==15 && get(drawn,0x1500+x)==15 &&
            get(drawn,0x1680+x)==field(now,false,0x3680,x) && (get(drawn,0x1540+x)&0xa0)==0x80;
        drawn_upstairs|=shown==3 && get(now,0x34)==3;
    }
    require(drawn_upstairs,"A cleared orange group did not respawn and show while the ball was upstairs");
    std::cout<<"Cabinet play: "<<drawn_checks<<" drawn/game actor checks, "<<crossings<<" crossings, "
        <<away_palette<<" stored palette steps; stored half updates by type:";
    for(auto [type,count]:away_updates) if(count) std::cout<<" "<<type<<"x"<<count;
    std::cout<<"\n";
    require(crossings>=6 && drawn_checks>100000,"Cabinet play lacks coverage");
    for(unsigned type:{15u,21u,19u,70u,71u})
        require(away_updates[type]>50,"Stored half's type "+std::to_string(type)+" hardly moved");
    require(away_palette>100,"Stored half's palette scripts did not run");
}

// Runtimes marked -aliens1 (the lower aliens native in the ball's half as
// well) still play as they did: the original's play until the ball first
// goes upstairs, compared where both games wait for vblank.
unsigned legacy_aliens(const std::filesystem::path& path) {
    struct Sample {Ram ram;bool waiting;};
    const auto sample=[](const ReferenceRuntime& runtime) {
        const auto pc=runtime.video_snapshot().pc;
        return Sample{copy(runtime.work_ram()),pc==0xe2fa || pc==0xe2fc};
    };
    unsigned compared=0;
    for(unsigned seed:{0u,3u}) {
        std::vector<Sample> original;
        {
            auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet);runtime->load(path);
            for(unsigned n=0;n<4000;++n) {
                runtime->advance(route(seed,n));original.push_back(sample(*runtime));
                if(get(original.back().ram,0x79)==1 && table_scene(runtime->work_ram())) break;
            }
        }
        auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::cabinet,1.0,{},ActorMode::cabinet_aliens);
        runtime->load(path);
        require(runtime->revision().find(kCabinetAliensRevision)!=std::string_view::npos &&
            recorded_actor_mode(runtime->revision())==ActorMode::cabinet_aliens,"Legacy runtime revision lacks its marker");
        for(unsigned n=0;n<original.size();++n) {
            runtime->advance(route(seed,n));
            const auto now=sample(*runtime);
            if(!now.waiting || !original[n].waiting) continue;
            ++compared;
            for(unsigned p=0;p<8192;++p) {
                if(p>=0x100 && p<0x200) continue;
                require(now.ram[p]==original[n].ram[p],"Legacy aliens route "+std::to_string(seed)+" frame "+
                    std::to_string(n+1)+" RAM "+std::to_string(p)+" differs");
            }
        }
    }
    require(compared>1500,"Legacy alien routes lack coverage");
    return compared;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        stored_half(rom);
        std::cout<<"Stored half: routines checked\n";
        Coverage coverage;
        const unsigned upstairs=shadow_routes(rom,path,coverage);
        const unsigned scenario_frames=shadow_scenarios(rom,path,coverage);
        std::set<unsigned> types;for(auto [type,action]:coverage) types.insert(type);
        std::cout<<"Shadow verification: 36000 route frames ("<<upstairs<<" upstairs) and "<<scenario_frames
            <<" scenario frames match the ROM; "<<coverage.size()<<" type/action states over "<<types.size()<<" types\n";
        for(unsigned type:{7u,8u,9u,10u,11u,12u,13u,14u,15u,19u,20u,21u,24u,26u,27u,29u,30u,32u,33u,34u,35u,37u,
            40u,41u,42u,44u,45u,46u,47u,48u,50u,51u,52u,53u,55u,56u,59u,60u,66u,70u,71u,72u,73u,74u,75u,76u,77u})
            require(types.count(type),"Shadow verification never ran type "+std::to_string(type));
        require(coverage.size()>=140,"Shadow verification lacks coverage");
        native_play(rom,path);
        std::cout<<"Legacy aliens: "<<legacy_aliens(path)<<" frames match the original before the ball first goes upstairs\n";
        std::cout<<"PASS: cabinet actors\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
