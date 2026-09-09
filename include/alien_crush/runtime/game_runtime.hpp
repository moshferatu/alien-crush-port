// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/core/save_state.hpp"
#include "alien_crush/diagnostics/display_trace.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>

namespace alien_crush {
// Semantic actions are separate from the original controller bit layout.
// The reference baseline accepts pad input directly until game states are mapped.
struct GameInput {
    bool left_flipper{}, right_flipper{}, launch{}, nudge{}, start{}, select{};
    bool up{}, down{}, left{}, right{};
    std::uint8_t plunger_pull{};
    bool plunger_release{};
};
enum Pad : std::uint16_t { button_i=1, button_ii=2, select=4, run=8,
    up=16, right=32, down=64, left=128 };
struct PadInput {
    std::uint16_t player_one{}, player_two{};
    bool operator==(const PadInput&) const = default;
};
struct Frame {
    std::uint64_t number{};
    int width{}, height{};
    // Views survive until the next runtime operation; diagnostics copy when needed.
    std::span<const std::uint32_t> pixels;
    std::span<const std::int16_t> stereo;
};
class GameRuntime {
public:
    virtual ~GameRuntime() = default;
    virtual void load(const std::filesystem::path&) = 0;
    virtual void reset() = 0;
    virtual Frame advance(PadInput) = 0;
    virtual double frames_per_second() const = 0;
    virtual int sample_rate() const = 0;
    virtual std::string_view revision() const = 0;
    virtual state::SaveStateContainer save() const = 0;
    virtual void restore(const state::SaveStateContainer&) = 0;
};
// Kept out of the game interface: native game state will not be a RAM image.
class ReferenceDiagnostics {
public:
    virtual ~ReferenceDiagnostics() = default;
    virtual std::span<const std::byte> work_ram() const = 0;
    virtual bool supports_display_observation() const = 0;
    virtual void observe_display(bool enabled) = 0;
    virtual std::span<const diagnostics::DisplayRow> display_rows() const = 0;
    virtual std::span<const diagnostics::PaletteWrite> palette_writes() const = 0;
    // Full initial VRAM at the first observed row, followed by row-boundary deltas.
    virtual std::span<const render::RasterWrite> vram_rows() const = 0;
};
class ReferenceRuntime : public GameRuntime, public ReferenceDiagnostics {
public:
    // Explicit optional cabinet bridge; never used by canonical pad replays.
    virtual bool apply_plunger_draw(std::uint8_t percent) = 0;
    virtual bool cancel_plunger_draw() = 0;
};
std::unique_ptr<ReferenceRuntime> make_reference_runtime();
}
