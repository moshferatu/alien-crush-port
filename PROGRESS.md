# Progress

Qualifying score and name entry (2026-09-08):

- Found a controller-only continuation of the room-3 clear route that qualifies
  with 1,505,700 points at frame 9656. It uses nine-frame flipper pulses before
  releasing the pad at 1,100,000; original results scoring supplies the rest.
  No score, name, ROM or game-state bytes are patched to create the result.
- Verified original first-place insertion, character changes, decrement wrap,
  I advancement and confirmation. Name bytes 41/5B/41 and the new score survive
  title return and starting another three-ball game, with the other four ranks
  and names unchanged. Alternate confirmation/back controls remain untested.
- Added the verified practice game-end call signature to the cabinet score
  observer. The score window now shows rankings throughout its 280-frame call,
  including name entry and confirmation; it previously showed the candidate
  score field after the sort had replaced it with a displaced score. The
  original playfield continues to handle name editing. Preview inspected.
- Added exact frame-9400 restore through 10500 (RAM/video/audio and score-panel
  parity) and an 1100-frame three-window host replay. Automatic disk persistence,
  ordinary-game qualifying entry, other bonus clears and physical cabinet
  acceptance remain pending.
- Validation: all 6 targeted Release checks pass (graphics, bonus fixture,
  name-entry replay/host and paused score-viewer replay/host), plus all 3
  core-only Debug tests. Builds have no warnings and `git diff --check` is clean.

Paused high-score viewer (2026-09-08):

- Added a five-rank score-window layout for the original Run -> Select viewer.
  The observer uses the complete paused EBE6 call signature and mode 7A=1;
  that mode remains set after exit, so checking it alone would leave the score
  window stuck on rankings. Title/demo retain precedence. The original playfield
  viewer retains its names, while the cabinet panel shows the five scores.
- Added a controller-only replay: pause at 601, open scores at 651, leave at
  901, then resume play. It covers 250 viewer frames, original-view fallback,
  resumed stitching and unchanged score/name bytes. Frame-600 restore through
  1200 reproduces RAM/video/audio and the score panel exactly. A new 600-frame
  three-window host replay covers the same path. Both previews were inspected.
- Read Devil's Crush's HighScoreAutosave and traced Alien Crush's boot table,
  six-entry ranking/swap and name-editing routines. The five defaults are all
  1,000,000; existing natural practice probes reached only 753,900. Qualifying
  score/name-entry coverage and automatic persistence remain pending, along
  with other bonus clears and physical cabinet acceptance.
- Validation: 9 focused Release checks pass (graphics, viewer/host, ordinary
  bonus, match, game over, bonus results, attract and title), plus all 3
  core-only Debug tests. Builds have no warnings and `git diff --check` is clean.

Successful match award and return to play (2026-09-08):

- Corrected the earlier "continue selection" description: the original branch
  is a score-digit match. I/II can stop the spin; a match grants another ball.
  Traced 03:83CC's score remainder comparison, 83E9's award flag, 9196's ball
  increment and 01:4153's return to ball setup. No game state is patched.
- Added a controller-only winning route with 21,700 points: last ball consumed
  at 4785, one ball awarded at 4874, ending flags cleared at 5311. The game-over
  heading clears on the award, and stitched play and ball movement resume.
  Frame-4800 restore through 5700 reproduces RAM/video/audio and the score panel
  exactly. A generated 900-frame replay exercises all three host windows.
- Found a moving-score digit upload mismatch at frame 4792. The available VRAM
  rows do not capture the sprite pattern fetch latch, so visible moving digits
  during a digit upload now use the original viewport without a stale hold.
  This conservative guard leaves full sprite fetch timing recovery pending.
  All 1,282 stitched frames in the tested interval match the active original
  raster outside intentional HUD removal and seam; 375 occur after return.
- Visually checked the awarded-ball score panel. High-score entry/persistence,
  other bonus clears and physical cabinet acceptance remain pending.
- Validation: all 30 Release tests and 3 core-only Debug tests pass. Original
  replay/display parity remains exact; builds have no warnings and
  `git diff --check` is clean.

Ordinary game over and HUD scanline limits (2026-09-08):

- Added GAME OVER presentation after the final ordinary ball is consumed,
  preserving the 22,200-point final score in the covered route. The observer
  follows RAM 78 bits 0/1 with zero balls and excludes practice/demo/bonus scenes.
  Title/demo layouts retain precedence. The score preview was visually inspected.
