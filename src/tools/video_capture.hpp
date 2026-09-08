// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/core/save_state.hpp"
#include <filesystem>
namespace alien_crush::diagnostics {
void export_video(const std::filesystem::path& directory,const state::SaveStateContainer& saved);
void export_table_assets(const std::filesystem::path& directory,const std::filesystem::path& rom);
void verify_discovery(const std::filesystem::path& rom,const std::filesystem::path& report={});
void verify_actor_trace(const std::filesystem::path& rom,const std::filesystem::path& trace);
void verify_background_trace(const std::filesystem::path& rom,const std::filesystem::path& trace);
void verify_palette_fade_trace(const std::filesystem::path& trace);
void verify_palette_pulse_trace(const std::filesystem::path& trace);
}
