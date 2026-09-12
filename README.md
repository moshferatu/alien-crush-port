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

## Publish to the cabinet

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Publish-Cabinet.ps1
```

Builds the static `windows-cabinet` Release executable and publishes to
`D:\Workspace\alien-crush-cabinet` (a sibling of your checkout). Use
`-Destination 'E:\Syncthing\Alien Crush'` to choose another folder, `-RomPath`
for a differently located ROM, or `-SkipBuild` to publish the existing cabinet
build. The script finds CMake on PATH or through Visual Studio; `-CMakePath`
overrides discovery. It works from any current directory.

The package includes the executable, ROM, licenses, documentation and
`Launch-Cabinet.cmd`. SDL and the Microsoft runtime are linked statically.
Run the launcher on the cabinet, or configure your frontend to launch
`alien-crush.exe --cabinet`. Close the game before syncing an executable update.
Republishing preserves local settings and other files in the destination.

Point Syncthing at **only the published folder**. Before the first sync, add
the following Ignore Patterns on **both computers**:

```text
/portable-data
/logs
/captures
/traces
*.state
*.acrec
```

The script creates the workstation's `.stignore`, retaining existing custom
rules and including the packaged `syncthing-ignore.txt`. Once that file exists
on the cabinet, `#include syncthing-ignore.txt` can replace the patterns above.
[Syncthing does not sync `.stignore` itself](https://docs.syncthing.net/users/ignoring.html),
so set up the cabinet's exclusions separately. `CABINET-SETUP.txt` in the package
contains these instructions. Monitor layouts, input/plunger bindings, custom
backglass artwork and quick states under `portable-data` stay machine-local;
copy any desired bindings or artwork to the cabinet manually once.

## Play

```powershell
.\out\build\windows-vs2022\Release\alien-crush.exe --rom '.\Alien Crush.pce'
```

Live play uses the Devil's Crush cabinet control layout:

| Input | Action |
| --- | --- |
| Enter / gamepad Start | Launch while seated; start/confirm/pause elsewhere |
| Left Shift / Left / left shoulder | Left flipper |
| Right Shift / Right / right shoulder | Right flipper |
| Gamepad south button (A / Cross) | Launch / confirm |
| X / east button | Nudge |
| Backspace / gamepad Back | Select speed/music |
| F5 / F9 | Save / restore quick state |
| F11 | Borderless fullscreen |
| Escape | Exit |

Hold Enter (or the south gamepad button) to draw the launcher, then release to
fire; a configured physical plunger also works. Enter remains a launch control
at full charge and cannot pause until the ball has left
the shooter; a launch press must be released before a fresh press can pause.
Both flippers remain controllable
while the ball is seated, leaving the launcher or absent during table play.
Pressing both flippers works; launching never operates a flipper. Flipper keys
send directions in menus/name entry, so they cannot accidentally confirm a letter.
On the high-score initials screen, use Left/Right or the flippers to choose a
letter and press Enter to accept it and advance. The third press submits the
name. Gamepad Start and a cabinet Run button follow the same behavior; the
dedicated Launch / gamepad south button also accepts letters.
Pause and the cartridge's tilt gates remain active. Z is no longer a flipper key.

Desktop quick states live beside the executable in `portable-data/alien-crush.desktop-controls.state`.
Live cabinet mode uses `alien-crush.cabinet-controls.state` and a distinct runtime identity.
Recordings select their recorded runtime automatically: existing reference recordings
retain original loading timing and controls; new cabinet recordings use continuous
handoffs and separate controls. The previous `.reference.state`/`.cabinet.state`
files are preserved, but cannot be loaded into the changed-controls runtime.
Quick states cannot be exchanged between these execution modes.
Use `--data-dir` to select another location. Missing ROM paths default beside the executable.

Record with `--record session.acrec`, replay with `--replay session.acrec`.
Recordings embed the initial state and every sampled controller input with an integrity hash.
Replay also supports per-frame plunger draw/cancel commands (`ACREC002`); pad-only
recordings retain `ACREC001`. Commands run before the frame and a rejected launcher
operation stops playback. Both the SDL host and diagnostic tool support this format.
Live cabinet recordings include accepted plunger commands when analog input is configured.
Quick-state operations are disabled during recording and replay to retain a linear timeline.

For the three-window cabinet host with a stitched playfield, add `--cabinet`. Move and resize the
playfield, backglass and score windows onto your monitors, or use these keys:

| Cabinet key | Action |
| --- | --- |
| F6 / Shift+F6 | Focus the next / previous window: playfield, backglass, score |
| F10 / Shift+F10 | Move the focused window to the next / previous connected monitor |
| R / Shift+R | Rotate the focused window clockwise / counterclockwise by 90 degrees |
| F11 | Toggle fullscreen for the focused window |

Monitor changes center the window on the destination, fit its size to the usable
area, and retain its rotation and fullscreen setting. F10 does nothing when only
one monitor is connected. Keyboard repeats do not repeat setup actions.
Placement, monitor names and rotation save on normal exit to
`portable-data/cabinet.ini` (or your `--data-dir`). This single file also holds
button mappings and plunger calibration. Older separate settings files are
imported when the corresponding settings are absent and retained as backups;
normal exit saves the combined configuration. Missing monitors fall
back to connected displays with the windows clamped into view. Moving focus
between cabinet windows keeps the game running; switching away pauses it.
Live monitor connection, position, orientation and resolution changes also recover
displaced windows. Visible windows stay on their current monitor; reconnecting a
monitor does not automatically move them back (use F10). Recovery preserves
rotation and fullscreen without explicitly raising windows. If no displays are
available, gameplay waits for a usable layout; exiting during that wait preserves
the previously saved window settings. Display changes clear queued audio and
rearm cabinet controls, cancelling an owned analog draw before gameplay resumes.
The score window uses the original game digits for score, high score and ball
counter in a fixed layout with sharp scaling and black borders. Digit size stays
stable as values grow. The three-ball DMD counts up: BALL 1, BALL 2, BALL 3,
retaining BALL 3 through the final drain. During the last-chance extra-ball spin,
the playfield displays the original three match digits and the DMD replaces its
high-score field with MATCH and the same value, including a stopped result.
During the title and selection menus it shows the high
score and a "PRESS RUN" prompt, hiding the inactive score and ball counter.
Attract-mode demos show the same high score and prompt with a "DEMO" label;
simulated scores and ball counts stay hidden. Run returns to the original menus.
In a bonus room, the score remains the game's cumulative total and the lower-right
field shows BONUS 1–4 instead of the main-table ball counter. Bonus rooms use the
original single-screen playfield in cabinet mode, aligned to the bottom of the
window like the main table.
When the original bonus loop ends, the panel displays ROUND OVER while retaining
the cumulative score and room number. It follows the game's timing and clears
as the original scene advances.
During results, BONUS CLEAR appears when the game's completion flag is set.
The score continues to follow the original tally and completion awards. A
controller-only room-3 clear replay verifies progression with all three balls
preserved; clears in the other rooms still need coverage.
An ordinary-play replay also verifies bonus-room-2 entry and return to both main
table halves. Cabinet mode fades both halves in together, keeping the full-table
projection from the initial load and throughout bonus returns.
After the final ordinary ball is consumed, the score window shows GAME OVER
with the final score. The original score-digit match can award another ball;
I or II stops the match animation. A loss returns to the title menus, while a
win resumes play with the score preserved. Replays cover both outcomes, the
return to the stitched table and exact save/restore. Cabinet mode suppresses
moving score digits on the playfield; the separate score window follows the
award. Digit uploads never switch the main table to a single viewport.
To view the five high scores during ordinary play, press **Enter** (Run) to
pause, then **Backspace** (Select). The score window shows all five ranks while
the playfield shows the original score-table screen. Press Backspace or Enter
to leave that screen, then Enter to resume play. Scores currently last for the
running session or a quick state; automatic persistence is not implemented yet.
The cabinet also shows the ranked table during normal-game and bonus-practice
name entry. Name editing remains on the original playfield screen.
A qualifying replay checks a 1,505,700-point score, name confirmation and
retention when starting another game in the same session.
Put your artwork at `portable-data/backglass.bmp` (or in
your `--data-dir`) to display it on the backglass. Use an uncompressed 24-bit or
32-bit Windows BMP, at most 4096x4096 pixels and 64 MiB; pixels are opaque.
Artwork loads at startup, fits without stretching or cropping, and honors saved
backglass rotation. Black borders fill unused space. Restart to change artwork.
Without a local image, the backglass displays the original Alien Crush title logo,
decoded directly from your ROM. It continuously cycles from the blue artwork to
the readable ALIEN CRUSH lettering and its orange glow, then reverses the reveal
to loop smoothly. The logo stays centered on black with corrected cartridge pixel
aspect and saved rotation. Animation images are prepared at startup; playback
does not run another game core or affect gameplay/audio. Invalid local artwork reports an
error and is preserved for correction. Artwork stays local and is not stored in
recordings. Other bonus-room clears, broader ordinary-play routes and
physical cabinet acceptance still need broader testing.

Raw cabinet buttons can be configured for boards such as PinOne, including devices
SDL does not recognize as gamepads. First run the hardware probe (no ROM required):

```powershell
.\out\build\windows-vs2022\Release\alien-crush.exe --input-probe
```

The window and console show controller names, zero-based pressed button numbers,
and raw axis readings. Pull the plunger to identify its axis and rest/full-pull
endpoints; these readings are diagnostic and do not control launch power yet.
Escape closes the probe. `--frames N` limits it to N samples at 100ms intervals.

Edit `input.buttons` in `portable-data/cabinet.ini` using the observed
controller name and button numbers:

```text
input.buttons=ACINPUTS1 "PinOne" 0 1 2 3 4 -1 -1 -1
```

The eight numbers map, in order, to left flipper, right flipper, nudge,
start/launch/confirm/pause, select, up, down and right/menu. The second button
now operates only the right flipper. The fourth launches while the ball is seated.
For an additional dedicated launch/confirm button, use `ACINPUTS2` with a ninth
index (for example `0 1 2 3 4 -1 -1 -1 5`). Existing eight-entry `ACINPUTS1`
files remain readable. **The numbers above are examples, not
verified PinOne defaults.** Use `-1` to disable a mapping; valid indices are 0–255.
The name is a nonempty case-insensitive substring; the first matching device with
all configured buttons is selected. Choose a distinctive name when several boards
are connected. Settings load at startup in `--cabinet` mode.
An empty `input.buttons=` leaves raw buttons disabled; malformed settings report an error.
Keyboard controls and other gamepads remain available. The selected board's default
gamepad mapping is suppressed to prevent duplicate actions.

Release all mapped buttons after startup, reconnect, quick-load or returning from
another application before using them. Disconnecting clears held raw input and
reconnecting finds the board by name even if its SDL instance ID changes. Recordings
store the resulting controller bits and accepted plunger commands; replay ignores hardware mappings.
Plunger setup accepts the same settings as Devil's Crush. Copy the working
`input.plunger.*` lines from that game's `portable-data/cabinet.ini` into Alien
Crush's existing `portable-data/cabinet.ini`, alongside its window and button
settings. No separate plunger configuration file is required.
Alternatively, read the existing file directly without copying or modifying it:

```powershell
.\alien-crush.exe --cabinet --plunger-config 'D:\Path\To\DevilsCrush\portable-data\cabinet.ini'
```

The same PinOne defaults apply, so an axis-only configuration is sufficient
for a board using the standard center-rest, positive-pull Z axis:

```text
input.plunger.axis=2
```

Full settings use Devil's Crush's names and values:

```text
input.plunger.device=PinOne
input.plunger.axis=2
input.plunger.rest=0
input.plunger.drawn=32767
input.plunger.deadzone=8
input.plunger.release=25
```

An omitted/empty device name selects the first joystick with the configured axis;
a supplied name matches a case-insensitive substring. Axis -1 disables input.
To measure a different board, run `alien-crush.exe --cabinet-plunger-probe` in a
terminal. As in Devil's Crush, leave the rod at rest for two seconds, then fully
pull and release it during the twelve-second measurement. The probe prints the
settings to paste into `cabinet.ini`; it does not change existing calibration.
`--input-probe` remains available for the live raw button/axis display.

Restart after editing. Explicit `--plunger-config` takes precedence, followed by
the data directory's `cabinet.ini`. The older `cabinet-plunger.txt` format below
is imported only when the INI has no plunger settings:

```text
ACPLUNGER1
"PinOne"
2 0 32767 8 25
```

The five numbers are **axis index, rest value, full-pull value, deadzone percent,
release-fall percent**. These are examples; measure your board with `--input-probe`.
Endpoints may run in either direction and must be distinct values in -32768..32767.
The deadzone is 0..99; release threshold is 1..100. Choose a specific device name
substring in the legacy format; the first matching board with that axis is used. Buttons and the plunger
may use different boards. Missing configuration disables analog input; invalid
configuration reports an error without overwriting it. Restart after editing.

The rod must rest after startup, reconnect and quick-load. A seated ball follows
the calibrated draw and fires through the original game code when the rod releases.
During a rod-controlled draw, I/II from buttons or keyboard cannot change the shot.
Pause cancels the draw. Focus loss or disconnect queues cancellation before the
next emulated frame; an unfocused game stays paused until focus returns. After
cancellation or release, rest the rod and release I/II before another shot.
F5 quick-save requires releasing an owned draw first; F9 discards the old input
ownership. Replay reproduces accepted commands without needing the device.
This has virtual SDL coverage; physical PinOne calibration and cabinet acceptance
are still pending.

Cabinet mode always uses the 256×456 full-table view. The former host
`--stitched` option and V view toggle have been removed. Both backgrounds
come from your ROM; the active half has live sprites and background changes,
with standard score digits removed. The original pixel aspect ratio is preserved.
The inactive half keeps shared palette cycles live, mirrors the actual flipper
poses through each half's ROM frame tables, and continues recognized in-progress
sprite animation sequences in a private visual bank. The small bouncing orbs
continue bobbing and respawn after their saved timer; saved orb explosions finish.
The orbs beside the upper-left brain also continue their sideways wobble and
finish saved hops, using the original randomized burst lengths and rest times.
A private copy of the ROM random-number routine keeps their timing independent
without advancing gameplay randomness.
Their initial five-orb group is seeded from ROM positions in the private cabinet
view before the first upper-half visit, when the saved spawner is still pending.
Orange side aliens continue emerging and retreating, using fixed cosmetic waits
without consuming gameplay randomness. These inactive movements only change the
cabinet image; real actor positions, collisions and scoring follow the original
game. Other idle collision-triggered objects stay at rest. Animation pauses with
the game and reseeds after a table change, scene change or quick load. The cabinet
draws the current ball above table artwork, sprites and point labels so it remains
visible through overlaps. Persistent indicator colors and each half's
table overlay are restored from game state, as is the lower table's seven-variant
counter graphic and the two large background actors that redraw when a table
is restored. Other inactive background changes
and transient indicator blinking remain unfinished. During a confirmed table
change, the last complete table artwork stays visible until the new half is
ready, for at most 32 emulated frames. The ball uses its actual stored coordinates
on both ready and held tables, above decorative overlaps. Loading, fade-in, nudges and score awards retain
the full-table projection. Title menus, high-score menus and bonus rooms use the
original display. Upper-half artwork stays fixed across the 24-row join, while visible
sprites from both halves remain present. Ball movement is not interpolated.
The artwork offset is 216 pixels, eight less than the original ball handoff.
Live cabinet mode replaces main-table graphics decompression and actor-bank copying
with synchronous transfers and bypasses the handoff's two display waits. Collision
pointers, actor restoration and gameplay setup still run in their original order.
Every game frame supplies its full audio batch; no seam frames or music samples
are discarded. This applies to both live play and attract-mode demos in cabinet
mode. Desktop mode and old reference
recordings retain the cartridge's loading timeline. The default video-enabled
backend is required for continuous handoffs.
For timing diagnostics, `--timing-log <file.csv>` records frame start/end times,
audio sample counts and queued audio bytes.
With the default reference observer enabled, the active half uses observed row
timing for display/VRAM and pixel timing for palette changes. Pixels outside the
removed HUD, current-position ball and deliberately changed overlap
match the reference on all 1,262 supported canonical replay frames. Builds with
the observer compiled out retain the frozen-state renderer.

```powershell
.\out\build\windows-vs2022\Release\alien-crush.exe --rom '.\Alien Crush.pce' --cabinet
```

The diagnostic tools retain their separate `--stitched` capture option for exact
reference comparisons. For per-frame availability/hashes, add `--stitched`
to a tools capture with `--video-every 120`. Stitched BMPs are written only for
supported or held frames; `stitched.csv` records every fallback and marks held
images in its `held` column. `ball_updated` marks both ready and held images whose
ball was redrawn from its current stored state.

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
  The stitched view uses the timed renderer when observation is available.
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