- Extended the ordinary bonus-return route through three drains at frames
  2845/3760/4732, 771 game-over frames and the title at 5517. Frame-4700 restore
  through 5700 reproduces RAM/video/audio and the game-over score panel exactly.
  A new 1000-frame three-window replay covers the final drain through title return.
- Fixed a rendering mismatch found at frame 4952: removing HUD SAT entries had
  freed scanline fetch slots and revealed sprites the original console dropped.
  HUD and ball-backdrop suppression now preserves fetch cost using a hidden-slot
  mask. ROM-free tests cover wide sprites, overflow/block masks, slot 63 and raster
  propagation. All 964 stitched frames in the covered end interval match the
  original active raster outside the intended HUD removal and seam.
- Continue selection, high-score entry/persistence, other bonus clears and
  physical cabinet acceptance remain pending.
- Validation: all 28 Release tests and 3 core-only Debug tests pass. Original
  replay/display parity remains exact; builds have no warnings and
  `git diff --check` is clean.

Ordinary bonus return and table fade presentation (2026-09-08):

- Added a controller-only ordinary-play route: room 2 starts at frame 854 and
  returns to the main table at 2199 with 14,400 points and all three balls.
  Practice and demo flags stay off. The route covers 1,345 bonus-scene frames.
- Verified the ordinary FF04 return: the restored lower bank contains the ROM's
  19 initial actor types and its 1,920 actor bytes stay dormant until the next
  lower-table handoff. Both main-table halves subsequently resume stitched play.
- Fixed the partial return fade exposing a fully lit inactive half beside a dark
  active half. Exact known table fade-in palettes now keep the original viewport
  until the fade settles. This reuses the trace-verified palette fade helper and
  changes presentation only. The first stitched return is now frame 2243; its
  complete table preview was visually inspected.
- Through frame 2800, 386 lower and 112 upper returned stitched frames match the
  original active raster outside the intended HUD removal and seam. Frame-800
  restore reproduces final RAM/video/audio and the first stitched return exactly.
  A new 300-frame three-window replay covers the results-to-main-table transition.
- The canonical table replay now yields 1009 lower and 253 upper stitched frames;
  30 initial fade frames use the original viewport. The 150 transition holds and
  75/75 ball updates are unchanged. Broader ordinary routes, other bonus clears
  and physical cabinet acceptance remain pending.
- Validation: all 26 Release tests and 3 core-only Debug tests pass. Original
  replay/display parity remains exact; builds have no warnings and
  `git diff --check` is clean.

Bonus results tally and successful room-3 clear (2026-09-08):

- Added BONUS CLEAR presentation using the original results flag (RAM 85 bit 0)
  in known bonus rooms. Score and room stay visible, with title/demo precedence.
  The generated ROM-digit preview was visually inspected.
- Traced FED5 -> EE86 -> 1D:400C -> 1F:946E. Completion is checked by 970B
  after the results tally, and 9736 sets the flag. A zero flag during the earlier
  ROUND OVER pause alone does not establish failure; the previous route's ball
  consumption confirms its losses. Updated the assertion and state notes.
- A controller-only room-1 route ends play at frame 2836 with 13,200 points,
  then gains 80,000 in five tally changes before entering room 2 at 3244 with
  two balls. Frame-2800 restore through 3300 reproduces RAM/video/audio exactly.
- A controller-only room-3 route sets the clear flag at frame 4974 with 448,200
  points, shows the flag for 241 frames and reaches room 4 with all three balls.
  Frame-4800 restore through 5500 reproduces RAM/video/audio and the clear panel
  exactly. A new 700-frame three-window host replay covers this result transition.
- Other room clears, ordinary-play bonus entry/return and physical cabinet
  acceptance remain pending. No ROM or emulated RAM patches were used.
- Validation: all 24 Release tests and 3 core-only Debug tests pass. The original
  replay/display parity remains exact; builds have no warnings and
  `git diff --check` is clean.

Bonus round endings and practice exit (2026-09-08):

- The score header shows ROUND OVER when a known bonus room has RAM 78 bit 0
  set. Original FDDF exits the bonus loop on that bit; FEC0 retains the scene for
  150 frames before FED3 clears the flags. Cumulative score and room number stay
  visible. Title/demo layouts retain precedence; a preview was visually inspected.
