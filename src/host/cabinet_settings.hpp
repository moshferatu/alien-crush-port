// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/core/files.hpp"
#include <map>
#include <sstream>
#include <string>
#include <stdexcept>
namespace alien_crush::host {
// The older settings format (then cabinet.ini), before Devil's Crush's format (cabinet_config.hpp):
// lenient key=value lines with comments. Read only, to bring its settings over.
using CabinetSettings=std::map<std::string,std::string>;
inline CabinetSettings read_cabinet_settings(const std::filesystem::path& path) {
    CabinetSettings result;
    if(!std::filesystem::exists(path)) return result;
    const auto bytes=core::read_file(path,65536);
    std::string text(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(text.starts_with("\xef\xbb\xbf")) text.erase(0,3);
    auto trim=[](std::string s) {
        const auto first=s.find_first_not_of(" \t\r");
        return first==s.npos?std::string{}:s.substr(first,s.find_last_not_of(" \t\r")-first+1);
    };
    std::istringstream lines(text);
    for(std::string line;std::getline(lines,line);) {
        line=trim(line);
        if(line.empty() || line[0]=='#' || line[0]==';') continue;
        const auto equal=line.find('=');
        if(equal==line.npos) throw std::runtime_error("Expected key=value in alien-crush.ini");
        result[trim(line.substr(0,equal))]=trim(line.substr(equal+1));
    }
    return result;
}
}
