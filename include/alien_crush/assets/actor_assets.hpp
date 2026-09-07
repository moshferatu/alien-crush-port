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
class ActorAssets {
public:
    explicit ActorAssets(std::span<const std::byte> rom);
    // Original capacity guard: returns no pieces if count + first_slot >= 64.
    std::vector<ActorPiece> pieces(std::uint8_t descriptor,unsigned first_slot=0) const;
private:
    std::array<std::byte,16384> banks_{};
};
struct ActorSprite {
    std::uint8_t actor{},descriptor{},slot{};
    std::array<std::uint16_t,4> words{};
};
// Reconstructs the main-table sprite pass (01:85E2), without changing RAM.
// Caller must establish scene and rendering epoch; post-frame RAM can be newer
// than source SAT. Excludes HUD, background updates and the later 01:8666 pass.
std::vector<ActorSprite> assemble_table_actors(const ActorAssets& assets,std::span<const std::byte> ram);
}
