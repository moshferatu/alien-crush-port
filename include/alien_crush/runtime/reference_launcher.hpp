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
// 00:FAB6 stops the score at 999,999,900. The near-maximum start leaves 900
// points to earn: a few bumpers.
inline constexpr std::uint32_t kMaximumScore=999999900,kNearMaximumScore=999999000;
// Give the game in play a score, a multiple of 100 up to the maximum, as
// 00:FA27 leaves one: the three bytes in hundreds, and RAM 68 bit 2 for FA49
// to redraw the HUD. Only with a ball seated in the shooter of an ordinary
// game; false leaves every RAM byte intact.
bool set_reference_score(std::span<std::byte> ram,std::uint32_t points);
// The cheat that carries the ball: put it at X and Y in the game's own actor
// coordinates and give it the velocity that took it there, so letting go
// throws it. On the main table Y is the upper half's; the lower half's is 224
// more, and the ball changes halves by the game's own crossing (03:894F),
// which this only steers. Only for a ball in play on the table or in a bonus
// room, not paused and not the demo's; false leaves every RAM byte intact.
bool hold_reference_ball(std::span<std::byte> ram,double x,double y);
}
