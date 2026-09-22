// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
namespace alien_crush::render {
struct Image { int width{},height{}; std::vector<std::uint32_t> pixels; };
// Raster background and final SAT ownership (zero background, 1..64 sprites).
struct CompositionCapture { Image background; std::vector<std::uint8_t> owner; };
// Exact color expansion used by the pinned reference's RGB565 output path.
std::uint32_t pce_color(std::uint16_t raw) noexcept;
Image background(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,int width_tiles,int height_tiles);
struct Sprite {
    int x{},y{},width{},height{};
    std::uint16_t pattern{},flags{};
    bool foreground{};
    Image image;
};
// Diagnostic images retain transparent pixels. This is not a scanline compositor.
std::array<Sprite,64> sprites(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,std::uint16_t memory_width);
Image sprite_sheet(const std::array<Sprite,64>& sprites);
struct Viewport {
    int width{256},height{240};
    std::uint16_t scroll_x{},scroll_y{},memory_width{},control{0xc0};
    bool burst_mode{};
};
// Frozen-register diagnostic composition with the sixteen-piece scanline limit.
// Register/palette/VRAM changes during a frame are not modeled.
// Scroll is the background origin at output (0,0); sprites use screen coordinates.
// Optional width*height mask is overwritten with visible sprite coverage after
// SAT order, fetch limits and background priority, independent of RGB equality.
// Optional blocked mask also includes sprite pixels hidden by the background and
// fully occupied fetch rows. It conservatively guards insertion of another sprite.
// Hidden SAT slots (one bit per slot) suppress pixels but retain scanline fetch cost.
Image compose(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport,std::span<std::uint8_t> visible_sprites={},
    std::span<std::uint8_t> blocked_sprites={},std::uint64_t hidden_sprites=0,CompositionCapture* capture=nullptr);
enum class RasterTarget { vram, palette, sat, control, scroll_x, scroll_y, memory_width, burst_mode };
struct RasterWrite {
    std::uint16_t row{};
    RasterTarget target{};
    std::uint16_t address{},value{};
};
// Writes take effect before their output row, in supplied order. Register addresses
// must be zero. Inputs are immutable; rows must be sorted and inside the viewport.
// The caller supplies effective timing: bus latency, hardware register latches and
// within-scanline changes are not inferred here. Scroll remains relative to row zero.
Image compose_raster(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport,std::span<const RasterWrite> writes,
    std::span<std::uint8_t> visible_sprites={},std::span<std::uint8_t> blocked_sprites={},std::uint64_t hidden_sprites=0,CompositionCapture* capture=nullptr);
struct PalettePixelWrite { std::uint32_t pixel{}; std::uint16_t address{},value{}; };
// Palette writes apply before the specified linear output pixel. Row writes may
// change other state but cannot also target the palette. All inputs stay immutable.
Image compose_palette_raster(std::span<const std::uint16_t> vram,std::span<const std::uint16_t> palette,
    std::span<const std::uint16_t> sat,const Viewport& viewport,std::span<const RasterWrite> rows,
    std::span<const PalettePixelWrite> colors,std::span<std::uint8_t> visible_sprites={},
    std::span<std::uint8_t> blocked_sprites={},std::uint64_t hidden_sprites=0,CompositionCapture* capture=nullptr);
}
