// SPDX-License-Identifier: GPL-3.0-or-later
// The upper table's actors for gameplay::CabinetActors. Each is its ROM
// routine (addresses below), in the order the routine does things.
#include "cabinet_actor_bank.hpp"
namespace alien_crush::gameplay::cabinet {
namespace {
// FD35's step negated, as the ROM does with SEC/CLA/SBC.
void reverse(Bank& b,unsigned x) {
    const unsigned step=(65536-(b(0x3b80,x)|(b(0x3bc0,x)<<8)))&65535;
    b.put(0x3b80,x,step);b.put(0x3bc0,x,step>>8);
}
// 02:6F4C / 6F60 / 6F71: the ball held in a mouth, placed from a table of
// offsets.
void hold_ball(Actor& a,unsigned table,unsigned index) {
    Bank& b=a.b;
    a.carry(0,b.rom().at(table+index*2),b.rom().at(table+index*2+1),true);
}
// The ball's launch from a mouth (6979 / 6B20 / 6CE2 / 6E83): a direction
// at speed $B1, and an eighth of it for 03:9022; the ball's action loses
// bit 0.
void launch_ball(Bank& b,unsigned ball,unsigned direction) {
    b.scratch(0x06,b.global(0xb1));
    b.heading(ball,direction,b.global(0xb1));b.eighth(ball);
    b.put(0x3840,ball,b(0x3840,ball)&0xfe);
}
// A mouth closing or opening over the ball: FD2B, then 03:8577 steps the
// pose back while the ball touches it.
void over_ball(Actor& a,std::uint16_t pc) {
    Bank& b=a.b;const unsigned x=a.x;
    b.animate(x);
    if(!b.touching(pc)) return;
    b.put(0x3c40,x,b(0x3c40,x)-1);
    if(b(0x3c40,x)&0x80) b.put(0x3c40,x,b(0x3c40,x)+1);
}
// 03:911B for an upper table actor ($79 = 1, 913F): a step of the upper
// progress. Nine lit steps light lamp $17 + $4B, advance $4B and set $6B bit
// 7, which raises the riser again (01:496C).
void upper_progress(Bank& b) {
    if(b.global(0x4a)!=9) {b.set_global(0x4a,b.global(0x4a)+1);b.set_global(0x55,1);return;}
    if(b.global(0x4b)==6) return;
    b.set_global(0x4a,0);
    b.lamp(true,0x17+b.global(0x4b)); // 9210
    b.set_global(0x4b,b.global(0x4b)+1);b.set_bits(0x6b,0x80);
}
// 01:5AC8, stored half only: the riser starts down once its flag clears.
// The ball clears it leaving the upper table (01:496C), usually for the
// launcher, so the riser drops then rather than on the ball's return; its 20
// points and progress step are the ones the ROM gives on that return.
void lower_riser(Actor& a) {
    Bank& b=a.b;const unsigned x=a.x;
    a.next();b.score(0x14,true);upper_progress(b);
    if(!a.reversing()) reverse(b,x);
}
// 01:51FE: velocity towards the bumper destination for layout A, target
// $A9, in 16 updates; $A9 moves on.
void bumper_course(Actor& a,unsigned layout) {
    Bank& b=a.b;const unsigned x=a.x;
    const unsigned twice=(layout*2)&255,six=(((layout*4)&255)+twice)&255;
    b.scratch(0x02,twice);b.scratch(0x0a,six);
    const unsigned y=((b.global(0xa9)*2)&255)+six;
    const unsigned dx=(b.rom()(1,0x526b+(y&255))-(b(0x35c0,x)|(b(0x3600,x)<<8)))&65535;
    const unsigned dy=(b.rom()(1,0x526c+(y&255))-(b(0x3680,x)|(b(0x36c0,x)<<8)))&65535;
    b.put(0x3780,x,dy<<4);b.put(0x37c0,x,(dy<<4)>>8);
    b.put(0x3700,x,dx<<4);b.put(0x3740,x,(dx<<4)>>8);
    b.scratch(0x02,dx<<4);b.scratch(0x03,(dx<<4)>>8);b.scratch(0x04,dy<<4);b.scratch(0x05,(dy<<4)>>8);
    b.set_global(0xa9,b.global(0xa9)+1==3?0:b.global(0xa9)+1);
}
}

bool upper_actor(Bank& b,unsigned type,unsigned x) {
    Actor a{b,x};
    switch(type) {
    case 7: { // 01:4FE9: the brain.
        if(!a.started()) {a.set(0xa0);b.put(0x3a00,x,0);b.put(0x3880,x,9);}
        if(b.global(0x68)&2) a.set(0x10);
        switch(a.action()) {
        case 1: // Opened by nine touches; the next one sets it off (5061).
            if(a.hit()) b.resume(0x5061);
            return true;
        case 2: // Opening; open, it releases four flyers (50F0).
            b.animate(x);a.set(0x10);
            if(!a.reversing()) return true;
            b.scratch(0x01,4);
            for(unsigned left=4;left;--left) {
                const int y=b.free_slot();
                if(y<0) break;
                const unsigned slot=static_cast<unsigned>(y);
                b.put(0x3500,slot,0x37);
                const unsigned across=b(0x35c0,x)+0x20,down=b(0x3680,x)+0x20;
                b.put(0x35c0,slot,across);b.put(0x3600,slot,b(0x3600,x)+(across>>8));
                b.put(0x3680,slot,down);b.put(0x36c0,slot,b(0x36c0,x)+(down>>8));
                b.put(0x38c0,slot,left*2);b.scratch(0x01,left-1);
            }
            a.next();b.put(0x3880,x,0xc0);return true;
        case 3:
            if(a.countdown()) a.next();
            return true;
        case 4: // Closing; closed, the lamps are relit (03:9415).
            if(b(0x3c40,x)) {b.animate(x);a.set(0x10);return true;}
            b.resume(0x50d7);
        default: // Each touch counts down (5026).
            if(a.hit()) b.resume(0x5026);
            b.clear_bits(0x6b,1);return true;
        }
    }
    case 8: case 37: case 38: case 39: { // 01:57CD, 580A, 5846, 587F: targets.
        const unsigned i=type==8?0:type-36;
        if(!a.started()) {
            a.set(0x80);b.put(0x3a00,x,0);b.put(0x3b80,x,0x20);b.put(0x3a40,x,3);
            constexpr unsigned kDown[]{1,2,1,2},kBit[]{2,1,4,8},kLamp[]{8,7,6,5};
            unsigned count=kDown[i];
            if((b.global(0xaf)&1) && !(b.global(0xab+i)&0x80)) {b.put(0x3ac0,x,0x80);count=b.global(0xab+i)+1;}
            b.put(0x3880,x,count);b.put(0x3900,x,kBit[i]);b.put(0x3940,x,kLamp[i]);
        }
        const auto pointer=[&] {return (b(0x3880,x)>>1)?0xbf38u:0xbecfu;};
        const auto state=[&](unsigned value) {b.set_global(0x20ab+((x-7)&255),value);};
        const auto count=[&](int delta) {
            const unsigned counter=0x51+(b(0x3880,x)>>1);
            b.set_global(counter,b.global(counter)+static_cast<unsigned>(delta));
        };
        unsigned action=a.action();
        if(action!=1 && action!=2) {
            if(a.hit()) b.resume(0x58d1);
            if(!(b.global(0x6c)&4) || !b(0x3c40,x)) return true;
            b.lamp(true,b(0x3940,x),0x590f); // 590F: up again.
            b.put(0x3ac0,x,0);a.action(2);action=2;
        }
        b.animate(x,pointer());
        if(action==1) { // Going down.
            if(!a.reversing()) return true;
            a.action(0);a.clear(0x40);state(b(0x3880,x));count(1);
        } else { // Coming up.
            if(a.reversing()) return true;
            b.put(0x3c00,x,0);a.action(0);a.clear(0x40);state(0x80);count(-1);
        }
        return true;
    }
    case 9: { // 02:65FD: the rebound.
        if(!a.started()) {a.set(0x88);b.put(0x3a00,x,0x20);b.put(0x3a40,x,4);b.put(0x3a80,x,4);}
        const unsigned action=a.action();
        if(action!=1 && action!=2) {
            if(a.hit()) b.resume(0x662b);
            return true;
        }
        const auto settle=[&] { // 66F1 / 674D
            a.action(0);a.clear(0x40);b.put(0x3c00,x,0);b.put(0x3c40,x,0);b.put(0x3ac0,x,0);b.pose(x);
        };
        b.clear_bits(0x6b,4);
        if(!(b(0x3b80,x)|b(0x3bc0,x))) {settle();return true;}
        b.animate(x);
        if(action==1) { // Each far reflection loses $3B00 of speed.
            if(!a.reversing()) return true;
            reverse(b,x);
            const unsigned low=b(0x3b00,x)+b(0x3b80,x);
            b.put(0x3b80,x,low);b.put(0x3bc0,x,b(0x3bc0,x)+0xff+(low>>8));
            if((b(0x3bc0,x)&0x80) || !(b(0x3bc0,x)|b(0x3b80,x))) {settle();return true;}
            b.put(0x3c00,x,0);b.put(0x3c40,x,0);b.score(4);b.sound(0x23,0x66ed);
            return true;
        }
        if(a.reversing()) return true;
        reverse(b,x);
        const unsigned low=b(0x3b00,x)+b(0x3b80,x);
        b.put(0x3b80,x,low);b.put(0x3bc0,x,b(0x3bc0,x)+(low>>8));
        if(!(b(0x3bc0,x)&0x80) || !(b(0x3bc0,x)|b(0x3b80,x))) {settle();return true;}
        b.put(0x3c00,x,0);b.put(0x3c40,x,b(0x3a40,x));
        return true;
    }
    case 10: // 01:5129: bumpers.
        if(!a.started()) {
            a.set(0x80);b.put(0x3a00,x,0x20);b.put(0x3a40,x,3);b.put(0x3b80,x,0xc0);b.put(0x3bc0,x,0xff);
            b.put(0x3c00,x,0);b.put(0x3c40,x,2);
            b.put(0x3a80,x,b.rom()(1,0x51b2+b.global(0xa9)));
            b.set_global(0xa9,b.global(0xa9)+1==3?0:b.global(0xa9)+1);
        }
        // The upper lane sensor ($82 bit 0) sends them travelling (type 51).
        if(b.global(0x82)&1) {b.put(0x3500,x,51);a.flags(0);return true;}
        if(a.action()==1) {
            b.animate(x);
            if(b(0x3c40,x)==2) a.action(0);
            return true;
        }
        if(a.hit()) b.resume(0x5179);
        return true;
    case 51: // 01:51B5: a bumper travelling to its place in layout $A8.
        if(!a.started()) {
            a.set(0x80);b.put(0x3a00,x,0x23);bumper_course(a,8);b.put(0x3880,x,0x10);a.action(0);
        }
        b.move(x);
        if(a.action()==1) {
            if(a.countdown()) {b.put(0x3500,x,10);a.flags(0);}
            return true;
        }
        if(a.countdown()) {bumper_course(a,b.global(0xa8));a.next();b.put(0x3880,x,0x10);}
        return true;
    case 11: { // 02:684B: the mouth that returns the ball from 01:499E.
        if(!a.started()) {a.set(0x88);b.put(0x3a00,x,0);b.put(0x3a40,x,4);b.put(0x3b80,x,0xe0);b.put(0x3bc0,x,0xff);
            b.put(0x3c00,x,0);b.put(0x3c40,x,4);}
        const bool returning=(b.global(0x5b)&0x40)?b.global(0x83)<2:(b.global(0x5b)&0x80)!=0;
        if(returning) {
            b.set_global(0x5b,0);
            if(a.action()) a.action(3);
            else {a.action(4);a.carry(0,6,8,false);b.put(0x3880,x,0x40);}
        }
        switch(a.action()) {
        case 1: // Opening, over the ball.
            if(!a.resting()) {over_ball(a,0x68f7);return true;}
            a.next();return true;
        case 2: // Open while $6F bit 2 holds; swallows the ball (690D).
            if(!(b.global(0x6f)&4)) {a.action(7);return true;}
            if(a.hit()) b.resume(0x690d);
            return true;
        case 3: // Swallowing.
            if(a.reversing()) {a.next();b.put(0x3880,x,0x40);b.put(0x3540,0,b(0x3540,0)|0x20);return true;}
            b.animate(x);b.scratch(0x04,0xd7);b.scratch(0x05,0x69);hold_ball(a,0x69d7,b(0x3c40,x));
            return true;
        case 4:
            if(a.countdown()) a.next();
            return true;
        case 5: // Spitting the ball out.
            b.animate(x);
            if(!a.resting()) return true;
            launch_ball(b,0,4);b.put(0x3540,0,b(0x3540,0)&0xdf);
            b.put(0x3880,x,8);a.next();b.sound(0x2e,0x69a6);return true;
        case 6:
            if(a.countdown()) {a.clear(0x40);b.put(0x3ac0,x,0);a.next();}
            return true;
        case 7: // Closing.
            if(!a.reversing()) {over_ball(a,0x69c7);return true;}
            a.action(0);b.put(0x3ac0,x,0);b.clear_bits(0x6f,4);b.lamp(false,2);return true;
        default:
            if(!(b.global(0x6f)&4)) return true;
            a.next();b.lamp(true,2);return true;
        }
    }
    case 12: { // 02:6A0C: the mouth that returns the ball from 01:49BA.
        if(!a.started()) {a.set(0x88);b.put(0x3a00,x,0);a.oscillator(4,0x20);}
        if((b.global(0x5b)&0x40) && b.global(0x83)==3) {
            b.set_global(0x5b,0);a.carry(0,0x10,0x0e,false);b.put(0x3540,0,b(0x3540,0)&0xdf);
            b.put(0x3880,x,0x40);a.action(4);
        }
        switch(a.action()) {
        case 1:
            if(!a.reversing()) {over_ball(a,0x6aa5);return true;}
            a.next();a.clear(0x40);return true;
        case 2:
            if(!(b.global(0x6f)&2)) {a.action(7);return true;}
            if(a.hit()) b.resume(0x6ac3);
            return true;
        case 3: // Chewing on the ball, then off to the lower table (49BA).
            if(a.resting()) {
                if(a.countdown()) {b.put(0x3880,x,0x40);a.next();b.resume(0x6b09);}
                hold_ball(a,0x6b84,b(0x3880,x));
            }
            b.animate(x);return true;
        case 4:
            if(!a.countdown()) return true;
            a.next();
            [[fallthrough]];
        case 5: { // Spitting the ball out at phase 2.
            b.animate(x);
            if(b(0x3c40,x)!=2) return true;
            const unsigned ball=b(0x3ac0,x)&0x3f;
            launch_ball(b,ball,0x0c);
            b.put(0x3880,x,8);a.next();b.sound(0x2e,0x6b48);return true;
        }
        case 6:
            if(!a.reversing()) over_ball(a,0x6b54);
            if(a.countdown()) {a.clear(0x40);a.next();}
            return true;
        case 7:
            if(!a.resting()) {over_ball(a,0x6b74);return true;}
            a.action(0);b.put(0x3ac0,x,0);b.clear_bits(0x6f,2);b.lamp(false,3);return true;
        default:
            if(!(b.global(0x6f)&2)) return true;
            a.next();b.lamp(true,3);return true;
        }
    }
    case 13: { // 02:6B8C: the mouth that returns the ball from 01:49AC.
        if(!a.started()) {
            a.set(0x88);b.put(0x3a00,x,0);b.put(0x35c0,x,0x20);b.put(0x3600,x,0);b.put(0x3680,x,0xd0);b.put(0x36c0,x,0);
            b.put(0x3800,x,0x5b);a.oscillator(4,0x20);
        }
        if((b.global(0x5b)&0x40) && b.global(0x83)==2) {
            b.set_global(0x5b,0);a.carry(0,0x10,0x10,false);b.put(0x3880,x,2);a.action(4);b.put(0x3a40,x,2);
            b.put(0x3540,0,b(0x3540,0)&0xdf);
        }
        if(b.global(0x70)&1) { // 6C47: closed at once.
            a.action(0);a.clear(0x40);b.put(0x3ac0,x,0);b.put(0x3800,x,0x5b);
            b.clear_bits(0x70,1);b.clear_bits(0x6f,8);b.lamp(false,0x0e);return true;
        }
        switch(a.action()) {
        case 1:
            if(a.reversing()) {a.next();return true;}
            over_ball(a,0x6c6b);return true;
        case 2:
            if(!(b.global(0x6f)&8)) {a.action(6);return true;}
            if(a.hit()) b.resume(0x6c81);
            return true;
        case 3: // Swallowing, then off to the lower table (49AC).
            if(!a.resting()) {hold_ball(a,0x6d34,b(0x3c40,x));b.animate(x);return true;}
            b.put(0x3880,x,4);b.put(0x3a40,x,2);a.next();b.resume(0x6cc1);
        case 4:
            b.animate(x);
            if(a.resting() && a.countdown()) a.next();
            return true;
        case 5:
            if(!a.reversing()) {b.animate(x);return true;}
            launch_ball(b,0,0x1d);b.put(0x3880,x,8);a.next();b.sound(0x2e,0x6d06);return true;
        case 6: // Closed again; the lamp is kept off meanwhile.
            if(!a.resting()) {b.animate(x);return true;}
            if(a.countdown()) {
                a.action(0);b.put(0x3ac0,x,0);b.put(0x3a40,x,3);b.clear_bits(0x6f,8);a.clear(0x40);
            }
            b.lamp(false,0x0e);return true;
        default:
            if(!(b.global(0x6f)&8)) return true;
            a.next();a.oscillator(4,0x20);b.lamp(true,0x0e);return true;
        }
    }
    case 34: { // 02:6D3C: the tentacled mouth.
        if(!a.started()) {a.set(0x88);b.put(0x3a00,x,0);}
        const auto phase=[&](unsigned frames,unsigned step_low,unsigned step_high,unsigned at) {
            b.put(0x3a40,x,frames);b.put(0x3b80,x,step_low);b.put(0x3bc0,x,step_high);
            b.put(0x3c00,x,0);b.put(0x3c40,x,at);
        };
        switch(a.action()) {
        case 1: // Opening to phase 3.
            if(b(0x3c40,x)!=3) {over_ball(a,0x6daa);return true;}
            a.next();a.clear(0x40);phase(4,0xe0,0xff,4);return true;
        case 2: // Open while $6F bit 0 holds; swallows the ball (6DDB).
            if(!(b.global(0x6f)&1)) {a.action(7);phase(7,0x20,0,3);return true;}
            if(a.hit()) b.resume(0x6ddb);
            b.animate(x);return true;
        case 3:
            if(a.reversing()) {
                b.put(0x3540,0,b(0x3540,0)|0x20);a.next();b.put(0x3880,x,0x60);b.set_bits(0x6c,8);return true;
            }
            b.animate(x);b.scratch(0x04,0x13);b.scratch(0x05,0x6f);hold_ball(a,0x6f13,b(0x3c40,x));
            return true;
        case 4:
            if(!a.countdown()) return true;
            if(b.global(0x6f)&0x80) {b.put(0x3880,x,5);return true;}
            a.next();return true;
        case 5:
            if(b(0x3c40,x)!=4) {b.animate(x);return true;}
            launch_ball(b,0,9);b.put(0x3540,0,b(0x3540,0)&0xdf);
            b.put(0x3880,x,8);a.next();a.set(0x40);b.sound(0x2e,0x6eb7);return true;
        case 6:
            if(b(0x3c40,x)!=3) b.animate(x);
            if(a.countdown()) {a.clear(0x40);a.next();phase(7,0x20,0,3);}
            return true;
        case 7:
            if(!a.reversing()) {over_ball(a,0x6ef4);return true;}
            a.action(0);b.put(0x3ac0,x,0);a.clear(0x40);b.clear_bits(0x6c,8);b.clear_bits(0x6f,1);
            b.palette_rate(0x33);b.lamp(false,1);return true;
        default:
            if(!(b.global(0x6f)&1)) return true;
            a.next();a.set(0x40);phase(7,0xe0,0xff,7);b.palette_start(0x33);b.lamp(true,1);return true;
        }
    }
    case 14: { // 02:63DB: the upper skull guard, raised while $70 bit 3 is set.
        if(!a.started()) {
            a.set(0xe0);b.put(0x3a00,x,1);b.put(0x3780,x,0x80);b.put(0x37c0,x,0xff);b.put(0x3880,x,0x20);
        }
        // $70 bit 1: the ball left the upper table (03:8A24); back down.
        if(b.global(0x70)&2) {a.action(0);a.set(0x60);b.clear_bits(0x70,0x0a);return true;}
        switch(a.action()) {
        case 1:
            b.move(x);b.push(0x644f);
            if(a.countdown()) {a.next();a.clear(0x40);}
            return true;
        case 2:
            if(b.global(0x70)&8) return true;
            a.set(0x40);b.put(0x3780,x,0x80);b.put(0x37c0,x,0);b.put(0x3880,x,0x20);a.next();
            b.score(0x0a);return true;
        case 3:
            b.move(x);
            if(a.countdown()) {a.action(0);a.set(0x60);b.put(0x3ac0,x,0);}
            return true;
        default:
            if(!(b.global(0x70)&8)) return true;
            b.put(0x35c0,x,0x88);b.put(0x3600,x,0);b.put(0x3680,x,0x2c);b.put(0x36c0,x,1);
            b.put(0x3780,x,0x80);b.put(0x37c0,x,0xff);a.clear(0x20);a.next();b.put(0x3880,x,0x20);
            return true;
        }
    }
    case 19: { // 02:76CA: the brain's orbs.
        if(!a.started()) {
            a.set(0x80);b.put(0x3a00,x,0x16);a.oscillator(2,0x20);
            b.put(0x3880,x,b.random(x));b.put(0x3800,x,0x2c);
        }
        if(a.hit()) b.resume(0x76fc);
        const auto hop=[&] { // 7786: a hop straight up, with gravity.
            a.next();b.put(0x3b40,x,0xe0);b.put(0x3980,x,0);b.put(0x39c0,x,1);
            b.put(0x3a00,x,b(0x3a00,x)|0x49);
        };
        switch(a.action()) {
        case 1: // Wobbling; the brain's touch ($6B bit 0) makes them hop.
            if(b.global(0x6b)&1) {hop();return true;}
            b.move(x);
            if(b(0x3700,x)!=b(0x3900,x) || b(0x3740,x)!=b(0x3940,x) || !a.countdown()) return true;
            b.put(0x3880,x,b.random(x)>>1);a.action(0);return true;
        case 2: // Hopping.
            b.move(x);
            if(b(0x3780,x)|b(0x37c0,x)) {b.put(0x3800,x,(b(0x37c0,x)&0x80)?0x2b:0x2c);return true;}
            if(!(b(0x3b40,x)&0x80)) return true;
            b.put(0x3800,x,0x2a);a.oscillator(2,0x20);a.action(0);b.put(0x3a00,x,b(0x3a00,x)&0xb6);
            return true;
        default: // Resting.
            if(b.global(0x6b)&1) {a.next();hop();return true;}
            b.put(0x3800,x,0x2c);
            if(!a.countdown()) return true;
            b.put(0x3700,x,0x80);b.put(0x3740,x,0);b.put(0x3900,x,0x80);b.put(0x3940,x,0);
            b.put(0x3b00,x,0xc0);b.put(0x3b40,x,0xff);a.next();b.put(0x3880,x,b.ranged(x,4)+1);
            return true;
        }
    }
    case 66: // 02:7626: the orbs' controller. 300 ticks after the last one
             // goes, five come back (7673).
        if(!a.started()) {a.set(0x80);b.put(0x3a00,x,0);b.put(0x3880,x,1);}
        switch(a.action()) {
        case 1:
            if(a.countdown16()) a.next();
            return true;
        case 2: {
            if(b.global(0x67)==4) return true;
            std::array<unsigned,6> slots{};
            if(!b.allocate(5,slots)) return true;
            for(unsigned i=0;i<5;++i) {
                const unsigned slot=slots[i],row=0x76b6+((b.global(0x65)*4)&255);
                b.put(0x3500,slot,0x13);
                b.put(0x35c0,slot,b.rom()(2,row));b.put(0x3600,slot,b.rom()(2,row+1));
                b.put(0x3680,slot,b.rom()(2,row+2));b.put(0x36c0,slot,b.rom()(2,row+3));
                b.set_global(0x65,b.global(0x65)+1);
            }
            a.action(0);b.put(0x3880,x,0x2c);b.put(0x38c0,x,1);return true;
        }
        default:
            if(!b.global(0x65)) a.next();
            return true;
        }
    case 24: case 35: // 02:6776 / 67F0: set off by other actors.
        if(!a.started()) {a.set(0x88);b.put(0x3a00,x,0);a.oscillator(3,0x40);}
        if(a.action()==1) {b.animate(x);if(a.reversing()) a.next();return true;}
        if(a.action()==2) {
            b.animate(x);
            if(a.resting()) {a.clear(0x40);a.action(0);}
        }
        return true;
    case 32: // 01:5A43: rises while $6F bit 5 is set.
        if(!a.started()) {
            a.set(0x88);b.put(0x3a00,x,0);
            b.put(0x3700,x,0);b.put(0x3740,x,0xfe);b.put(0x3780,x,0);b.put(0x37c0,x,2);
        }
        switch(a.action()) {
        case 1:
            if(!(b.global(0x6f)&0x20)) {
                if(b.active() || !b.lowers_riser()) b.resume(0x5abf);
                a.next();lower_riser(a);return true;
            }
            if(a.reversing()) {a.next();return true;}
            over_ball(a,0x5aab);return true;
        case 2:
            if(b.global(0x6f)&0x20) return true;
            if(b.active() || !b.lowers_riser()) b.resume(0x5ac8);
            lower_riser(a);return true;
        case 3:
            if(!a.resting()) {b.animate(x);b.push(0x5afa);return true;}
            a.action(0);b.put(0x3ac0,x,0);b.lamp(false,0x0f);return true;
        default:
            if(!(b.global(0x6f)&0x20)) return true;
            a.oscillator(4,0x20);b.score(0x14);b.sound(0x30,0x5a95);
            a.next();b.lamp(true,0x0f);return true;
        }
    case 42: { // 02:6FA3: the upper progress indicator.
        if(!a.started()) {a.set(0xc0);b.put(0x3a00,x,0);a.oscillator(3,0,1);}
        switch(a.action()) {
        case 1:
            if(a.reversing()) a.next();else b.animate(x);
            return true;
        case 2:
            if(b.global(0x4a) && (!(b.global(0x57)&0x80) || x!=b.global(0x59))) return true;
            if(b.global(0x4a)) {b.clear_bits(0x57,0x80);b.set_global(0x59,b.global(0x59)-1);}
            a.next();return true;
        case 3:
            if(!a.resting()) b.animate(x);else a.action(0);
            return true;
        default:
            if(!b.global(0x55)) return true;
            b.set_global(0x55,b.global(0x55)-1);a.next();b.set_global(0x59,x);return true;
        }
    }
    case 44: // 02:700B: lit while $6F bit 7 holds, for up to 45,060 ticks.
        if(!a.started()) {a.set(0xc0);b.put(0x3a00,x,0x20);b.put(0x3a80,x,9);}
        switch(a.action()) {
        case 1:
            if(!a.reversing()) {b.animate(x);return true;}
            b.put(0x38c0,x,0xb0);b.put(0x3880,x,4);a.next();return true;
        case 2:
            if(b.global(0x6f)&0x80) {
                if(b.global(0x54)&0x80) return true;
                if(!b(0x38c0,x)) b.put(0x3880,x,b(0x3880,x)-1);
                b.put(0x38c0,x,b(0x38c0,x)-1);
                if(b(0x38c0,x)|b(0x3880,x)) return true;
                b.set_bits(0x6c,4);
            }
            b.clear_bits(0x6f,0x80);a.next();
            b.palette_rate(0x39);b.palette_rate(0x3a);b.palette_rate(0x26);return true;
        case 3:
            if(!a.resting()) {b.animate(x);return true;}
            a.action(0);b.put(0x3a80,x,9);return true;
        default:
            if(!(b.global(0x6f)&0x80)) return true;
            b.put(0x3a80,x,b.global(0x54)==3?9:0x0a);a.oscillator(3,0,1);a.next();
            b.palette_start(0x39);b.palette_start(0x3a);b.palette_start(0x26);return true;
        }
    case 48: // 02:69E5: shown only while the previous slot's phase is not 0.
        if(!a.started()) {a.set(0xc0);b.put(0x3a00,x,0);}
        if(b(0x3c40,(x-1)&255)) a.set(0x20);else a.clear(0x20);
        return true;
    case 55: // 01:559C: flyers from the brain.
        if(!a.started()) {
            a.set(0x84);b.put(0x3a00,x,3);a.oscillator(4,0x20);b.put(0x3800,x,0x76);
            b.scratch(0x06,0x82);b.heading(x,b(0x38c0,x),0x82);b.put(0x38c0,x,0);
            b.put(0x3b00,x,0x10);b.put(0x3b40,x,0x10);
        }
        if(b.global(0x54)&0x80) {b.remove(x);return true;}
        if(a.hit()) {b.put(0x3500,x,59);a.flags(0);return true;}
        b.animate(x);b.move(x);
        if(a.flags()&1) b.remove(x);
        return true;
    case 74: // 01:5BF2: the upper lane sensor.
        if(!a.started()) {a.set(0xb0);b.put(0x3a00,x,0);a.oscillator(4,0,1);}
        switch(a.action()) {
        case 1:
            if(a.reversing()) {b.put(0x3880,x,8);a.next();return true;}
            b.animate(x);a.set(0x10);return true;
        case 2:
            if(a.countdown()) a.next();
            return true;
        case 3:
            if(a.reversing()) {b.animate(x);a.set(0x10);return true;}
            a.action(0);b.put(0x3ac0,x,0);return true;
        default:
            if(a.hit()) b.resume(0x5c23);
            b.clear_bits(0x82,1);return true;
        }
    default:
        return false;
    }
}
}
