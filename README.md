# Alien Crush PC Port

Windows x64 port of Alien Crush for desktop and a three-monitor PinOne pinball cabinet.
The development baseline runs the original game in an independent, pinned Beetle PCE
reference runtime. Native gameplay and full-table cabinet presentation are being developed.
See [PLAN.md](PLAN.md) for the target and [PROGRESS.md](PROGRESS.md) for verified status.

## ROM

Supply your own `Alien Crush.pce`: 262,144 bytes, SHA-256
`1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f`.
Other ROM revisions are rejected. ROMs and generated game assets are not distributed.

## Build

Requires Visual Studio 2022 C++ tools, Windows SDK, and CMake 3.30+.
Initial configuration fetches the pinned SDL and Beetle sources.

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-release
ctest --preset windows-release
```

## Play

```powershell
.\out\build\windows-vs2022\Release\alien-crush.exe --rom '.\Alien Crush.pce'
```

The baseline uses the original controller behavior:

| Input | Action |
| --- | --- |
| Enter / gamepad Start | Start and pause |
| Left Shift / Left / left shoulder | Left flipper |
| Right Shift / Z / right shoulder / south button | Right flipper and original launcher |
| X / east button | Nudge |
| Backspace / gamepad Back | Select speed/music |
| F5 / F9 | Save / restore quick state |
| F11 | Borderless fullscreen |
| Escape | Exit |

Quick states live beside the executable in `portable-data/alien-crush.reference.state`.
Use `--data-dir` to select another location. Missing ROM paths default beside the executable.

Record with `--record session.acrec`, replay with `--replay session.acrec`.
Recordings embed the initial state and every sampled controller input with an integrity hash.
Quick-state operations are disabled during recording and replay to retain a linear timeline.

For the initial three-window cabinet host, add `--cabinet`. Move and resize the
playfield, backglass and score windows onto your monitors; F11 toggles fullscreen
for the focused window and R rotates the playfield clockwise by 90 degrees.
Placement, monitor names and rotation save on normal exit to
`portable-data/cabinet-windows.txt` (or your `--data-dir`). Missing monitors fall
back to connected displays with the windows clamped into view. Moving focus
between cabinet windows keeps the game running; switching away pauses it.
This prototype uses the original scrolling view, a plain title backglass and
observed score/ball fields. Stitched full-table rendering, ROM backglass artwork,
scene-aware score presentation and physical plunger integration remain pending.

## Diagnostics

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --validate-rom
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --verify-replay --frames 1800 --capture out/captures/baseline
```

The headless tools record per-frame RAM/video/audio fingerprints, periodic BMP screenshots,
RAM snapshots, raw per-frame RAM/pixels, and a replayable input recording. A 1,800-frame
capture takes about 440 MB. No game-specific execution patches are installed.
`--verify-replay` checks repeated execution, midpoint restore, and boot-state restore.
Use `--replay session.acrec` to diagnose a recorded play session instead of the canonical route.
Add `--video-every 120` to a capture to export native background, sprite and combined
`composite.bmp` previews. Composition uses frozen registers and latched sprites;
scanline timing and live renderer integration remain open.
Exports also include the reference image, exact difference masks and per-row counts.
`--compare-video --capture <directory> --compact` writes `composite-epochs.csv` for
every frame, comparing current and previous-frame latched sprite tables separately.
Additional fixed candidates use previous VRAM, palette, registers, or all video
state, always with the previous sprite table. Exports retain these inputs and
each candidate's image, mask, and row report to investigate update timing.
Composition enforces the hardware's sixteen 16-pixel sprite pieces per row.
The previous-frame table matches 1,793/1,799 comparable canonical frames; remaining
differences are diagnostic, and this option does not enforce an equality gate.

The native renderer also accepts changes at explicit output-row boundaries. Replay
a supplied schedule against an exported video snapshot without running the emulator:

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --render-raster out/captures/example/video-758 --raster-writes out/schedule.csv --capture out/captures/raster-example
```

The CSV header is `row,target,address,value`, with decimal integers and targets
`vram`, `palette`, `sat`, `control`, `scroll_x`, `scroll_y`, `memory_width`, or `burst_mode`.
Rows must be sorted; changes apply before that row in file order. Register addresses
are zero. Initial state comes from `reference.state`; row-zero writes can explicitly
replace initial values. The command requires `reference.bmp` and a new output
directory, and saves the schedule, rendered image, difference mask, row counts and
input hashes. Exact pixels validate the supplied schedule's composition; hardware
timing and live scheduling remain separate work.

`--compare-display --capture <directory> --compact` also records reference display
control at the existing row-draw boundary, plus palette writes at the next output
pixel, and feeds them to the native raster renderer.
It writes `display-rows.csv`, `display-comparison.csv`, and a provenance manifest;
video exports include replayable `display-writes.csv`. This candidate matches
1,795/1,798 frames with complete observations using display control alone;
including palette timing raises this to 1,796/1,798, and observed scanline VRAM
raises it to **1,798/1,798**. The first two startup frames
have incomplete coverage and are excluded. Video exports also contain
`palette-timed.bmp`, its mask and row counts, initial palette and pixel-write schedule.
The schedule still depends on reference timing. The observer is inactive by default
at runtime and can be compiled out with `-DAC_REFERENCE_VIDEO_OBSERVER=OFF`.
Exports include a `vram-timed/` directory with the combined image, mask, palette
inputs and `vram-rows.csv` (full initial VRAM followed by row deltas).
`--require-display-match --capture <directory> --compact --verify-replay` enforces
exact rendering and complete coverage after the two startup frames on the canonical
route. The ROM-enabled `reference_video` test runs this 1,800-frame gate and checks
restored display, palette and VRAM observations as well as RAM/video/audio output.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Verify-Baseline.ps1 -RomPath '.\Alien Crush.pce'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Inspect-ReferenceState.ps1 -PayloadPath out/captures/verified-baseline/checkpoint.payload
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Disassemble-Rom.ps1 -RomPath '.\Alien Crush.pce' -Bank 00 -Pc E2FC -Count 32
```

