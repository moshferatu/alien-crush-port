# High-refresh cabinet animation

The reference is Zombies Ate My Neighbors' `src/layers.h`, `src/smooth.h` and
SDL host: preserve the game tick rate, interpolate positions, and draw separate
layers onto a larger GPU target so fractional console pixels survive.

Cabinet smoothing covers both main-table actor banks, the ball, all four bonus
rooms, title/score-view sprites, palette animations, title background scrolling,
and the backglass logo's color transitions. The DMD's changing numbers remain
instantaneous. Desktop mode retains the original viewport. F6 toggles smoothing;
`--no-smooth` disables it at startup. It activates when the playfield refresh
exceeds 1.1 times the original 59.826 Hz game rate. `--refresh <Hz>` overrides
detection; `--no-vsync` selects timer pacing.

## Layers and identity

The PCE compositor optionally captures the exact background behind sprites and
the SAT owner of every visible pixel. This respects fetch limits, background
priority and raster palette writes. It does not infer ownership from RGB colors.
The normal compositor output is unchanged when capture is enabled or disabled.

Table layers have identities derived from physical table half, actor slot,
actor type and piece index. SAT reordering and actor-bank exchanges therefore do
not reset motion. Interpolation uses the original integer raster endpoints, so
enabling smoothing never adds a fractional RAM offset to a stationary object.
Actor anchors are separate from pose-specific sprite offsets; changing launcher
layouts blends complete poses without sliding them or dropping sprite pieces.
The four bonus-overlay pieces have fixed identities independent of SAT slots,
which change when the flippers select different poses.
The ball retains its continuous world-to-artwork seam mapping. Swallowing mouth
pieces render after the ball; decorative pieces render before it. Off-viewport
slingshot flight, overlays and match digits retain their existing presentation.

Bonus layers use actor/type/piece identities decoded from the RAM belonging to
the displayed SAT snapshot (one snapshot behind live state). Each reconstructed
sprite must exactly match all four SAT words before receiving an actor identity
and pose anchor. This keeps segmented enemies attached to their own animation
history when flipper poses shift the sprite slots. HUD slots remain fixed;
unverified dynamic sprites use a frame-local identity rather than interpolating
against an unrelated occupant. Menu layers retain their original SAT identity.
Pixels affected
by raster changes that frozen registers cannot explain remain in the original
background image, preserving them exactly rather than guessing their sprite
ownership. Those residual pixels still receive temporal color/pose blending.

## GPU presentation

Regression coverage for alignment and flashing includes exact Windows GPU
comparisons of smoothing on/off at game-frame endpoints during launcher and
flipper operation; a changing-origin, two-piece-to-one-piece launcher fixture
at every quarter frame; fixed bonus-overlay identities across the replay; and
integer endpoint checks for the ball and all table sprites. These checks pass
with both SDL's software renderer and the Windows GPU renderer.
The four bonus-room routes additionally check unique actor identities through
real sprite reordering: 479, 219, 380 and 527 persistent sprite identities across
slot changes, respectively. Nine room-1 sprite samples use the conservative
fallback; all other visible dynamic sprites in these intervals match exactly.

Backgrounds and sprite atlases upload once per simulation tick. Extra refreshes
do not advance the emulator, input, audio, palette scripts or private actor
clocks. Each refresh draws the previous and current poses at the same interpolated
positions, then blends those two opaque scenes. This gives correct transparent
pose edges without drawing moving sprites at two separated old/new positions.
Background palette changes and background-drawn mouths blend too. Pixel art
poses are blended, not converted into newly drawn in-between artwork.

Flippers are the exception, on the table (both halves) and in the bonus rooms.
The game draws a flip as four poses, one per tick, so a crossfade only blurs
each step. Instead, between ticks the current pose is drawn turned back toward
the previous pose's angle about the flipper's pivot, through
`degrees * (1 - fraction)`, so the flip is a continuous swing and every tick
still shows the game's own pose exactly. `flipper_swing()` measures each step
from the pixels of actor types 2 and 3: the turn is the change in the long axis
of the flipper's drawn pixels, and the pivot is the one point that turn leaves
in place. The three steps measure about 22, 24 and 24 degrees, with the pivot
inside the round end. A step is accepted only when the flipper stayed put, the
turn is between one and sixty degrees, the pivot lies on the flipper's own
pixels and the previous pose, turned, lands at least 80% on the current one;
otherwise the pose is shown as drawn.

At 240 Hz that is four angles per tick, each a sharp flipper about six degrees
from the last, so each picture also draws a fading trail of the angles swept
since the previous picture. The trail reaches back into the previous tick only
when the flipper was already turning the same way. A picture at the tick itself
with no picture before it has no trail and is pixel-exact. The turn is applied
in console pixels before scaling. Collisions still use only the game's poses.

The title's 96-pixel repeating background uses wrapped scroll deltas and padded
textures translated on the GPU. It does not dissolve between displaced patterns.
At least four target pixels per source pixel preserve fractional movement before
the existing sharp downsample, rotation and cabinet alignment.

Missing frames, scene/size changes, quick-load/focus resets and moves exceeding
32 pixels discard motion history. Removed entities disappear immediately; new
entities start at their current position. Pausing uses the existing frozen table
image and resets interpolation. Scene-entry blanking remains authoritative, so
bonus and score screens cannot blend an outgoing table into their first frame.
Interpolation adds up to one game tick of visual latency and never predicts
beyond known game state.

## Verification

`motion_smoothing` checks independent fractional actor motion, pose color mixing,
transparent edges, foreground occlusion, scene changes/removal/reset, title-scroll
wrap and 120/144/165/240 Hz fractions. It checks a flipper's measured turn and
pivot, its halfway angle, trail and exact pose on its own tick, and that the
table's flippers (120 swings) and each bonus room's (30) swing on the real-ROM
routes. It reconstructs 571 sampled real-ROM table
backdrops pixel-for-pixel across a seam crossing, checks all four bonus rooms and
title frames against their original viewport, and verifies rendering leaves RAM
unchanged. GPU screenshots are generated in `smoothing-previews`.

Graphics, mouth layers, cabinet windows, inactive animations and bonus/high-score
transition regressions also pass. Software and Windows GPU renderer checks are
run separately. Intermediate alpha blends can round by one RGB level on SDL's
software renderer; original frame endpoints remain exact.

A short timer-paced Windows GPU replay at requested 240 Hz submitted 418 pictures
for 120 simulation ticks with the expanded renderer. Table composition averaged
about 3.77 ms, compared with about 2.44 ms for the earlier ball-only renderer.
These are local measurements, not a guarantee of the cabinet's scanout rate.
VSync, compositor/driver scheduling, other displays and GPU capacity affect the
result. The exit counter reports submitted pictures, not confirmed scanouts.