- Extended the first practice route to 3,000 frames. Failed rounds end at frames
  740, 1463 and 2404, each yielding 150 round-over frames. Practice advances rooms
  and consumes balls at 1064/1787, exhausts balls at 2728, and returns to the title
  at 2877 with bonus presentation cleared. RAM 85 bit 0 is clear at all three
  endings; these are failed rounds, not claimed clears.
- Restoring the frame-450 checkpoint and replaying through frame 3000 reproduces
  final RAM, video and audio exactly. The other three room routes retain their
  existing entry/render/restore coverage. A new 300-frame three-window recording
  spans the first round's final play, round-over interval and subsequent fade.
- Successful bonus-room clears, ordinary-play bonus entry/return and physical
  cabinet acceptance remain pending.
- Validation: the extended bonus replay and new ending host test pass, as do the
  other 21 Release tests and 3 core-only Debug tests. Replay/video parity remains
  exact; builds have no warnings and `git diff --check` is clean.

Bonus-room cabinet presentation (2026-09-08):

- The score panel now shows BONUS 1..4 in place of BALL while retaining the
  original cumulative score and high score. RAM 79=2 and RAM 83=0..3 identify
  the room. Title/demo layouts retain precedence. A preview was visually inspected.
- Original FDE5 temporarily uses 83+2 to load room actors through E942, then
  FE23 restores shared table code 2. This is presentation mapping only.
- Added controller-only bonus-practice replay coverage through the original
  01:4444 selector: Up 3, Down 6, Left 8, Right 3 title presses, then I 0..3 times
  selects the starting room. Canonical Run presses advance the menus. No RAM or
  ROM modifications are used; the practice path remains original game behavior.
- All four routes enter the requested room by frame 450 and cover 562 bonus
  frames through frame 900. Frame-450 restoration reproduces final RAM, video and
  audio exactly. Rooms 1..3 award points in this route; room 4 does not, so its
  coverage establishes entry/presentation/restore, not scoring or completion.
- Confirmed bonus scenes reject main-table stitching and clear a held table image.
  Four short recordings and score previews are generated locally; a three-window
  host test plays the room-4 fixture in stitched mode using the original viewport.
- Ordinary-play bonus entry/return, complete round outcomes and physical cabinet
  acceptance still require broader coverage.
- Validation: both new bonus tests and all 20 existing Release tests pass, along
  with 3 core-only Debug tests. Replay/video parity remains exact; builds have no
  warnings and `git diff --check` is clean.

Attract-mode score presentation (2026-09-08):

- Demo setup/play now displays the leading high score, DEMO and PRESS RUN, hiding
  simulated score and ball values. Title-menu presentation takes precedence during
  the outgoing title fade. A local preview was visually inspected.
- The observer follows original RAM 76's nonzero branch at 01:4009. Title timeout
  sets 76=1 at 1C:404E/4050; 01:4021 increments it to 2 before the demo loop.
  This selects presentation only and does not change original demo controls.
- A 6,000-frame neutral-input test covers one complete attract cycle: title at
  15, timeout selector at 1545, demo setup at 1563, active demo at 1609, selector
  reset at 5513 and title reentry at 5526. Restoring frame 1800 and advancing to
  6000 reproduces RAM, video and audio exactly. Run exits demo to the title.
- Layout tests cover stale-field hiding, the DEMO label, title precedence and
  the original nonzero selector semantics. Bonus-specific scoring presentation
  and physical cabinet acceptance remain pending.

- Validation: the new attract replay and all 19 existing Release tests pass,
  along with 3 core-only Debug tests. Canonical replay/video parity remains exact;
  builds have no warnings and `git diff --check` is clean.

Title-menu score presentation (2026-09-08):

- The score window now shows the leading high score and a host-authored
  "PRESS RUN" prompt during the title/selection menus, retaining original HUD
  digits. It hides the inactive score and ball counter, while gameplay retains
  its existing layout. A local preview was visually inspected.
- Menu detection requires the complete nine-byte outer call-stack signature at
  RAM 1F7..1FF: EB0C return, saved MPR6..2 = 18/04/03/02/01, and 4005 return.
  These come from 01:4003 JSR EAEB, five MPR pushes, and 00:EB0A JSR 4000.
  This spans title menus and fades; it is presentation evidence, never a gate for
  changing game state. No cached scene flag survives a quick-load.
