// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/gameplay/cabinet_aliens.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <algorithm>
#include <stdexcept>
namespace alien_crush::gameplay {
namespace {
constexpr std::uint16_t kSound=0xea62,kOrangeCleared=0x917f,kGreenCleared=0x9168;
constexpr unsigned kOrange=15,kSilverOrange=16,kGreen=21,kSilverGreen=22,kBase=25,
    kOrangeController=67,kGreenController=68,kOrangeDefeat=72,kGreenDefeat=73;
}
// One actor bank in work RAM, addressed like the ROM: $3500+field+X. The
// active bank is the ROM's own; the inactive one is stored $1300 lower.
class CabinetAliens::Bank {
public:
    Bank(const CabinetAliens& tables,std::span<std::byte> ram,bool active)
        :tables_(tables),ram_(ram),base_(active?0x1500:0x200),active_(active) {}
    bool active() const {return active_;}
    unsigned operator()(unsigned field,unsigned x) const {return std::to_integer<unsigned>(ram_[at(field,x)]);}
    void put(unsigned field,unsigned x,unsigned value) {ram_[at(field,x)]=static_cast<std::byte>(value&255);}
    // Zero page and $2000..$3FFF, shared by both halves.
    unsigned global(unsigned address) const {return std::to_integer<unsigned>(ram_[address&0x1fff]);}
    void set_global(unsigned address,unsigned value) {ram_[address&0x1fff]=static_cast<std::byte>(value&255);}
    // Zero-page temporaries of the ROM routines, kept for exact play in the
    // ball's half. The inactive half has no ROM routine to leave them.
    void scratch(unsigned address,unsigned value) {if(active_) set_global(address,value);}
    // $69 carries this frame's events for the ball's half (01:4815 clears it).
    unsigned events() const {return active_?global(0x69):0;}
    // 00:F9A2. The inactive half draws from RAM without changing it, so the
    // game's own sequence stays the original's while its half is away.
    unsigned random(unsigned x) {
        const auto step=[](unsigned state) {return (state+(((state+0x503f)&65535)*2))&65535;};
        const unsigned lo=global(0x3217),state=lo|(global(0x3218)<<8);
        if(!active_) {
            const unsigned seed=state^(global(0x0e)*0x0101)^(x*0x3b1d)^
                (std::to_integer<unsigned>(ram_[0x1580])<<8)^std::to_integer<unsigned>(ram_[0x1640]);
            const unsigned mixed=step(step(seed));
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
    // 00:FADA. These actors move on Y (FB03) and X (FB59) only; bit 0 of
    // $3540 marks them off screen.
    void move(unsigned x) {
        const unsigned flags=(*this)(0x3a00,x);
        if(flags&1) axis(x,0x3640,0x3780,0x140,true);
        if(flags&2) axis(x,0x3580,0x3700,0x120,false);
    }
    // 00:FD2B: FD35 steps the phase between 0 and $3A40, reflecting at both
    // ends; FD8F then shows that pose from the type's sequence.
    void animate(unsigned x) {
        unsigned sum=(*this)(0x3c00,x)+(*this)(0x3b80,x);
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
        const unsigned type=((*this)(0x3500,x)-1)&255;
        const unsigned pointer=byte(tables_.sequences_,0xbdf7+type)|(byte(tables_.sequences_,0xbe51+type)<<8);
        scratch(0x04,pointer);scratch(0x05,pointer>>8);
        put(0x3800,x,byte(tables_.sequences_,pointer+(*this)(0x3c40,x)));
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
    // Palette scheduler requests, 00:EF04..EF85. Slots are fixed per half,
    // so they are the same for either bank.
    unsigned palette(unsigned field,unsigned slot) const {return global(field+slot);}
    void set_palette(unsigned field,unsigned slot,unsigned value) {set_global(field+slot,value);}
    void palette_start(unsigned slot) { // EF41
        slot&=255;
        if(palette(0x3245,slot)) return;
        for(auto [field,table]:{std::pair{0x3245u,0xefd2u},{0x3405u,0xf012u},{0x3305u,0xf052u},
            {0x3345u,0xf092u},{0x33c5u,0xf0d2u},{0x32c5u,0xf112u}})
            set_palette(field,slot,byte(tables_.system_,table+slot));
    }
    void palette_idle(unsigned slot) { // EF79
        slot&=255;set_palette(0x3245,slot,8);set_palette(0x33c5,slot,byte(tables_.system_,0xf152+slot));
    }
    void palette_rate(unsigned slot) { // EF6C
        slot&=255;
        if(palette(0x3245,slot)) set_palette(0x3285,slot,byte(tables_.system_,0xef92+slot));
    }
    void palette_reveal(unsigned slot) { // EF21
        slot&=255;set_palette(0x3245,slot,2);set_palette(0x3285,slot,0x20);set_palette(0x33c5,slot,0);
        set_palette(0x3345,slot,0);set_palette(0x3305,slot,0x80);set_palette(0x3405,slot,8);
    }
    void palette_cleared(unsigned slot) { // EF04
        set_palette(0x3245,slot,2);set_palette(0x3285,slot,0x10);set_palette(0x33c5,slot,0);
        set_palette(0x3305,slot,0);set_palette(0x3345,slot,1);set_palette(0x3405,slot,8);
    }
    // 00:FA27 adds hundreds of points unless scoring is off ($9C bit 0);
    // FAB6 caps the score at 999,999,900.
    void score(unsigned hundreds) {
        set_global(0x06,hundreds);set_global(0x07,0);
        if(global(0x9c)&1) return;
        const unsigned total=((global(0x322a)<<16)|(global(0x3224)<<8)|global(0x321e))+hundreds;
        unsigned capped=total&0xffffff;
        if(capped>=0x98967f) {capped=0x98967f;set_global(0x9d,global(0x9d)|0x80);}
        set_global(0x321e,capped);set_global(0x3224,capped>>8);set_global(0x322a,capped>>16);
        set_global(0x68,global(0x68)|4);
    }
    unsigned spawn_byte(unsigned address) const {return byte(tables_.actors_,address);}
private:
    static unsigned byte(const std::array<std::byte,8192>& bank,unsigned address) {
        return std::to_integer<unsigned>(bank[address&0x1fff]);
    }
    std::size_t at(unsigned field,unsigned x) const {return base_+field-0x3500+x;}
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
    const CabinetAliens& tables_;
    std::span<std::byte> ram_;
    std::size_t base_;
    bool active_;
};
CabinetAliens::CabinetAliens(std::span<const std::byte> rom) {
    if(!rom::matches_supported_rom(rom.size(),rom::compute_digests(rom))) throw std::runtime_error("Unsupported ROM for cabinet aliens");
    std::copy_n(rom.begin(),system_.size(),system_.begin());
    std::copy_n(rom.begin()+0x2000,actors_.size(),actors_.begin());
    std::copy_n(rom.begin()+0x8000,sequences_.size(),sequences_.begin());
    reset();
}
void CabinetAliens::reset() noexcept {inactive_frame_.fill(~std::uint64_t{});inactive_table_=2;}
bool CabinetAliens::owns(std::uint8_t type) noexcept {return entry(type)!=0;}
std::uint16_t CabinetAliens::entry(std::uint8_t type) noexcept {
    switch(type) {
    case kOrange: return 0x5f82;
    case kSilverOrange: return 0x60e1;
    case kGreen: return 0x619d;
    case kSilverGreen: return 0x630c;
    case kBase: return 0x63cd;
    case kOrangeController: return 0x5da5;
    case kGreenController: return 0x5e70;
    case kOrangeDefeat: return 0x6552;
    case kGreenDefeat: return 0x65ab;
    default: return 0;
    }
}
std::uint16_t CabinetAliens::update_active(std::span<std::byte> ram,unsigned x,std::uint64_t frame) {
    if(ram.size()!=8192 || x<4 || x>=64) throw std::runtime_error("Invalid cabinet alien update");
    const unsigned table=std::to_integer<unsigned>(ram[0x79]);
    if(table==inactive_table_ && inactive_frame_[x]==frame) return kReturn;
    Bank bank(*this,ram,true);
    return run(bank,x);
}
void CabinetAliens::update_inactive(std::span<std::byte> ram,std::uint64_t frame) {
    if(ram.size()!=8192) throw std::runtime_error("Invalid cabinet alien update");
    const unsigned table=std::to_integer<unsigned>(ram[0x79]);
    inactive_table_=2;
    if(table>1) return;
    inactive_table_=1-table;
    Bank bank(*this,ram,false);
    // The dispatcher reads each type as it goes, so actors spawned into later
    // slots run in the same pass, and removed ones do not.
    for(unsigned x=4;x<64;++x) {
        const unsigned type=bank(0x3500,x),action=bank(0x3840,x);
        if(!owns(static_cast<std::uint8_t>(type))) continue;
        const bool hit=(bank(0x3ac0,x)&0x80)!=0;
        if(hit && (((type==kOrange) && action==3) || (type==kGreen && action==2) ||
            ((type==kSilverOrange || type==kSilverGreen) && action==1))) continue;
        inactive_frame_[x]=frame;
        run(bank,x);
    }
}
std::uint16_t CabinetAliens::run(Bank& b,unsigned x) const {
    const unsigned type=b(0x3500,x);
    const auto countdown=[&] {b.put(0x3880,x,b(0x3880,x)-1);return b(0x3880,x)==0;};
    const auto solid=[&](bool on) {b.put(0x3540,x,on?b(0x3540,x)&0xbf:b(0x3540,x)|0x40);};
    // Common to the four aliens after initialization: $69 bit 4 hides them
    // for the frame, and bit 5 (game over) removes them.
    const auto frame_events=[&] {
        b.put(0x3540,x,(b.events()&0x10)?b(0x3540,x)|0x20:b(0x3540,x)&0xdf);
        if(!(b.events()&0x20)) return false;
        b.remove(x);return true;
    };
    const auto oscillator=[&](unsigned rate) {
        b.put(0x3a40,x,4);b.put(0x3b80,x,rate);b.put(0x3bc0,x,0);b.put(0x3c00,x,0);b.put(0x3c40,x,0);
    };
    // Last alien of a group: 02:60D3 / 618F (orange), 62FD / 63BF (green).
    const auto cleared=[&](bool orange) -> std::uint16_t {
        const unsigned count=orange?0x34:0x36;
        b.set_global(count,b.global(count)-1);
        if(b.global(count)) return kReturn;
        const unsigned bit=orange?1:2;
        b.set_global(0x69,b.global(0x69)|bit);b.set_global(0x98,b.global(0x98)|bit);
        b.set_global(0xb0,b.global(0xb0)&(orange?0x7f:0xbf));
        b.palette_cleared(orange?5:2);
        return orange?kOrangeCleared:kGreenCleared;
    };
    const auto emerge=[&](unsigned ticks) { // 02:5FF2 / 6021 / 6106
        b.put(0x3700,x,0xc0);b.put(0x3740,x,0xff);b.put(0x3580,x,0xc0);
        b.put(0x3780,x,0x40);b.put(0x37c0,x,0);b.put(0x3640,x,0);b.put(0x3880,x,ticks);
    };
    switch(type) {
    case kOrange: { // 01:5F82..02:60DE
        if(!(b(0x3540,x)&0x80)) {
            b.put(0x3540,x,b(0x3540,x)|0xc0);b.put(0x3a00,x,0x23);oscillator(0x10);
            b.put(0x3880,x,b.random(x)+0x10);b.palette_idle(b(0x3a80,x)+0x10);
        }
        if(frame_events()) return kReturn;
        switch(b(0x3840,x)) {
        case 1: // Waiting in the hole after a retreat; $B0 bit 7 forces a rise.
            if(!(b.global(0xb0)&0x80) && !countdown()) return kReturn;
            b.put(0x3840,x,2);emerge(0x20);return kReturn;
        case 2: // Emerging diagonally; solid once out.
            b.move(x);
            if(!countdown()) return kReturn;
            b.put(0x3840,x,3);b.put(0x3880,x,b.ranged(x,0x78)+0x78);b.put(0x3ac0,x,0);solid(true);
            return kReturn;
        case 3: // Out and solid.
            if(b(0x3ac0,x)&0x80) { // 02:60B2: hit.
                b.palette_start(b(0x3a80,x)+0x10);b.score(0x0a);
                b.put(0x3500,x,kOrangeDefeat);b.put(0x3540,x,0);
                return cleared(true);
            }
            if(!countdown()) return kReturn;
            b.put(0x3700,x,0x40);b.put(0x3740,x,0);b.put(0x3580,x,0);b.put(0x3780,x,0xc0);
            b.put(0x37c0,x,0xff);b.put(0x3640,x,0xc0);b.put(0x3880,x,0x20);
            b.put(0x3840,x,4);solid(false);return kReturn;
        case 4: // Retreating.
            b.move(x);
            if(!countdown()) return kReturn;
            b.put(0x3880,x,b.random(x)+0x0a);b.put(0x3840,x,1);return kReturn;
        default: // First wait after spawning.
            if(!(b.global(0xb0)&0x80) && !countdown()) return kReturn;
            b.put(0x3840,x,2);emerge(0x30);return kReturn;
        }
    }
    case kSilverOrange: { // 02:60E1..619C
        if(!(b(0x3540,x)&0x80)) {
            b.put(0x3540,x,b(0x3540,x)|0xc0);b.put(0x3a00,x,0x23);oscillator(0x10);emerge(0x30);
        }
        if(frame_events()) return kReturn;
        if(b(0x3840,x)==1) {
            if(!(b(0x3ac0,x)&0x80)) return kReturn;
            // The hit reveals the color the controller stored in $3900.
            b.put(0x3a80,x,b(0x3900,x));b.palette_reveal(b(0x3900,x)+0x10);b.score(0x14);
            b.put(0x3500,x,kOrangeDefeat);b.put(0x3540,x,0);
            return cleared(true);
        }
        b.move(x);
        if(!countdown()) return kReturn;
        // STA $3880 stores FADA's final A: $3A00 & $10, which is zero.
        b.put(0x3840,x,b(0x3840,x)+1);b.put(0x3880,x,0);b.put(0x3ac0,x,0);solid(true);
        return kReturn;
    }
    case kGreen: { // 02:619D..62C4
        if(!(b(0x3540,x)&0x80)) {
            b.put(0x3540,x,b(0x3540,x)|0xc0);b.put(0x3a00,x,0x21);oscillator(0x10);
            b.put(0x3880,x,b.random(x)+0x10);b.palette_idle(b(0x3a80,x)+0x10);
        }
        if(frame_events()) return kReturn;
        switch(b(0x3840,x)) {
        case 1: // Rising 24 pixels; solid once up.
            b.move(x);
            if(!countdown()) return kReturn;
            b.put(0x3840,x,2);b.put(0x3880,x,b.ranged(x,0x78)+0x78);b.put(0x3ac0,x,0);solid(true);
            return kReturn;
        case 2: // Up, animating, and solid.
            if(b(0x3ac0,x)&0x80) { // 02:62C5: hit.
                b.palette_start(b(0x3a80,x)+0x10);
                b.put(0x3500,x,kGreenDefeat);b.put(0x3540,x,0);b.score(0x0a);
                oscillator(0x40);b.put(0x3840,x,4);
                return cleared(false);
            }
            if(!countdown()) {b.animate(x);return kReturn;}
            b.put(0x3780,x,0x80);b.put(0x37c0,x,0);b.put(0x3880,x,0x30);
            b.put(0x3840,x,3);solid(false);b.palette_rate(b(0x3a80,x)+0x10);
            return kReturn;
        case 3: // Lowering.
            b.move(x);
            if(!countdown()) return kReturn;
            b.put(0x3880,x,b.random(x)+0x10);b.put(0x3840,x,0);return kReturn;
        default: // Down; $B0 bit 6 forces a rise.
            if(!(b.global(0xb0)&0x40) && !countdown()) return kReturn;
            b.put(0x3840,x,b(0x3840,x)+1);b.put(0x3780,x,0x80);b.put(0x37c0,x,0xff);b.put(0x3880,x,0x30);
            return kReturn;
        }
    }
    case kSilverGreen: { // 02:630C..63CC
        if(!(b(0x3540,x)&0x80)) {
            b.put(0x3540,x,b(0x3540,x)|0xc0);b.put(0x3a00,x,0x21);oscillator(0x10);
            b.put(0x3780,x,0x80);b.put(0x37c0,x,0xff);b.put(0x3880,x,0x30);
        }
        if(frame_events()) return kReturn;
        if(b(0x3840,x)==1) {
            if(!(b(0x3ac0,x)&0x80)) {b.animate(x);return kReturn;}
            b.put(0x3a80,x,b(0x3900,x));b.palette_reveal(b(0x3900,x)+0x10);
            b.put(0x3500,x,kGreenDefeat);b.put(0x3540,x,0);b.score(0x14);
            oscillator(0x40);b.put(0x3840,x,4);
            return cleared(false);
        }
        b.move(x);
        if(!countdown()) return kReturn;
        b.put(0x3840,x,b(0x3840,x)+1);b.put(0x3ac0,x,0);solid(true);
        return kReturn;
    }
    case kBase: // 02:63CD: the green alien's hole; never solid.
        if(!(b(0x3540,x)&0x80)) {b.put(0x3540,x,b(0x3540,x)|0xc0);b.put(0x3a00,x,0);}
        return kReturn;
    case kOrangeController:
    case kGreenController: { // 01:5DA5 / 5E70
        const bool orange=type==kOrangeController;
        if(!(b(0x3540,x)&0x80)) {
            b.put(0x3540,x,b(0x3540,x)|0x80);b.put(0x3a00,x,0);b.put(0x3880,x,0x2c);b.put(0x38c0,x,1);
        }
        const unsigned action=b(0x3840,x);
        if(action==1 || action==2) {
            // 5E06 / 5ED1: the group, in the first free slots. A full bank
            // retries next frame; a spawned group removes its controller.
            const unsigned spawned=orange?(action==1?kOrange:kSilverOrange):(action==1?kGreen:kSilverGreen);
            const unsigned count=orange?3:6;
            b.scratch(0x00,spawned);
            std::array<unsigned,6> slots{};
            if(!b.allocate(count,slots)) return kReturn;
            b.scratch(0x01,count);b.scratch(0x04,0);b.scratch(0x05,0);
            unsigned previous=b.global(0x02);
            for(unsigned i=0;i<count;++i) {
                const unsigned slot=slots[i],row=(orange?0x5f4c:0x5f5e)+i*6;
                const unsigned actor=!orange && !(i&1)?kBase:spawned;
                b.put(0x3500,slot,actor);
                b.put(0x35c0,slot,b.spawn_byte(row));b.put(0x3600,slot,b.spawn_byte(row+1));
                b.put(0x3680,slot,b.spawn_byte(row+2));b.put(0x36c0,slot,b.spawn_byte(row+3));
                b.put(0x3800,slot,b.spawn_byte(row+4));
                // Normal aliens take a palette slot; the silver ones keep the
                // color their hit reveals, and bases a zero.
                b.put(actor==kOrange || actor==kGreen?0x3a80:0x3900,slot,b.spawn_byte(row+5));
                if(!orange) { // Each alien records its base, for its defeat.
                    b.put(0x38c0,slot,previous);previous=slot;b.scratch(0x02,slot);
                }
                b.scratch(0x04,i+1);b.scratch(0x05,(i+1)*6);b.scratch(0x01,count-1-i);
            }
            b.set_global(orange?0x34:0x36,3);
            b.palette_start(orange?5:2);
            b.remove(x);
            return kReturn;
        }
        // A 300-tick wait. A lane sensor ($82 bit 1 / bit 3) seen once fewer
        // than $E0 ticks of the low byte remain makes the group silver.
        if(!b(0x3880,x)) b.put(0x38c0,x,b(0x38c0,x)-1);
        b.put(0x3880,x,b(0x3880,x)-1);
        if(!(b(0x3880,x)|b(0x38c0,x))) {b.put(0x3840,x,action+1);return kReturn;}
        if(b(0x3880,x)<0xe0 && (b.global(0x82)&(orange?2:8))) b.put(0x3840,x,2);
        return kReturn;
    }
    case kOrangeDefeat:
    case kGreenDefeat: { // 02:6552 / 65AB: a pause, then flying off.
        const bool orange=type==kOrangeDefeat;
        bool sound=false;
        if(!(b(0x3540,x)&0x80)) {
            b.put(0x3540,x,b(0x3540,x)|0xc0);b.put(0x3a00,x,orange?0x23:0x21);
            b.put(0x3880,x,0x0e);b.put(0x3840,x,0);sound=b.active();
        }
        if(b(0x3840,x)==1) {
            b.move(x);
            if(countdown()) {
                // A green alien takes its base with it; 38C0 names the base.
                if(!orange) b.remove(b(0x38c0,x));
                b.remove(x);
            }
        } else if(countdown()) {
            if(orange) {b.put(0x3700,x,0);b.put(0x3740,x,1);b.put(0x3580,x,0);b.put(0x3640,x,0xc0);}
            b.put(0x3780,x,0);b.put(0x37c0,x,orange?0xff:2);b.put(0x3880,x,0x0c);
            b.put(0x3840,x,b(0x3840,x)+1);
        }
        if(!sound) return kReturn;
        b.set_global(0x73,0x2a); // 00:EA62 plays sound $2A.
        return kSound;
    }
    default:
        throw std::runtime_error("Cabinet alien update for an actor it does not own");
    }
}
}
