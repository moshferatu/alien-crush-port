// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/core/save_state.hpp"
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include <filesystem>
namespace alien_crush::diagnostics {
struct VideoCandidate { const char* name; render::Image image; };
// Fixed candidates: current; previous SAT; previous SAT plus one older component;
// then the complete previous video snapshot. Never select by pixel match quality.
std::vector<VideoCandidate> compose_video_epochs(const ReferenceSnapshot& current,
    const ReferenceSnapshot* previous,int width=256,int height=240);
void export_video(const std::filesystem::path& directory,const state::SaveStateContainer& saved,
    const render::Image* reference=nullptr,const ReferenceSnapshot* previous=nullptr);
void render_raster_capture(const std::filesystem::path& video,const std::filesystem::path& writes,
    const std::filesystem::path& output);
void export_palette_comparison(const std::filesystem::path& directory,std::uint64_t frame,
    std::span<const std::uint16_t> initial,std::span<const render::PalettePixelWrite> writes,
    const render::Image& candidate,const render::Image& reference);
void export_table_assets(const std::filesystem::path& directory,const std::filesystem::path& rom);
void verify_discovery(const std::filesystem::path& rom,const std::filesystem::path& report={});
void verify_actor_trace(const std::filesystem::path& rom,const std::filesystem::path& trace);
void verify_background_trace(const std::filesystem::path& rom,const std::filesystem::path& trace);
void verify_palette_fade_trace(const std::filesystem::path& trace);
void verify_palette_pulse_trace(const std::filesystem::path& trace);
}