- Canonical frames 15..318 select the menu panel; boot and subsequent game setup
  do not. Tests cover all nine corrupted signature bytes, zero RAM, fixed-size
  maximum high scores, hidden stale fields, menu restore and exact RAM after
  advancing the restored recording back into game setup.
- Bonus/demo-specific score layouts and physical cabinet acceptance remain.
- Validation: all 19 Release and 3 core-only Debug tests pass, including canonical
  replay/video parity. Builds have no warnings and `git diff --check` is clean.

ROM-derived default backglass (2026-09-08):

- Missing `backglass.bmp` now displays the original Alien Crush title actor B9,
  decoded directly from the supported local ROM. The 192x144 logo uses the initial
  blue title palette on black, with 5:4 cartridge pixel aspect (5:3 display aspect).
  Local BMP overrides retain square pixels, saved rotation and existing validation.
- Traced 00:EAEB -> 1C:4055 title initialization and 00:EC58 -> 12:400A/4025.
  The sprite stream at ROM 24125 yields 8192 words at VRAM 4000; palette 38BD0
  supplies 512 colors. Actor B9 at ROM 2D1F2 has thirteen pieces; settled actor
  X=128/Y=78 comes from 1C:42DD/4309. No emulator is advanced to create artwork.
- In the first 300 canonical frames, 283 match the loaded sprite stream, 194
  match the initial sprite palette, and 128 match the complete cropped logo
  reconstructed from original SAT/VRAM with that initial palette. The last check
  intentionally removes live palette animation. A local PNG preview was inspected.
- This replaces the plain-title fallback. Bonus/idle score presentation and
  physical cabinet acceptance remain pending.
- Validation: all 19 Release tests and 3 core-only Debug tests pass, including
  canonical replay/video parity. Builds have no warnings; diff checks are clean.

Live cabinet display recovery (2026-09-08):

- The host now coalesces SDL display-change events and recovers all three windows
  before the next game frame. Windows already fitting a display stay unchanged;
  partial windows use the display with the greatest overlap, while fully detached
  windows use the role's connected-display fallback. Oversized windows shrink to
  fit. Fullscreen uses full display bounds, windowed mode uses usable bounds.
- Recovery preserves rotation and fullscreen, does not explicitly raise windows,
  and does not move visible windows back when a monitor reconnects. F10 provides
  explicit reassignment. Temporarily missing displays defer frame advancement
  while events remain responsive; exiting then retains the last saved layout.
- Display events clear queued audio and rearm cabinet controls. Owned analog
  draws follow the existing cancellation path before the next emulated frame.
  These host actions do not add gameplay frames to live or recorded timelines.
- Synthetic geometry cases cover three monitors, negative origins, greatest
  overlap, detached windows, reduced resolution, oversized windows, no displays,
  invalid bounds and coordinate overflow. SDL dummy tests recover all three
  displaced windows and check idempotence, fullscreen and rotation persistence.
- Physical unplug/reconnect, focus behavior and fullscreen migration still need
  cabinet acceptance. ROM-derived default artwork and bonus/idle presentation
  remain pending.
- Validation: all 18 Release tests pass, including canonical replay/video parity.
  The suite was interrupted after 17 passing tests; the remaining cabinet smoke
  test passed separately. Release build has no warnings; diff checks are clean.

Cabinet keyboard window setup (2026-09-08):

- Added F6/Shift+F6 to select the next/previous cabinet role and F10/Shift+F10
  to move the focused window between connected monitors. Monitor cycling wraps,
  centers and fits the window, preserving rotation and fullscreen state. A
  single connected monitor leaves the placement unchanged.
- R now rotates the focused cabinet window, including backglass and score;
  Shift+R reverses rotation. F11 remains scoped to the focused window. Setup
  actions ignore keyboard repeat and use the existing normal-exit persistence.
- Adapted the display-selection pattern from Devil's Crush's
  `cycle_assignment_display` at 0f81cb3. SDL window synchronization separates
  leaving fullscreen, moving and reentering fullscreen.
- Added SDL dummy-driver coverage for all three roles, independent rotations,
  wrap/reverse rotation, repeat/unknown-key rejection, fullscreen toggles,
  single-display cycling and saved/restored settings. Real focus transfer,
  multi-monitor movement and fullscreen transitions still require physical
  cabinet testing. Live display hotplug recovery remains pending.
- Validation: all 18 Release tests pass, including canonical replay/video parity.
  Release build has no warnings and `git diff --check` is clean.

