// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/core/files.hpp"
#include <map>
#include <sstream>
#include <string>
#include <stdexcept>
namespace alien_crush::host {
// One cabinet.ini for all host settings. Preserve unrecognized fields when
// updating window placement or migrating the older separate files.
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
        if(equal==line.npos) throw std::runtime_error("Expected key=value in cabinet.ini");
        result[trim(line.substr(0,equal))]=trim(line.substr(equal+1));
    }
    return result;
}
inline void write_cabinet_settings(const std::filesystem::path& path,const CabinetSettings& settings) {
    std::string text="# Alien Crush cabinet settings\n";
    for(const auto& [key,value]:settings) text+=key+'='+value+'\n';
    core::write_file_atomic(path,std::as_bytes(std::span(text.data(),text.size())));
}
}
