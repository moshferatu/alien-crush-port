// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/pce_graphics.hpp"
namespace alien_crush::render {
// Original title actor B9, at its settled position, with the initial title
// palette. A static cabinet image on black, without menu text or animation.
Image title_logo(std::span<const std::byte> rom);
}
