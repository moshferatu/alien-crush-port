// SPDX-License-Identifier: GPL-3.0-or-later
// Private to src/gameplay/cabinet_actors*.cpp: one actor bank of work RAM and
// the ROM services the actor routines call, for gameplay::CabinetActors.
#pragma once
#include "alien_crush/gameplay/cabinet_actors.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>
namespace alien_crush::gameplay::cabinet {
// The rest of this update belongs to the ROM, from `pc` (with A = `a` when
// it is not negative). Only thrown in the ball's half.
struct Resume {std::uint16_t pc;int a;};
// This update needs the ball, the game's rules, a score or the screen: the
// actor waits, unchanged, until its half is the ball's again.
struct Wait {};

// The ROM banks the routines read: 00 at $E000, 01 at $4000, 02 at $6000,
// 03 at $8000 and 04 at $A000.
struct Rom {
    std::array<std::array<std::byte,8192>,5> banks{};
    unsigned operator()(unsigned bank,unsigned address) const {
        return std::to_integer<unsigned>(banks[bank][address&0x1fff]);
    }
    unsigned word(unsigned bank,unsigned address) const {return (*this)(bank,address)|((*this)(bank,address+1)<<8);}
    // A byte by CPU address with the table's mapping.
    unsigned at(unsigned address) const {
        switch(address>>13) {
        case 2: return (*this)(1,address);
        case 3: return (*this)(2,address);
        case 4: return (*this)(3,address);
        case 5: return (*this)(4,address);
        case 7: return (*this)(0,address);
        default: return 0;
        }
    }
};

// One actor bank in work RAM, addressed like the ROM: $3500+field+X. The
// active bank is the ROM's own; the stored one is $1300 lower (01:46FD).
// In the stored bank every write is logged, so an update that has to wait
// leaves no trace.
class Bank {
public:
    // `legacy`: the -aliens1 runtimes, whose aliens ran in the ball's half
    // too, finishing their sound calls last.
    Bank(const Rom& rom,std::span<std::byte> ram,bool active,bool legacy=false)
        :rom_(rom),ram_(ram),base_(active?0x1500:0x200),active_(active),legacy_(legacy) {}
    bool legacy() const {return legacy_;}
    // Each update starts its own log and its own private random draws.
    void begin() {log_.clear();draws_=0;}
    bool active() const {return active_;}
    const Rom& rom() const {return rom_;}
    unsigned operator()(unsigned field,unsigned x) const {return std::to_integer<unsigned>(ram_[at(field,x)]);}
    void put(unsigned field,unsigned x,unsigned value) {write(at(field,x),value);}
    // Zero page and $2000..$3FFF, shared by both halves.
    unsigned global(unsigned address) const {return std::to_integer<unsigned>(ram_[address&0x1fff]);}
    void set_global(unsigned address,unsigned value) {write(address&0x1fff,value);}
    void set_bits(unsigned address,unsigned bits) {set_global(address,global(address)|bits);}
    void clear_bits(unsigned address,unsigned bits) {set_global(address,global(address)&~bits);}
    // Zero-page temporaries of the ROM routines, kept for exact play in the
    // ball's half. The stored half has no ROM routine to leave them.
    void scratch(unsigned address,unsigned value) {if(active_) set_global(address,value);}
    // $69 carries this frame's events for the ball's half (01:4815 clears it).
    unsigned events() const {return active_?global(0x69):0;}
    void rollback() {
        for(auto it=log_.rbegin();it!=log_.rend();++it) ram_[it->first]=it->second;
        log_.clear();
    }
    void commit() {log_.clear();}

    [[noreturn]] void resume(std::uint16_t pc,int a=-1) const {
        if(active_) throw Resume{pc,a};
        throw Wait{};
    }
    [[noreturn]] void wait() const {throw Wait{};}
    // 00:EA62 with sound $73. In the ball's half the ROM plays it from the
    // routine's own JSR (or tail JMP) at `pc` and runs the rest; the stored
    // half is silent and carries on.
    void sound(unsigned id,std::uint16_t pc) {
        if(!active_) return;
        set_global(0x73,id);
        throw Resume{pc,static_cast<int>(id&255)};
    }
    // 00:EA7B: the actor type's own sound (table 00:EA86), from the
    // routine's JSR or JMP at `pc`.
    void type_sound(std::uint16_t pc) {if(active_) throw Resume{pc,-1};}