The baseline verifier compares against a frozen census from untouched Beetle. The default
runtime extends serialization to preserve audio history and fractional timestamps; see
[provenance](third_party/REUSE.md). Set `AC_REFERENCE_STATE_EXTENSION=OFF` in a separate
build directory to reproduce the untouched source baseline. Its incomplete audio saves
are unsuitable for the deterministic restored-audio gate.

For an independent Mesen 2.1.1 comparison, supply the executable from its complete
Windows distribution. The capture script checks its hash, copies runtime binaries to an
isolated output directory, and sets the capture palette/controller configuration there.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-MesenCapture.ps1 -MesenPath '<Mesen directory>\Mesen.exe' -RomPath '.\Alien Crush.pce' -Frames 600
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --frames 600 --capture out/captures/oracle
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Compare-MesenCapture.ps1 -ReferenceDirectory out/captures/oracle -MesenDirectory out/mesen/baseline
```

The comparator reports differences; it does not turn nonzero mismatches into a passing
equivalence assertion. See [PROGRESS.md](PROGRESS.md) for current results and coverage gaps.

## State and graphics discovery

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --extract-assets out/assets
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --verify-discovery
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --verify-discovery --capture out/captures/actors
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --frames 1800 --capture out/captures/discovery --video-every 120 --compact --verify-replay
```

Asset extraction produces both initial table backgrounds and sprite pattern sets
directly from the ROM, without executing the game, under
`out/assets/<ROM SHA-256>/table-background-v1` and `table-sprites-v1`.
Sprite exports include VRAM, initial palettes, a stream manifest with ROM offsets
and hashes, and 16 palette previews per level. Each preview shows pattern IDs
256 through 511 in row order as individual 16-by-16 tiles. Actor placement,
multi-tile assembly and palette animation remain separate from these assets.

The native actor assembler decodes selected animation descriptors and reproduces
sprite ordering, placement and attributes. Discovery verification enforces exact
actor sprite words at two lower/upper samples. Adding `--capture <new-directory>`
exports per-sprite comparisons, a zero-to-three-frame timing sweep with source
SAT retention and field-level differences, and native sprite-layer previews. The wider report
still contains epoch/transition differences; the assembler is diagnostic and does
not yet replace the live renderer. See the evidence document for counts and limits.

For instruction-boundary validation, Mesen capture accepts
`-TraceActorsLastFrames 120`. Feed its `actor-passes.bin` to
`alien-crush-tools --verify-actor-trace <file>` to check native sprite words and
end slots against paired render-entry/main-pass-end observations. See
[state and asset evidence](docs/STATE_AND_ASSETS.md) for commands and limitations.
The same capture also emits `overlay-passes.bin`; pass that file to the same
verifier to check the native four-sprite table overlay and original SAT tail clearing.
Actor-driven background rectangles are decoded natively as well. The capture's
`background-passes.bin` can be checked with `--verify-background-trace <file>`;
this compares complete before/after VRAM and the actor's update acknowledgement.
Dynamic sprite artwork transfers are available through
`ActorAssets::pattern_transfer`. Use `--verify-pattern-trace <pattern-passes.bin>`
to compare their native decoding with the bounded Mesen capture.
Asset extraction also produces `table-hud-v1` with the standard HUD pattern
stream, source hashes and a digit preview. Native score/ball sprite assembly is
checked against all 40 HUD words on eligible gameplay frames; live timing remains separate.
The palette fade-in step is also native. Bounded tracing produces
`palette-fades.bin`; `--verify-palette-trace <file>` compares all colors against
the original step's output before palette upload.
The verifier also accepts `palette-fade-outs.bin`, checking channel darkening
and the original already-black completion count, including its final extra pass.
Palette-zero pulse decoding and `--verify-pulse-trace <palette-pulses.bin>` are
also available. Current traces cover the inactive path; active cycles have
synthetic tests and still need an original-game execution sample.

Captures include `game-observations.csv` with recovered score, ball/table counters
and high scores. `--video-every` also exports background/sprite BMPs, attributes,
raw video memory and metadata. `--compact` omits large per-frame raw RAM/pixel
streams; use a fresh output directory per capture. Mesen comparison requires raw mode.

See [state and asset evidence](docs/STATE_AND_ASSETS.md) for recovered addresses and
validation. `Invoke-MesenCapture.ps1` accepts `-TraceVideoLastFrames 1..10` for short
read-only VDC/VCE bus traces with master clocks, raster positions and pre-write
address/data state. It saves video snapshots around the trace window.
`Inspect-VideoTiming.ps1 -TraceDirectory <Mesen capture>` validates the trace and
writes `video-events.csv` with decoded VRAM requests, palette writes and display
control changes. Bus request times are not necessarily VRAM commit times.
`Find-VideoRomMatches.ps1 -RomPath <ROM> -VideoDirectory
<video-N directory>` locates candidate source data.

Enable local ROM tests with `-DAC_ENABLE_ROM_TESTS=ON -DAC_ROM_PATH='<absolute ROM path>'`
at configuration. Public CI runs ROM-free tests. `windows-core-only` builds without SDL or Beetle.
