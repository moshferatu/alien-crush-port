# Cabinet actor animation audit

## Flying-alien explosions (2026-09-15)

Mouth flyers (type 56) become type 59 on a hit (`01:5650..5666`).
Types 50, 59 and 60 share the finite explosion handler at `02:7880`:
initialize an 8-, 4- or 3-frame sequence, run FD2B with action zero,
then clear all 32 actor fields when the phase returns to zero. The former
inactive animation skipped types 50/59 at action zero and looped type 60.
All three now initialize, animate and expire privately without replaying
their sound effects. RAM 68's scene-clear signal is refreshed from live RAM.
Fixtures cover exact lifetimes and repeated seam crossings after expiration.

## Lower central mouth hold (2026-09-15)

Type 20 (`01:52A1..53F2`) is the large lower-table mouth that releases three
type-56 flyers. Its open pose lasts 600 ticks without calling FD2B, followed
by a single close. The former generic inactive oscillator incorrectly kept
opening and closing it while the ball was in the upper half. Private animation
now follows its opening endpoint, 16-bit hold timer, timed capture reaction,
closing endpoint and nonanimated cooldown. Idle actors cannot start a new
cycle; spawning, ball capture, scoring and sound remain runtime-owned.

Regression fixtures reproduce the former hold failure, check complete and
capture/cooldown cycles, and include type 20 in resumed/departing seam checks.

Audited 2026-09-13 against the supported ROM's actor dispatch tables at
file offsets `9C8E`/`9CE8`, the actors present in the 1,800-frame main-table
capture, and `ActorAssets::advance_animation`. Addresses below are bank:PC.
This is a code audit, not a claim that every possible gameplay event has been
visually exercised. Bonus-stage actors do not need inactive-half animation.

## Green aliens: implemented

- Type 21 (`02:619D`): wait, rise 24 pixels over 48 ticks, animate while
  raised for 120–239 ticks, lower 24 pixels over 48 ticks, repeat. The lowered
  wait uses the original eight-bit random-plus-16 distribution, including
  zero wrapping to a 256-tick wait. Only the raised wait calls FD2B.
- Type 22 (`02:630C`): related variant that rises over 48 ticks and stays
  raised, using FD2B until a real hit changes its actor type.
- Type 25 (`02:63CD`): stationary paired base. After initialization its
  update is just RTS; it must not rise with the moving alien.

Presentation continues initialized actors from their saved position, velocity,
timer and animation phase. Random waits use the private presentation RNG.
Collision handling, forced-rise gameplay triggers, group state, scoring,
spawning and sound remain owned by the runtime. Authoritative actor changes
still replace the cosmetic state. This does not simulate offscreen gameplay.

## Finite reactions and movement: implemented

These are triggered movements or finite reactions, rather than additional
ordinary ambient rise/lower loops. Dedicated handling now finishes their saved
motion and holds the intended endpoint instead of oscillating indefinitely.

| Actors | ROM evidence | Presentation behavior |
| --- | --- | --- |
| Skull guards 14 / 30 | `02:644C..64A2`, `02:64FE..6551`: 32-tick rise/retract using FADA | Finish saved motion, hold while the live enable bit is set, retract and hide when released. Type 14's pending-drain reset still hides it immediately. New activation remains runtime-owned. |
| Rebound actor 9 | `02:669D..6761`: FD2B plus signed step damping and a final idle pose | Dampen on the correct reflection endpoint in either direction, then settle; handle zero-speed states as well. |
| Hit target 10 | `01:51A3..51AE`: return to idle when phase reaches 2 | Stop and retain the final pose. |
| Targets 8 / 37 / 38 / 39 | `01:591F..5999`: select BECF or BF38 from timer, stop at the appropriate reflection endpoint | Select the correct sequence for each variant; stop opening/closing at the original endpoints. |
| Green defeat effects 72 / 73 | `02:6552..65FC`: wait 14 ticks, move for 12 ticks, remove | Initialize newly defeated actors, finish the wait/flight, and clear all actor fields. Type 73 removes its saved type-25 base; a slot reused for another actor is protected. |

