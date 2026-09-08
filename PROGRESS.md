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

Actor assembly continuation:

- Added native ROM descriptor decoding and main-table actor ordering, signed
  placement, pattern-base arithmetic, attribute overrides and SAT capacity handling.
  The assembler observes animation selection without advancing gameplay.
- All 324 actor sprite words at the fixed lower/upper source frames 600/720 match
  exactly when reconstructed from the preceding RAM frames. Arithmetic edge checks
  and ROM-free descriptor bounds tests are included.
- Added per-sprite native/source CSV evidence and assembled sprite-layer previews.
  The broader one-frame-offset diagnostic matches 1,151/1,320 frames; 3,130 of
  209,268 words differ. Epoch/transition classification remains open, so this is
  not yet a replacement renderer or a whole-route equivalence result.

Actor timing continuation:

- Added reproducible timing reports comparing all RAM lags 0..3 on the same
  1,318-frame interval, with source SAT retention, table codes, PC/bank and
  mismatches separated into coordinates, patterns and attributes.
- Lag one is strongest (1,149 exact candidate frames). Of its 169 inexact frames,
  141 retain the prior source SAT and 28 change it. The longest unchanged source
  run is 13 frames; 130 frames have no exact candidate at any tested lag.
- These are diagnostic classifications, not proof of runtime update phases.
  Existing 324-word sample gates remain intact; live rendering is unchanged.

Actor instruction-boundary continuation:

- Added bank-checked, observational Mesen actor-phase tracing and paired RAM/SAT
  captures, plus a native verifier of emitted sprite words and end slots.
- Three sampled intervals validate 90 captured passes / 13,932 sprite words,
  including two passes crossing frame boundaries (the first two intervals overlap).
- Traces establish render-before-update order during ordinary play and an absent
  main render pass during frames 608..620 of the first table transition.
- Control capture matches all 600 RAM/CPU frames and video frames 20..600;
  initial 19 video frames differ before tracing starts. Whole-route native timing,
  later sprite passes and live renderer integration remain open.
- Release CTest remains 5/5; malformed trace and end-slot corruption are rejected.

Later sprite-pass continuation:

- Translated the four-sprite table overlay at `01:4741`, including visibility,
  strict capacity, table positions, camera subtraction and byte-level pattern arithmetic.
- Added paired overlay entry/return tracing and verification: all 80 sampled passes
  match 1,280 sprite words, every end slot and 3,708 cleared trailing SAT words.
  Main actor-pass validation also remains exact in both intervals.
- Added ROM-free branch/arithmetic tests; Release passes 5/5 and Debug passes 2/2.
  A corrupted SAT tail is rejected. Live renderer integration remains open.

Actor background-update continuation:

- Added native rectangular BAT descriptor decoding, including 32-word row stride,
  wrapped tile addition and optional high-byte attribute overrides.
- Added instruction-boundary captures and whole-VRAM comparison: ten sampled
  calls / 420 native writes match, including actor update-flag acknowledgements.
- Added ROM-free bounds and arithmetic tests; Release passes 5/5 and Debug 2/2.
  Truncation and a VRAM mutation outside the update region are rejected.
- Native update scheduling, dynamic pattern transfers, palettes and live rendering
  remain open; diagnostic code does not write reference game state.

Dynamic sprite-pattern continuation:

- Recovered actor-type destinations and all 17 dynamic word-RLE streams. Each
  contains 384 words; decoding returns source provenance and destination data.
- Four observed transfers match all 1,536 writes, complete VRAM and pending-flag
  acknowledgements, including one transfer across a frame boundary. These samples
  cover stream 1; the other streams have layout checks but not execution coverage.
- Existing actor/overlay/background checks and RAM/CPU captures remain unchanged.
  Release passes 5/5; ROM-free Debug passes 2/2. Palettes, HUD, native scheduling
  and live renderer integration remain open.

Main-table HUD continuation:

- Extracted the standard HUD stream: 768 words match both table samples.
  ROM-hashed exports include pattern data, provenance and a checked digit preview.
- Added settled score/ball sprite assembly. All 57,480 HUD words match across
  1,437 eligible frames, including coordinates, patterns and attributes.
- Added ROM-free numeric bounds and digit-order tests; Release passes 5/5 and
  Debug passes 2/2. Alternate HUD assets, incremental update timing, palettes and
  live renderer integration remain open.

Palette fade-out continuation:

- Added native channel darkening and the original already-black completion count,
  preserving the final extra all-black pass.
- All eight captured passes / 4,096 color words and counters match, including two
  computations across frame boundaries. Fade-in and RAM/CPU control checks still pass.
- ROM-free tests cover all 512 PCE colors; Release passes 5/5 and Debug 2/2.
  Palette-zero pulsing, upload timing and live renderer integration remain open.

Palette-zero pulse continuation:

- Added native pulse state transitions, direction/completion flags and byte/VCE
  packing, with synthetic coverage of the 14-write rise/fall cycle.
- Added pulse observations and instruction-boundary traces. All 107 captured
  inactive calls match state and complete palettes; RAM/CPU control logs match.
- The 10,000-frame canonical route has no observed active pulse. Active execution
  equivalence remains open and is reported separately from synthetic test coverage.
- Release passes 5/5 and Debug 2/2. Upload scheduling and live integration remain open.

Native diagnostic composition continuation:

- Combined background and sprites with scroll wrapping, clipping, layer-enable
  behavior and SAT priority resolved before background priority.
