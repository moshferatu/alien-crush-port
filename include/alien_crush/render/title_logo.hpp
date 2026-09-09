// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/pce_graphics.hpp"
namespace alien_crush::render {
// Original title actor B9, at its settled position, with the initial title
// palette. A static cabinet image on black, without menu text or animation.
Image title_logo(std::span<const std::byte> rom);
// ROM palette reveal and glow, followed by a reverse reveal for a seamless loop.
// Decoded once; frame selection neither allocates nor touches the game runtime.
class TitleLogoAnimation {
    std::vector<Image> images_;
    std::vector<std::uint64_t> ends_;
public:
    explicit TitleLogoAnimation(std::span<const std::byte> rom);
    const Image& frame(std::uint64_t tick) const;
    std::uint64_t duration() const { return ends_.back(); }
};
}
