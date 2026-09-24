// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace alien_crush::host {
// The ROM's boot frames display a white backdrop while both VDC layers are
// disabled. Mask only this initial interval, not later in-game blanking/effects.
class StartupDisplay {
    bool ready_{};
public:
    bool blank(std::uint16_t control) {
        ready_=ready_ || (control&0xc0)!=0;
        return !ready_;
    }
    bool ready() const {return ready_;}
};
}
