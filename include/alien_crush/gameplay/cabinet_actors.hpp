// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <span>
#include <vector>
namespace alien_crush::gameplay {
namespace cabinet {struct Rom;class Bank;}
// Cabinet mode's table actors. The cartridge runs only the half of the table
// the ball is in, but the cabinet shows both halves, so there the port runs
// the stored half's actors and palette scripts itself: what is drawn is what
// the ball meets when it gets there. Each rule is the ROM's (addresses in the
// routines). State stays in the actor banks of work RAM, where the original
// ball physics collides with it and quick states save it.
//
// The ball's half runs the ROM's own routines, in their own time: the port's
// updates take no emulated cycles, and running them there would take away the
// original's slowdown. The stored half runs without touching anything the
// original would not: a private random source, no sound, no scratch, no ball,
// and the frame's $69 events are the ball's half's alone. An actor that would
// score, need the ball, run the game's rules or draw on the ball's table waits,
// unchanged, for its half to be the ball's again.
//
// Verification predicts each ROM update in the ball's half with the same
// routines, which must match it byte for byte (ActorMode::verify).
class CabinetActors {
public:
    // Which actors the port runs: every table actor that moves on its own,
    // or, as runtimes marked -aliens1 did, only the lower table's aliens, but
    // in both halves.
    enum class Scope {aliens,table};
    // `stored_half` false leaves the stored half to the original too, for
    // verification. `lowers_riser` false keeps the upper riser up until the
    // ball returns, as runtimes marked -actors1 did. `carries_flyers` false
    // leaves the brain's flyers to the upper half, as runtimes up to -actors2
    // did.
    CabinetActors(std::span<const std::byte> rom,Scope scope,bool stored_half=true,bool lowers_riser=true,
        bool carries_flyers=true);
    ~CabinetActors();
    CabinetActors(const CabinetActors&)=delete;
    CabinetActors& operator=(const CabinetActors&)=delete;
    bool owns(std::uint8_t type) const noexcept {return owned(type,scope_);}
    static bool owned(std::uint8_t type,Scope scope) noexcept;
    // The ROM routine the dispatcher (01:45A8) enters for a type.
    std::uint16_t entry(std::uint8_t type) const noexcept;
    // The routines of the owned types.
    std::vector<std::uint16_t> entries() const;
    // Where the dispatcher's call continues after an update: pc 0 returns
    // to the dispatcher; otherwise the ROM goes on from pc, with A = a when
    // a is not negative.
    struct Next {std::uint16_t pc;int a;};
    // Slot x of the active bank ($3500), for its dispatch: none when the ROM
    // runs it. An actor already run in this frame's inactive pass is not run
    // again after the exchange (03:89D8 / 8A33 dispatch the new half).
    std::optional<Next> update_active(std::span<std::byte> ram,unsigned x,std::uint64_t frame);
    // The inactive bank ($2200, exchanged by 01:46FD), once per game loop.
    void update_inactive(std::span<std::byte> ram,std::uint64_t frame);
    // The maximum score's ending (01:44C8) turns the ball's half's actors into
    // explosions one slot at a time. The stored half's follow the same slot
    // counter, and run as in play, so the whole table blows up.
    void update_ending(std::span<std::byte> ram,std::uint64_t frame);
    // The palette scheduler (00:F292, each vblank) for the stored half's 32
    // slots, which the ROM leaves for the ball's half. Only their state in
    // RAM changes; the colors are uploaded when their half is shown.
    void update_palettes(std::span<std::byte> ram);
    void reset() noexcept;
    // The brain's flyers (type 55) belong to the upper half, which removes
    // them at its own bottom edge, Y $140: in mid-air on the whole table. One
    // that flies down to the seam is given to the lower half's bank instead,
    // where the same routine flies it on to the bottom of the table and the
    // ball can still burst it. While it straddles the two pictures (upper Y
    // 272..280, the lower half's 216 less) both banks hold it, a step apart
    // at most; then the upper one goes. Run by update_inactive.
    void carry_flyers(std::span<std::byte> ram) const;
    // Verification: the port's update of slot x in the ball's half, on a copy
    // of RAM, as the ROM is about to run it. Returns the PC where the ROM's
    // RAM should then equal the prediction: 01:45C8 after the routine's RTS,
    // or where the port hands the routine back to the ROM.
    std::uint16_t predict(std::span<const std::byte> ram,unsigned x);
    // At that PC, with the ROM's A: what differs, or nothing. A vblank
    // interrupt in between voids the comparison.
    std::string mismatch(std::span<const std::byte> ram,unsigned a) const;
private:
    std::unique_ptr<cabinet::Rom> rom_;
    Scope scope_;
    bool stored_half_,lowers_riser_,carries_flyers_;
    std::array<std::uint64_t,64> inactive_frame_{};
    std::array<std::byte,8192> prediction_{};
    unsigned predicted_slot_{},predicted_type_{},predicted_pc_{};
    int predicted_a_{-1};
    unsigned inactive_table_{2};
    unsigned ending_slot_{64};
};
}
