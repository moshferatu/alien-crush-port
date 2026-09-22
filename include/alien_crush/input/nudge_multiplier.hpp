// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace alien_crush::input {
inline std::optional<double> parse_nudge_multiplier(std::string_view text) {
    double value{};
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size() ||
        !std::isfinite(value) || value<0 || value>16) return std::nullopt;
    return value;
}
inline std::uint16_t scale_nudge_motion(std::uint16_t motion,std::uint16_t impulse,double factor) {
    if(factor==1) return motion;
    const auto signed_word=[](std::uint16_t v) {return v<0x8000?int(v):int(v)-65536;};
    const int before=signed_word(static_cast<std::uint16_t>(motion-impulse));
    return static_cast<std::uint16_t>(std::clamp<long long>(
        before+std::llround(signed_word(impulse)*factor),-32768,32767));
}
inline std::string nudge_revision_suffix(double factor) {
    if(!std::isfinite(factor) || factor<0 || factor>16)
        throw std::runtime_error("Nudge multiplier must be a number from 0 to 16");
    if(factor==1) return {};
    char buffer[64];
    const auto result=std::to_chars(buffer,buffer+sizeof(buffer),factor,
        std::chars_format::general,std::numeric_limits<double>::max_digits10);
    return "-nudge="+std::string(buffer,result.ptr);
}
inline double recorded_nudge_multiplier(std::string_view revision) {
    const auto pos=revision.find("-nudge=");
    if(pos==std::string_view::npos) return 1;
    const auto value=parse_nudge_multiplier(revision.substr(pos+7));
    if(!value) throw std::runtime_error("Invalid recorded nudge multiplier");
    return *value;
}
}
