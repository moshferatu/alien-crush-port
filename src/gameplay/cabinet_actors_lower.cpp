// SPDX-License-Identifier: GPL-3.0-or-later
// The lower table's actors for gameplay::CabinetActors. Each is its ROM
// routine (addresses below), in the order the routine does things.
#include "cabinet_actor_bank.hpp"
#include <stdexcept>
namespace alien_crush::gameplay::cabinet {
namespace {
constexpr unsigned kOrange=15,kSilverOrange=16,kGreen=21,kSilverGreen=22,kBase=25,
    kOrangeController=67,kGreenController=68,kOrangeDefeat=72,kGreenDefeat=73;
// The orange and green aliens and everything around them.
void aliens(Actor a,unsigned type) {
    Bank& b=a.b;const unsigned x=a.x;
    const auto solid=[&](bool on) {if(on) a.clear(0x40);else a.set(0x40);};
    // Common to the four aliens after initialization: $69 bit 4 hides them
    // for the frame, and bit 5 (game over) removes them.
    const auto frame_events=[&] {
        if(b.events()&0x10) a.set(0x20);else a.clear(0x20);
        if(!(b.events()&0x20)) return false;
        b.remove(x);return true;
    };
    // Last alien of a group: 02:60D3 / 618F (orange), 62FD / 63BF (green).
    const auto cleared=[&](bool orange) {
        const unsigned count=orange?0x34:0x36;
        b.set_global(count,b.global(count)-1);
        if(b.global(count)) return;
        const unsigned bit=orange?1:2;
        b.set_bits(0x69,bit);b.set_bits(0x98,bit);
        b.clear_bits(0xb0,orange?0x80:0x40);
        b.palette_cleared(orange?5:2);
        b.resume(orange?0x917f:0x9168);
    };
    const auto emerge=[&](unsigned ticks) { // 02:5FF2 / 6021 / 6106
        b.put(0x3700,x,0xc0);b.put(0x3740,x,0xff);b.put(0x3580,x,0xc0);
        b.put(0x3780,x,0x40);b.put(0x37c0,x,0);b.put(0x3640,x,0);b.put(0x3880,x,ticks);
    };
    switch(type) {
    case kOrange: { // 01:5F82..02:60DE
        if(!a.started()) {
            a.set(0xc0);b.put(0x3a00,x,0x23);a.oscillator(4,0x10);
            b.put(0x3880,x,b.random(x)+0x10);b.palette_idle(b(0x3a80,x)+0x10);
        }
        if(frame_events()) return;
        switch(a.action()) {
        case 1: // Waiting in the hole after a retreat; $B0 bit 7 forces a rise.
            if(!(b.global(0xb0)&0x80) && !a.countdown()) return;
            a.action(2);emerge(0x20);return;
        case 2: // Emerging diagonally; solid once out.
            b.move(x);
            if(!a.countdown()) return;
            a.action(3);b.put(0x3880,x,b.ranged(x,0x78)+0x78);b.put(0x3ac0,x,0);solid(true);
            return;
        case 3: // Out and solid.
            if(a.hit()) { // 02:60B2
                b.palette_start(b(0x3a80,x)+0x10);b.score(0x0a);
                b.put(0x3500,x,kOrangeDefeat);a.flags(0);
                cleared(true);return;
            }
            if(!a.countdown()) return;
            b.put(0x3700,x,0x40);b.put(0x3740,x,0);b.put(0x3580,x,0);b.put(0x3780,x,0xc0);
            b.put(0x37c0,x,0xff);b.put(0x3640,x,0xc0);b.put(0x3880,x,0x20);
            a.action(4);solid(false);return;
        case 4: // Retreating.
            b.move(x);
            if(!a.countdown()) return;
            b.put(0x3880,x,b.random(x)+0x0a);a.action(1);return;
        default: // First wait after spawning.
            if(!(b.global(0xb0)&0x80) && !a.countdown()) return;
            a.action(2);emerge(0x30);return;
        }
    }
    case kSilverOrange: // 02:60E1..619C
        if(!a.started()) {a.set(0xc0);b.put(0x3a00,x,0x23);a.oscillator(4,0x10);emerge(0x30);}
        if(frame_events()) return;
        if(a.action()==1) {
            if(!a.hit()) return;
            // The hit reveals the color the controller stored in $3900.
            b.put(0x3a80,x,b(0x3900,x));b.palette_reveal(b(0x3900,x)+0x10);b.score(0x14);
            b.put(0x3500,x,kOrangeDefeat);a.flags(0);
            cleared(true);return;
        }
        b.move(x);
        if(!a.countdown()) return;
        // STA $3880 stores FADA's final A: $3A00 & $10, which is zero.
        a.next();b.put(0x3880,x,0);b.put(0x3ac0,x,0);solid(true);
        return;
    case kGreen: // 02:619D..62C4
        if(!a.started()) {
            a.set(0xc0);b.put(0x3a00,x,0x21);a.oscillator(4,0x10);
            b.put(0x3880,x,b.random(x)+0x10);b.palette_idle(b(0x3a80,x)+0x10);
        }
        if(frame_events()) return;
        switch(a.action()) {
        case 1: // Rising 24 pixels; solid once up.
            b.move(x);
            if(!a.countdown()) return;
            a.action(2);b.put(0x3880,x,b.ranged(x,0x78)+0x78);b.put(0x3ac0,x,0);solid(true);
            return;
        case 2: // Up, animating, and solid.
            if(a.hit()) { // 02:62C5
                b.palette_start(b(0x3a80,x)+0x10);
                b.put(0x3500,x,kGreenDefeat);a.flags(0);b.score(0x0a);
                a.oscillator(4,0x40);a.action(4);
                cleared(false);return;
            }
            if(!a.countdown()) {b.animate(x);return;}
            b.put(0x3780,x,0x80);b.put(0x37c0,x,0);b.put(0x3880,x,0x30);
            a.action(3);solid(false);b.palette_rate(b(0x3a80,x)+0x10);
            return;
        case 3: // Lowering.
            b.move(x);
            if(!a.countdown()) return;
            b.put(0x3880,x,b.random(x)+0x10);a.action(0);return;
        default: // Down; $B0 bit 6 forces a rise.
            if(!(b.global(0xb0)&0x40) && !a.countdown()) return;
            a.next();b.put(0x3780,x,0x80);b.put(0x37c0,x,0xff);b.put(0x3880,x,0x30);
            return;
        }
    case kSilverGreen: // 02:630C..63CC
        if(!a.started()) {
            a.set(0xc0);b.put(0x3a00,x,0x21);a.oscillator(4,0x10);
            b.put(0x3780,x,0x80);b.put(0x37c0,x,0xff);b.put(0x3880,x,0x30);
        }
        if(frame_events()) return;
        if(a.action()==1) {
            if(!a.hit()) {b.animate(x);return;}
            b.put(0x3a80,x,b(0x3900,x));b.palette_reveal(b(0x3900,x)+0x10);
            b.put(0x3500,x,kGreenDefeat);a.flags(0);b.score(0x14);
            a.oscillator(4,0x40);a.action(4);
            cleared(false);return;
        }
        b.move(x);
        if(!a.countdown()) return;
        a.next();b.put(0x3ac0,x,0);solid(true);
        return;
    case kBase: // 02:63CD: the green alien's hole; never solid.
        if(!a.started()) {a.set(0xc0);b.put(0x3a00,x,0);}
        return;
    case kOrangeController:
    case kGreenController: { // 01:5DA5 / 5E70
        const bool orange=type==kOrangeController;
        if(!a.started()) {a.set(0x80);b.put(0x3a00,x,0);b.put(0x3880,x,0x2c);b.put(0x38c0,x,1);}
        const unsigned action=a.action();
        if(action==1 || action==2) {
            // 5E06 / 5ED1: the group, in the first free slots. A full bank
            // retries next frame; a spawned group removes its controller.
            const unsigned spawned=orange?(action==1?kOrange:kSilverOrange):(action==1?kGreen:kSilverGreen);
            const unsigned count=orange?3:6;
            b.scratch(0x00,spawned);
            std::array<unsigned,6> slots{};
            if(!b.allocate(count,slots)) return;
            b.scratch(0x01,count);b.scratch(0x04,0);b.scratch(0x05,0);
            unsigned previous=b.global(0x02);
            for(unsigned i=0;i<count;++i) {
                const unsigned slot=slots[i],row=(orange?0x5f4c:0x5f5e)+i*6;
                const unsigned actor=!orange && !(i&1)?kBase:spawned;
                b.put(0x3500,slot,actor);
                b.put(0x35c0,slot,b.rom()(1,row));b.put(0x3600,slot,b.rom()(1,row+1));
                b.put(0x3680,slot,b.rom()(1,row+2));b.put(0x36c0,slot,b.rom()(1,row+3));
                b.put(0x3800,slot,b.rom()(1,row+4));
                // Normal aliens take a palette slot; the silver ones keep the
                // color their hit reveals, and bases a zero.
                b.put(actor==kOrange || actor==kGreen?0x3a80:0x3900,slot,b.rom()(1,row+5));
                if(!orange) { // Each alien records its base, for its defeat.
                    b.put(0x38c0,slot,previous);previous=slot;b.scratch(0x02,slot);
                }
                b.scratch(0x04,i+1);b.scratch(0x05,(i+1)*6);b.scratch(0x01,count-1-i);
            }
            b.set_global(orange?0x34:0x36,3);
            b.palette_start(orange?5:2);
            b.remove(x);
            return;
        }
        // A 300-tick wait. A lane sensor ($82 bit 1 / bit 3) seen once fewer
        // than $E0 ticks of the low byte remain makes the group silver.
        if(a.countdown16()) {a.action(action+1);return;}
        if(b(0x3880,x)<0xe0 && (b.global(0x82)&(orange?2:8))) a.action(2);
        return;
    }
    case kOrangeDefeat:
    case kGreenDefeat: { // 02:6552 / 65AB: a pause, then flying off.
        const bool orange=type==kOrangeDefeat;
        bool sound=false;
        if(!a.started()) {
            a.set(0xc0);b.put(0x3a00,x,orange?0x23:0x21);b.put(0x3880,x,0x0e);a.action(0);sound=true;
            if(!b.legacy()) b.sound(0x2a,orange?0x6570:0x65c9);
        }
        if(a.action()==1) {
            b.move(x);
            if(a.countdown()) {
                // A green alien takes its base with it; 38C0 names the base.
                if(!orange) b.remove(b(0x38c0,x));
                b.remove(x);
            }
        } else if(a.countdown()) {
            if(orange) {b.put(0x3700,x,0);b.put(0x3740,x,1);b.put(0x3580,x,0);b.put(0x3640,x,0xc0);}
            b.put(0x3780,x,0);b.put(0x37c0,x,orange?0xff:2);b.put(0x3880,x,0x0c);
            a.next();
        }
        if(sound && b.legacy()) b.sound(0x2a,0xea62);
        return;
    }
    default: break;
    }
}

// Explosions (types 50, 59, 60): 02:77E4, 7826, 7854. One reflection of
// FD2B, then gone; $68 bit 1 removes them at once.
void explosion(Actor a,unsigned type) {
    Bank& b=a.b;const unsigned x=a.x;
    if(!a.started()) {
        a.set(0xc4);b.put(0x3a00,x,0);a.oscillator(type==50?8:type==59?4:3,0x40);
        b.put(0x3800,x,0x9e);
        if(type==50 && (b.global(0xdb)&0x80)) {b.clear_bits(0xdb,0x80);b.sound(0x39,0x7816);}
        else b.sound(0x35,type==50?0x7820:type==59?0x784f:0x787d);
    }
    if(b.global(0x68)&2) {b.remove(x);return;} // 7880
    b.animate(x);
    if(a.resting()) b.remove(x);
}
}

bool lower_actor(Bank& b,unsigned type,unsigned x) {
    Actor a{b,x};
    switch(type) {
    case kOrange: case kSilverOrange: case kGreen: case kSilverGreen: case kBase:
    case kOrangeController: case kGreenController: case kOrangeDefeat: case kGreenDefeat:
        aliens(a,type);return true;
    case 50: case 59: case 60:
        explosion(a,type);return true;
    case 20: { // 01:52A1..53F2: the central mouth.
        if(!a.started()) {a.set(0xe0);b.put(0x3a00,x,0);a.oscillator(3,0x40);}
        if(b.global(0x68)&2) a.set(0x10);
        switch(a.action()) {
        case 1: // Opening; open, it releases three flyers (5308).
            if(a.reversing()) b.resume(0x5308);
            b.animate(x);a.set(0x10);return true;
        case 2: // Open for 600 ticks, or until it swallows the ball (5356).
            if(a.hit()) b.resume(0x5356);
            b.put(0x3ac0,x,0);
            if(!a.countdown16()) return true;
            a.action(a.action()+2);a.set(0x40);b.clear_bits(0x6d,0x80);
            b.lamp(false,4);return true;
        case 3: // Swallowed: closing on the ball.
            b.animate(x);a.set(0x10);
            if(a.countdown()) a.next();
            return true;
        case 4: // Closing.
            if(b(0x3c40,x)) {b.animate(x);a.set(0x10);return true;}
            a.clear(0x10);b.put(0x3ac0,x,0);b.clear_bits(0x6d,8);
            b.palette_rate(1);b.palette_rate(0x0e);a.action(0);return true;
        case 5:
            if(a.countdown()) a.action(0);
            return true;
        default: // Closed until $6D bit 3.
            if(b.global(0x6d)&8) {a.set(0x10);a.next();}
            return true;
        }
    }
    case 26: case 27: { // 01:541E / 544B: the cocoons.
        if(!a.started()) {
            a.set(0x88);b.put(0x3a00,x,0);a.oscillator(0x11,0x40);
            b.put(0x3880,x,9);b.put(0x3900,x,type==26?0x15:0x16);
        }
        if(b.global(0x68)&2) a.set(2);
        switch(a.action()) {
        case 1: // Hit nine times; the next hit hatches it (54D9).
            if(a.hit()) b.resume(0x54d9);
            return true;
        case 2: { // Hatching: at phase 14 an alien comes out (5504).
            b.oscillate(x);a.set(2);b.pattern(x); // 00:FD1D
            if(!b(0x38c0,x) && b(0x3c40,x)==0x0e) {
                const int y=b.free_slot();
                if(y>=0) a.place(static_cast<unsigned>(y),b(0x3900,x)==0x15?40:41,-8);
                b.palette_rate(b(0x3900,x));
                b.put(0x38c0,x,b(0x38c0,x)+1);
            }
            if(!a.reversing()) return true;
            a.next();b.put(0x3880,x,0xb4);a.set(0x60);return true;
        }
        case 3: // Hidden for 180 ticks, then back once the ball is clear.
            if(b(0x3880,x)) {b.put(0x3880,x,b(0x3880,x)-1);return true;}
            if(b.touching(0x5535)) return true;
            a.action(0);b.put(0x38c0,x,0);a.oscillator(0x11,0x40);b.put(0x3880,x,9);
            a.clear(0x60);a.set(2);b.pattern(x);return true;
        default:
            if(a.hit()) b.resume(0x5495);
            return true;
        }
    }
    case 29: { // 01:59A0: rises while $6D bit 0 is set.
        if(!a.started()) {a.set(0x80);b.put(0x3a00,x,0);a.oscillator(4,0x20);}
        switch(a.action()) {
        case 1:
            if(a.reversing()) {a.next();b.put(0x3ac0,x,0);return true;}
            b.animate(x);
            if(b.touching(0x59df)) {
                b.put(0x3c40,x,b(0x3c40,x)-1);
                if(b(0x3c40,x)&0x80) b.put(0x3c40,x,b(0x3c40,x)+1);
            }
            return true;
        case 2: // Up until hit or $6D bit 0 clears (59FE scores it).
            if((b.global(0x6d)&1) && !a.hit()) return true;
            b.clear_bits(0x6d,1);a.next();b.put(0x3880,x,0x20);b.score(0x0a);return true;
        case 3:
            if(a.countdown()) a.next();
            return true;
        case 4:
            b.animate(x);b.push(0x5a20);
            if(!a.resting()) return true;
            a.action(0);b.put(0x3ac0,x,0);return true;
        default:
            if(b.global(0x6d)&1) a.next();
            return true;
        }
    }
    case 30: { // 02:64A3: the lower skull guard, raised while $6D bit 2 is set.
        if(!a.started()) {
            a.set(0xe0);b.put(0x3a00,x,1);b.put(0x3780,x,0x80);b.put(0x37c0,x,0xff);b.put(0x3880,x,0x20);
        }
        switch(a.action()) {
        case 1:
            b.move(x);b.push(0x6501);
            if(a.countdown()) {a.next();a.clear(0x40);}
            return true;
        case 2:
            if(b.global(0x6d)&4) return true;
            a.set(0x40);b.put(0x3780,x,0x80);b.put(0x37c0,x,0);b.put(0x3880,x,0x20);a.next();
            b.score(0x0a);return true;
        case 3:
            b.move(x);
            if(a.countdown()) {a.action(0);a.set(0x60);}
            return true;
        default:
            if(!(b.global(0x6d)&4)) return true;
            b.put(0x35c0,x,0x85);b.put(0x3600,x,0);b.put(0x3680,x,0x28);b.put(0x36c0,x,1);
            b.put(0x3780,x,0x80);b.put(0x37c0,x,0xff);a.clear(0x20);a.next();b.put(0x3880,x,0x20);
            return true;
        }
    }
    case 33: { // 01:5B08: rises while $6D bit 1 is set.
        if(!a.started()) {
            a.set(0x88);b.put(0x3a00,x,0);a.oscillator(6,0x20);
            b.put(0x3700,x,0);b.put(0x3740,x,0xfe);b.put(0x3780,x,0);b.put(0x37c0,x,2);
        }
        switch(a.action()) {
        case 1:
            if(!(b.global(0x6d)&2)) b.resume(0x5b93);
            if(a.reversing()) {a.next();return true;}
            b.animate(x);
            if(b.touching(0x5b7f)) {
                b.put(0x3c40,x,b(0x3c40,x)-1);
                if(b(0x3c40,x)&0x80) b.put(0x3c40,x,b(0x3c40,x)+1);
            }
            return true;
        case 2:
            if(b.global(0x6d)&2) return true;
            b.resume(0x5b9c);
        case 3:
            if(!a.resting()) {b.animate(x);b.push(0x5bce);return true;}
            a.action(0);b.put(0x3ac0,x,0);return true;
        default:
            if(!(b.global(0x6d)&2)) return true;
            a.oscillator(6,0x20);b.score(0x14);b.sound(0x30,0x5b6d);a.next();return true;
        }
    }
    case 40: case 41: // 01:5667 / 571A: an alien unfolding from a cocoon.
        if(!a.started()) {a.set(0xc4);b.put(0x3a00,x,0);a.oscillator(4,0x20);b.put(0x3800,x,type==40?0x4b:0x4c);}
        if(a.reversing()) {b.put(0x3500,x,type==40?52:53);a.action(0);return true;}
        b.scratch(0x0a,b(0x3a80,x));b.animate(x);
        return true;
    case 52: case 53: // 01:56AA / 575D: launched from the slingshots.
        if(b.global(0x53)&0x80) {b.remove(x);return true;}
        if(!a.action()) {
            a.clear(0x40);b.put(0x3a00,x,3);a.oscillator(3,0x20);
            const unsigned direction=type==52?0x14:0x1c;
            b.scratch(0x06,8);b.heading(x,direction,8);b.move(x);
            b.scratch(0x06,0x82);b.heading(x,direction,0x82);a.next();
        }
        b.animate(x);b.move(x);
        if(a.flags()&1) {b.remove(x);return true;}
        if(a.hit()) b.resume(type==52?0x5703:0x57b6);
        return true;
    case 56: // 01:5600: flyers from the central mouth.
        if(!a.started()) {
            a.set(0x84);b.put(0x3a00,x,3);a.oscillator(4,0x20);b.put(0x3800,x,0xb9);
            b.scratch(0x06,1);b.heading(x,b.ranged(x,0x10),1);
        }
        if(b.global(0x53)&0x80) {b.remove(x);return true;}
        if(a.hit()) b.resume(0x5650);
        b.animate(x);b.move(x);
        if(a.flags()&1) b.remove(x);
        return true;
    case 45: case 46: case 47: { // 02:70C5, 712D, 7195: lower progress indicators.
        const unsigned pending=type==45?0x5f:type==46?0x5c:0x62,index=pending+2;
        const unsigned enabled=type==45?0x4d:type==46?0x4c:0x4e,release=pending+1;
        if(!a.started()) {a.set(type==46?0xc8:0xc0);b.put(0x3a00,x,0);a.oscillator(3,0,1);}
        // 47 is one of nine eyes, each with its own poses at 04:BF56.
        const auto step=[&] {
            if(type!=47) {b.animate(x);return;}
            const unsigned i=(x-0x37)&255,low=((i*2)&255)+i;
            b.scratch(0x0a,i);
            b.animate(x,0xbf00+(low&255)+0x56);
        };
        switch(a.action()) {
        case 1:
            if(a.reversing()) a.next();else step();
            return true;
        case 2:
            if(b.global(enabled) && (!(b.global(release)&0x80) || x!=b.global(index))) return true;
            if(b.global(enabled)) {b.clear_bits(release,0x80);b.set_global(index,b.global(index)-1);}
            a.next();return true;
        case 3:
            if(!a.resting()) step();else a.action(0);
            return true;
        default:
            if(!b.global(pending)) return true;
            b.set_global(pending,b.global(pending)-1);a.next();b.set_global(index,x);
            return true;
        }
    }
    case 70: // 02:7892: bouncing orbs.
        if(!a.started()) {
            a.set(0xe0);b.put(0x3a00,x,0x49);a.oscillator(0x32,0,1);b.put(0x38c0,x,b(0x3680,x));
            b.put(0x3b40,x,0xe0);b.put(0x3980,x,0);b.put(0x39c0,x,1);b.put(0x3880,x,1);b.put(0x3800,x,0x2a);
        }
        switch(a.action()) {
        case 1: case 2: break;
        default:
            // Hidden until the timer runs out and the ball is above 208
            // (low byte). The stored half has no ball near it.
            if(b(0x3880,x)) {b.put(0x3880,x,b(0x3880,x)-1);if(b(0x3880,x)) return true;}
            if(b.active() && b(0x3680,0)>=0xd0) return true;
            a.next();a.clear(0x60);return true;
        }
        [[fallthrough]];
    case 71: { // 02:7988 (upper), sharing 78FA..797C.
        if(type==71 && !a.started()) {
            a.set(0xe0);b.put(0x3a00,x,0x49);a.oscillator(0x32,0,1);b.put(0x38c0,x,b(0x3680,x));
            b.put(0x3880,x,1);b.put(0x3800,x,0x2a);b.put(0x3b40,x,0xe0);b.put(0x3980,x,0);b.put(0x39c0,x,1);
        }
        switch(a.action()) {
        case 1: // Bobbing.
            if(a.hit()) b.resume(0x792e);
            b.move(x);
            if(b(0x3780,x)|b(0x37c0,x)) {b.put(0x3800,x,(b(0x37c0,x)&0x80)?0x2b:0x2c);return true;}
            if(b(0x3b40,x)&0x80) {b.put(0x3800,x,0x2a);a.next();b.put(0x3880,x,8);}
            return true;
        case 2: // Resting on the bottom.
            if(a.hit()) b.resume(0x792e);
            if(a.countdown()) a.action(a.action()-1);
            return true;
        default: // 71 only: hidden until its timer runs out.
            if(a.countdown()) {a.next();a.clear(0x60);}
            return true;
        }
    }
    case 75: case 76: case 77: // 01:5CA5, 5CFA, 5D50: lower lane sensors.
        if(!a.started()) {a.set(0xb0);b.put(0x3a00,x,0);a.oscillator(4,0,1);}
        switch(a.action()) {
        case 1: // 5C62
            if(a.reversing()) {b.put(0x3880,x,8);a.next();return true;}
            b.animate(x);a.set(0x10);return true;
        case 2: // 5C7C
            if(a.countdown()) a.next();
            return true;
        case 3: // 5C85
            if(a.reversing()) {b.animate(x);a.set(0x10);return true;}
            a.action(0);b.put(0x3ac0,x,0);return true;
        default: // The sensor's $82 bit lasts one update.
            if(a.hit()) b.resume(type==75?0x5cdf:type==76?0x5d34:0x5d8a);
            b.clear_bits(0x82,type==75?2:type==76?4:8);
            return true;
        }
    default:
        return false;
    }
}
}
