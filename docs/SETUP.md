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
| Keyboard | The keys for each PC Engine pad input, the launch keys, and each flipper's pad inputs |

It is worked by mouse, keyboard or controller: Up and Down move, Left and
Right change a value, Enter opens a list, starts editing, or waits for a key, Tab (or L1 and R1) turns the tabs, Ctrl+S saves, and
Ctrl+Enter, F5 or Start plays. The ROM can also be chosen with Browse or
dropped on the window. A click on the launcher acts even when another window
has the focus.

With no options the game plays as `alien-crush.ini` says, which is
what the launcher edits: `game.rom`, `game.cabinet`, `game.bonus_stage`,
`game.near_max_score`, `audio.music_voice_overlay`, `video.smoothing` and `input.nudge_multiplier`,
beside the window, key, button, nudge and plunger fields described below. An
option on the command line beats the file for that run (`--rom`, `--cabinet`
or `--no-cabinet`, `--bonus-stage`, `--near-max-score`, `--music-voice-overlay` or `--no-music-voice-overlay`,
`--smooth` or `--no-smooth`, `--nudge-multiplier`), and replays and `--frames`
runs ignore these seven, though `--bonus-stage` and `--near-max-score`
themselves still work with `--frames`. F7 is remembered in `video.smoothing` when a cabinet session exits
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
area, and retain its rotation and fullscreen setting. Placement, monitor names and
rotation save on normal exit to `alien-crush.ini`.

## Backglass artwork

Put your artwork at `backglass.bmp` beside the executable (or in
your `--data-dir`) to display it on the backglass. Use an uncompressed 24-bit or
32-bit Windows BMP, at most 4096x4096 pixels and 64 MiB; pixels are opaque.
Artwork loads at startup, fits without stretching or cropping, and honors saved
backglass rotation. Black borders fill unused space. Restart to change artwork.

## Cabinet buttons

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
joystick instead, set them under `input.buttons.` in `alien-crush.ini`
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
