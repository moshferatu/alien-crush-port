// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_actor_bank.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <algorithm>
#include <string>
#include <optional>
#include <stdexcept>
namespace alien_crush::gameplay {
namespace cabinet {
// 03:8F52 with 00:F726 (signed multiply) or 00:F81B (signed divide), and
// their temporaries: $02/$03, $04/$05, $8E/$8F and the sign in $92.
void Bank::heading(unsigned x,unsigned direction,unsigned scale) {
    const unsigned y=(direction*2)&255;
    put(0x3700,x,rom_(4,0xbc4e+y));put(0x3740,x,rom_(4,0xbc4f+y));
    put(0x3780,x,rom_(4,0xbc0e+y));put(0x37c0,x,rom_(4,0xbc0f+y));
    const bool divide=(scale&0x80)!=0;
    const unsigned factor=scale&0x7f;
    if(factor==1) return;
    for(const unsigned field:{0x3700u,0x3780u}) {
        unsigned value=(*this)(field,x)|((*this)(field+64,x)<<8);
        const bool negative=(value&0x8000)!=0;
        // $92: F892 leaves $80 or the positive high byte; F81B the high byte.
        scratch(0x92,negative && !divide?0x80:value>>8);
        if(negative) value=(65536-value)&65535;
        unsigned result;
        if(!divide) {
            result=(value*factor)&65535;
            scratch(0x02,factor);scratch(0x03,0);scratch(0x8e,result);scratch(0x8f,result>>8);
        } else {
            // F7ED: restoring division, 16 steps.
            unsigned quotient=value,rest=0;
            for(unsigned i=0;i<16;++i) {
                const unsigned carry=(quotient>>15)&1;
                quotient=(quotient<<1)&65535;
                rest=((rest<<1)|carry)&65535;
                const unsigned difference=(rest-factor)&65535;
                if(rest>=factor) {quotient|=1;rest=difference;}
            }
            result=quotient;
            scratch(0x02,rest);scratch(0x03,rest>>8);scratch(0x8e,rest);scratch(0x8f,rest>>8);
        }
        if(negative) result=(65536-result)&65535;
        scratch(0x04,result);scratch(0x05,result>>8);
        put(field,x,result);put(field+64,x,result>>8);
    }
}
}
namespace {
using cabinet::Bank;using cabinet::Resume;using cabinet::Wait;
bool alien(unsigned type) {
    switch(type) {
    case 15: case 16: case 21: case 22: case 25: case 67: case 68: case 72: case 73: return true;
    default: return false;
    }
}
// Table actors that change without the ball: everything the cabinet used to
// animate privately. Flippers follow the controls, and the ball-end
// sequence's actors never outlive a visit to the other half.
bool table(unsigned type) {
    switch(type) {
    case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 19: case 20:
    case 24: case 26: case 27: case 29: case 30: case 32: case 33: case 34: case 35:
    case 37: case 38: case 39: case 40: case 41: case 42: case 44: case 45: case 46: case 47:
    case 48: case 50: case 51: case 52: case 53: case 55: case 56: case 59: case 60: case 66:
    case 70: case 71: case 74: case 75: case 76: case 77:
        return true;
    default: return alien(type);
    }
}
// One palette slot's script, 00:F2E5..F460, without the uploads (F463,
// F4CA, F51F) that only the ball's half's colors get. Fields: $3245 script,
// $3285 control, $32C5 end frame, $3305/$3345 step, $3385/$33C5 phase (the
// frame), $3405 frame count.
void palette_script(Bank& b,unsigned x) {
    const auto field=[&](unsigned f) {return b.palette(f,x);};
    const auto put=[&](unsigned f,unsigned v) {b.set_palette(f,x,v);};
    const auto finish=[&] {for(unsigned f=0x3245;f<=0x3405;f+=0x40) put(f,0);}; // F5E2
    const auto negate=[&] { // The step's two's complement.
        const unsigned step=(65536-(field(0x3305)|(field(0x3345)<<8)))&65535;
        put(0x3305,step);put(0x3345,step>>8);
    };
    const auto ping_pong=[&] { // F57E
        const unsigned low=field(0x3305)+field(0x3385);
        put(0x3385,low);put(0x33c5,field(0x3345)+field(0x33c5)+(low>>8));
        if(!(field(0x3345)&0x80)) {
            if(field(0x33c5)!=field(0x3405)) return;
            put(0x3385,field(0x3305));negate();put(0x33c5,field(0x33c5)-1);
        } else if(!field(0x33c5)) {negate();put(0x3385,0);}
    };
    switch(field(0x3245)) {
    case 1: // F2E5
        ping_pong();
        if(!(field(0x3285)&0x80)) return;
        if(field(0x3285)&0x40) {put(0x33c5,field(0x32c5));finish();}
        else if(field(0x33c5)==field(0x32c5)) finish();
        return;
    case 2: // F3B3: a flash for $3285 frames.
        ping_pong();
        put(0x3285,field(0x3285)-1);
        if(!field(0x3285)) finish();
        return;
    case 6: { // F319: wraps to 0 at the count.
        const unsigned low=field(0x3305)+field(0x3385);
        put(0x3385,low);put(0x33c5,field(0x3345)+field(0x33c5)+(low>>8));
        if(field(0x33c5)==field(0x3405)) {put(0x3385,0);put(0x33c5,0);}
        if((field(0x3285)&0x80) && field(0x33c5)==field(0x32c5)) finish();
        return;
    }
    case 7: // F33D: waits on $68 bit 4 between its phases.
        switch(field(0x3285)) {
        case 1:
            if(!(b.global(0x68)&0x10)) return;
            put(0x3285,2);
            [[fallthrough]];
        case 2:
            if(field(0x3345)&0x80) {put(0x3285,field(0x3285)+1);return;}
            ping_pong();return;
        case 3:
            if(b.global(0x68)&0x10) return;
            put(0x3285,4);put(0x3385,0);
            [[fallthrough]];
        case 4:
            ping_pong();
            if(((field(0x3405)-1)&255)==field(0x33c5)) finish();
            return;
        default:
            if(!(field(0x3345)&0x80)) {put(0x3285,field(0x3285)+1);return;}
            ping_pong();return;
        }
    case 8: // F3C6: one upload.
        finish();return;
    case 9: // F3CC
        ping_pong();
        if(field(0x33c5)==field(0x32c5)) finish();
        return;
    case 10: { // F3EA: eight changes, shown offset by the bumper layout.
        const unsigned before=field(0x33c5);
        ping_pong();
        if(field(0x33c5)!=before) put(0x3285,field(0x3285)+1);
        if(field(0x3285)!=8) return;
        put(0x33c5,(b.rom()(1,0x5c9d+b.global(0xa8))+field(0x32c5))&255);finish();
        return;
    }
    case 11: // F42C
        if(field(0x3285)&0x80) {put(0x33c5,field(0x32c5));finish();return;}
        if(field(0x3285)==1) {
            put(0x3385,field(0x3385)-1);
            if(!field(0x3385)) put(0x3285,0);
            return;
        }
        ping_pong();
        if(field(0x3385)|field(0x33c5)) return;
        put(0x3285,field(0x3285)+1);put(0x3385,0x10);
        return;
    default: return; // 0 and the RTS scripts 3, 4 and 5.
    }
}
void run(Bank& b,unsigned x) {
    const unsigned type=b(0x3500,x);
    if(!cabinet::lower_actor(b,type,x) && !cabinet::upper_actor(b,type,x))
        throw std::runtime_error("Cabinet actor update for an actor it does not own");
}
}
CabinetActors::CabinetActors(std::span<const std::byte> rom,Scope scope,bool stored_half,bool lowers_riser)
    :rom_(std::make_unique<cabinet::Rom>()),scope_(scope),stored_half_(stored_half),lowers_riser_(lowers_riser) {
    if(!rom::matches_supported_rom(rom.size(),rom::compute_digests(rom))) throw std::runtime_error("Unsupported ROM for cabinet actors");
    for(unsigned bank=0;bank<rom_->banks.size();++bank)
        std::copy_n(rom.begin()+bank*0x2000,0x2000,rom_->banks[bank].begin());
    reset();
}
CabinetActors::~CabinetActors()=default;
void CabinetActors::reset() noexcept {inactive_frame_.fill(~std::uint64_t{});inactive_table_=2;}
bool CabinetActors::owned(std::uint8_t type,Scope scope) noexcept {return scope==Scope::aliens?alien(type):table(type);}
std::uint16_t CabinetActors::entry(std::uint8_t type) const noexcept {
    // 01:45A8 returns into the routine through the tables at 04:BC8E / BCE8.
    if(!type || type>90) return 0;
    return static_cast<std::uint16_t>(((*rom_)(4,0xbc8e+type-1)|((*rom_)(4,0xbce8+type-1)<<8))+1);
}
std::vector<std::uint16_t> CabinetActors::entries() const {
    std::vector<std::uint16_t> result;
    for(unsigned type=1;type<=90;++type) if(owns(static_cast<std::uint8_t>(type))) result.push_back(entry(static_cast<std::uint8_t>(type)));
    std::sort(result.begin(),result.end());
    result.erase(std::unique(result.begin(),result.end()),result.end());
    return result;
}
std::optional<CabinetActors::Next> CabinetActors::update_active(std::span<std::byte> ram,unsigned x,std::uint64_t frame) {
    if(ram.size()!=8192 || x<4 || x>=64) throw std::runtime_error("Invalid cabinet actor update");
    const unsigned table=std::to_integer<unsigned>(ram[0x79]);
    if(table==inactive_table_ && inactive_frame_[x]==frame) return Next{0,-1};
    // Table actors run as the ROM's own code in the ball's half, in its own
    // time: the port's faster update would take the original's slowdown away.
    if(scope_==Scope::table) return std::nullopt;
    Bank bank(*rom_,ram,true,true);
    bank.begin();
    try {run(bank,x);}
    catch(const Resume& resume) {return Next{resume.pc,resume.a};}
    return Next{0,-1};
}
std::uint16_t CabinetActors::predict(std::span<const std::byte> ram,unsigned x) {
    if(ram.size()!=8192 || x<4 || x>=64) throw std::runtime_error("Invalid cabinet actor prediction");
    std::copy(ram.begin(),ram.end(),prediction_.begin());
    predicted_slot_=x;predicted_type_=std::to_integer<unsigned>(ram[0x1500+x]);
    Bank bank(*rom_,prediction_,true);
    bank.begin();
    try {run(bank,x);}
    catch(const Resume& resume) {
        predicted_pc_=resume.pc;
        if(resume.a>=0) predicted_a_=resume.a;
        return resume.pc;
    }
    predicted_pc_=0x45c8;predicted_a_=-1;
    return 0x45c8; // The dispatcher, after the routine's RTS.
}
std::string CabinetActors::mismatch(std::span<const std::byte> ram,unsigned a) const {
    if(ram.size()!=8192) throw std::runtime_error("Invalid cabinet actor prediction");
    // A vblank interrupt in between (it counts in $0E) changes more than the
    // routine does; $0F is the VDC status every interrupt saves.
    if(ram[0x0e]!=prediction_[0x0e]) return {};
    std::string result;
    for(unsigned p=0;p<8192;++p) {
        if((p>=0x100 && p<0x200) || p==0x0f || ram[p]==prediction_[p]) continue;
        if(result.empty()) result="Type "+std::to_string(predicted_type_)+" in slot "+std::to_string(predicted_slot_)+
            " at "+std::to_string(predicted_pc_)+":";
        if(result.size()<400) result+=" $"+std::to_string(0x2000+p)+" ROM "+std::to_string(std::to_integer<unsigned>(ram[p]))+
            " port "+std::to_string(std::to_integer<unsigned>(prediction_[p]));
    }
    if(result.empty() && predicted_a_>=0 && a!=static_cast<unsigned>(predicted_a_))
        result="Type "+std::to_string(predicted_type_)+" at "+std::to_string(predicted_pc_)+": A "+std::to_string(a)+
            " port "+std::to_string(predicted_a_);
    return result;
}
void CabinetActors::update_palettes(std::span<std::byte> ram) {
    if(ram.size()!=8192) throw std::runtime_error("Invalid cabinet palette update");
    const unsigned table=std::to_integer<unsigned>(ram[0x79]);
    // F292 runs while $68 bit 0 is set, and only the ball's half's slots.
    if(scope_!=Scope::table || !stored_half_ || table>1 || !(std::to_integer<unsigned>(ram[0x68])&1)) return;
    Bank bank(*rom_,ram,false);
    for(unsigned x=(1-table)*32;x<(2-table)*32;++x) {bank.begin();palette_script(bank,x);bank.commit();}
}
void CabinetActors::update_inactive(std::span<std::byte> ram,std::uint64_t frame) {
    if(ram.size()!=8192) throw std::runtime_error("Invalid cabinet actor update");
    const unsigned table=std::to_integer<unsigned>(ram[0x79]);
    inactive_table_=2;
    if(table>1 || !stored_half_) return;
    inactive_table_=1-table;
    Bank bank(*rom_,ram,false,scope_==Scope::aliens,lowers_riser_);
    // The dispatcher reads each type as it goes, so actors spawned into later
    // slots run in the same pass, and removed ones do not.
    for(unsigned x=4;x<64;++x) {
        const unsigned type=bank(0x3500,x),action=bank(0x3840,x);
        if(!owns(static_cast<std::uint8_t>(type))) continue;
        // A touch or hit from the ball waits for the ball's half. The upper
        // skull guard (02:63DB) and riser (01:5A43) never read theirs, only
        // clear it once down, so a touch before the ball left holds nothing.
        const bool hit=(bank(0x3ac0,x)&0x80)!=0 && type!=14 && type!=32;
        if(hit && (!alien(type) || (type==15 && action==3) || (type==21 && action==2) ||
            ((type==16 || type==22) && action==1))) continue;
        bank.begin();
        try {run(bank,x);bank.commit();inactive_frame_[x]=frame;}
        catch(const Wait&) {bank.rollback();}
    }
}
}
