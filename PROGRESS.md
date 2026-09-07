# Progress

The approved plan is [PLAN.md](PLAN.md). This file records verified implementation,
not intended capabilities. Last updated: 2026-09-07.

| Milestone | Status |
| --- | --- |
| 1. Trustworthy desktop baseline | Playable and deterministic; independent-emulator differences remain under investigation |
| 2. State and assets | Native extraction of both table backgrounds and sprite pattern sets; score, high-score storage, ball counter and level observations recovered; remaining gameplay mapping open |
| 3. Playable full-table cabinet | Not yet implemented |
| 4. Native gameplay | Not yet implemented |
| 5. Standalone native release | Not yet implemented |

Implemented foundation: strict ROM identity, pinned reference integration,
separate SDL host and diagnostic executable, fixed game cadence, sharp scaling,
original controller inputs, quick-state containers, deterministic input recordings,
RAM/video/audio census and screenshots, core utility tests, and PinOne calibration model.

Verified on Windows x64 with MSVC 19.43 / Windows SDK 10.0.22621:

- Release CTest: 5/5 pass (core, graphics, 1,800-frame replay, asset discovery, SDL dummy desktop smoke).
- ROM-free Debug CTest: 2/2 pass, with both reference and SDL disabled.
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
rendering, DMD presentation, or game-specific analog launcher support is claimed yet.
Physical cabinet, three-monitor, focus/hotplug and prolonged-play acceptance remain open.

State/asset continuation:

- Added bounded C++ snapshot parsing, background/sprite decoding, compact video capture,
  per-frame game observations and optional Mesen VDC-write tracing.
- Both ROM background pattern sets match reference VRAM exactly; 1,437 score/ball
  comparisons match original source HUD data during the canonical route.
- Native word-RLE and palette extraction builds both initial background layers before
  game execution. Outputs are versioned by ROM hash and decoder schema.
- Synthetic graphics/parser/RLE/state tests pass in Debug and Release. Video capture
  plus per-frame game observations preserve the frozen pristine baseline exactly.
- Details and evidence: [docs/STATE_AND_ASSETS.md](docs/STATE_AND_ASSETS.md).

Sprite asset continuation:

- Recovered the shared, level-selected and local sprite streams from the original
  loaders, including the different middle-stream pointer used by the upper level.
- All 16,384 sprite pattern words per level match reference VRAM at frames 600/720.
- Asset exports include versioned sprite VRAM, initial palettes, stream provenance
  and hashes, and previews of every palette. Ball and flipper artwork is present;
  native actor assembly, animation and gameplay integration remain open.
