// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/pce_graphics.hpp"
namespace alien_crush::diagnostics {
struct DisplayRow { std::uint16_t row{},control{}; bool burst_mode{}; };
struct PaletteWrite { int row{},x{}; std::uint16_t address{},before{},after{}; bool frame_done{}; };
// Requires one observation per output row in order. These are already-latched
// reference observations, not bus writes or independently emulated latch timing.
std::vector<render::RasterWrite> display_control_schedule(std::span<const DisplayRow> rows,int height);
std::vector<render::PalettePixelWrite> palette_pixel_schedule(std::span<const std::uint16_t> initial,
    std::span<const std::uint16_t> final,std::span<const PaletteWrite> writes,int width,int height);
}