- Added `composite.bmp` to video exports. Lower/upper previews at frames 600/720
  were visually checked; all 1,800 reference RAM/video/audio fingerprints retain
  the frozen census hash with exports enabled.
- Release passes 5/5 and ROM-free Debug 2/2, including composition edge cases.
- This remains a frozen-register preview. Scanline limits/timing, continuous-table
  presentation and live renderer integration remain open.

Composition timing comparison continuation:

- Added reference images, exact pixel masks and per-row mismatch reports to video
  exports, plus compact `--compare-video` reports for every captured frame.
- Compared current and preceding-frame latched SAT independently. Current SAT
  matches 503/1,800 canonical frames; previous SAT matches 1,767/1,799, including
  all 15 periodic samples. The first preceding-frame result is explicitly absent.
- The remaining 32 mismatching frames are retained for investigation; frame 938
  has five mismatching pixels on one row. Independently verified its exported
  masks/counts against BMP bytes. No full-frame equality gate is claimed.
- Comparison retains the exact frozen reference census. Live renderer integration
  and the remaining scanline/transition differences remain open.
- Release regression tests pass 5/5; ROM-free Debug passes 2/2.

Sprite scanline limit continuation:

- Added the hardware's sixteen 16-pixel fetches per row, including transparent
  and offscreen pieces, partial wide sprites, and flipped wide-sprite ordering.
- This resolves 26 of the 32 previous-SAT mismatching frames. All-frame native
  comparison now matches 1,793/1,799 frames; six remain unresolved, with no formerly
  exact frame regressing. The frozen reference RAM/video/audio census is unchanged.
- Added ROM-free boundary tests; Release passes 5/5 and Debug 2/2.
  Register/VRAM/palette timing and live renderer integration remain open.

Video component timing continuation:

- Extended comparison reports with fixed previous-VRAM, palette, register and
  complete-video candidates, preserving input snapshots and per-candidate masks.
- Older control values reproduce three transition frames. Frame 19 mixes palette
  states across rows, and frame 758 mixes BAT states; no single frozen snapshot
  reproduces either. Frame 608's remaining difference is confined to its first
  two rows. Exact update scheduling remains open.
- Retained the original 1,793/1,799 previous-SAT matches and frozen reference census.
  Verified all six exported masks/row reports independently; Release passes 5/5.
  These are timing diagnostics, not additional live renderer fixes.

Video bus timing continuation:

- Added Mesen VDC/VCE traces with raster positions, master clocks and pre-write
  address/data state, plus a bounded inspector that decodes and validates requests.
- Frame 758 BAT requests occur around raster rows 42/43, supporting the observed
  mixed-VRAM frame. Control writes at raster row 1 support the enable/disable
  timing explanation. Mesen's startup palette timing differs from Beetle's.
- All 235 final requested words in the 758 trace match final memory. Traced/control
  RAM and CPU logs are identical; pixels match from frame 20 onward, with startup
  variation confined to frames 1..19. Malformed trace fixtures are rejected.
- Exact fetch/commit scheduling and live renderer integration remain open.

Native raster composition continuation:

- Added ordered, bounded native video writes at explicit output-row boundaries,
  preserving earlier rows, sprite priority/limits and immutable caller state.
- Added offline CSV schedule replay with input hashes, image/mask and row reports.
  Explicit reconstruction schedules reproduce frames 19 and 758 exactly. These
  use boundaries identified from pixels; automatic hardware scheduling remains open.
- All six fixed candidate counts and the frozen reference census remain unchanged
  across 1,800 frames. Release passes 5/5 and Debug 2/2, including raster edge cases.
- The automatic previous-SAT result remains 1,793/1,799; live integration is pending.

Observed display-control scheduling continuation:

- Added an optional read-only observer at Beetle's existing row-draw boundary,
  with bounded runtime capture and native translation of latched control/burst mode.
- The automatic reference-assisted candidate matches 1,795/1,798 complete frames,
  resolving three enable transitions. Startup frames 1/2 lack complete coverage;
  palette/VRAM differences remain at 19/608/758. This is not independent device timing.
- All 900 restored row observations repeat exactly. Observer-enabled and compiled-out
  captures preserve the frozen RAM/video/audio census. The optional build path is
  verified and observation support is restored in the final build.
- Release passes 5/5; Debug 2/2. Palette/VRAM scheduling and live integration remain open.

Observed pixel palette scheduling continuation:

- Added bounded VCE palette observation and native pixel-position palette updates,
  preserving palette identity through composition and validating complete write chains.
- Found that burst-mode output uses palette 256. Modeling that explicitly and
  scheduling its observed write reproduces startup frame 19 automatically.
- Combined display/palette timing matches 1,796/1,798 complete frames; remaining
  frames 608/758 still need VRAM timing. This remains reference-assisted rendering.
- All 900 restored observations repeat; enabled/compiled-out observers preserve
  the frozen census. Release passes 5/5, Debug 2/2. Live integration remains open.

Palette fade-in continuation:

- Translated the original discrete channel-by-channel fade-in, including brightness
  thresholds and red/green/blue tie priority.
- All 21 traced steps / 10,752 color words match, including nine computations across
  frame boundaries. The untraced control retains identical RAM and CPU/clock logs.
- Added ROM-free fade bounds/tie/completion tests; Release passes 5/5 and Debug 2/2.
  Corrupted output is rejected. Fade-out, palette-zero pulse, upload timing and
  live renderer integration remain open.
