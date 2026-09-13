// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <span>
#include <vector>
#include <string_view>
namespace alien_crush::core {
inline std::filesystem::path path_from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()),text.size()));
}
std::vector<std::byte> read_file(const std::filesystem::path&, std::size_t limit = 64U * 1024U * 1024U);
void write_file_atomic(const std::filesystem::path&, std::span<const std::byte>,int commit_budget_ms=5000);
}