Local cabinet backglass artwork (2026-09-08):

- Added optional `backglass.bmp` in the data directory. Cabinet startup loads
  opaque, uncompressed 24/32-bit Windows BMP artwork once, with bounded file size
  and dimensions. The existing scaler preserves aspect and saved rotation.
- Missing artwork retains the plain title. Invalid files fail clearly and stay
  untouched; desktop mode ignores them. Both live and replay cabinet sessions use
  the local image, independently of recorded gameplay.
- Pixel tests cover top-down/bottom-up rows, 24-bit padding, 32-bit reserved bytes,
  distinct corners, absent files and thirteen malformed inputs. Host smoke covers
  live/replay artwork, rotated backglass placement and file preservation.
- Devil's Crush's title-logo backglass was inspected at 0f81cb3. Its logo data and
  extraction addresses are not reused. A ROM-derived Alien Crush default remains
  pending, along with bonus/idle scene coverage and physical cabinet acceptance.
- Validation: all 17 Release tests pass, including canonical replay/video parity.
  Release build has no warnings and `git diff --check` is clean.

Cabinet score panel (2026-09-08):

- Replaced the score window's debug text with a fixed 256x96 panel using Alien
  Crush's original HUD digit artwork and palette. Score is the large upper row;
  the leading high-score entry and existing ball counter have separate lower fields.
- Numeric size stays fixed across digit-count changes and supports the complete
  decoded integer range. Host-authored labels identify fields. The existing sharp
  scaler fits the 8:3 panel with black borders and honors saved score rotation.
- All ten digit images match the original loaded HUD at canonical frame 600.
  Synthetic tests cover maximum values, zero, field bounds and malformed glyphs.
  A local frame-600 preview was visually inspected. Palette extraction now avoids
  loading table backgrounds solely to render HUD digits.
- This follows the Devil's Crush separation of display layout and decoded values;
  its game-specific font addresses, bonus logic and idle gates are not reused.
  Backglass artwork, bonus/idle presentation and physical cabinet acceptance remain.
- Validation: all 16 Release and 3 core-only Debug tests pass. Canonical replay
  and video parity remain exact; builds have no warnings and diff checks are clean.

Inactive lower counter restoration (2026-09-08):

- Restored the lower table's separate 3x2 counter rectangle from seven original
  ROM tile variants and RAM 7F. This is presentation only; gameplay state and
  the original off-screen update schedule are unchanged.
- Original 03:94B9 caps the lower value at 7; 03:94F6..9564 draws six words at
  BAT 022D/024D from 03:9565. Unknown values leave the base artwork unchanged.
- All 1,039 usable lower canonical frames match the restored BAT exactly. The
  canonical route only reaches value 1; six other variants have explicitly
  synthetic inactive rendering coverage confined to the 24x16 counter rectangle.
- Validation: all 16 Release tests and 3 core-only Debug tests pass, including
  canonical replay/video parity. Builds have no warnings; diff checks are clean.

Live cabinet plunger (2026-09-08):

- Added opt-in `cabinet-plunger.txt` with named raw SDL axis, measured endpoints,
  deadzone and release threshold, reusing the Devil's Crush normalization logic.
  Missing settings leave analog input disabled; invalid settings are preserved.
- Accepted draw/release commands now drive the reference adapter and are recorded
  in ACREC002. Pad-only sessions retain ACREC001. Replay ignores live hardware.
- Rod ownership suppresses competing I/II input. Focus loss/disconnect queues
  cancellation before the next emulated frame; pause also cancels. Rearming
  requires current rod rest and released I/II. Quick-load drops stale ownership;
  quick-save rejects an owned draw rather than saving an orphaned pinned launcher.
- Virtual SDL tests cover reversed calibration, release strength, shared buttons,
  focus loss, disconnect/reconnect while drawn, pause, quick-load, rearming and
  exact RAM/video/audio replay of each hardware-driven session. Physical PinOne
  and three-monitor acceptance remain pending.
- Validation: all 16 Release tests and 3 core-only Debug tests pass. Builds have
  no warnings; `git diff --check` is clean. Canonical replay/video parity passes.

Plunger recording and replay (2026-09-08):

- Added checksummed ACREC002 with a plunger command for each pad frame. Existing
  pad-only files remain ACREC001 and replay unchanged. Invalid commands, controller
  bits, timeline lengths and cancellation with I/II held are rejected.
