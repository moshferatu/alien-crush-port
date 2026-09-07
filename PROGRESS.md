# Progress

The approved plan is [PLAN.md](PLAN.md). This file records verified implementation,
not intended capabilities. Last updated: 2026-09-07.

| Milestone | Status |
| --- | --- |
| 1. Trustworthy desktop baseline | Playable and deterministic; independent-emulator differences remain under investigation |
| 2. State and assets | Diagnostic capture, named state extraction, and bank-aware disassembly implemented; game state not yet mapped |
| 3. Playable full-table cabinet | Not yet implemented |
| 4. Native gameplay | Not yet implemented |
| 5. Standalone native release | Not yet implemented |

Implemented foundation: strict ROM identity, pinned reference integration,
separate SDL host and diagnostic executable, fixed game cadence, sharp scaling,
original controller inputs, quick-state containers, deterministic input recordings,
RAM/video/audio census and screenshots, core utility tests, and PinOne calibration model.

Verified on Windows x64 with MSVC 19.43 / Windows SDK 10.0.22621:

- Release CTest: 3/3 pass (core, 1,800-frame replay, SDL dummy desktop smoke).
- ROM-free Debug CTest: 1/1 pass, with both reference and SDL disabled.
- All 1,800 canonical frames match across cold boots and initial-state restore;
  all 900 frames after midpoint restore match RAM, pixels, and PCM exactly.
- A separately reopened 600-frame `.acrec` reproduces the original census exactly,
  including an additional midpoint restore.
- Serialization extension preserves every uninterrupted frame against pristine Beetle:
  1,800-frame census SHA-256 `ee560dad47ece1a3c2e55a4326bcd4ef97276ad1ac0912d6c36eb29298c6572e`.
- Independent Mesen capture: exact RAM on 537/600 frames; exact shared viewport
  on 581/600. Differences cluster around boot and scene loading. Mesen captures
  only 239 overlapping game rows; Alien Crush's final artwork row cannot be
  compared by borrowing Devil's Crush's black-row assumption. This is a diagnostic
  result, not a passed full-equivalence gate.
- Raw named state inspection extracts 251 CPU, VDC, palette, PSG and other fields
  without inserting execution hooks.

No native gameplay ownership, full-table
rendering, DMD state mapping, or game-specific analog launcher support is claimed yet.
Physical cabinet, three-monitor, focus/hotplug and prolonged-play acceptance remain open.
