# Provenance

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
