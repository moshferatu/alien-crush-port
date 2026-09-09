// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/pce_graphics.hpp"
#include <filesystem>
#include <optional>
namespace alien_crush::host {
// Optional, user-owned uncompressed 24/32-bit Windows BMP. Loaded once at startup.
std::optional<render::Image> load_backglass(const std::filesystem::path& path);
}
