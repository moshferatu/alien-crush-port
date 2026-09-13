# Cabinet actor animation audit

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

## Other confirmed gaps: follow-up work

These are triggered movements or finite reactions, rather than additional
ordinary ambient rise/lower loops. They need dedicated handling; applying the
generic oscillator to them indefinitely would also be incorrect.

| Actors | ROM evidence | Current presentation limitation |
| --- | --- | --- |
| Skull guards 14 / 30 | `02:644C..64A2`, `02:64FE..6551`: 32-tick rise/retract using FADA | A guard saved mid-motion does not finish moving privately. Type 14's existing pending-drain reset does hide it immediately. Activation and release are gameplay-controlled. |
| Rebound actor 9 | `02:669D..6761`: FD2B plus signed step damping and a final idle pose | Generic FD2B omits damping and settling after a hit. |
| Hit target 10 | `01:51A3..51AE`: return to idle when phase reaches 2 | Generic animation lacks this stop condition. |
| Targets 8 / 37 / 38 / 39 | `01:591F..5999`: select BECF or BF38 from timer, stop at the appropriate reflection endpoint | Need the selected sequence and finite opening/closing states; the generic type table is insufficient. |
| Green defeat effects 72 / 73 | `02:6552..65FC`: wait 14 ticks, move for 12 ticks, remove | Saved effects need finite motion/removal rather than a frozen or repeating sprite. Type 73 can also remove its paired actor; that relationship needs a fixture before implementation. |

The same dispatch scan found transient movement in types 51, 52, 53, 55 and
56 (`01:51B5`, `56AA`, `575D`, `559C`, `5600`). These include type changes,
spawned effects and bounds/removal logic. Their reachability across a main-table
handoff needs targeted recordings; they are not verified fixed by this change.

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
