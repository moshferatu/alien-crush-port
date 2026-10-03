// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/data_files.hpp"
#include <array>
#include <system_error>
#include <utility>
namespace alien_crush::core {
DataMigration migrate_legacy_data_folder(const std::filesystem::path& base) {
    DataMigration result;
    const auto folder=base/kLegacyDataFolderName;
    std::error_code error;
    if(!std::filesystem::is_directory(folder,error)) return result;
    // The settings, the saves, the player's own backglass, and the separate
    // settings files from before cabinet.ini, which are still read beside it.
    const std::array<std::pair<const char*,const char*>,8> files{{
        {kLegacyConfigFileName,kConfigFileName},
        {kHighScoreFileName,kHighScoreFileName},
        {kCabinetQuickStateFileName,kCabinetQuickStateFileName},
        {kDesktopQuickStateFileName,kDesktopQuickStateFileName},
        {kBackglassFileName,kBackglassFileName},
        {"cabinet-windows.txt","cabinet-windows.txt"},
        {"cabinet-inputs.txt","cabinet-inputs.txt"},
        {"cabinet-plunger.txt","cabinet-plunger.txt"},
    }};
    for(const auto& [old_name,new_name]:files) {
        const auto from=folder/old_name,to=base/new_name;
        const auto label=std::string(kLegacyDataFolderName)+"/"+old_name+" -> "+new_name;
        error.clear();
        if(!std::filesystem::is_regular_file(from,error)) continue;
        if(std::filesystem::exists(to,error)) {
            result.problems.push_back(label+": not moved, because "+new_name+" already exists");
            continue;
        }
        std::filesystem::rename(from,to,error);
        if(error) result.problems.push_back(label+": "+error.message());
        else result.moved.push_back(label);
    }
    error.clear();
    if(std::filesystem::is_empty(folder,error) && !error) std::filesystem::remove(folder,error);
    return result;
}
}