- Host and diagnostic playback share command-before-frame execution and fail on
  a rejected launcher gate instead of silently replaying a different shot.
- A 120-frame file fixture draws, cancels, redraws and releases at a new strength.
  It verifies exact RAM/video/audio playback, including restoration from a pinned
  draw, and runs through the dummy SDL three-window stitched host.
- Live SDL axis calibration, ownership, focus/disconnect cancellation and rearming
  remain the next integration step. No physical cabinet acceptance yet.
- Validation: all 15 Release tests and 3 core-only Debug tests pass; builds have
  no warnings and `git diff --check` is clean. Canonical replay/video parity passes.

Reference plunger cancellation (2026-09-08):

- Added an explicit cancellation operation for adapter-pinned draws, including
  the known pause/resume wait. It returns the launcher and seated ball to rest
  without assigning ball velocity or entering the original firing path.
- Twenty cases cover five strengths cancelled immediately, during active play,
  while paused and during resume. Each stays seated for 240 frames, repeats
  exactly after save/restore, and accepts a fresh draw and original release.
- Natural digital draws, fired launchers and fifteen invalid-state mutations
  reject cancellation unchanged. SDL ownership, focus/disconnect handling,
  rearming and analog recording still need host integration.

Reference plunger draw adapter (2026-09-08):

- Added a conservative seated-ball gate covering actor identity, ROM seat
  geometry, contact, active play, pause and the 16-frame resume wait.
- The explicit runtime adapter holds five requested draw levels for 300 updates
  each, then lets the original release/collision code launch. Ball velocities
  are never assigned by the adapter; charged saves repeat exactly.
- Rejection is transactional; the canonical gate opens only on 135 frames before
  first launch. SDL axis binding, cancellation/rearming and analog recordings
  remain before enabling this path in the cabinet host.

Launcher contact continuation (2026-09-08):

- Added native vertical launcher contact response, preserving original contact
  branches, signed rounding and word wrap. It remains isolated from live physics.
- Seven read-only Mesen contact calls match exactly across full, quarter, half
  and return-phase releases; six apply a response. The untraced control has
  identical RAM and CPU logs, with only the known first-19-frame pixel variation.
- Expanded natural-input replay to five hold durations/four release strengths.
  Physical rod mapping and full launcher eligibility remain pending.

Launcher discovery continuation (2026-09-08):

- Recovered the type-6 launcher draw, plateau, automatic return and release
  paths from ROM; added a bounded, read-only RAM capture inspector.
- Natural-input replay branches verify four hold durations, three distinct
  release strengths and a long-hold return cycle without modifying RAM.
- Physical analog mapping, complete launcher eligibility and collision transfer
  remain pending. See `docs/STATE_AND_ASSETS.md` for addresses and evidence.

The approved plan is [PLAN.md](PLAN.md). This file records verified implementation,
not intended capabilities. Last updated: 2026-09-08.

| Milestone | Status |
| --- | --- |
| 1. Trustworthy desktop baseline | Playable and deterministic; independent-emulator differences remain under investigation |
| 2. State and assets | Native extraction of both table backgrounds and sprite pattern sets; score, high-score storage, ball counter and level observations recovered; remaining gameplay mapping open |
| 3. Playable full-table cabinet | Three windows, stitched table, configurable raw buttons and calibrated analog plunger with deterministic recording; remaining scene coverage, auxiliary artwork and physical cabinet acceptance pending |
| 4. Native gameplay | Not yet implemented |
| 5. Standalone native release | Not yet implemented |

Implemented foundation: strict ROM identity, pinned reference integration,
separate SDL host and diagnostic executable, fixed game cadence, sharp scaling,
original controller inputs, quick-state containers, deterministic input recordings,
RAM/video/audio census and screenshots, core utility tests, and PinOne calibration model.

Verified on Windows x64 with MSVC 19.43 / Windows SDK 10.0.22621:

- Release CTest: 13/13 pass, including native launcher contact, launcher branches,
  raw input, probe, video parity, table replay and cabinet checks (68.98 seconds).
- ROM-free Debug CTest: 3/3 pass, with both reference and SDL disabled (0.10 seconds).
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

No native gameplay ownership, complete full-table actor rendering, finished DMD
presentation, or game-specific analog launcher support is claimed yet.
Physical cabinet, three-monitor, focus/hotplug and prolonged-play acceptance remain open.

Raw cabinet buttons and input probe:

