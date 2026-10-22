# Setup and configuration

Controls, settings and cabinet hardware for the Alien Crush PC port. The
[README](../README.md) has the overview.

## Launcher

`alien-crush-launcher.exe` sits beside the game and is the easy way to set it
up: `alien-crush.ini` a tab at a time, and a Play button that saves
and starts the game, then comes back when the game closes.

| Tab | |
| --- | --- |
| Game | The ROM, cabinet or desktop mode, a starting bonus stage, and a start near the maximum score |
| Video | Smoothing, and for the cabinet: which monitor each window is on and whether it is fullscreen, and the playfield's rotation |
| Audio | The music voice overlay |
| Cabinet | The nudge strength and nudge board, and the analog plunger |
| Keyboard | The keys for each PC Engine pad input, and the window hotkeys |

It is worked by mouse, keyboard or controller: Up and Down move, Left and
Right change a value, Enter opens a list, starts editing, or waits for a key, Tab (or L1 and R1) turns the tabs, Ctrl+S saves, and
Ctrl+Enter, F5 or Start plays. The ROM can also be chosen with Browse or
dropped on the window. A click on the launcher acts even when another window
has the focus.

With no options the game plays as `alien-crush.ini` says, which is
what the launcher edits: `game.rom`, `game.cabinet`, `game.bonus_stage`,
`game.near_max_score`, `game.drag_ball`, `audio.music_voice_overlay`, `video.smoothing` and `input.nudge_multiplier`,
beside the window, key, button, nudge and plunger fields described below. An
option on the command line beats the file for that run (`--rom`, `--cabinet`
or `--no-cabinet`, `--bonus-stage`, `--near-max-score`, `--drag-ball`, `--music-voice-overlay` or `--no-music-voice-overlay`,
`--smooth` or `--no-smooth`, `--nudge-multiplier`), and replays and `--frames`
runs ignore these eight, though `--bonus-stage` and `--near-max-score`
themselves still work with `--frames`. F6 is remembered in `video.smoothing` when a cabinet session exits
normally.

## Start options

To start a game directly in one of the four bonus stages, add `--bonus-stage`
with a stage number from 1 through 4, or choose Starting Bonus Stage in the
launcher. This works in both desktop and cabinet modes and uses the game's
normal bonus-stage transition, taken with the first ball from the shooter: the
stage's score counts, and when it ends the ball returns to the table and the
game goes on with all three balls.

```powershell
alien-crush.exe --bonus-stage 3 --rom "D:\path\to\Alien Crush.pce"
```

The score stops at 999,999,900, and reaching it ends the game: the table blows
up, a congratulations screen follows, and the score takes first place in the
table. To see that without earning it, add `--near-max-score`, or turn on Start
Near Maximum Score in the launcher. A new game then starts with its first ball
in the shooter and 999,999,000 points, 900 short, which a few bumpers make up.
It is an ordinary game from there, so the score is saved to
`alien-crush.hiscore` like any other; add `--data-dir` with a scratch folder to
keep your own table. It combines with `--bonus-stage`.

```powershell
alien-crush.exe --near-max-score --data-dir "D:\scratch\alien-crush-test"
```

To put the ball where a table event needs it, add `--drag-ball`, or turn on
Drag Ball With Mouse in the launcher. The mouse pointer then shows over the
game, and while the left button is held on the playfield the ball is carried
under it, on the table and in the bonus stages, in desktop and cabinet modes.
The ball keeps the pointer's motion when the button is let go, so it can be
thrown. The game's own rules still run: the ball changes table halves by the
game's crossing (in desktop mode the other half loads, as it does in play), and
holding it inside a wall or a flipper gets it pushed out again. It is a cheat
for testing, so scores made with it are saved like any others, and it is not
available while recording or replaying.

## Settings file

`alien-crush.ini` is in Devil's Crush's format, with Devil's Crush's keys wherever
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
each window (Devil's Crush has only the playfield's). The file before this
format, with its `window.*` lines, and the separate `cabinet-windows.txt` and
`cabinet-plunger.txt` before that, are still read; the launcher's Save, or a
cabinet session's exit, rewrites them in this format. Earlier builds also read
a board's raw buttons from `input.buttons.*` (or `cabinet-inputs.txt`); those
are now ignored, and dropped on the next save.

