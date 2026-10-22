// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace alien_crush::host {
// Standalone hardware inspection; never loads a ROM or changes configuration.
int input_probe(std::uint64_t samples);
}
