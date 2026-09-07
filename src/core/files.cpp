// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/atomic_file.hpp"
#include <fstream>
#include <stdexcept>
#include <Windows.h>
#include <atomic>
namespace alien_crush::core {
std::vector<std::byte> read_file(const std::filesystem::path& path, std::size_t limit) {
    const auto size = std::filesystem::file_size(path);
    if (size > limit) throw std::runtime_error("File exceeds size limit: " + path.string());
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
        throw std::runtime_error("Could not read " + path.string());
    return bytes;
}
void write_file_atomic(const std::filesystem::path& path, std::span<const std::byte> bytes) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    static std::atomic<unsigned> sequence{};
    auto temp = path;
    temp += "." + std::to_string(GetCurrentProcessId()) + "." + std::to_string(sequence++) + ".tmp";
    try {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.flush();
        if (!file) throw std::runtime_error("Could not write " + temp.string());
        file.close();
        if (!commit_replacement(temp, path).committed) throw std::runtime_error("Could not commit " + path.string());
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        throw;
    }
}
}
