// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace alien_crush::core {
// The files the game keeps between runs, all beside the executable (or in
// --data-dir): the folder the launcher starts the game from.
inline constexpr const char* kConfigFileName="alien-crush.ini";
inline constexpr const char* kHighScoreFileName="alien-crush.hiscore";
inline constexpr const char* kCabinetQuickStateFileName="alien-crush.cabinet-controls.state";
inline constexpr const char* kDesktopQuickStateFileName="alien-crush.desktop-controls.state";
inline constexpr const char* kBackglassFileName="backglass.bmp";

// The subfolder earlier builds kept them in, where the settings were
// `cabinet.ini`.
inline constexpr const char* kLegacyDataFolderName="portable-data";
inline constexpr const char* kLegacyConfigFileName="cabinet.ini";

struct DataMigration {
    // What was moved, as "old -> new", and what could not be.
    std::vector<std::string> moved;
    std::vector<std::string> problems;
};

// Moves the files an earlier build left in `portable-data` into `base`, once.
//
// A file already at the new name wins and the old one is left where it is:
// the new one was written by a build that has already moved, so it is the
// newer. The folder is removed only if that leaves it empty, so anything
// else a player put there stays. Called by the game and the launcher before
// either reads anything, so whichever starts first does it.
[[nodiscard]] DataMigration migrate_legacy_data_folder(const std::filesystem::path& base);
}