The same dispatch scan found transient movement in types 51, 52, 53, 55 and
56 (`01:51B5`, `56AA`, `575D`, `559C`, `5600`). Initialized type 51 now finishes
its two travel legs using the ROM coordinate table at `01:526B`, then returns
to type 10 and settles. Initialized flying types 52/53/55/56 continue their
saved velocity and sprite animation until the ROM bounds or live removal
signal expire them. Spawning, flight initialization, hits and score changes
remain runtime-owned. Synthetic fixtures cover these rare saved states;
the ordinary replay is not evidence that every rare effect crossed a seam.

Slingshot follow-up (2026-09-14): types 40/41 finish their four-pose emergence
(`01:5667/571A`) and become 52/53. Their deterministic launch (`56B6/5769`)
uses direction 20/28 from the ROM velocity tables, applies the eightfold
launch offset, and then flies at half speed. The first flight sprite is
allowed to replace the emergence descriptor. This closes both action-zero
gaps in the earlier animation helper.

Cabinet flight continues beyond the lower viewport using full-table bounds.
The ordinary prediction still uses ROM bounds, allowing an expected native
offscreen removal without deleting the remaining cabinet flight. Hits and
slot reuse still replace the visual actor. Composition projects the clipped
portion into the upper half using the originating sprite patterns and palette.
Tests cover both flyers, emergence/launch, repeated bank switches with native
dispatch and removal, expiration, and visible pixels above the seam.

## Existing coverage and audit limits

Dedicated presentation behavior already covers orange aliens (15), brain-side
orbs (19), bouncing orbs (70/71), orb explosions (60), indicators (42/45/46/47),
flipper mirroring (2/3), and the table's palette scripts (including tentacles
and fleshy borders). Last-chance digits (43) are deliberately runtime-owned.
Mouth capture/release, group controllers and spawn decisions must also remain
runtime-owned; they must not independently swallow or launch an offscreen ball.

Tests cover green cycles and no position drift, type 22 staying raised,
uninitialized actors remaining untouched, movement comparisons against live
ROM frames, visible green pixels while inactive, 640 frames of repeated bank
swaps with identical composed output, and the existing pause/lifecycle checks.
The 1,800-frame replay verifies unchanged runtime RAM, video and audio.

Follow-up on 2026-09-14: seam continuity also requires recognizing the first
or last ordinary runtime dispatch on the crossing frame. This is now predicted
for both actor banks and both palette scheduler halves. Random wait selection
in green/orange aliens and brain orbs is reconciled separately from gameplay
events: only the wait byte may differ at a known transition, while the other
actor fields must match. Orange collision-enable flag transitions now also
match their ROM routine. Tests cover resumed/departing dispatch for 11 actor
types, tentacle/flesh palette phases, differing RNG waits, and authoritative
hits/type changes. Frozen source-bank swaps alone did not expose this defect.

Further real-play verification found that RAM 79 changes before E705/46FD
finish exchanging the banks. The loader/exchange call frames now retain the
previous coherent source banks for all actors and palette slots. The 12,000
frame `green_continuity` route checks green aliens during and immediately after
real transfers in both directions; mixed-bank fixtures cover other finite
actors. Forced green/orange rises through RAM B0 are also predicted without
rewinding private animation. These are separate from ordinary seam dispatch
and randomized wait selection.

`tests/finite_animation_checks.hpp` adds both guard endpoints/release, both
rebound directions and zero speed, both target sequences/open-close endpoints,
defeat delays and positions, paired removal and slot reuse, flying bounds,
target travel destinations and settling. A further 640-tick timeline comparison
crosses the seam every seven ticks and verifies that finite actors neither
restart nor resurrect. Existing rendered seam and pause checks also run.
