// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace alien_crush::rom {

inline constexpr std::uintmax_t kSupportedRomSize = 262'144U;
inline constexpr std::string_view kSupportedMd5 =
    "0088d1d20861490677edb147d2de60bd";
inline constexpr std::string_view kSupportedSha256 =
    "1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f";

struct Digests {
    std::string md5;
    std::string sha256;
};

enum class ValidationCode {
    supported,
    file_not_found,
    read_error,
    wrong_size,
    unsupported_hash,
    hash_error,
};

struct ValidationResult {
    ValidationCode code = ValidationCode::read_error;
    std::filesystem::path path;
    std::uintmax_t size = 0;
    Digests digests;
    std::string message;

    [[nodiscard]] bool supported() const noexcept
    {
        return code == ValidationCode::supported;
    }
};

[[nodiscard]] Digests compute_digests(std::span<const std::byte> bytes);

[[nodiscard]] bool matches_supported_rom(
    std::uintmax_t size,
    const Digests& digests) noexcept;

[[nodiscard]] ValidationResult validate_file(const std::filesystem::path& path);

} // namespace alien_crush::rom
