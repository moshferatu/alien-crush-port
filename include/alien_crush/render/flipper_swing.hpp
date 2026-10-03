// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/animation_scene.hpp"
#include <optional>
#include <span>
namespace alien_crush::render {
// How far to turn a flipper's current pose, from +x toward +y about the pivot
// in scene coordinates, to stand at its previous pose's angle. Between ticks
// the presenter turns it through degrees*(1-fraction), so the game's own pose
// is shown exactly at each tick and the steps between become a swing.
struct FlipperSwing { double pivot_x{},pivot_y{},degrees{}; };
// Turns at most this far, in degrees, are drawn as a flipper's next pose.
inline constexpr double largest_flipper_swing=60;
// Measures the turn from the long axis of every drawn pixel of the actor's
// pieces in each scene, and the pivot as the one point that turn leaves in
// place, so no flipper's angles or pivot are written down by hand. Nothing is
// returned unless the actor swings, stayed put, and the step is a turn of
// between one and sixty degrees about a point on its own pixels.
std::optional<FlipperSwing> flipper_swing(std::span<const AnimationLayer> previous,
    std::span<const AnimationLayer> current,std::uint64_t actor_group);
}
