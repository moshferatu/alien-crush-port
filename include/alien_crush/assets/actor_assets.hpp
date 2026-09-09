// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace alien_crush::assets {
struct ActorPiece { int y{},x{}; std::uint16_t pattern{},flags{}; };
// Sprite form of 16:A006 only. Background-update descriptors use another format.
std::vector<ActorPiece> decode_actor_pieces(std::span<const std::byte> descriptor);
struct BackgroundWrite { std::uint16_t address{},value{}; };
struct ActorPatternTransfer {
    std::uint16_t destination{};
    std::size_t rom_offset{},compressed_bytes{};
    std::vector<std::uint16_t> words;
};
// 16:A1C3 rectangular BAT descriptor. Rejects forms that wrap its byte index.
std::vector<BackgroundWrite> decode_actor_background(std::span<const std::byte> descriptor,std::uint8_t high_override=0);
class ActorAssets {
public:
    explicit ActorAssets(std::span<const std::byte> rom);
    // Original capacity guard: returns no pieces if count + first_slot >= 64.
    std::vector<ActorPiece> pieces(std::uint8_t descriptor,unsigned first_slot=0) const;
    std::vector<BackgroundWrite> background(std::uint8_t descriptor,std::uint8_t high_override=0) const;
    // Dynamic stream selected by 16:A0DF; callers establish the pending bit/epoch.
    ActorPatternTransfer pattern_transfer(std::uint8_t type,std::uint8_t selection,std::uint16_t pattern_base) const;
    // Visual-only continuation of the ROM's FD35/FD8F animation oscillator.
    // Input uses the active actor layout but must be a private presentation copy.
    void advance_animation(std::span<std::byte> ram) const;
    void mirror_flippers(std::span<std::byte> projected,std::span<const std::byte> live,unsigned half) const;
private:
    std::array<std::byte,16384> banks_{};
    std::array<std::byte,8192> sequences_{};
};
struct ActorSprite {
    std::uint8_t actor{},descriptor{},slot{};
    std::array<std::uint16_t,4> words{};
};
// Reconstructs the main-table sprite pass (01:45E2), without changing RAM.
// Caller must establish scene and rendering epoch; post-frame RAM can be newer
// than source SAT. Excludes HUD, background updates and the later 01:4741 pass.
std::vector<ActorSprite> assemble_table_actors(const ActorAssets& assets,std::span<const std::byte> ram);
// Presentation-only projection of the inactive bank swapped by 01:46FD..4740.
// Slots 0..3 (including the ball) remain absent. Never writes runtime RAM.
std::array<std::byte,8192> inactive_table_actor_ram(std::span<const std::byte> ram);
// Background actor types 7/20 explicitly redraw on table return (RAM 68 bit 1).
// Accepts active-layout RAM, including a presentation projection of the stored bank.
// Other actors can retain stale descriptors and are deliberately excluded.
std::vector<BackgroundWrite> restored_actor_backgrounds(const ActorAssets& assets,std::span<const std::byte> ram);
struct TableSprite {
    std::uint8_t slot{};
    std::array<std::uint16_t,4> words{};
};
// Four-sprite table overlay from 01:4741. Input is RAM at the later-pass entry;
// first_slot is the main pass's end slot. Does not clear the remaining SAT slots.
std::vector<TableSprite> assemble_table_overlay(std::span<const std::byte> ram,unsigned first_slot);
// Settled main-table HUD state, not the original incremental score updater.
// Score is in points (multiples of 100); counter is the stored 1..10 value.
std::array<TableSprite,10> assemble_table_hud(std::uint32_t score_points,unsigned ball_counter);
}
