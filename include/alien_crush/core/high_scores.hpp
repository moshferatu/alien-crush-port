// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <optional>
#include <span>
namespace alien_crush::core {
using HighScores=std::array<std::byte,30>;
inline constexpr std::array<unsigned,6> score_rows{0x1219,0x121f,0x1225,0x122b,0x1231,0x1237};
inline bool valid_high_scores(const HighScores& table) {
    unsigned previous=0xffffff;
    for(unsigned rank=0;rank<5;++rank) {
        const unsigned score=std::to_integer<unsigned>(table[rank])|
            (std::to_integer<unsigned>(table[5+rank])<<8)|(std::to_integer<unsigned>(table[10+rank])<<16);
        if(score>previous) return false;
        previous=score;
        for(unsigned row=3;row<6;++row) {
            const auto c=std::to_integer<unsigned>(table[row*5+rank]);
            if(c<0x40 || c>0x5b) return false;
        }
    }
    return true;
}
inline std::optional<HighScores> read_high_scores(std::span<const std::byte> ram) {
    if(ram.size()!=8192) return {};
    HighScores result{};
    for(unsigned row=0;row<6;++row) std::copy_n(ram.begin()+score_rows[row],5,result.begin()+row*5);
    if(!valid_high_scores(result)) return {};
    return result;
}
inline bool write_high_scores(std::span<std::byte> ram,const HighScores& table) {
    if(ram.size()!=8192 || !valid_high_scores(table)) return false;
    for(unsigned row=0;row<6;++row) std::copy_n(table.begin()+row*5,5,ram.begin()+score_rows[row]);
    return true;
}
// ACHS, version 1, five ranks, thirty ranked-column bytes, FNV-1a checksum.
inline std::array<std::byte,40> encode_high_scores(const HighScores& table) {
    std::array<std::byte,40> result{std::byte{'A'},std::byte{'C'},std::byte{'H'},std::byte{'S'},std::byte{1},std::byte{5}};
    std::copy(table.begin(),table.end(),result.begin()+6);
    unsigned hash=2166136261u;
    for(unsigned i=0;i<36;++i) hash=(hash^std::to_integer<unsigned>(result[i]))*16777619u;
    for(unsigned i=0;i<4;++i) result[36+i]=static_cast<std::byte>(hash>>(i*8));
    return result;
}
inline std::optional<HighScores> decode_high_scores(std::span<const std::byte> bytes) {
    if(bytes.size()!=40) return {};
    HighScores table{};std::copy_n(bytes.begin()+6,30,table.begin());
    const auto encoded=encode_high_scores(table);
    if(!valid_high_scores(table) || !std::equal(bytes.begin(),bytes.end(),encoded.begin())) return {};
    return table;
}
}
