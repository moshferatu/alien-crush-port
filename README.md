# Alien Crush PC Port

Windows x64 port of Alien Crush for desktop and a three-monitor PinOne pinball cabinet.
The development baseline runs the original game in an independent, pinned Beetle PCE
reference runtime. Native gameplay and full-table cabinet presentation are being developed.
See [PLAN.md](PLAN.md) for the target and [PROGRESS.md](PROGRESS.md) for verified status.

For frame-rate measurements and cabinet timing diagnostics, see
[Frame pacing investigation](docs/FRAME_PACING_INVESTIGATION.md).

The playfield uses VSync by default while the game retains its original
59.826 Hz clock. Backglass and DMD windows do not wait on their monitors.
To compare with timer pacing, run `alien-crush.exe --cabinet --no-vsync`
(or `Launch-Cabinet.cmd --no-vsync`). Both modes use the optimized stitched
renderer. If VSync is unavailable, timer pacing is selected automatically.

On high-refresh displays, cabinet mode smooths moving sprites throughout the
main table and bonus stages, including flippers, aliens, bumpers, guards and the
ball. Sprite positions interpolate separately; pose and palette changes blend
at those positions. The title background scrolls at fractional positions, and
the backglass logo's color animation also blends between ticks. Swallowing
mouths still cover the ball. Physics, audio, animation scripts and recordings
retain the original 59.826 Hz rate. Smoothing adds up to one game tick of visual
latency. Pausing freezes the table, and scene changes discard outgoing history.
Desktop mode retains the original presentation.
Smoothing defaults on when the playfield display reports more than 66 Hz.
Use **F7** to compare while playing, or `Launch-Cabinet.cmd --no-smooth` to
disable it at startup (`--smooth` enables it). F6 still cycles cabinet windows.
If refresh detection is incorrect, use `--refresh 144` (or your monitor's actual
rate). `--no-vsync --refresh 240` exercises the 240 Hz timer presentation path;
the monitor must support that rate to display every generated picture.
Normal exit reports presentation pictures separately from simulation frames.
See [smoothing implementation and checks](docs/MOTION_SMOOTHING.md) for measured
results and the difference between generated pictures and physical refreshes.

On normal exit, the executable automatically prints a native coverage summary
to the console, including session frame counts for the native stitched playfield
and the reference viewport. These count game frames, including catch-up frames,
but exclude repeated VSync presentations and quick-state restoration warm-up.
Unlike Devil's Crush, this backend does not yet translate gameplay CPU routines
or measure execution cycle ownership, so its report explicitly marks the
outside-wait/stall CPU percentage as unavailable. Native cabinet rendering,
animation, and controls are not counted as translated CPU execution.

## ROM

Supply your own `Alien Crush.pce`: 262,144 bytes, SHA-256
`1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f`.
Other ROM revisions are rejected. ROMs and generated game assets are not distributed.

## Launcher

`alien-crush-launcher.exe` sits beside the game and is the easy way to set it
up: `portable-data/cabinet.ini` a tab at a time, and a Play button that saves
and starts the game, then comes back when the game closes.

| Tab | |
| --- | --- |
| Game | The ROM, and cabinet or desktop mode |
| Video | Smoothing, and for the cabinet: which monitor each window is on and whether it is fullscreen, and the playfield's rotation |
| Audio | The music voice overlay |
| Cabinet | The nudge strength and nudge board, and the analog plunger |
| Keyboard | The keys for each PC Engine pad input, the launch keys, and each flipper's pad inputs |

It is worked by mouse, keyboard or controller: Up and Down move, Left and
Right change a value, Enter opens a list, starts editing, or waits for a key, Tab (or L1 and R1) turns the tabs, Ctrl+S saves, and
Ctrl+Enter, F5 or Start plays. The ROM can also be chosen with Browse or
dropped on the window. A click on the launcher acts even when another window
has the focus.

With no options the game plays as `portable-data/cabinet.ini` says, which is
what the launcher edits: `game.rom`, `game.cabinet`,
`audio.music_voice_overlay`, `video.smoothing` and `input.nudge_multiplier`,
beside the window, key, button, nudge and plunger fields described below. An
option on the command line beats the file for that run (`--rom`, `--cabinet`
or `--no-cabinet`, `--music-voice-overlay` or `--no-music-voice-overlay`,
`--smooth` or `--no-smooth`, `--nudge-multiplier`), and replays and `--frames`
runs ignore these five. F7 is remembered in `video.smoothing` when a cabinet
session exits normally.

