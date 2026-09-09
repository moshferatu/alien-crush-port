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
// Explicit cabinet adapter: modifies draw/appearance and carries a seated ball.
// It does not write ball velocity or implement a release. Caller supplies I
// while drawing, then clears I/II on release so the cartridge fires normally.
// False leaves every RAM byte intact. This is separate from read-only evidence.
bool apply_reference_launcher_draw(std::span<std::byte> ram,std::uint8_t percent);
// Return an adapter-pinned draw to rest without firing. Also accepts the known
// pause/resume wait; natural draws and release phases are rejected unchanged.
// Caller tracks ownership and suppresses I/II until the physical rod rearms.
bool cancel_reference_launcher_draw(std::span<std::byte> ram);
}
