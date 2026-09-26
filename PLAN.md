# Alien Crush PC and PinOne Cabinet Port

Approved direction: independent Windows x64 C++20/SDL3 project, selectively reusing
Devil's Crush at `95a53cd`. Deliver playable emulation first and migrate to readable
native C++ with no required HuC6280 instruction execution or Beetle linkage at completion.
Preserve original physics, arithmetic, scheduling, RNG, rules, sound, and camera-dependent
activation. The original ROM remains the local source of assets.

## Architecture

Separate host, input devices, asset decoders, game adapters, native game systems,
reference runtime, and diagnostics. A single simulation drives all windows. Desktop
and cabinet rendering consume the same typed game state; display refresh never owns
game time. Raw memory/CPU observation belongs exclusively to reference diagnostics.

The first reference runtime accepts the original pad bits. Once launcher and scene
state are mapped, introduce a semantic input adapter (flippers, launch, normalized
pull/release, nudge, menu actions) shared with the native runtime. Keep the low-level
reference interface available for unmodified controller fidelity tests.

Native domain APIs expose named ball, actor, scene, score, rule, and scheduler state.
Frame events carry scoring, sound, scene and mechanical events. Use original ROM
data as decoded assets, never an instruction representation masquerading as native logic.

## Milestones and exit gates

1. **Trustworthy desktop baseline:** CMake, SDL, source/license provenance, strict
   ROM validation, original video/audio/input, quick saves, deterministic recording,
   and independent Mesen checks. Repeated boot-to-play recordings match RAM, video,
   and audio; observation does not change the clean reference.
2. **State and assets:** map scenes, both table levels, camera transitions, ball,
   flippers, actors, launcher, score, bonuses, RNG, and update epochs. Decode backgrounds,
   sprites, palettes, animation, font, and title graphics into a ROM/hash-versioned local
   cache. Prove mappings with natural routes and separately labeled rare-state fixtures.
3. **Playable cabinet:** continuous full-table view, original bonus/menu presentation,
   DMD score/ball/bonus and idle high scores, static preview then animated-logo backglass,
   missing-display tolerance, persistent display assignment/rotation, configurable buttons,
   raw joystick device selection and PinOne calibration. Map physical draw to recovered
   launcher charge, retaining original velocity calculation. Gate bridge writes on proven
   states. Separate launch/pause/flippers/menu confirmation. Validate a full cabinet session.
4. **Native gameplay:** original clock/input/scheduler/RNG first; launcher, ball,
   flippers, nudge, and collisions next; main-table actors/rules/scoring/transitions;
   then all four bonus rounds and their outcomes. Compare shadow domains at true update
   boundaries, then sustain native ownership across successive updates. No silent fallback.
5. **Standalone native release:** native original-camera and full-table rendering,
   title/menu/HUD/transitions, music driver and sound scheduling, saves and completed
   backglass. Validate native PSG helpers separately. Ship a build without Beetle;
   retain reference emulation as an optional development target. Test clean-machine
   launch, portable updates, long sessions and launcher focus return.

## Cabinet invariants

- Fit the entire table with sharp pixel scaling, correct aspect, and true black borders.
- Both table levels must be correct before a first camera visit, including all flippers.
- Synchronize sprite, scroll, and state capture; handle palette animation, hidden actors,
  dynamic slot reuse, transitions, and effects explicitly.
- Preserve the original off-screen simulation schedule; presentation never advances actors.
  Exception (2026-09-25): cabinet gameplay runs the lower table's aliens in both halves
  natively, so the drawn aliens are the ones the ball hits (docs/CABINET_ALIENS.md).
  Cabinet mode may differ from the original where showing both halves requires it.
- Auxiliary windows do not own input focus or application lifetime.
- PinOne pull/rest endpoints and axis are calibrated; no automatic thumbstick/plunger guess.
- High scores and settings are backend-independent; quick saves are backend/version-specific.

## Validation workflow

The untouched pinned Beetle core is the primary reference. Mesen 2.1.1 provides an
independent check with documented initialization/palette/viewport normalization.
Do not inherit Devil's Crush native-registry goldens: its workflow documents a strict
assembly/registry parity failure. Keep accepted baselines separate from hypotheses.

Record ROM hash, revision/settings, initial state, sampled inputs, and first divergence.
Use isolated processes for concurrent reference instances. Require exact gameplay
state/event ordering and normalized original-camera pixels and sound. Native models
need not recreate inactive stack storage. Validate cabinet remapping separately.

Run fast ROM-free Debug/Release tests per change, targeted Release ROM replays for each
domain, and the full Release corpus at subsystem and milestone gates. Expensive bus traces
are diagnostic. Track behavior coverage and remaining ownership, not routine counts.

Acceptance includes both table levels, all bonus rounds, speed/music options, scoring,
extra balls, drains, game over and rare/end states; palette/actor lifetime transitions;
simultaneous and rapid flippers; weak/full pulls, inverted axes, noise, spring bounce,
hotplug, menus; three monitors, rotation/DPI/refresh changes, missing displays, audio,
focus and prolonged play; deterministic saves, corruption/version rejection, preserved
high scores and ROM-free packaging.

## Scope and delivery

Windows x64; PinOne first; buttons, analog plunger and digital nudge in the cabinet release.
Physical nudge, DOF, lighting and surround-sound feedback are deferred. Animated game-logo
backglass has a user-image fallback. External launcher changes and Devil's Crush changes
are outside this repository. Runtime state stays in machine-local portable-data.
Public distributions exclude ROMs, decoded assets, screenshots, traces and save states;
private staging may explicitly include the user's ROM. Retain all reused source notices.

Only the supplied 262144-byte ROM is initially supported: SHA-256
`1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f`.
Identify region/revision through evidence, not the filename. Estimate later work after
the state/asset discovery gate, rather than promising an ungrounded conversion date.