`cabinet.ini` is in Devil's Crush's format, with Devil's Crush's keys wherever
the two games share a setting, so its window, key, flipper, launch, nudge and
plunger lines read the same in either game:

```text
version=1
playfield.display=Main monitor
playfield.x=0
playfield.y=0
playfield.width=540
playfield.height=720
playfield.rotation=1
playfield.fullscreen=0
input.one=Return
input.right=Right Shift,Right
input.left_flipper=left
input.right_flipper=right,up
input.launch=Return
game.cabinet=1
audio.music_voice_overlay=1
```

Every line is `key=value`, values are percent-escaped (`%2C` is a comma in a
name), and a file with a repeated key or a value the game cannot read is
reported and left alone rather than replaced; unknown keys are ignored. The
score window is `dmd`, as the display that shows the score is in Devil's
Crush. Each window has `display`, `x`, `y`, `width`, `height`, `rotation` (the
monitor's own orientation, as SDL numbers it) and `fullscreen`, all or none;
a window with none, or no size, opens where the game first puts it. What only
Alien Crush has is added in the same style: `<window>.artwork_rotation` for
each window (Devil's Crush has only the playfield's), and `input.buttons.*`
for a cabinet board's buttons. The file before this format, with its
`window.*` and `input.buttons=ACINPUTS...` lines, and the separate
`cabinet-windows.txt`, `cabinet-inputs.txt` and `cabinet-plunger.txt` before
that, are still read; the launcher's Save, or a cabinet session's exit,
rewrites them in this format.

The launcher's icon and heading are the title screen's logo, decoded from the
ROM at build time (`tools/make_launcher_art.cpp`), since nothing of the ROM's
is kept in the repository. Without `Alien Crush.pce` in the source folder, or
`-DAC_LAUNCHER_ART_ROM=<path>`, it builds without an icon and with a text
heading.

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

The package includes the game, its launcher, the ROM, licenses, documentation
and `Launch-Cabinet.cmd`. SDL and the Microsoft runtime are linked statically.
Run `alien-crush-launcher.exe` or `Launch-Cabinet.cmd` on the cabinet, or
configure your frontend to launch `alien-crush.exe --cabinet` (or
`alien-crush.exe` alone, once the launcher has saved cabinet mode). Close the game before syncing an executable update.
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

Live play uses the Devil's Crush cabinet control layout (the keys are [rebindable](#keys)):

| Input | Action |
| --- | --- |
| Enter / gamepad Start | Launch while seated; start/confirm/pause elsewhere |
| Left Shift / Left / left shoulder | Left flipper |
| Right Shift / Right / right shoulder | Right flipper |
| Gamepad south button (A / Cross) | Launch / confirm |
| Space / east button | Nudge |
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

### Keys

The keys above are the defaults, and each is rebindable, as in Devil's Crush:
on the launcher's Keyboard tab, or under `input.` in `portable-data/cabinet.ini`.
Each of the eight PC Engine pad inputs (`input.one`, `two`, `select`, `run`,
`up`, `right`, `down`, `left`) takes up to four keys by their SDL names,
comma-separated; an empty value leaves the input unbound. A name SDL does not
know is reported and skipped, and the input's other keys still work.

```text
input.one=Return
input.run=Return
input.left=Left Shift,Left
input.right=Right Shift,Right
input.launch=Return
input.left_flipper=left
input.right_flipper=right,up
```

`input.launch` lists keys that send button I alone while the ball is seated in
the shooter, so a key that is also Run launches there rather than pausing; a
held launch stays one until it is let go. `input.left_flipper` and
`input.right_flipper` are the pad inputs each flipper answers to during play,
by name (`one`, `two`, `select`, `run`, `up`, `right`, `down`, `left`); empty
leaves the cartridge's own. The right flipper's Up is its stand-in while both
flippers are held, since the pad cannot send Left and Right at once. Gamepad
shoulders and a cabinet board's flipper buttons send the first input on their
flipper's list. Flipper inputs other than the defaults are part of the
runtime's identity, as a nudge multiplier is: recordings replay with the
flippers they were made with, and quick states need the same ones.

### Music voice overlay

The sound driver has six channels, and a sound effect takes a channel from
the music while it plays: the music's voice there keeps going, unheard, until
the effect ends. The music voice overlay plays those voices on a second sound
generator, so the music keeps all of its parts under the flippers, bumpers and
aliens, as Devil's Crush does. It is on by default; turn it off on the
launcher's Audio tab, with `audio.music_voice_overlay=0`, or for one run with
`--no-music-voice-overlay`. It only adds to the sound: the game, its
recordings and its quick states are the same either way, and replays and
`--frames` runs play the original's sound unless `--music-voice-overlay` asks.
See [docs/MUSIC_VOICE_OVERLAY.md](docs/MUSIC_VOICE_OVERLAY.md).

### Cabinet actors

The cartridge runs only the half of the table the ball is in. The cabinet
shows both, so in cabinet mode the port runs the other half itself: its
aliens, mouths, cocoons, orbs, risers, targets and every other actor that moves
on its own, and its palette scripts. The cabinet draws both halves straight
from the game's state, so what is drawn is what the ball meets. The half with
the ball runs the cartridge's own code, at the original's speed, slowdown
included. Anything that would score, touch the ball or run the game's rules
waits for the ball to come back. Play is the original's until the ball first
changes halves; after that, the other half is wherever its actors have got
to. Desktop mode runs the cartridge alone. Cabinet recordings made earlier
replay as they were made; earlier cabinet quick states don't load. See
[docs/CABINET_ACTORS.md](docs/CABINET_ACTORS.md).

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
| F7 | Toggle high-refresh cabinet animation smoothing |
| F10 / Shift+F10 | Move the focused window to the next / previous connected monitor |
| R / Shift+R | Rotate the focused window clockwise / counterclockwise by 90 degrees |
| F11 | Toggle fullscreen for the focused window |

Monitor changes center the window on the destination, fit its size to the usable
area, and retain its rotation and fullscreen setting. F10 does nothing when only
one monitor is connected. Keyboard repeats do not repeat setup actions.
Placement, monitor names and rotation save on normal exit to
`portable-data/cabinet.ini` (or your `--data-dir`). This single file also holds
the keys, button mappings and plunger calibration, in the format described under
[Launcher](#launcher). A file in the older format, and the older separate settings
files, are read and kept as backups; normal exit rewrites the configuration in the
current format. A replay keeps its window layout too, and reads nothing else. Missing monitors fall
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
field shows BONUS 1â€“4 instead of the main-table ball counter. Bonus rooms use the
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
to leave that screen, then Enter to resume play. The five ranked scores and
initials automatically save to `portable-data/alien-crush.hiscore` and restore
at the next launch, in both desktop and cabinet modes. Changes save after about
one second of stability, with a final flush on normal exit. No manual save or
configuration is needed. `--data-dir` also selects the high-score location.
Replays use their recorded scores and never read or overwrite this file.
Loading a quick state restores its score table; subsequent autosaves preserve
that table, matching the Devil's Crush behavior.
The cabinet also shows the ranked table during normal-game and bonus-practice
name entry. Name editing remains on the original playfield screen.
After an attract demo finishes, the original high-score table appears on the
playfield and all five rankings appear on the DMD before returning to the title.
During the ROM's brief graphics reload and title reset, after a demo, a normal
game or bonus practice, the playfield stays black and the DMD keeps showing the
rankings, so neither flashes table art or gameplay counters.
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

A cabinet whose buttons arrive as key presses needs none of this: bind the keys
on the launcher's Keyboard tab. For a board that reports its buttons as a
joystick instead, set them under `input.buttons.` in `portable-data/cabinet.ini`
(they are not on a launcher tab) using the observed controller name and button
numbers:

```text
input.buttons.device=PinOne
input.buttons.left_flipper=0
input.buttons.right_flipper=1
input.buttons.nudge=2
input.buttons.run=3
input.buttons.select=4
input.buttons.up=-1
input.buttons.down=-1
input.buttons.right=-1
input.buttons.launch=5
```

`left_flipper` and `right_flipper` send their flipper's first input (see
[Keys](#keys)); `nudge` is II, `run` starts, confirms and pauses and launches
while the ball is seated, `select`, `up`, `down` and `right` are those pad
inputs, and `launch` is a dedicated launch/confirm button (I). **The numbers
above are examples, not verified PinOne defaults.** Use `-1` to disable a
mapping; valid indices are 0–255. The older `input.buttons=ACINPUTS1 "PinOne"
0 1 2 3 4 -1 -1 -1` line (or `ACINPUTS2` with a ninth, launch, index) is still
read, and rewritten in these keys.
The name is a nonempty case-insensitive substring; the first matching device with
all configured buttons is selected. Choose a distinctive name when several boards
are connected. Settings load at startup in `--cabinet` mode.
An empty `input.buttons.device=` leaves raw buttons disabled; malformed settings report an error.
Keyboard controls and other gamepads remain available. The selected board's default
gamepad mapping is suppressed to prevent duplicate actions.

Release all mapped buttons after startup, reconnect, quick-load or returning from
another application before using them. Disconnecting clears held raw input and
reconnecting finds the board by name even if its SDL instance ID changes. Recordings
store the resulting controller bits and accepted plunger commands; replay ignores hardware mappings.
Physical nudging is enabled automatically in `--cabinet` for a connected joystick
whose name contains `PinOne` (case insensitive). Shake the cabinet to trigger the
same nudge as Space / button II. Both directions on the accelerometer's X/Y axes
(0 and 1) trigger this single original-game action; shake strength does not change
the in-game impulse. The plunger's Z axis is ignored. No Steam mapping or separate
configuration file is needed.

Optional overrides go in the existing `portable-data/cabinet.ini`:

```text
input.nudge.enabled=1
input.nudge.device=PinOne
input.nudge.axis_x=0
input.nudge.axis_y=1
input.nudge.threshold=20
input.nudge.release=8
input.nudge.cooldown_ms=250
```

Threshold and release are percentages of the signed joystick range from zero
(32767). Lower `threshold` for lighter nudges, or raise it if flipper/solenoid
vibration triggers nudges. `release` must be below `threshold`. After startup,
focus return, quick-load, reconnect, or each shake, both axes must settle below
`release` for 100ms. Each trigger presses nudge for 50ms; the cooldown prevents
rapid retriggering. Set `enabled=0` to disable physical nudging. Device matching
uses a literal, case-insensitive name substring; axis indices are zero-based.
Restart after changing settings. `--input-probe` shows the raw readings for tuning.
Calibrate the board's centered resting output and sensitivity with the
[PinOne accelerometer tool](https://pinball-docs.clevelandsoftwaredesign.com/docs/PinOne/Configuring/accelerometer/).
The default axes follow the manufacturer's
[joystick mapping](https://pinball-docs.clevelandsoftwaredesign.com/docs/PinOne/steam-calibration/).
Physical shakes are ignored while paused, in menus, and during demos. Recordings
store the resulting nudge button presses; replay ignores live accelerometer input.
Keyboard/button nudging remains available.

Use `--nudge-multiplier 2` to double the force applied to the ball, matching
Devil's Crush's option. For example, run `Launch-Cabinet.cmd --nudge-multiplier 2`
or `alien-crush.exe --cabinet --nudge-multiplier 2`. Decimal factors from 0 to 16
are accepted: 1 preserves the original force, 0 removes the ball impulse, and
0.5 halves it. This applies to keyboard, controller and physical nudges, in both
desktop and cabinet modes. It does not change accelerometer sensitivity, screen
shake, cooldown or tilt penalties. Attract demos retain their original force.
Recordings retain their multiplier automatically; an explicitly conflicting replay
flag is rejected. Quick states require the same multiplier used when saving.

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
is read only while `cabinet.ini` is still in the older format and has no plunger
settings, and the next save moves it into `cabinet.ini`:

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

Cabinet mode always uses the 256Ã—456 full-table view. The former host
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
visible through overlaps, except for the swallowing mouths: their opaque sprite
pixels cover the ball as it enters and leaves, matching the original layering.
Persistent indicator colors and each half's
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
