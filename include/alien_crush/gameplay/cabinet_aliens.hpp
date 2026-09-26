// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
namespace alien_crush::gameplay {
// Cabinet mode's lower-table aliens: the orange and green groups, their silver
// variants, the green aliens' bases, both groups' respawn controllers and the
// defeat effects. The cartridge runs only the half the ball is in, but the
// cabinet shows both halves, so there the port runs these actors itself, in
// either half: what is drawn is what the ball hits. Each rule is the ROM's
// (addresses below). State stays in the actor banks of work RAM, where the
// original ball physics collides with it and quick states save it.
//
// In the ball's half, an update is the ROM routine's, down to its game RNG
// calls and zero-page scratch, so play matches the original until the ball
// first goes upstairs. The other half runs without touching anything the
// original would not: a private random source, no sound, no scratch, and the
// frame's $69 events (hide/remove) are the ball's half's alone. An alien with
// an unprocessed hit waits for its half to become active.
class CabinetAliens {
public:
    explicit CabinetAliens(std::span<const std::byte> rom);
    static bool owns(std::uint8_t type) noexcept;
    // The ROM routine the dispatcher (01:45A8) enters for an owned type, or 0.
    static std::uint16_t entry(std::uint8_t type) noexcept;
    // Where the dispatcher's call continues after an update: a return to it,
    // or a ROM routine the update ends in (a sound, or the group-cleared rules).
    static constexpr std::uint16_t kReturn=0;
    // Slot x of the active bank ($3500), for its dispatch. An actor already
    // run in this frame's inactive pass is not run again after the exchange.
    std::uint16_t update_active(std::span<std::byte> ram,unsigned x,std::uint64_t frame);
    // The inactive bank ($2200, exchanged by 01:46FD), once per game loop.
    void update_inactive(std::span<std::byte> ram,std::uint64_t frame);
    void reset() noexcept;
private:
    class Bank;
    std::uint16_t run(Bank&,unsigned x) const;
    std::array<std::byte,8192> system_{};    // Bank 00 at $E000: palette tables.
    std::array<std::byte,8192> actors_{};    // Bank 01 at $4000: spawn tables.
    std::array<std::byte,8192> sequences_{}; // Bank 04 at $A000: FD8F sequences.
    std::array<std::uint64_t,64> inactive_frame_{};
    unsigned inactive_table_{2};
};
}