    // 00:F9A2. The stored half draws from RAM without changing it, so the
    // game's own sequence stays the original's while its half is away.
    unsigned random(unsigned x) {
        const auto step=[](unsigned state) {return (state+(((state+0x503f)&65535)*2))&65535;};
        const unsigned lo=global(0x3217),state=lo|(global(0x3218)<<8);
        if(!active_) {
            const unsigned seed=state^(global(0x0e)*0x0101)^(x*0x3b1d)^(draws_*0x9e37)^
                (std::to_integer<unsigned>(ram_[0x1580])<<8)^std::to_integer<unsigned>(ram_[0x1640]);
            ++draws_;
            const unsigned mixed=step(step(seed&65535));
            return ((mixed&255)+(mixed>>8))&255;
        }
        const unsigned mixed=step(state),result=(lo+(mixed>>8))&255;
        set_global(0x008e,mixed);set_global(0x008f,mixed>>8);
        set_global(0x3217,mixed);set_global(0x3218,result);
        return result;
    }
    // 00:F993: the high byte of random * range (00:F6A3 multiplies).
    unsigned ranged(unsigned x,unsigned range) {
        const unsigned product=random(x)*range;
        scratch(0x04,product);scratch(0x05,product>>8);
        return product>>8;
    }

    // 00:FADA: Y motion (FB03), X motion (FB59), Y acceleration (FBB6) and
    // X acceleration (FC10), as $3A00 bits 0, 1, 3 and 4 select. Bit 0 of
    // $3540 marks the actor off screen.
    void move(unsigned x) {
        if((*this)(0x3a00,x)&0x01) axis(x,0x3640,0x3780,0x140,true);
        if((*this)(0x3a00,x)&0x02) axis(x,0x3580,0x3700,0x120,false);
        if((*this)(0x3a00,x)&0x08) accelerate(x,0x3780,0x3b40,0x3980,0x40);
        if((*this)(0x3a00,x)&0x10) accelerate(x,0x3700,0x3b00,0x3900,0x04);
    }
    // 00:FD35: the phase steps between 0 and $3A40, reflecting at both ends.
    void oscillate(unsigned x) {
        const unsigned sum=(*this)(0x3c00,x)+(*this)(0x3b80,x);
        put(0x3c00,x,sum);
        put(0x3c40,x,(*this)(0x3c40,x)+(*this)(0x3bc0,x)+(sum>>8));
        const auto negate=[&] {
            const unsigned step=(65536-((*this)(0x3b80,x)|((*this)(0x3bc0,x)<<8)))&65535;
            put(0x3b80,x,step);put(0x3bc0,x,step>>8);
        };
        if(!((*this)(0x3bc0,x)&0x80)) {
            if((*this)(0x3c40,x)==(*this)(0x3a40,x)) {
                negate();put(0x3c40,x,(*this)(0x3c40,x)-1);put(0x3c00,x,0);
            }
        } else if(!(*this)(0x3c40,x) || ((*this)(0x3c40,x)&0x80)) {
            negate();put(0x3c00,x,0);put(0x3c40,x,0);
        }
    }
    // 00:FD8F: the pose for the phase, from the type's sequence.
    void pose(unsigned x) {
        const unsigned type=((*this)(0x3500,x)-1)&255;
        show(x,rom_(4,0xbdf7+type)|(rom_(4,0xbe51+type)<<8));
    }
    void animate(unsigned x) {oscillate(x);pose(x);} // 00:FD2B
    // 00:FD30: FD35, then the pose from the sequence at `pointer` ($04/$05).
    void animate(unsigned x,unsigned pointer) {oscillate(x);show(x,pointer);}
    // 00:FDA8: the dynamic pattern for the phase ($3A80), for 00:FD1D.
    void pattern(unsigned x) {
        const unsigned type=((*this)(0x3500,x)-1)&255;
        const unsigned pointer=rom_(4,0xbdf7+type)|(rom_(4,0xbe51+type)<<8);
        scratch(0x04,pointer);scratch(0x05,pointer>>8);
        put(0x3a80,x,rom_.at((pointer+(*this)(0x3c40,x))&0xffff));
    }