- Added a raw SDL joystick path based on Devil's Crush's device-name selection
  and reconnect approach. Cabinet boards no longer need an SDL gamepad mapping.
- Optional, bounded `cabinet-inputs.txt` maps eight original controller actions
  using a case-insensitive device-name substring and zero-based button indices.
  Missing settings disable raw input; invalid settings fail without overwriting
  the file. A missing configured controller does not prevent startup.
- Raw inputs require all mapped buttons released after connection, focus loss or
  quick-load. Disconnect clears held input; reconnect resolves the new instance.
  The selected board cannot also invoke default gamepad mappings. Recording stores
  final controller bits and replay bypasses hardware/configuration.
- `--input-probe` displays raw controller names, pressed buttons and axis readings
  in a standalone window and console without loading a ROM. It supports hotplug
  and bounded sample runs. Axis readings do not yet control the game's launcher.
- SDL virtual-device tests cover device selection, insufficient button counts,
  every mapping, simultaneous flippers, release-to-arm and reconnect with changed
  instance IDs. Host smoke checks also cover absent devices and input-file
  preservation. Actual PinOne, multi-monitor focus and physical gameplay acceptance
  remain outstanding; example button indices are deliberately not hardware defaults.

Ball occlusion refinement:

- Replaced whole-rectangle blockers with compositor output: pixels claimed by
  sprites (including those hidden by background priority), plus rows whose sixteen
  fetch slots are occupied. Transparent pixels no longer stall the handoff.
- Both frozen and raster composition produce blockers in the same pass as the
  cached background, preserving VRAM changes, sprite ordering and display enables.
- Ball replacement now covers all 150 canonical loading frames, 75 per direction.
  The replay test requires complete coverage; real overlap and fetch exhaustion
  still retain the conservative hold. Synthetic checks cover hidden pixels,
  transparency, disabled raster rows, fetch exhaustion and cache reset.

Ball handoff continuation (historical):

- The host and capture tool cache a separately composed background with the
  uniquely identified visible ball removed. During a confirmed transition, they
  draw its ROM sprite at the game's stored coordinates with the original 224-row
  translation. No interpolation, extra simulation or runtime RAM writes occur.
- Conservative sprite bounds include hidden and transparent portions. If the ball
  overlaps another sprite, or its form/background is uncertain, presentation keeps
  the complete original hold. This avoids guessing sprite order or leaving a ghost.
- The canonical replay redraws the ball on 132/150 held frames (62 returning lower,
  70 entering upper); 18 use the conservative fallback. Synthetic tests verify old
  ball removal and overlap fallback; replay tests verify both coordinate handoffs.
- Full-table motion is still not completely seamless, and physical cabinet
  performance/acceptance remains open.

Stable playfield join (historical):

- Upper-half artwork now owns the 16-row overlap consistently. Sprite visibility
  masks preserve sprites from either half, with active-half priority when they
  overlap, so the ball and flippers are not cut off by a rectangular crop.
- Masks come from native composition after SAT priority, background priority and
  scanline fetch limits, including raster schedules. Equal RGB colors cannot
  erase coverage. Synthetic tests cover hidden sprites, equal colors, raster
  disabling, fixed artwork and sprite conflicts at the join.
- Replay pixel checks exclude the intentionally altered overlap as well as the
  removed HUD. Reference video parity itself remains unchanged. Ball movement
  across the held loading interval is still not seamless.

Table transition presentation (historical):

- Added a bounded presentation hold after a confirmed main-table code change.
  The host and capture tool retain the last complete stitched image while the
  next half loads, then immediately resume native composition. No extra game
  steps, RAM changes or interpolated ball positions are introduced.
- The canonical replay covers 150 held frames across ten table changes, alongside
  1,292 directly rendered frames. Both initial transition directions retain the
  stitched layout throughout their 15-frame loading intervals.
- Holds expire after 32 frames and clear on skipped epochs, camera/scene changes,
  quick load and view toggles. A second table change cannot extend a pending hold.
- The three-window stitched smoke recording now crosses a table transition.
  Overlap changes and ball handoff remain visible; this is not seamless motion.

Inactive background actor continuation (historical):

- Actor types 7/20 explicitly request a background redraw on table return at
  `01:4FFB..5003` and `01:52C1..52C9`. Their stored descriptors now restore the
  inactive BAT before persistent indicator palettes, in original return order.