## Controls

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
| F6 | Toggle high-refresh cabinet animation smoothing |
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
on the launcher's Keyboard tab, or under `input.` in `alien-crush.ini`.
Each of the eight PC Engine pad inputs (`input.one`, `two`, `select`, `run`,
`up`, `right`, `down`, `left`) takes up to four keys by their SDL names,
comma-separated; an empty value leaves the input unbound. A name SDL does not
know is reported and skipped, and the input's other keys still work.

```text
input.one=Return
input.run=Return
input.left=Left Shift,Left
input.right=Right Shift,Right
```

The keys bound to I launch: while the ball is seated in the shooter they send
I alone, so a key that is also Run launches there rather than pausing, and a
held launch stays one until it is let go. The flippers are Left and Right, so
they are whatever keys those two are bound to. The right flipper sends Up
while both are held, since the pad cannot send Left and Right at once.
Gamepad shoulders send Left and Right.
Earlier builds had `input.launch`, `input.left_flipper` and
`input.right_flipper` settings; they are now ignored.

## Files

Everything the game keeps between runs sits beside the executable:

| File | What it holds |
| --- | --- |
| `alien-crush.ini` | The launcher's settings, window placement, key and button bindings, and plunger calibration |
| `alien-crush.hiscore` | The high-score table |
| `alien-crush.desktop-controls.state`, `alien-crush.cabinet-controls.state` | The F5/F9 quick states |
| `backglass.bmp` | Your own backglass artwork, if you add any |

Earlier builds kept these in a `portable-data` subfolder, with the settings
named `cabinet.ini`. The game and the launcher move them out on first start,
and remove the folder if nothing else is left in it. A folder chosen with
`--data-dir` is used as it is, so rename a `cabinet.ini` there by hand.

Record with `--record session.acrec`, replay with `--replay session.acrec`.
Recordings embed the initial state and every sampled controller input with an integrity hash.
Replay also supports per-frame plunger draw/cancel commands (`ACREC002`); pad-only
recordings retain `ACREC001`. Commands run before the frame and a rejected launcher
operation stops playback. Both the SDL host and diagnostic tool support this format.
Live cabinet recordings include accepted plunger commands when analog input is configured.
Quick-state operations are disabled during recording and replay to retain a linear timeline.

## Cabinet windows

For the three-window cabinet host with a stitched playfield, add `--cabinet`. The score and
backglass windows float: they stay above the playfield, even a fullscreen one, and never take the
keyboard, which stays with the playfield. Move and resize them onto your monitors, or use these
keys, which act on the selected window (the playfield to begin with):

| Cabinet key | Action |
| --- | --- |
| F10 / Shift+F10 | Select the next / previous window: playfield, score, backglass |
| F11 | Toggle fullscreen for the selected window |
| F12 / Shift+F12 | Move the selected window to the next / previous connected monitor |
| R / Shift+R | Rotate the selected window clockwise / counterclockwise by 90 degrees |

F6, F10, F11 and F12 can be rebound on the launcher's Keyboard tab (`input.smoothing`,
`input.next_window`, `input.fullscreen` and `input.next_monitor` in `alien-crush.ini`). Only
one window can be fullscreen on a monitor, so F11 does nothing while another cabinet window
fills that monitor, and F12 passes over such a monitor for a fullscreen window.

Monitor changes center the window on the destination, fit its size to the usable
area, and retain its rotation and fullscreen setting. Placement, monitor names and
rotation save on normal exit to `alien-crush.ini`.

## Backglass artwork

Put your artwork at `backglass.bmp` beside the executable (or in
your `--data-dir`) to display it on the backglass. Use an uncompressed 24-bit or
32-bit Windows BMP, at most 4096x4096 pixels and 64 MiB; pixels are opaque.
Artwork loads at startup, fits without stretching or cropping, and honors saved
backglass rotation. Black borders fill unused space. Restart to change artwork.