    // 01:46AD clears all 32 fields.
    void remove(unsigned x) {
        for(unsigned field=0;field<32;++field) put(0x3500+field*64,x,0);
        scratch(0x02,0);scratch(0x03,0x3d);
    }
    // 01:46DE: the first free slots from 4. It stops at $3C only while
    // searching, as the ROM does; $06.. list the slots.
    bool allocate(unsigned count,std::array<unsigned,6>& slots) {
        unsigned found=0,y=4;
        scratch(0x01,count);
        for(;;) {
            if(!(*this)(0x3500,y)) {
                slots[found]=y;scratch(0x06+found,y);++found;
                y=(y+1)&255;scratch(0x01,count-found);
                if(found==count) return true;
                continue;
            }
            y=(y+1)&255;
            if(y==0x3c) return false;
        }
    }
    // 01:46CE: the first free slot from 4 to $3B, or none.
    int free_slot() const {
        for(unsigned y=4;y<0x3c;++y) if(!(*this)(0x3500,y)) return static_cast<int>(y);
        return -1;
    }

    // Palette scheduler requests, 00:EF04..EF91. Slots are fixed per half,
    // so they are the same for either bank.
    unsigned palette(unsigned field,unsigned slot) const {return global(field+(slot&255));}
    void set_palette(unsigned field,unsigned slot,unsigned value) {set_global(field+(slot&255),value);}
    void palette_start(unsigned slot) { // EF41
        slot&=255;
        if(palette(0x3245,slot)) return;
        for(auto [field,table]:{std::pair{0x3245u,0xefd2u},{0x3405u,0xf012u},{0x3305u,0xf052u},
            {0x3345u,0xf092u},{0x33c5u,0xf0d2u},{0x32c5u,0xf112u}})
            set_palette(field,slot,rom_(0,table+slot));
    }
    void palette_idle(unsigned slot) { // EF79
        slot&=255;set_palette(0x3245,slot,8);set_palette(0x33c5,slot,rom_(0,0xf152+slot));
    }
    void palette_rate(unsigned slot) { // EF6C
        slot&=255;
        if(palette(0x3245,slot)) set_palette(0x3285,slot,rom_(0,0xef92+slot));
    }
    void palette_reveal(unsigned slot) { // EF21
        slot&=255;set_palette(0x3245,slot,2);set_palette(0x3285,slot,0x20);set_palette(0x33c5,slot,0);
        set_palette(0x3345,slot,0);set_palette(0x3305,slot,0x80);set_palette(0x3405,slot,8);
    }
    void palette_cleared(unsigned slot) { // EF04
        slot&=255;set_palette(0x3245,slot,2);set_palette(0x3285,slot,0x10);set_palette(0x33c5,slot,0);
        set_palette(0x3305,slot,0);set_palette(0x3345,slot,1);set_palette(0x3405,slot,8);
    }
    void palette_frame(unsigned slot,unsigned frame) { // EF86, frame in $06
        slot&=255;set_palette(0x3245,slot,8);set_palette(0x33c5,slot,frame);
    }

