# Provenance

The default Alien Crush backglass follows the existing Devil's Crush title-logo
presentation concept (src/app/main.cpp at 0f81cb3), but its stream at ROM 24125,
palette at 38BD0, descriptor B9 and settled position were traced independently
from Alien Crush's cartridge. Asset pixels are decoded from the user's ROM at
runtime; no generated preview or extracted asset is bundled.

Cabinet keyboard monitor cycling follows `cycle_assignment_display` in Devil's
Crush `src/app/main.cpp` at `0f81cb3be58a92601fe8e03fd6026c2ba72babbc`:
enumerate connected displays, wrap the selection, center and fit the target
window. Alien Crush uses focused-window shortcuts and retains the existing size,
rotation and fullscreen state; no assignment overlay or game data is copied.

The optional local BMP backglass follows the auxiliary-image presentation pattern
in Devil's Crush `src/app/main.cpp`, inspected at commit
`0f81cb3be58a92601fe8e03fd6026c2ba72babbc`. The bounded BMP reader is implemented
locally; no Devil's Crush title artwork, logo extraction addresses or asset data
are copied.

Selected infrastructure was adapted from `D:/Workspace/devils-crush` at commit
`95a53cdabba8876b021f9917e6542158b75b80e1`, under GPL-3.0-or-later:

- ROM validation and Windows cryptographic hashes.
- Quick-state container encoding, integrity checks, and atomic replacement.
- Sharp pixel scaling geometry.
- Hardware-independent plunger calibration and release detection.
- CMake compiler options, SDL dependency pin, and Beetle compilation source list.
- Reference core option values and libretro callback integration patterns.
- Dependency license inventory and notices.
- Generic HuC6280 opcode/addressing-mode table for the disassembly tool.
- Generic background tile/bitplane addressing from `src/render/pce_background.cpp`.

Namespaces, ROM identity, and state magic are Alien Crush-specific. Plunger sample
normalization is reused; Devil Shooter addresses and its launcher equation are not.

The reference starts from pristine pinned Beetle sources. The default build generates
four local source copies with serialization-only additions in `cmake/ReferenceState.cmake`:
Owl audio filter history, PSG channel history, and CPU/VCE fractional timestamps.
Use `AC_REFERENCE_STATE_EXTENSION=OFF` to compile the untouched source baseline.
No instruction, device update, or game logic is replaced by this extension. Its backend
revision suffix prevents loading extended saves into the pristine build.

No Devil's Crush downstream
execution patches, native device models, routine registry, gameplay state maps, or
table renderer are part of this baseline. Native components will require independent
Alien Crush validation before admission.

Local build caches may be populated from a pristine `git archive` at the pinned
upstream revision. Modified Devil's Crush dependency working trees must not be used
as Alien Crush's reference source directory.

The initial cabinet host follows the display enumeration, named-monitor placement,
auxiliary-window and shared-clock patterns in Devil's Crush `src/app/main.cpp` and
`src/cabinet/display_config.cpp`, inspected at commit
`e0cfb2f0be41bbda7a0c98af84f727ce0c4b6592`. Alien Crush's smaller window-only format,
renderer ownership and focus handling are implemented locally; no input remapping
patches or Devil's Crush game-specific score/launcher logic are reused.

The initial full-table presentation also references the separate-background and
active-viewport approach in `src/render/pce_background.cpp` and the world-coordinate
discussion in `src/render/pce_sprite_overlay.cpp` at that revision. Stitch geometry,
scene checks and HUD recognition are Alien Crush-specific implementations; no
Devil's Crush table offsets or actor-retention code are copied.

For inactive actors, Alien Crush uses its own original 01:46FD RAM-bank exchange
instead of Devil's Crush's persistent actor cache. Only the separation of active
and inactive presentation informed this choice; the bank projection, excluded
ball slots and ROM pattern reconstruction are independently recovered here.

The raw cabinet input module follows Devil's Crush's raw-SDL-joystick selection
and reconnect approach in `src/app/main.cpp`, inspected at
`c2164484723778a7abbacee47a69c01b2e673386`. The bounded `ACINPUTS1` button format,
release-to-arm behavior, probe display and virtual-device tests are implemented
locally. No Devil's Crush launcher addresses or charge mapping are reused.

The score panel follows the fixed-size numeric reserves and separation of layout
from decoded state in Devil's Crush `include/devils_crush/cabinet/dmd_layout.hpp`
and the ROM-font approach in `src/cabinet/dmd_font.cpp`, inspected at
`0f81cb3be58a92601fe8e03fd6026c2ba72babbc`. Alien Crush uses its independently
decoded HUD sprite digits and palette, with host-authored labels. No Devil's Crush
font data, ROM addresses, bonus state or idle-scene logic is copied.

`cmake/ReferenceSnapshot.cmake` adds read-only CPU/VCE getters to generated
copies of the pinned Beetle sources. Constructor-installed callbacks read the
CPU registers, VCE palette and existing VDC PeekVRAM/PeekSAT/GetRegister methods;
they perform no emulated bus accesses, synchronization or game-state writes.
These getters replace per-frame serialization in the cabinet renderer and are
checked against the serialized snapshot on all 1800 canonical frames. The source
checkout, serialization schema and emulation routines remain unchanged.

`cmake/ContinuousHandoff.cmake` separately adds optional Alien Crush execution
replacements to generated Beetle CPU/VDC/VCE sources. These are original local
implementations of Alien Crush ROM transfer routines, verified against the CPU;
they are not copied from Devil's Crush's addresses or gameplay replacements.
The Devil's Crush frame/audio loop and execution-hook separation informed the
architecture. The original reference mode and upstream checkout remain unchanged.

The settings launcher (`src/launcher/launcher.cpp`) and its icon tool
(`tools/make_launcher_art.cpp`) are adapted from Devil's Crush
`src/launcher/launcher.cpp` and `tools/make_launcher_art.cpp` at commit
`cbff624`, under GPL-3.0-or-later: the SDL/stb_truetype drawing, layout,
dropdowns, sliders, text fields, mouse/keyboard/controller handling, monitor
list, key capture and ROM-logo icon pipeline. `cabinet.ini` is read and written
through this port's host module (`src/host/cabinet_config.cpp`), and the icon is Alien Crush's title logo decoded by
`TitleLogoAnimation`. No Devil's Crush ROM
addresses or artwork are used. `third_party/stb/stb_truetype.h` is vendored
unmodified from the same upstream revision Devil's Crush pins.

`portable-data/cabinet.ini`'s format (`src/host/cabinet_config.cpp`) follows
Devil's Crush `src/cabinet/display_config.cpp` at commit `cbff624`, under
GPL-3.0-or-later: its keys, percent-escaping, strict one-key-per-line reading,
key-name bindings, flipper input lists, launch keys and the opposing-direction
resolution. Alien Crush adds per-window artwork rotation, raw cabinet buttons
and the reading of its own older formats. The flipper input masks are written
by this port's own hook at `01:4B7B`; no Devil's Crush addresses are used.

The music voice overlay (`src/runtime/music_voice_overlay.hpp`,
`cmake/MusicVoiceOverlay.cmake`) follows the design of Devil's Crush
`src/audio/music_voice_overlay.cpp` at commit `cbff624`, under
GPL-3.0-or-later: a second PSG, built from the music slots' latch stores and
programmed at the push for the channels effects take. Alien Crush's sound
driver addresses (bank `$19`) were traced independently from this cartridge
(`docs/MUSIC_VOICE_OVERLAY.md`), and its second PSG is upstream Beetle's
`PCE_PSG` rather than Devil's Crush's native one.