## Cabinet buttons

A cabinet's buttons need to arrive as key presses or as a gamepad: bind the keys
on the launcher's Keyboard tab. A gamepad's shoulders work the flippers, its
south button launches and confirms, and Start starts and pauses.

To find a board's axes for the plunger and nudging, run the hardware probe (no
ROM required):

```powershell
.\out\build\windows-vs2022\Release\alien-crush.exe --input-probe
```

The window and console show controller names, zero-based pressed button numbers,
and raw axis readings. Pull the plunger to identify its axis and rest/full-pull
endpoints. Escape closes the probe. `--frames N` limits it to N samples at 100ms
intervals. Recordings store the resulting controller bits and accepted plunger
commands; replay ignores hardware settings.

## Nudging

Physical nudging is enabled automatically in `--cabinet` for a connected joystick
whose name contains `PinOne` (case insensitive). Shake the cabinet to trigger the
same nudge as Space / button II. Both directions on the accelerometer's X/Y axes
(0 and 1) trigger this single original-game action; shake strength does not change
the in-game impulse. The plunger's Z axis is ignored. No Steam mapping or separate
configuration file is needed.

Optional overrides go in the existing `alien-crush.ini`:

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

## Plunger

Plunger setup accepts the same settings as Devil's Crush. Copy the working
`input.plunger.*` lines from that game's `devils-crush.ini` into Alien
Crush's existing `alien-crush.ini`, alongside its window and button
settings. No separate plunger configuration file is required.
Alternatively, read the existing file directly without copying or modifying it:

```powershell
.\alien-crush.exe --cabinet --plunger-config 'D:\Path\To\DevilsCrush\devils-crush.ini'
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
settings to paste into `alien-crush.ini`; it does not change existing calibration.
`--input-probe` remains available for the live raw button/axis display.

Restart after editing. Explicit `--plunger-config` takes precedence, followed by
the data directory's `alien-crush.ini`. The older `cabinet-plunger.txt` format below
is read only while `alien-crush.ini` is still in the older format and has no plunger
settings, and the next save moves it into `alien-crush.ini`:

```text
ACPLUNGER1
"PinOne"
2 0 32767 8 25
```

The five numbers are **axis index, rest value, full-pull value, deadzone percent,
release-fall percent**. These are examples; measure your board with `--input-probe`.
Endpoints may run in either direction and must be distinct values in -32768..32767.
The deadzone is 0..99; release threshold is 1..100. Choose a specific device name
substring in the legacy format; the first matching board with that axis is used.
Missing configuration disables analog input; invalid
configuration reports an error without overwriting it. Restart after editing.

The rod must rest after startup, reconnect and quick-load. A seated ball follows
the calibrated draw and fires through the original game code when the rod releases.
During a rod-controlled draw, I/II from buttons or keyboard cannot change the shot.
Pause cancels the draw. Focus loss or disconnect queues cancellation before the
next emulated frame; an unfocused game stays paused until focus returns. After
cancellation or release, rest the rod and release I/II before another shot.
F5 quick-save requires releasing an owned draw first; F9 discards the old input
ownership. Replay reproduces accepted commands without needing the device.

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
/alien-crush.ini*
/alien-crush.hiscore*
/backglass.bmp
/cabinet-*.txt
/portable-data
/logs
/captures
/traces
*.state*
*.acrec
```

The script creates the workstation's `.stignore`, retaining existing custom
rules and including the packaged `syncthing-ignore.txt`. Once that file exists
on the cabinet, `#include syncthing-ignore.txt` can replace the patterns above.
[Syncthing does not sync `.stignore` itself](https://docs.syncthing.net/users/ignoring.html),
so set up the cabinet's exclusions separately. `CABINET-SETUP.txt` in the package
contains these instructions. Monitor layouts, input/plunger bindings, custom
backglass artwork, high scores and quick states sit beside the executable and
stay machine-local; copy any desired bindings or artwork to the cabinet manually
once. A cabinet set up with the earlier patterns needs the new ones before it
runs this build, or its settings will sync back to the workstation.
