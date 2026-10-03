// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace alien_crush {
struct ReferenceLauncherSeat {
    unsigned slot{},travel{},x{},y{};
};
// Conservative supported-ROM gate, not a general scene classifier.
std::optional<ReferenceLauncherSeat> reference_launcher_seat(std::span<const std::byte> ram);
// Read-only input context covering the entire shooter session, including
// transient draw/return poses and departure. Not a gate for writing plunger RAM.
bool reference_launcher_active(std::span<const std::byte> ram);
// Explicit cabinet adapter: modifies draw/appearance and carries a seated ball.
// It does not write ball velocity or implement a release. Caller supplies I
// while drawing, then clears I/II on release so the cartridge fires normally.
// False leaves every RAM byte intact. This is separate from read-only evidence.
bool apply_reference_launcher_draw(std::span<std::byte> ram,std::uint8_t percent);
// Return an adapter-pinned draw to rest without firing. Also accepts the known
// pause/resume wait; natural draws and release phases are rejected unchanged.
// Caller tracks ownership and suppresses I/II until the physical rod rearms.
bool cancel_reference_launcher_draw(std::span<std::byte> ram);
inline constexpr unsigned kBonusRooms=4;
// Ask for bonus room 1..4 the way a table feature does: the game loop at
// 01:40AA calls FDC1 on RAM 78 bit 2, for the room in RAM 83, and the ordinary
// FF04 return brings the ball back to the shooter. Only with a ball seated in
// the shooter of an ordinary game; false leaves every RAM byte intact.
bool request_reference_bonus_room(std::span<std::byte> ram,unsigned room);
}