- The 25 lower and 100 upper reconstructed tiles match the original at frames
  600/720. The full stitched replay retains active pixel parity and restore checks.
- Types 74..77 do not share that forced-redraw path and can retain descriptors
  after a background reload; those stale descriptors remain excluded. Broader
  background restoration and seamless transitions are still unfinished.

Inactive indicator continuation (historical):

- Recovered the return-to-table indicator restoration at `03:9415/925F`. Native
  rendering now reapplies persistent RAM flags to ROM-defined BAT rectangles,
  preserving tile identity. All 26 enabled upper-table tiles match at frame 720.
- Added each inactive half's existing four-sprite table overlay after its stored
  actors, using the per-half selector and original visibility/capacity guards.
- Actor-driven BAT changes and transient indicator blinking remain incomplete;
  seamless transitions and physical cabinet acceptance are still pending.

Live stitched raster continuation (historical):

- The default cabinet renderer and stitched captures now consume the existing
  reference display/VRAM row observations and pixel palette schedule. Builds
  without the observer keep the frozen rendering path.
- All non-HUD active pixels match the reference on 1,292 supported frames in the
  expanded 1,800-frame table replay. Raster timing corrects one frozen stitched
  image; incomplete observations fall back safely, and restored output matches.
- The inactive-background investigation found stale descriptors and mixed sprite
  forms. Naively replaying them does not reproduce later BAT checkpoints, so
  inactive background changes remain pending along with overlays and transitions.

Initial stitched playfield continuation (historical):

- Original bank 03 transition routines add/subtract 224 from local ball Y. The
  two visible 240-row halves therefore form a 256×464 image with 16 overlapping
  rows. The active half owns that overlap; the artwork is not identical there.
- Added opt-in `--stitched` and cabinet V toggle. ROM backgrounds seed both halves;
  native frozen video draws the active half with previous SAT and standard HUD
  digits removed. Retains original pixel aspect ratio, rotation and shared clock.
- Strict pattern/load, display-register, camera and consecutive-epoch checks fall
  back to original video during unsupported states. Quick load clears the epoch.
- The canonical capture produces 1,039 lower / 253 upper stitched frames and 508
  fallbacks, preserving the frozen RAM/pixels/audio census and replay checks.
- Inactive actors and animation, seamless transitions, full live raster scheduling,
  and physical cabinet acceptance remain open. The stitched view is experimental.

Inactive actor continuation:

- Recovered the original 01:46FD bank exchange: 32 fields of slots 4..63 swap
  between RAM offsets 0204 and 1504. Ball/reserved slots 0..3 are excluded.
- Render inactive sprites and flippers directly from the stored bank using native
  ROM descriptors and per-table patterns/palettes. Restore the lower arm pattern
  selections from ROM. No runtime writes, off-screen simulation or cache state.
- All 1,920 inactive actor bytes at frame 620 match the departing lower bank at
  frame 607; its rendered image stays identical through frame 720. Restoring an
  earlier save reconstructs both halves without stale presentation data.
- Inactive BAT changes and overlays, live raster timing and seamless transitions
  still need work. Stored flippers preserve their pose; they do not animate in
  response to buttons while their half is inactive, matching stored game state.
- The 1,800-frame capture retains the frozen census and all replay/restore checks;
  stitched coverage remains 1,039 lower / 253 upper frames. Release 9/9 and Debug
  2/2 pass; final builds are warning-free. Physical cabinet testing remains open.

Scanline VRAM and cabinet host continuation:

- Read-only VRAM observations at the existing VDC HDW boundary resolve frames
  608 and 758 automatically. All 1,798 complete canonical frames now match;
  startup frames 1/2 remain explicitly unavailable. No frame-specific timing or
  pixel-derived decisions are used. This is reference-assisted native rendering.
- All 900 restored VRAM schedules repeat, and the enabled/compiled-out observer
  preserves the frozen 1,800-frame RAM/video/audio census. Both remaining frame
  exports have byte-identical reference and native BMPs.
- Added the required-equality `reference_video` regression gate, including coverage.
- Added `--cabinet`: three windows, named-monitor placement, atomic layout saving,
  missing-monitor recovery, playfield rotation, per-window fullscreen and shared
  focus. Uses Devil's Crush's host/display-placement pattern; no game state maps
  or execution patches were copied. Backglass and score presentation are preliminary;
  the playfield still scrolls. Tests use SDL dummy displays, not physical monitors.

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