    // 00:FA27 adds hundreds of points unless scoring is off ($9C bit 0);
    // FAB6 caps the score at 999,999,900. Points belong to the ball's half.
    void score(unsigned hundreds) {
        if(!active_) throw Wait{};
        set_global(0x06,hundreds);set_global(0x07,hundreds>>8);
        if(global(0x9c)&1) return;
        const unsigned total=((global(0x322a)<<16)|(global(0x3224)<<8)|global(0x321e))+hundreds;
        unsigned capped=total&0xffffff;
        if(capped>=0x98967f) {capped=0x98967f;set_global(0x9d,global(0x9d)|0x80);}
        set_global(0x321e,capped);set_global(0x3224,capped>>8);set_global(0x322a,capped>>16);
        set_global(0x68,global(0x68)|4);
    }
    // 03:925F / 9277: a table lamp on or off ($D6.. bits). The ROM redraws
    // it at once when it belongs to the ball's table; a lamp of the stored
    // table is drawn from these bits when its table is shown.
    // `pc`, if given, is where the ROM continues instead: a routine that
    // goes on after its JSR resumes at the load before it.
    void lamp(bool on,unsigned lamp,int pc=-1) {
        if(active_ && pc>=0) throw Resume{static_cast<std::uint16_t>(pc),-1};
        if(active_) throw Resume{static_cast<std::uint16_t>(on?0x925f:0x9277),static_cast<int>(lamp&255)};
        lamp&=255;
        const unsigned row=lamp*6;
        if((rom_(3,0x9358+row)>>4)==global(0x2079)) throw Wait{};
        const unsigned address=0xd6+(lamp>>3);
        if(on) set_global(address,global(address)|rom_(3,0x9405+(lamp&7)));
        else set_global(address,global(address)&rom_(3,0x940d+(lamp&7)));
        switch((lamp*2)&255) { // 928E compares A*2.
        case 4: set_bits(0xda,1);break;
        case 2: clear_bits(0xda,2);break;
        case 0x0e: clear_bits(0xda,4);break;
        default: break;
        }
    }
    // 03:8F52: velocity for a direction from the tables at 04:BC0E/BC4E,
    // scaled by $06 (bit 7: divided).
    void heading(unsigned x,unsigned direction,unsigned scale);
    // 03:9022: an eighth of the velocity, into $3900/$3980.
    void eighth(unsigned x) {
        for(auto [from,to]:{std::pair{0x3700u,0x3900u},{0x3780u,0x3980u}}) {
            unsigned value=(*this)(from,x)|((*this)(from+64,x)<<8);
            for(unsigned i=0;i<3;++i) value=(value>>1)|(value&0x8000);
            scratch(0x04,value);scratch(0x05,value>>8);
            put(to,x,value);put(to+64,x,value>>8);
        }
    }
    // 03:8585, the ball touching this actor. The ROM tests it in the ball's
    // half from `pc`; the stored half has no ball.
    bool touching(std::uint16_t pc) {if(active_) throw Resume{pc,-1};return false;}
    // 03:852F pushes the ball off a moving actor, from `pc` in the ball's half.
    void push(std::uint16_t pc) {if(active_) throw Resume{pc,-1};}
private:
    std::size_t at(unsigned field,unsigned x) const {
        // Slots 0..3 (the ball and flippers) are the ball's half's alone; the
        // stored bank has none.
        if(!active_ && x<4) throw Wait{};
        return base_+field-0x3500+(x&255);
    }
    void write(std::size_t address,unsigned value) {
        if(!active_) log_.emplace_back(address,ram_[address]);
        ram_[address]=static_cast<std::byte>(value&255);
    }
    void show(unsigned x,unsigned pointer) {
        scratch(0x04,pointer);scratch(0x05,pointer>>8);
        put(0x3800,x,rom_.at((pointer+(*this)(0x3c40,x))&0xffff));
    }
    // FB03/FB59: signed 8.8 velocity added to a 16.8 position. Off screen is
    // [limit, $FFC0); X is tested only when Y left the flag clear.
    void axis(unsigned x,unsigned position,unsigned velocity,unsigned limit,bool vertical) {
        const unsigned high=(*this)(velocity+64,x),sign=(high&0x80)?255:0;
        scratch(0x00,sign);
        const unsigned low=(*this)(position,x)+(*this)(velocity,x);
        put(position,x,low);
        const unsigned middle=(*this)(position+64,x)+high+(low>>8);
        put(position+64,x,middle);
        put(position+128,x,(*this)(position+128,x)+sign+(middle>>8));
        if(!vertical && ((*this)(0x3540,x)&1)) return;
        const unsigned value=(*this)(position+64,x)|((*this)(position+128,x)<<8);
        scratch(0x0a,value);scratch(0x0b,value>>8);
        const bool off=value>=limit && value<0xffc0;
        put(0x3540,x,off?(*this)(0x3540,x)|1:(*this)(0x3540,x)&0xfe);
    }
    // FBB6/FC10: a signed byte of acceleration; with the `reverse` bit of
    // $3A00, it turns around when the speed equals the cap.
    void accelerate(unsigned x,unsigned velocity,unsigned acceleration,unsigned cap,unsigned reverse) {
        const unsigned a=(*this)(acceleration,x);
        const unsigned sum=(*this)(velocity,x)+a;
        put(velocity,x,sum);
        put(velocity+64,x,(*this)(velocity+64,x)+((a&0x80)?0xff:0)+(sum>>8));
        if(!((*this)(0x3a00,x)&reverse)) return;
        unsigned speed=(*this)(velocity,x)|((*this)(velocity+64,x)<<8);
        if(speed&0x8000) speed=(65536-speed)&65535;
        scratch(0x04,speed);scratch(0x05,speed>>8);
        if(speed==((*this)(cap,x)|((*this)(cap+64,x)<<8))) put(acceleration,x,(256-(*this)(acceleration,x))&255);
    }
    const Rom& rom_;
    std::span<std::byte> ram_;
    std::size_t base_;
    bool active_,legacy_;
    unsigned draws_{};
    std::vector<std::pair<std::size_t,std::byte>> log_;
};

// Fields every routine touches, for slot x of bank b.
struct Actor {
    Bank& b;unsigned x;
    unsigned flags() const {return b(0x3540,x);}
    void flags(unsigned v) {b.put(0x3540,x,v);}
    void set(unsigned bits) {flags(flags()|bits);}
    void clear(unsigned bits) {flags(flags()&~bits);}
    bool started() const {return (flags()&0x80)!=0;}
    unsigned action() const {return b(0x3840,x);}
    void action(unsigned v) {b.put(0x3840,x,v);}
    void next() {action(action()+1);}
    bool hit() const {return (b(0x3ac0,x)&0x80)!=0;}
    // DEC $3880,X, true when it reaches zero.
    bool countdown() {b.put(0x3880,x,b(0x3880,x)-1);return b(0x3880,x)==0;}
    // The 16-bit countdown in $3880 (low) and $38C0 (high).
    bool countdown16() {
        if(!b(0x3880,x)) b.put(0x38c0,x,b(0x38c0,x)-1);
        b.put(0x3880,x,b(0x3880,x)-1);
        return !(b(0x3880,x)|b(0x38c0,x));
    }
    bool resting() const {return !(b(0x3c00,x)|b(0x3c40,x));}
    bool reversing() const {return (b(0x3bc0,x)&0x80)!=0;}
    // $3A40 frames, stepping $3BC0:$3B80 per update, from phase 0.
    void oscillator(unsigned frames,unsigned step_low,unsigned step_high=0) {
        b.put(0x3a40,x,frames);b.put(0x3b80,x,step_low);b.put(0x3bc0,x,step_high);
        b.put(0x3c00,x,0);b.put(0x3c40,x,0);
    }
    // Slot y (the ball) at this actor's position plus dx, dy; 02:6F80 goes
    // through $00/$01.
    void carry(unsigned y,unsigned dx,unsigned dy,bool through_scratch) {
        if(through_scratch) {b.scratch(0x00,dx);b.scratch(0x01,dy);}
        const unsigned left=b(0x35c0,x)+dx,top=b(0x3680,x)+dy;
        b.put(0x35c0,y,left);b.put(0x3600,y,b(0x3600,x)+(left>>8));
        b.put(0x3680,y,top);b.put(0x36c0,y,b(0x36c0,x)+(top>>8));
    }
    // A copy of this actor's position in slot y, dy pixels lower.
    void place(unsigned y,unsigned type,int dy=0) {
        b.put(0x3500,y,type);b.put(0x35c0,y,b(0x35c0,x));b.put(0x3600,y,b(0x3600,x));
        const unsigned value=(b(0x3680,x)|(b(0x36c0,x)<<8))+static_cast<unsigned>(dy);
        b.put(0x3680,y,value);b.put(0x36c0,y,value>>8);
    }
};

// Per-table routines, in cabinet_actors_lower.cpp and cabinet_actors_upper.cpp:
// false for a type that is not theirs. An update returns to the dispatcher,
// throws Resume to finish in ROM code, or Wait (stored half only) to change
// nothing.
bool lower_actor(Bank&,unsigned type,unsigned x);
bool upper_actor(Bank&,unsigned type,unsigned x);
}
