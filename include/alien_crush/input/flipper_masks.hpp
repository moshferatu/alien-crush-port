// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace alien_crush::input {
// The pad inputs cabinet controls give each flipper piece at `$3900,X`, as a
// pad-byte mask: the left pieces' and the right pieces'. Zero leaves the
// cartridge's own mask. The defaults are what cabinet controls have always
// written: Left for the left flipper, Right or Up for the right.
struct FlipperMasks {
    std::uint8_t left=0x80,right=0x30;
    bool operator==(const FlipperMasks&) const=default;
};
// Masks other than the defaults are part of a runtime's identity, as a nudge
// multiplier is, so a recording replays with the flippers it was made with.
inline std::string flipper_revision_suffix(FlipperMasks masks) {
    if(masks==FlipperMasks{}) return {};
    constexpr char hex[]="0123456789ABCDEF";
    return std::string("-flip=")+hex[masks.left>>4]+hex[masks.left&15]+hex[masks.right>>4]+hex[masks.right&15];
}
inline FlipperMasks recorded_flipper_masks(std::string_view revision) {
    const auto pos=revision.find("-flip=");
    if(pos==std::string_view::npos) return {};
    const auto digits=revision.substr(pos+6,4);
    unsigned value=0;
    for(const char c:digits) {
        const int digit=c>='0' && c<='9'?c-'0':c>='A' && c<='F'?c-'A'+10:-1;
        if(digit<0 || digits.size()!=4) throw std::runtime_error("Invalid recorded flipper inputs");
        value=value<<4|static_cast<unsigned>(digit);
    }
    if(digits.size()!=4) throw std::runtime_error("Invalid recorded flipper inputs");
    return {static_cast<std::uint8_t>(value>>8),static_cast<std::uint8_t>(value&0xff)};
}
}
