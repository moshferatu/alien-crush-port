// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/runtime/game_runtime.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
namespace alien_crush::host {
// The eight PC Engine pad inputs, in the order alien-crush.ini has them. Pad
// inputs rather than game actions: the cartridge decides what each does, and
// not the same in a menu as on the table.
enum class PadButton : std::size_t {one=0,two=1,select=2,run=3,up=4,right=5,down=6,left=7};
inline constexpr std::size_t kPadButtonCount=8;
inline std::string_view pad_button_field(PadButton button) {
    constexpr std::array<std::string_view,kPadButtonCount> names{"one","two","select","run","up","right","down","left"};
    return names[static_cast<std::size_t>(button)];
}
// Its bit in the pad byte, which is also its bit in a flipper's input mask.
inline std::uint8_t pad_button_bit(PadButton button) {
    return static_cast<std::uint8_t>(1U<<static_cast<unsigned>(button));
}

// Which pad inputs work each flipper during play: Left for the left, Right for
// the right with Up as its stand-in while both are held. Fixed, so a flipper is
// whatever keys the pad's Left and Right are bound to. Cabinet controls write
// these into each flipper piece's input mask at `$3900,X`
// (src/runtime/handoff_hook.inc).
struct FlipperInputs {
    std::vector<PadButton> left{PadButton::left},right{PadButton::right,PadButton::up};
    bool operator==(const FlipperInputs&) const=default;
};
inline std::uint8_t flipper_input_mask(std::span<const PadButton> inputs) {
    std::uint8_t mask=0;
    for(const auto input:inputs) mask=static_cast<std::uint8_t>(mask|pad_button_bit(input));
    return mask;
}
// The input a gamepad shoulder sends for its flipper: the first on the
// flipper's list, or its own side's direction when the list is empty.
inline std::uint16_t flipper_pad(std::span<const PadButton> inputs,bool right) {
    return inputs.empty()?(right?Pad::right:Pad::left):pad_button_bit(inputs.front());
}

// The pad cannot be pushed both ways, and the cartridge drops the whole input
// byte when it is, so two flippers on opposing directions would silence each
// other. A flipper whose list offers another, non-opposing input moves to it
// for exactly the frames both are held -- Devil's Crush's rule, and the reason
// the right flipper's defaults include Up. Pressed alone, each keeps its own.
inline bool has_opposing_directions(std::uint8_t pad) {
    return ((pad&Pad::left) && (pad&Pad::right)) || ((pad&Pad::up) && (pad&Pad::down));
}
inline std::uint8_t resolve_opposing_directions(std::uint8_t pad,const FlipperInputs& flippers) {
    if(!has_opposing_directions(pad)) return pad;
    for(const auto* list:{&flippers.right,&flippers.left}) {
        if(list->size()<2) continue;
        const auto owned=flipper_input_mask(*list);
        if(!(pad&owned)) continue;
        for(const auto input:*list) {
            const auto candidate=static_cast<std::uint8_t>((pad&~owned)|pad_button_bit(input));
            if(!has_opposing_directions(candidate)) return candidate;
        }
    }
    return pad;
}
}
