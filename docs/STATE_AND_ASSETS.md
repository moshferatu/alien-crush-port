# State and asset discovery

Continuous runtime handoffs (2026-09-09): see [CONTINUOUS_HANDOFF.md](CONTINUOUS_HANDOFF.md).
The live cabinet host now uses synchronous ROM-derived transfer replacements and
queues every frame's audio. The frame-compression notes below describe the earlier,
superseded workaround; they are retained as investigation history.

The paused high-score viewer is recognized by RAM 7A=1, demo selector 76=0,
and the complete nine-byte stack signature at 01F7..01FF:
`FD EB 03 02 01 B0 41 8E 40`. Original 01:408C calls 4187; Run enters the
pause loop and Select sets 7A=1 before 41AE calls EBE6. That wrapper pushes
MPR2..4 and calls 1C:4003 at EBFB. Mode 7A remains 1 after return, so the mode
alone must not select the cabinet score-table layout. This signature covers
the paused ordinary-play viewer; post-game name entry is not inferred from it.
The score window shows the five numerical ranks; original names remain on the
playfield. This presentation reads RAM only and adds no persistence writes.

The score-viewer replay uses canonical inputs through frame 600, Run at input
600, Select at 650, Select at 900 and Run at 1050. The viewer signature spans
exactly frames 651..900. The original viewport replaces the stitched table in
the settled viewer and stitching resumes after leaving pause. All five scores
remain 1,000,000 and their three name bytes remain 40. Restoring frame 600 and
replaying through 1200 reproduces RAM/video/audio and the frame-800 score panel
exactly. The 600-frame recording runs in the three-window host. Both the original
viewer and the cabinet panel were visually inspected.

High-score storage (automatic host persistence added 2026-09-13): boot
00:E0B5..E0DC copies five six-byte records from ROM 0018F into the six RAM
columns at 1219/121F/1225 (score / 100) and 122B/1231/1237 (name bytes).
The sixth column in each row belongs to the current candidate and must not be
treated as a ranked entry. 1C:470A sorts only in post-game mode 7A=2, outside
demo; 4B2E compares all six entries with unsigned 24-bit subtraction and swaps
all six score/name bytes, tracking the candidate's rank in RAM 1C. Equal
comparisons do not swap entries. A candidate left at rank 5 bypasses name entry.
Name codes cycle 40..5B using Left/Right (4877/4885); I advances the character,
II goes back, and Run/Select confirms. The qualifying replay below verifies
Left/Right changes, decrement wrap and I advancement/confirmation. II-back,
increment wrap and Run/Select confirmation remain disassembly-only coverage.
Devil's Crush's HighScoreAutosave provides the persistence reference: restore at
the verified boot initialization boundary, sample settled table/name changes,
write atomically, and isolate recording/replay. Alien Crush still needs its own
validated restore integration before persistence is complete.

The qualifying name-entry route starts from the room-3 clear recording at 4800,
replays it through 5500, then sends I for n%9<4, Left for (n+3)%9<4 and II for
n%181=100 while the current score is below 1,100,000. It then releases the pad.
At frame 9656, the original sort promotes 1,505,700 points above the five default
scores. The other four retained ranks stay at 1,000,000. No RAM or ROM is changed
to force this qualification. A distinct verified stack signature at 01F7..01FF,
`FD EB 03 02 01 8F 44 08 40`, with mode 7A=2 and practice 7C=1, identifies
01:448D's score-screen call. The cabinet now uses the ranking layout throughout
this 280-frame call instead of displaying the candidate field, which the sort
has replaced with a displaced score. Ordinary-play post-game call signatures
remain to be verified separately.

Right at input 9800 changes the first name byte 40->41; I at 9802 advances.
Left at 9804 wraps the second byte 40->5B; I at 9806 advances. Right at 9808
changes the third byte 40->41; I at 9810 confirms, setting RAM 120C=80. The
ranking layout remains during the confirmation pause. The title is present by
10000, then Run at 10100/10160/10220 starts another three-ball game with zero
current score. The new top score and raw name 41/5B/41 remain intact through
10500, and the other four names remain unchanged. The ranking font tiles
350..36B confirm the character mapping: 40 is a dot, 41..5A are A..Z, and
5B is a space. The cabinet DMD now shows all three initials between each
rank and score, using its small host font. It reads the five ranked names
from the same strided RAM columns as persistence and follows live editing;
the sixth candidate column is excluded. Invalid character codes display blank.

Frame-9400 restore through 10500 reproduces RAM/video/audio and the frame-9809
score panel exactly. The 1100-frame recording also runs in the three-window
stitched host. The cabinet ranking preview was visually inspected. This covers
session retention, not disk persistence or restoration after process restart.

Ordinary game-over presentation requires table code 0/1, zero remaining balls,
no practice/demo selector and RAM 78 bits 0 and 1 both set. 01:40D0 consumes a
ball; zero branches to 410F, which sets bit 1 before the end sequence. 417E..4182
clears both bits before E0DE returns to the title. The score window shows GAME
OVER while retaining the final score and lower fields; title/demo take precedence.
This observation changes presentation only, never the emulated state.

The game-over replay continues the ordinary bonus-return fixture from frame 2150.
It uses canonical inputs from index 2450 to 2799, then only I for n%240<100 to
launch subsequent balls with Left released (I also operates the right flipper).
Ball counts drop at frames
2845/3760/4732. The final score is 22,200; game-over state lasts 771 frames,
4732..5502. Title appears at 5517 with the current score reset. Frame-4700 restore
through 5700 reproduces RAM/video/audio and the frame-4800 score panel exactly.
The generated 1000-frame recording spans the final drain, game over and title
return in the three-window host. The score-panel preview was visually inspected.
The previously named "continue selection" gap is the original score-digit match
award described below. High-score entry and persistence remain separate gaps.

The winning controller-only route branches from the same ordinary return fixture.
It retains the losing route through frame 3760, then uses I when
`(input_index+168)%240<100`, with Left released. The final drain is at 4785,
with 21,700 points. At 4874 the game grants one ball and sets RAM 78 bit 3;
the score panel stops showing GAME OVER because the ball count is now one.
The award flag lasts 437 frames through 5310. Frame 5311 clears 53/78 and
resumes the original ball setup. Stitched play and ball movement resume without
a title restart. The frame-4800 checkpoint replays through 5700 with exact
RAM/video/audio and frame-4900 score-panel parity; the generated 900-frame
recording also runs through the three-window host. The route has 1,282 stitched
frames in the tested ending interval, including 375 after return; their active
pixels match outside intentional HUD removal and the seam. The score-panel
preview was visually inspected with 21,700 points and one ball.

Original match code: 03:82CC seeds the digit through F993 with range 10;
03:834A tests new I/II presses (RAM 27 bits 0/1) to stop the spin, otherwise
the timer expires. 03:83CC divides the internal score (displayed points / 100)
by 10 using F856 and compares the remainder with actor field 3C40. A match
sets RAM 78 bit 3 at 83E9, sets 53 bit 5 and calls 9196 to award a ball.
01:414D tests that flag; 4153 clears 53/78 and jumps to 40D7 for ball setup.
ED3B reloads graphics; it is not a continue-selection menu.

The winning route exposed a moving-score sprite mismatch at frame 4792,
pixel (89,62), during the digit upload to VRAM 0500..07FF. The reference
FetchSpriteData caches pattern words separately from the later visible-row
VRAM observation. TablePlayfield now declines stitching when a visible,
unsuppressed HUD digit coincides with a nonzero-row digit-pattern upload.
The host presents the original viewport without a stale transition hold for
that frame. This is a conservative presentation guard, not a recovered sprite
fetch-timing implementation; normal standard-HUD suppression is unaffected.

The losing route exposed a scanline overflow error at frame 4952, pixel (178,16).
Removing HUD SAT entries had freed fetch slots and exposed sprite pixels the
original console dropped during the GAME OVER animation. Compositors now accept
a 64-bit hidden-slot mask: hidden sprites still consume their original 16-pixel
fetch pieces but do not draw or claim pixels. Table HUD removal and ball-backdrop
rendering use this mask instead of erasing SAT entries. ROM-free checks cover
wide hidden sprites, overflow/block masks, slot 63 and both raster paths. All
964 stitched frames in the last-ball/end interval match the active original
raster outside the deliberately removed HUD and fixed overlap seam.

Bonus room presentation uses RAM 79=2 and room selector RAM 83=0..3, displayed
as BONUS 1..4. FDE5 clears the main-table actors; FE18 temporarily sets 79=83+2
for E942's actor-list loader, and FE23 restores shared table code 2. The score
panel retains the same cumulative score mapping and replaces only the ball field.
Unknown room selectors keep the normal layout; title and demo take precedence.

The original bonus-practice selector at 01:4444 checks title input counters:
RAM 123E/1241=3, 123F=6, 1240=8; RAM 1242&3 chooses the initial room. These
correspond to Up/Right/Down/Left/I presses counted by 1C:42A1. It sets 7C=1,
then runs FDC1 and advances 83 through four rooms (01:447D..4487). Tests enter
all four rooms using pad input, preserve this practice flag, and verify exact
frame-450 restore through frame 900. They establish score awards in rooms 1..3,
but this short route does not establish room-4 awards or ordinary-play bonus entry/return.
The cabinet host fixture verifies original-view fallback while stitched mode is on.

Bonus round-over presentation requires a known bonus room and RAM 78 bit 0.
FDDF exits the FDC7 gameplay loop on this bit; FEC0..FED3 waits 150 frames then
clears 78 before the teardown/return path. The score header changes to ROUND OVER
without changing score or room. The extended room-1 practice route observes
endings at 740/1463/2404, followed by failed results, room progression
and ball consumption at 1064/1787, zero balls at 2728, and title at 2877.
Frame-450 restoration through frame 3000 preserves exact final RAM/video/audio.
This covers practice losses and exit. The host's 300-frame loss fixture
covers the first round end and following fade in three-window stitched mode.

Completion is evaluated later, not at the initial round-over boundary: FED5 calls
EE86, which maps 1D..21 and calls 1D:400C -> 1F:946E. The results routine tallies
points, then 1F:9534 calls 970B. It requires RAM 84=16/15/9 for rooms 1/3/4 or
RAM 81=6 for room 2; 9736 sets RAM 85 bit 0 and 97C5 awards completion points.
The score panel shows BONUS CLEAR for that bit in a known bonus room; title/demo
still take precedence. FE7C resets the flag during the next room's initialization.
A zero flag during the initial 150-frame pause alone does not establish failure;
practice ball consumption and the later results flag establish the outcome.

Two additional controller-only routes cover the results sequence. Room 1 uses
21-frame flipper cycles (I for n%21<10, Left for (n+7)%21<10) and II at n%181=100,
starting at frame index 360. Live play ends at frame 2836 with 13,200 points;
five tally changes add 80,000 before room 2 begins at 3244 with two balls.
Room 3 uses 8-frame cycles (I for n%8<4, Left for (n+2)%8<4) and the same II
pulses. Its clear flag first appears at frame 4974 with 448,200 points, remains
observed for 241 frames, and the route reaches room 4 with all three balls.
Frame-2800/4800 restores reproduce final RAM/video/audio at 3300/5500 exactly.
The generated 700-frame clear recording also runs through all three cabinet
windows in stitched mode; its BONUS CLEAR panel preview was visually inspected.
Other room clears remain unverified. Ordinary-play return coverage follows below.

The ordinary bonus replay uses canonical inputs before frame index 360, then
I when n%240<100 or n%39<19, Left when (n+13)%39<19, and II at n%181=100.
It resumes canonical inputs at index 2199. RAM 7C remains zero and demo mode
remains off throughout. It enters room 2 at frame 854, spends 1,345 frames in
the bonus scene, and returns to main table code 1 at 2199 with 14,400 points
and all three balls. This exercises 01:40B4 -> FDC1 and the ordinary FF04 return.
FF47 loads the lower actor list, FF61 exchanges it into the stored bank, and
FF67 loads the upper actors. The first restored lower bank matches all 19
types in ROM 997B's six-byte entries (04:B97A count/list); its 1,920 actor bytes
remain dormant until the next lower-table handoff.

`--bonus-stage <1-4>` (`game.bonus_stage`, the launcher's Starting Bonus Stage)
uses this same ordinary entry. `start_in_bonus_room()` presses Run through the
title, waits for the first ball to seat in the shooter, then writes the room to
RAM 83 and sets RAM 78 bit 2, which 01:40AA answers with FDC1 on the next pass
of the game loop. RAM 7C stays zero, so this is an ordinary game and not the
title's practice mode: the room's score counts, FF04 returns the ball to the
table and all three balls remain. The request is refused, leaving RAM intact,
unless the shooter's exact seat gate holds. The start-up runs through the one
full-brightness frame FE37 shows before the fade (see the entry flash above),
so the session opens on the fade-in from black. `bonus_stage_launch` checks all
four rooms, entry through return and the next seated ball, on the original and
the cabinet runtime.

The return exposed a presentation issue: a stitched image at frame 2214 used a
partially faded upper palette alongside the fully lit inactive lower table.
TablePlayfield now precomputes the black and partial E4A1 fade-in palettes using
the existing, trace-verified advance_palette_fade helper. Exact 512-color matches
in either current or previous video state keep the original viewport; no palette
or gameplay state is modified. This is recognition of known table fade-ins,
not a general reconstruction of offscreen palette animation. Stitching resumes
at frame 2243, and a preview of that full table was visually inspected.

Through frame 2800, 386 lower and 112 upper returned stitched frames match the
active original raster outside the intentional HUD removal and overlap seam.
Frame-800 restore reproduces final RAM/video/audio and the first complete
stitched return image exactly. Generated 200-frame entry and 300-frame return
recordings remain local; the return runs through the three-window host test.
The original 1800-frame table test now yields 1009 lower and 253 upper ready
frames (30 initial partial-fade frames use the original viewport); its 150
transition holds and 75/75 ball updates are unchanged. Broader ordinary routes,
other bonus clears and physical cabinet acceptance remain pending.

The title-menu score panel uses a complete outer-call signature at RAM
1F7..1FF: `0C EB 18 04 03 02 01 05 40`. Original 01:4003 calls EAEB (return
4005), EAEB pushes MPR2..6, and EB0A calls the mapped title entry (return EB0C).
Canonical frames 15..318 retain this outer frame through menus and fades;
01:4006 replaces it as gameplay setup proceeds. This signature selects a
presentation layout only and grants no authority to modify gameplay RAM.
The panel displays high_scores[0] and a host-authored PRESS RUN prompt, hiding
score and ball. Restoring a quick state recomputes the choice from its RAM.

Demo presentation follows the nonzero RAM 76 branch at 01:4009. The title timeout
sets it to 1 at 1C:404E/4050, and 01:4021 increments it to 2 before the demo loop.
The cabinet shows high score, DEMO and PRESS RUN instead of simulated score/ball
values. The title signature takes precedence during the fade into demo setup.
With neutral input, demo setup starts at frame 1563, active demo at 1609, RAM 76
clears at 5513, and the title signature returns at 5526. The attract replay test
covers 6000 frames, a frame-1800 restore with exact final RAM/video/audio, and
Run returning from demo to title. Bonus room mapping is described above.

The default cabinet backglass is the original title actor B9, not a captured
gameplay frame. Title initialization 1C:4055 calls 00:EC58, which maps bank 12 and
loads the RLE sprite stream at CPU 4125 (ROM 24125) into VRAM 4000..5FFF. The
title palette comes from 1C:4BD0 (ROM 38BD0), copied by 1C:4089. B9's thirteen
pieces are at ROM 2D1F2; 1C:42DD initializes X=128 and 1C:4309 sets settled Y=78.
The cabinet renders its piece bounds (x32..223, y14..157) on black using the
initial sprite palette, without menu text, scrolling background or color cycling.
It uses 5:4 pixel aspect. The title-logo test verifies the naturally loaded stream,
palette and settled sprite composition during the first 300 canonical frames.

All addresses apply only to ROM SHA-256
`1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f`.
CPU work RAM at `$2000..$3FFF` corresponds to capture offsets `$0000..$1FFF`.
ROM offsets are physical file byte offsets; `bank:PC` uses 8-KiB banks.
Generated disassemblies, traces and artwork remain in ignored local output.

## Local backglass artwork

The cabinet can load `backglass.bmp` from the data directory at startup. This is
user-supplied presentation art, not a recovered ROM asset or game-state capture.
Only uncompressed Windows BMP with a DIB header of at least 40 bytes and 24/32-bit
pixels is accepted, bounded to 4096x4096 and 64 MiB. Both row orientations work;
32-bit BI_RGB's reserved byte is ignored and output is opaque. Invalid files are
preserved and reported. The optional image is outside recordings and save states.
It uses the existing aspect-preserving scaler and saved backglass rotation.
No runtime execution, RAM, ROM or replay input changes are needed for this image.
When absent, the cabinet uses the ROM-derived title logo described above.

## Initial stitched presentation

Bank 03 is mapped at `$8000` for the table transition routines. `$89B9..89C7`
adds `$00E0` to actor Y before `$89CA` increments the table code; `$8A0A..8A18`
subtracts it before `$8A21` selects the lower table. Canonical frames 607/608
show Y 59/283, an exact +224 translation. The reverse path updates Y before
waiting for the frame boundary, so comparing only frames with changed table codes
does not expose its subtraction reliably.

This establishes the ball handoff, but does not establish the artwork offset.
Visual comparison and 576 matching ROM side-wall pixels (lower rows 0..23,
columns 0..7 and 232..247) put lower artwork at upper row 216, not 224.
Preserving the two visible 240-row regions yields 256×456 pixels. The original
256×240 image is displayed at 4:3 (pixel aspect 5:4); the stitched display uses
320:456, or 40:57. Ball sprites use the same artwork projection as their half;
the original 224-coordinate handoff is not patched or interpolated.
The ROM BAT's final 16 rows are not added below the visible lower viewport.
The 24 overlapping visible rows have some different interior artwork. Upper-half artwork
owns them consistently; visible sprites from either half are retained as described
below, with no inferred blend or modified ROM art.

`TablePlayfield` seeds both backgrounds directly from ROM and composes the active
half from current VRAM/palette and previous SAT. Standard HUD sprites are removed
using their recovered positions, pattern range and attributes. Inactive actors
now use the stored bank described below. Two consecutive matching pattern
loads, enabled layers, zero scroll/camera and standard MWR are required. Unsupported
states use the original display. With the reference observer available, the active
half now uses the full observed raster candidate described below; builds without
that observer retain frozen presentation. Seamless transitions are still pending.

Local evidence: `out/captures/stitched-1800/stitched.csv` records 1,039 lower and
253 upper images, with 508 fallbacks. The 1,800-frame reference census SHA-256 stays
`ee560dad47ece1a3c2e55a4326bcd4ef97276ad1ac0912d6c36eb29298c6572e`,
including cold/initial/midpoint restore verification. Native stitched BMPs at 600
and `out/captures/stitched-upper-720/stitched-720.bmp` were visually inspected.
ROM-free tests cover bounds, overlap ownership and clipping. The table replay test
checks both halves, scene/epoch guards and restored output, and produces a short
gameplay recording for the SDL dummy three-window stitched smoke test.

### Stored inactive actors

`01:46FD..4740` exchanges 32 arrays of 60 bytes starting at CPU `$2204` and
`$3504`, advancing both pointers by 64 after each array. These are actor slots
4..63; slots 0..3, including the ball, never participate. `inactive_table_actor_ram`
projects those stored fields into an independent presentation buffer for the
existing actor assembler. The reference RAM is never written. Camera zero,
pattern base `$0200` and ten reserved HUD slots match the recovered main tables.

Each half uses its own decoded sprite streams and original palette. Lower actor
types 26/27 reconstruct their stored dynamic pattern selection even when the
pending upload flag has cleared. No frame cache or off-screen simulation is
needed, so quick-state restore naturally restores inactive presentation too.

The table code changes at frame 608, but the swap is not complete until frame 620.
The renderer's loading gate excludes that interval. All 1,920 bytes stored at 620
match the active bank at 607 exactly, and inactive lower pixels remain identical
through frame 720. These checks are part of `table_replay`; ROM-free tests check
bank bounds, reserved-slot exclusion and source immutability. Local capture
`out/captures/inactive-actors-1800` records both halves with inactive actors.
The later table overlay and persistent indicator colors are now included as
described below. Other inactive BAT modifications remain omitted; this is not a
claim that every off-screen pixel matches the table when next activated.

### Persistent inactive indicators and overlay

The transition calls `03:9415` after exchanging the actor banks. This iterates
set bits in RAM `$D6..D9`, calling `03:925F` to restore lit regions. The 29
six-byte descriptors at physical ROM `$7357` (CPU `$9357`) supply a table number,
BAT address, width, height and enabled/disabled palette bytes. `$92B9..9356`
preserves each tile's low 12 bits and replaces its palette nibble, advancing
rows by 32 words. `decode_table_indicators` and `restore_table_indicators`
translate this restoration path directly. They do not execute actor scripts or
write reference RAM/VRAM. Only enabled bits are reapplied to the ROM BAT, as in
the return-to-table routine. The transient blink routine at `$9437..94B6` is not
advanced for an inactive table.

The inactive sprite pass now also invokes the established `01:4741` overlay
assembler after the main actors, retaining its SAT capacity guard. Its selector
comes from the corresponding half's RAM `$99/$9A`, with global visibility flags
`$53/$54`, local camera zero and that half's sprite patterns.

At canonical frame 720, all 26 tiles in six enabled upper-table rectangles match
the original palette assignments. ROM-free tests cover table isolation, tile
identity preservation, rectangle stride, the final flag byte and validation
before mutation. The expanded table replay still preserves inactive lower pixels
through frames 620..720 and restores the complete stitched image after quick load.
Local visual evidence is `out/captures/inactive-indicators-1080/stitched-1080.bmp`.

### Background actors restored on table return

`01:45A8` dispatches actor scripts through the type-minus-one return-address tables
at `04:BC8E/BCE8`. Type 7 enters `01:4FE9`, type 20 enters `01:52A1`, and types
74..77 enter `01:5BF2/5CA5/5CFA/5D50`. The table transition sets RAM `$68` bit 1
around the actor update at `03:89D6..89DB` / `$8A31..8A36`. Types 7/20 test that
bit at `01:4FFB` / `$52C1` and set their background upload request regardless of
the earlier one-shot flag. Types 74..77 do not share this forced-redraw test.

`restored_actor_backgrounds` therefore restores only eligible types 7/20 from
their current descriptor and attribute override, using the existing bounded BAT
decoder. Inactive rendering supplies the stored-bank projection. Actor updates
precede indicator palette restoration, matching the transition's `45E2` then
`9415` call order. Reference RAM remains untouched and no actor script advances.
At frames 600/720, all 25/100 tiles in the selected lower/upper regions match the
original after indicator restoration. The other actors' stale descriptors remain
excluded; this is a partial background restoration, not an exact inactive BAT.
Visual capture: `out/captures/inactive-backgrounds-1080/stitched-1080.bmp`.

### Holding the stitched image during table loading

Canonical frames 608..622 and 845..859 lack a complete target table image. The
game changes RAM `$79`, reloads graphics, exchanges actor banks, then resumes
display. `TableTransitionHold` retains the immediately preceding complete stitched
image through that interval. It arms only on an adjacent-frame change between
main-table codes 0/1, with local camera zero and visibility flags `$53/$54` clear
of their sign bits. Same-table failures cannot start a hold. A second table change
while holding cancels it, and 32 frames is an explicit presentation timeout,
not a recovered game constant.

The host and capture tool share this class. Quick load and the V toggle reset it;
nonconsecutive epochs and unsupported camera/visibility states clear its cache.
It changes only presentation: game frames/audio continue normally. Background
pixels remain held while the ball may be replaced as described below. The next
complete native image replaces the hold immediately. `stitched.csv` adds a `held`
column so these images remain distinguishable from directly composed frames.

The 1,800-frame replay covers 150 held images across ten table changes, in addition
to the unchanged 1,292 directly composed images. Tests verify both initial loading
intervals and the first held image's exact pixels; ROM-free tests cover timeout,
ready-image replacement, resets and cancellation. The 32-frame SDL dummy smoke
recording now spans the first switch. The ball's handoff is not yet seamless.

### Ball presentation during loading

The standard type-1 ball uses descriptor 0 or 155..157: a single 16x16 foreground
sprite, offsets (-6,-6), pattern `$20/$22/$24/$26`, flags `$0080`. The table's
pattern base is `$0200`. `transition_ball` decodes that ROM form with stored
actor-0 X/Y, subtracts the hardware origin (32,64), and adds 216 for the lower
half. At frame 608 this gives (234,213); at 845 it gives (112,210). It does not
advance physics or interpolate. Attribute override modes outside this form retain
the original hold.

For each complete frame, an optional second composition removes the uniquely
matching *visible* SAT ball and preserves everything behind it, including raster
timing. Off-screen entries are excluded from matching: the upper table also has
a ball-form entry at hardware coordinates `$FFFA/$FFFA`. Zero or multiple visible
matches disable replacement. The clean composition avoids erasing an image
rectangle, which would destroy table artwork or leave a duplicate ball.

During a hold, the old ball is removed and the new ball's opaque pixels are drawn
except where another sprite has claimed a pixel or all sixteen fetch slots are
occupied. These uncertain intersections retain the backdrop pixel, rather than
freezing the entire ball. The compositor exports blockers before background priority,
so hidden sprite pixels remain blocking but transparent portions do not. Frozen
and raster paths generate the mask in the same pass as the cached background;
palette RGB equality does not determine coverage. The ball uses
the corresponding half's ROM sprite palette while background graphics load.
This is cabinet presentation, not a claim to reproduce the cartridge's blanked
loading display.

The 1,800-frame replay now requires 150 ball replacements (75 lower / 75 upper),
covering every held frame. The earlier rectangle guard produced 18 unnecessary
fallbacks. Synthetic tests verify hidden-pixel blocking, transparent-pixel freedom,
exhausted fetch rows, raster disabling, old-ball removal and partial overlap masking.
The CSV distinguishes `held` background and `ball_updated`; local frame 850 is
exported in `out/captures/pixel-ball-860`, and the earlier frame 622 view remains in
`out/captures/live-ball-622`. Actual opaque overlaps use conservative per-pixel occlusion;
this does not claim complete handoff coverage for every possible gameplay route.

### Fixed artwork at the join

The upper half supplies background artwork for global rows 216..239. The lower
half supplies rows 240..455. Within the overlap, visible lower sprites replace
upper artwork unless an upper sprite has already claimed the pixel and the upper
half is active. This preserves both halves' sprites and gives the active half
priority in an actual sprite conflict. It is an explicit cabinet presentation
choice; the cartridge never displays this combined region simultaneously.

Optional compositor masks record visible sprite coverage after the existing
SAT ordering, background priority and sixteen-piece scanline limit. Raster bands
clear and populate their own rows; pixel palette changes preserve that coverage.
RGB comparison is not used to infer transparency. Thus a sprite matching the
background color remains visible when placed over the other half's artwork.
Tests cover fixed artwork across active-half changes, sprite conflicts and mask
behavior with hidden sprites, equal colors and raster-disabled sprites.
Local views: `out/captures/fixed-join-1080/stitched-720.bmp` and `stitched-1080.bmp`.

### Cabinet upload pacing

The original RLE-only speedup still left actor uploads and frame waits visible,
and restarted the host deadline after every omitted frame. The superseded continuous
`TableTransitionClock` instead recognizes literal return addresses within
03:89B3..89EF and 03:8A0A..8A47, their enclosing 01:409E call, and known wait/copy
PC ranges and banks. It requires a usable stitched table, ordinary non-demo state,
consecutive samples and unchanged full fixed-point ball coordinates. Scheduler
PCs cannot be compressed even if the ball is stationary. The final ordinary
01:4086 wait is admitted once, immediately after a confirmed transition. A
32-frame bound, explicit reset on load/view change and Run/Select guard prevent
unbounded compression or accelerating menus and pauses.

World-coordinate comparison uses the cartridge's 224-pixel handoff, including
the lower path's subtraction before changing table code. Artwork still uses
216 pixels. The canonical replay removes 132 stationary frames across ten
crossings: twelve load/copy frames per crossing and twelve final waits overall.
Frames with fractional or integer ball movement retain their presentation slots.
Every original core frame and input record is retained, and the accelerated
frame-599..1800 replay reaches a byte-identical complete final emulator state.

The host spends the preceding frame's spare time on the known upcoming upload,
never resets the deadline during omitted frames, and disables raster observations
for frames that will not be displayed. It uses read-only direct video getters
instead of serializing the whole emulator on every frame. The generated CPU/VCE
getters perform no bus reads, synchronization or writes. All 1800 direct snapshots
match the serialized-state decoder field for field. Observer-free builds use the
existing serialized fallback. The pinned dependency checkout is unchanged.

The current-position ball is drawn on both ready and held backdrops. Previously,
switching back to delayed SAT after loading repeated frames 623/624 and could move
the ball backward after a current-position held frame. Tests now require movement
across that boundary. The raw raster compositor remains available for exact
reference comparisons; stitched host and capture intentionally update the ball.

Only omitted frames' presentation and audio are removed, so music advances across
that segment. Inputs/events are polled on every iteration. Original view and
offline diagnostics keep every frame and complete audio. The table test reports
complete batch execution time including the next visible frame, and checks all
ten crossings. Physical cabinet timing/audio still require hardware acceptance.

### Live stitched raster timing

The host and stitched capture enable the existing read-only observer when the
runtime supports it. `TableRaster` supplies complete display rows, pixel palette
writes and initial VRAM plus row deltas to `TablePlayfield`. The renderer uses
previous SAT and initial palette with the same schedules as the exact diagnostic
candidate. It removes HUD sprites before composition, preserving table pixels
beneath the digits. Missing rows or incomplete initial VRAM reject the stitched
frame instead of silently substituting frozen timing. Observer-free builds keep
the explicit frozen path; there is no independent device-timing implementation yet.

The expanded 1,800-frame `table_replay` checks every active pixel outside the two
removed HUD areas and intentional 16-row cabinet join against the reference on
all 1,292 supported frames. One image
differs from frozen stitching and is corrected by raster timing. Tests also cover
incomplete observations and save-state restoration. Inactive rendering remains
derived solely from stored RAM and ROM.

A read-only background reconstruction probe against the existing
`out/captures/inactive-actors-1800` checkpoints found that simply replaying stored
descriptors is insufficient: frame 360's BAT reconstructs exactly, but frames
720/1080/1440/1800 retain 25/16/36/36 differing words. Some flagged descriptors
are sprite forms, and even valid background descriptors can differ from the last
uploaded tiles. No such speculative updates were added to the inactive renderer.

## Established observations

### Launcher discovery (2026-09-08)

Actor type `$06` dispatches to `01:4E23`; the canonical lower table uses slot 6.
Type `$1C` at `01:49C8` first brings the ball from `(272,259)` upward for 64
updates, then changes it to type 1 and sets RAM `$67=1`. Neither launcher
presence nor `$67=1` alone establishes complete scene validity.

The launcher checks `$67=1`, the sign of `$53`, pad `$22 & 3`, and its contact
byte `$1AC0+slot` before starting a draw. It accepts either low pad bit. Phase
is `$1840+slot`, timer `$1880+slot`, travel `$1C00/1C40+slot`, and signed travel
step `$1B80/1BC0+slot` (low/high byte arrays).

- Phase 1 advances travel by `$0040` through `00:FD2B/FD35`. At descriptor
  limit 9 it clamps to `$0800`, reverses the step, and enters phase 2 with
  timer `$80`.
- Phase 2 holds for 128 updates. Continued button input enters phase 3 and
  returns travel toward zero. Release in phases 1, 2 or 3 calls `01:4F51`
  and enters phase 4. Holding longer does not always increase strength.
- Release negates travel, calls signed multiplication `00:F796` with `$A6/A7`,
  and writes launcher Y velocity `$1780/17C0+slot`. It sets the travel step to
  `$FF00` and clears ball X velocity/increments. At canonical factor `$0100`,
  tested travel 512/1024/2048 produces launcher velocity -512/-1024/-2048.
  This is not a direct assignment of ball launch velocity.
- `01:4F8F` positions the waiting ball from the nine-entry offset table `$4FC2`,
  indexed by integer travel. Phase 4 calls `03:852F`; its vertical collision
  response is recovered below. Complete eligibility and analog wiring remain open.

`launcher_replay` restores natural frame 480 and varies only controller input:
9, 17, 33, 100 and 177 held updates followed by release. It verifies travel,
release velocity and phase, plus the long-hold plateau/return cycle, without RAM
writes. The 177-update case releases in phase 3 at travel 1536.
Canonical frame 481 begins drawing and frame 581 releases at full travel.

`tools/Inspect-LauncherCapture.ps1 -RamPath <capture>/work-ram.bin -FirstFrame 420
-LastFrame 600` emits CSV for each active type-6 actor with raw travel, signed
velocity, pad, contact and scene fields. Frame numbers are one-based file block
indices, not embedded emulator timestamps. It accepts complete 8 KiB frames up
to 64 MiB and never modifies the capture. Local report: `out/launcher-baseline.csv`.

### Native vertical launcher contact response

`03:852F` sets collision-mode bit 7, finds ball/launcher overlap through `03:85B8`,
then calls `03:858F`. The type-6 collision dispatch tables at `04:BD42/BD9C`
select `01:4FD4`, which clears ball X velocity and enters `03:8B5E`. Velocity
projection is complete at `03:8B69`. The observed launcher surface is `$60`,
with basis `$D0/D1=$0100`, `$D2/D3=$0000` and zero ball tangent velocity.

`gameplay::vertical_launcher_contact` translates the response for that surface
and basis. It implements the original contact/separation branches, five-bit
arithmetic-shift damping and 16-bit wrap. With a new contact, the outgoing Y
velocity is `launcher_y + floor(-abs(ball_y)/32)`, where both inputs are raw
signed velocity words. Without a new contact, an already upward-moving ball is
ignored; an upward launcher may catch a stationary/downward ball. The receding
launcher branch retains the original response without adding launcher velocity.
This helper does not detect overlap, establish scene validity, or advance physics.

Read-only Mesen callbacks capture projected input at `$8B69` and output at
`$8BE1/$8BE4/$8BA1/$8BF8`. `-TraceLauncherLastFrames N` enables them.
`-LauncherHoldFrames N` optionally keeps the canonical first 480 inputs, then
holds only I for N frames and releases all controls; it never writes game RAM.
Manifests distinguish this route from canonical and record the hold duration.

The binary schema is `ACLC0001`, followed by fixed records: little-endian entry
frame u32, exit frame u32, exit PC u16, entry RAM[8192], exit RAM[8192]. The
`ac_launcher_contact_tests` executable accepts one or more trace paths, bounds
input to 64 MiB, validates records/actors/surface/basis/chronology, and checks
native branch choice and resulting ball velocity against each executed call.
Header, truncation, chronology, basis and output-corruption fixtures are rejected.

Local captures `out/mesen/launcher-contact-{600,quarter,half,return}` contain
7 matching calls, 6 applying a response. They cover canonical full travel,
9/17-update draws and a 177-update release during return. The canonical calls
at frames 582/583/584 ignore velocity -3, then produce -2049 and -2113 with
launcher velocity -2048. This verifies the actual contact response rather than
assuming ball velocity equals launcher velocity.

The untraced `launcher-contact-control-600` matches complete RAM and CPU/clock
logs byte-for-byte. Pixels match on frames 20..600; startup frames 1..19 retain
the previously observed Mesen capture variation. The native helper is verified
in isolation and is not yet used to replace the reference runtime's collisions.

### Cabinet score presentation

`render::ScorePanel` composes a 256x96 image from the decoded current score,
first high-score entry and ball-counter byte. It uses original 16x16 HUD digit
sprites (patterns 20..29, flags `$008F`, uploaded at VRAM `$0500`) with the lower
table's palette. The ROM palette is decoded independently from bank `1B:A000`,
avoiding a full two-table background allocation just to obtain HUD colors.
Small field labels are host-authored bitmap glyphs. The upper score uses a fixed
2x scale and lower fields 1x; all ten decimal digits of a uint32 value fit without
clipping or changing glyph size. The score window uses the existing sharp scaler
at 8:3 and therefore also honors its persisted rotation. Values are read from the
existing diagnostic mapping. Title menus use the separate layout described above;
demo mode adds a label and hides simulated values. Bonus rooms replace the ball
field with their room number. Ten rendered digits match the original loaded VRAM/palette at frame
600, including transparent pixels on black. ROM-free layout tests cover maximum
values, zero and invalid glyph dimensions. The table replay emits a private
`table-smoke.score.ppm` preview beside its recording; its frame-600 image was
converted to PNG locally for visual inspection. This does not alter the runtime
or replay timeline.

### Lower table counter artwork

The lower-only branch at `03:94B9` reads RAM `$7F`, stops at 7, otherwise
increments it and jumps to `03:94F6`. That drawing routine returns immediately
on the upper half, indexes `03:9565` by `12*(value-1)`, then writes three words
at BAT `$022D` and three at `$024D` (32-word row stride). The seven entries end
immediately before code resumes at `03:95B9`. Each word includes palette `$A`.
`decode_lower_counter_tiles` extracts those data entries from the supported ROM;
`restore_lower_counter` applies them after inactive actors and persistent indicator
restoration. It reads global RAM `$7F`, not an inactive actor field, and never
changes game state or advances an off-screen actor. Values outside 1..7 preserve
the base BAT. The patch covers local pixels x=104..127, y=136..151, corresponding
to stitched y=360..375. All 1,039 usable lower frames in the 1,800-frame canonical
route match original video memory. That route only observes value 1 (plus zero
during initialization). Values 2..7 have synthetic rendering tests with copied
RAM only: every changed inactive pixel stays within that rectangle. This is not
natural gameplay evidence for reaching those higher values.

### Guarded reference plunger draw adapter

`ReferenceRuntime::apply_plunger_draw(percent)` is an explicit cabinet adapter,
separate from read-only observation and canonical pad input. It accepts 1..100;
false leaves RAM intact. It is tested through the runtime API but not yet wired
to general native gameplay. The SDL and recording bridges described below now support these operations.

The seat gate requires the active lower table, mode `$67=1`, demo selector `$76=0`,
`$53` non-negative (the launcher's own test; a completed lane set leaves `$54` at
1..3 and can set `$53` bit 6 while the ball is seated), no death/game-over bits `$78 & $13`, active bit
`$68 & 1`, zero camera, and a visible type-1 ball. Exactly one active-table type-6
actor must have flags `$88`, phase 0..3, normal descriptor mode, limit 9, travel
0..2048 and descriptor 23..31 consistent with integer travel. The ball's contact
`$1A40` must name that slot. Launcher position must be `(258,240)`, ball X must
be 272, and ball Y must be within one pixel of its current ROM-defined seat.
The check is against the old draw position, before carrying the ball to a new one.

Pause calls `00:E9E8 -> 19:A00C -> 19:BA7F`, setting `$F6=3`; the sound driver
consumes bit 0 while bit 1 remains set. The gate rejects that bit. It also
requires the outermost stack return word at RAM `$01FE/01FF` to be `$4088`,
the normal `01:4086` frame wait. Pause and its 16-frame resume delay retain
the `$408E` caller of `01:4187`, so clearing the sound pause bit does not
prematurely reopen the gate. These are supported-ROM execution checks, not a
general scene classifier or proof for every possible gameplay route.

Accepted draw maps to `floor(percent*2048/100)`. The adapter writes travel,
phase 1, zero travel step, descriptor, and the seated ball's integer position.
The descriptor sequence comes from `04:BEC2` (23..31), selected by `00:FD8F`;
seat offsets come from `01:4FC2`. Ball velocity and fractional position are
untouched. While I is held, original phase-1 code maintains the chosen draw.
Clearing both I and II lets original `01:4F51` calculate release velocity and
phase 4 transfer motion through the original collision routine. No extra frames
are advanced by the adapter. Pinning the chosen draw is a cabinet control choice;
the original digital button still retains its timed draw/plateau/return behavior.

`launcher_replay` separates these modified-input trials from natural-ROM evidence:
five percentages (1/25/50/75/100) each stay fixed for 300 updates and release at
the expected strength. Charged save/restore repeats RAM/video/audio exactly.
Large upward/downward draw changes retain the seat. Fifteen rejected-state
mutations and invalid percentages leave every byte intact; natural pause/resume
is rejected. The unmodified 1,800-frame route opens the gate on 135 pre-release
frames and never during menus, flight or table transitions.

`ReferenceRuntime::cancel_plunger_draw()` returns an adapter-pinned draw to rest.
It uses the same seat/scene/identity checks, but also accepts the known `$408E`
pause/resume caller. It additionally requires phase 1 and a zero travel step;
natural digital draws and fired launchers are rejected without changing RAM.
The caller must track adapter ownership: these fields identify the pinned state,
not which input device requested it.

Cancellation zeros launcher travel and velocity, restores phase 0 and descriptor
23, and carries the ball to the rest seat (272,243). Ball velocities and fractional
positions are untouched. The original phase-0 path at `01:4E55` returns without
firing when I/II are clear. This is an explicit cabinet behavior, not a recovered
cartridge cancellation feature. Twenty runtime cases cover five strengths with
immediate cancellation, active play, pause and resume delay. All remain seated
for 240 neutral frames, repeat exactly after save/restore, and accept a new draw
and original release. Fifteen invalid-state mutations are transactional.

The cabinet host now binds a raw SDL axis using opt-in `ACPLUNGER1` configuration.
It reuses the Devil's Crush endpoint/deadzone/release normalization and records
accepted bridge commands with the effective pad bits. A draw owns I/II only after
the launcher gate accepts it. Releases clear both bits; other actions remain live.
Focus loss or disconnect marks an owned draw for cancellation before the next
emulated frame, keeping the cancellation on the recorded timeline even if the
host waits for focus. A paused owned draw cancels when the draw gate rejects it.
Rearming requires the rod currently at rest and I/II released; spring bounce and
a held rod after reconnect cannot reacquire a ball. Quick-load drops old ownership
and rearms; quick-save is refused while the rod owns a draw. A rejected out-of-seat
pull must return to rest before acquiring a later ball. Four virtual SDL sessions
verify launch, focus, disconnect and pause, with exact RAM/video/audio replay;
quick-load also verifies that rearming does not mutate the restored state.
Physical PinOne calibration and cabinet acceptance remain pending.

The recording layer accepts `ACREC001` (unchanged four-byte pad records) and
`ACREC002` (the same pad record followed by one command byte). Both retain the
embedded initial state, frame count and trailing SHA-256. Command 0 does nothing,
1..100 applies a draw percentage and 101 cancels. Release is expressed through
I/II being clear, including a draw and release within one frame. Cancellation
requires both buttons clear to prevent an immediate digital redraw. Commands
execute before advancing the runtime; gate rejection throws without advancing.
These are accepted gameplay bridge commands, not raw axis samples or calibration.
Pad-only writers retain version 1. A nonempty command vector must match the pad
timeline exactly; readers reject invalid commands even with a valid checksum.
The 120-frame launcher fixture includes cancellation, a new draw, and a stronger
same-frame draw/release. File playback matches RAM, video and audio, diagnostic
checkpoint replay resumes inside a pinned draw, and SDL dummy playback exercises
the same commands in the three-window stitched host.

| Meaning | Work-RAM offsets | Evidence and limits |
| --- | --- | --- |
| Current score, in hundreds of points | low `$121E`, middle `$1224`, high `$122A` | `00:FA27` adds an award; `00:FA49` converts to HUD digits. Two trailing zero digits are fixed. |
| Five high scores, in hundreds | low `$1219+i`, middle `$121F+i`, high `$1225+i`, `i=0..4` | Current score occupies column 5 of the same table. Initial values are 1,000,000 points each. Ranking insertion/persistence remains unmapped. |
| Score HUD update pending | `$0068`, bit 2 | Set at `00:FA46`, cleared at `00:FA4D`. |
| Ball counter | `$0046` | `03:8FF0` displays counter minus one. Drain and extra-ball lifecycle coverage remains open. |
| Main-table level | `$0079`: 0 lower, 1 upper | Correlates with natural table captures and selects palette/rendering. Other values and scene validity remain unclassified. |

These observations are not sufficient conditions for launcher writes or native ownership.
The canonical route reaches both levels. At frames 600/720/840/960/1080/1800, HUD
scores are 0/200/1200/2700/3200/10200. `--verify-discovery` compares score and ball
counter against the original HUD source SAT on 1,437 eligible frames. The latched
SAT can belong to the preceding DMA epoch, so source and displayed SAT must not be
treated as interchangeable when validating newly updated state.

## Native table background extraction

| Data | Lower ROM offset | Upper ROM offset | Encoding |
| --- | --- | --- | --- |
| Background patterns | `$0A0AF` | `$140AF` | Word RLE, destination VRAM word `$0800` |
| 32×32 BAT | `$132B8` | `$1C832` | Word RLE, destination VRAM word zero |
| Initial 512-entry VCE palette | `$36000` | `$36400` | Raw little-endian 9-bit colors |

Pattern/BAT loaders are `05:4006` and `0A:4006`. Consecutive ROM bank mappings are
corroborated by exact ROM/VRAM runs. `00:EB45` and `00:EB53` load the palettes from
bank `$1B`. The buffers filled by `05/0A:4088` are not VCE palettes; their purpose
remains to be mapped.

The `00:E3DD`/`00:E43B` word RLE format is:

- `00`: end of stream.
- `01..7F`: that many literal little-endian words.
- `81..FF`: repeat the following word `control & 7F` times.
- `80`: repeat 256 times, preserving the CPU decrement-from-zero wrap.

The bounded decoder rejects truncation and overflow. Lower and upper pattern streams
yield 13,424 and 13,216 words respectively; both match the reference VRAM byte-for-byte
at canonical frames 600 and 720. Each BAT has 1,024 words. Output paths include the
ROM hash and decoder schema.

Output is an initial background layer, not a complete playfield. Live BAT changes,
palette animation, sprites and transitions are separate. For example, the upper
brain's initial green palette changes during play. Both backgrounds can now exist
before either camera visit, without advancing off-screen actors.

## Native table sprite extraction

The same table loaders fill the entire sprite pattern region at VRAM words
`$4000..$7FFF` in three streams. Each uses the word RLE format above.

| Stream | Lower ROM offset | Upper ROM offset | VRAM word | Decoded words |
| --- | --- | --- | --- | --- |
| Shared | `$1E00F` | `$1E00F` | `$4000` | 6,144 |
| Level-selected | `$20372` | `$20EB8` | `$5800` | 2,048 |
| Level-local | `$10254` | `$19EF6` | `$6000` | 8,192 |

`05/0A:4019` selects the local stream. At `:403F`, both loaders map banks
`$0F..$11` to CPU `$8000..$DFFF`. The pointer at CPU `$8000` selects the shared
stream; pointers `$8002` (lower) and `$8004` (upper) select the middle stream.
The decoder reads these pointers and checks their mapped range and stream sizes.
All 16,384 decoded words per level match reference VRAM at canonical frames
600 and 720, alongside the background and HUD checks.

`table-sprites-v1` exports initial sprite VRAM and palettes plus a manifest with
physical source offsets, compressed lengths, destinations, decoded lengths and
stream SHA-256 hashes. Sixteen preview BMPs per level show all palette choices;
each is a 16-by-16 grid of 16-pixel patterns, IDs 256 through 511 in row order.
Transparent pixels appear dark gray. These are pattern atlases, not assembled
actors: actor descriptors select palette, size, orientation and animation.
HUD sprite patterns below VRAM `$4000` are outside this extraction.

## Native actor assembly

`ActorAssets` validates the ROM and reads the descriptor pointers at physical
`$2C24B` (low bytes) and `$2C34B` (high bytes). `00:EBA3` maps banks `$16/$17`
at CPU `$A000..$DFFF`; the sprite routine starts at `16:A006`.
The sprite descriptor starts with a piece count, followed by six bytes per piece:
signed Y offset, signed X offset, little-endian pattern word and attribute word.
The same pointer table also contains background-update descriptors consumed by
`16:A1C3`; these must not be decoded as sprite lists indiscriminately.

The native assembler reproduces the main-table pass at `01:85E2..8665`:

- Actor slots 4..63 with `$3540,X` bit 7 set and bit 5 clear are eligible.
  Bit 2 selects immediate emission; remaining actors with bit 3 set are emitted
  next, then actor zero (when its type is nonzero and flags permit), then the
  remaining actors. Each group preserves ascending actor index order.
- `$2050` supplies the first SAT slot. The descriptor count plus current slot
  must be strictly less than 64. This capacity test precedes piece decoding.
- `$3800,X` selects the descriptor. X comes from `$35C0,X/$3600,X`, Y from
  `$3680,X/$36C0,X`. Signed component offsets are added; camera `$2016/$2017`
  is subtracted from Y. These are hardware-coordinate words, not established
  full-table world coordinates. Arithmetic wraps to 16 bits.
- `$202E/$202F` is added to the descriptor pattern word. `$3A00,X` bit 5 enables
  OR-ing `$3A80,X` into the low attribute byte; bit 7 enables OR-ing `$3980,X`
  into the high byte. These overrides do not transform component offsets.

The assembler observes the ROM's selected animation descriptor; it does not
advance animation or gameplay. It omits HUD, background writes, pattern-transfer
side effects after `16:A0D7`, and the later pass called at `01:8666`.
Callers must establish scene validity and the rendering epoch before using it
for presentation. The bounded decoder rejects zero counts, truncation and lists
whose six-byte entries would wrap the original eight-bit Y index.

The canonical route provides these reproducible checks:

- RAM frames 599/719 reconstruct all 41/40 actor sprites in source SAT frames
  600/720: all 324 words match exactly, including the ball and flippers.
  These fixed lower/upper samples are enforced by `--verify-discovery`.
- Across RAM frames 480..1799 versus source SAT frames 481..1800, 1,151 of 1,320
  frames match completely. Of 209,268 compared words, 3,130 differ. This is an
  epoch diagnostic, not a passed whole-route actor-equivalence gate. A fixed
  one-frame offset is insufficient; rendering/update and transition timing
  remain to be classified before replacing the original renderer.
- ROM-free tests cover descriptor boundaries and signed offsets. ROM integration
  also checks coordinate/pattern wrapping, both attribute overrides and the
  strict SAT-capacity boundary using the recovered descriptors.

`--verify-discovery --capture <new-directory>` exports `actor-comparison.csv`
with every native/source word, actor index, descriptor, slot and both frame
numbers. `actor-manifest.txt` records provenance, coverage and limitations.
Frames 600/720 include `native-actor-layer.bmp`: assembled sprite-only previews
using native initial patterns and the current reference palette, on dark gray.
Lower SAT indices take precedence; background priority and scanline sprite
limits are not simulated. Reference snapshot exports are included for inspection.
Generated data remains in ignored local output.

The same command also exports `actor-epochs.csv` and `actor-epoch-manifest.txt`.
This observational sweep compares current source SAT against RAM from zero to
three frames earlier, using the common source interval 483..1800 (1,318 frames).
Every candidate is reported; no best-offset selection is used as a validation gate.

| RAM lag | Exact candidate frames | Mismatching / compared words |
| --- | ---: | ---: |
| 0 | 72 | 18,735 / 208,916 |
| 1 | 1,149 | 3,130 / 208,940 |
| 2 | 92 | 18,545 / 208,964 |
| 3 | 32 | 23,626 / 208,988 |

The source SAT's entire 256-word VRAM range is unchanged from the preceding
snapshot on 211 frames; its longest unchanged run in this interval is 13 frames.
Of the 169 inexact lag-one frames, 141 have unchanged source SAT and 28 have
changed source SAT. The mismatches comprise 1,638 Y, 620 X, 543 pattern and 329
attribute words. No lag has an exact candidate on 130 frames. These observations
show that a fixed frame offset does not establish the render phase; unchanged
snapshots alone do not prove that no writes occurred between snapshots.

Each row records both table codes, sampled PC and its mapped bank, source SAT
retention and consecutive unchanged-frame count, candidate sprite count, and
differences by field. An exact candidate must be nonempty and match every word
it emits. This does not check omitted/extra sprites, scene validity or later
sprite passes. The original fixed sample gates remain unchanged. Next, trace
the original actor-pass entry/exit and transition scheduling against this evidence
before introducing any runtime phase selection or holding native output.

### Actor instruction-boundary capture

`Invoke-MesenCapture.ps1 -TraceActorsLastFrames N` observes the final 1..120
frames with bank-checked execution callbacks. Bank 01 runs at CPU `$4000` in
this path: earlier disassembly labels such as `01:85E2` refer to the same ROM
offset as the actual `01:45E2`, not a second entry point. The trace observes
update entry `$45A8`, ball update `$45CE`, render entry `$45E2`, main-pass end
`$4666` and render return `$46AC`. It never writes game memory.

At render entry it saves RAM; immediately before the later sprite pass at
`$4666` it saves source SAT and the current end slot. Thus each comparison uses
the actual input and output of one main actor pass, even across frame boundaries.
`actor-phases.csv` records call order, frames, table code, slot and master clock.
The trace manifest reports paired passes, orphan ends and an unfinished entry;
these boundary omissions are not represented as successful comparisons.

```powershell
.\tools\Invoke-MesenCapture.ps1 -MesenPath <pinned-Mesen.exe> -RomPath '.\Alien Crush.pce' -OutputDirectory out/mesen/actor-trace -Frames 1600 -TraceActorsLastFrames 120
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --verify-actor-trace out/mesen/actor-trace/actor-passes.bin
```

The verifier checks every emitted word and the end slot, rejects empty or
truncated captures and invalid frame order, and requires nonempty candidates.
Binary schema `ACAT0001` has an eight-byte ASCII header followed by 8,713-byte
records: little-endian u32 entry/end frames, 8,192 RAM bytes, 512 source SAT
bytes (VRAM byte address `$0800`), and one end-slot byte. Captures remain local.

| Captured frame interval | Complete passes | Matching sprite words | Cross-frame passes |
| --- | ---: | ---: | ---: |
| 591..600 | 10 | 1,640 | 0 |
| 595..624 | 16 | 2,576 | 0 |
| 1481..1600 | 64 | 9,716 | 2 |

All end slots match; all three captures have zero orphan ends and zero pending
entries. The first two intervals overlap, so these are 90 captured passes,
not 90 distinct route passes. During ordinary play, render return precedes
actor update and then ball update. At the first transition, no main render
entry is observed in frames 608..620; an upper-table update occurs at 620,
followed by rendering at 621. This explains why end-of-frame lag comparisons
cannot establish assembly correctness during transitions. These Mesen samples
validate assembly at instruction boundaries, not the entire Beetle mismatch
set or a native scheduler.

A fresh tracing-disabled 600-frame control has identical RAM and CPU/clock logs
on all 600 frames, identical video on frames 20..600, and identical final VRAM,
SAT and palette. Video differs on startup frames 1..19, before the trace window
591..600; startup framebuffer reproducibility remains unclassified. Deliberately
truncated data and a corrupted end slot are rejected by the verifier.

### Later table overlay pass

`assemble_table_overlay` now translates `01:4741..480C`, called at `$4666`
after the main actor pass. It emits four consecutive SAT entries only when table
code `$79` is 0 or 1, neither `$53` nor `$54` has its sign bit set, and the
starting slot is less than 60. Coordinates from `01:480D..4814` are hardware SAT
coordinates: lower `(132,191)`, upper `(136,245)`, with camera `$16/$17`
subtracted from Y and X increasing by eight for each entry. Flags are `$008F`.

The first pattern word is `$0100 + pattern_base + (($99[table] * 2) & $FF)`.
The shift carry is explicitly discarded by the original routine. The other
three patterns use only the incremented high byte of the pattern base, with
zero low byte. Arithmetic wraps at 16 bits. The visible meaning of `$99/$9A`
has not yet been assigned; this is described as a table overlay without claiming
its gameplay role. The assembler returns sprite entries without modifying RAM
or clearing SAT; the caller must clear remaining slots as the original does.

Actor tracing additionally writes `overlay-passes.bin` with schema `ACOV0001`,
using the same record layout as `ACAT0001`. Input RAM is captured at `$4666`,
output SAT and end slot at `$46AC`, after the original clears unused entries.
The verifier selects the decoder from the header, uses RAM `$2D` as the overlay
starting slot, and also requires every trailing SAT word to be zero. Main actor
captures and their validation remain available separately.

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --verify-actor-trace out/mesen/actor-trace/overlay-passes.bin
```

| Captured frame interval | Overlay passes | Matching sprite words | Cleared trailing words |
| --- | ---: | ---: | ---: |
| 595..624 | 16 | 256 | 624 |
| 1481..1600 | 64 | 1,024 | 3,084 |

All end slots match and both captures have zero orphan or pending overlay
boundaries. None of these 80 passes takes the visibility/capacity early exit;
ROM-free tests cover those branches, both table positions, the slot 59/60
boundary, discarded shift carry, pattern/coordinate wrapping and invalid inputs.
An intentionally corrupted trailing SAT word is rejected. The new 624-frame
capture retains identical RAM and CPU logs to the prior main-actor trace.
Release tests pass 5/5 and ROM-free Debug tests pass 2/2. This adds a verified
native sprite pass; presentation timing, HUD, dynamic pattern uploads, background
updates and integration with the live renderer remain open.

### Actor-driven background updates

`ActorAssets::background` and `decode_actor_background` translate the rectangular
BAT-update format at `16:A1C3..A24A`. It shares the sprite descriptor pointer
table, but has a different layout: little-endian destination word address,
one-byte width and height, little-endian base tile word, then width×height
little-endian tile words. Rows advance by 32 VRAM words. Each output adds the
base tile word with 16-bit wrapping, then ORs an optional attribute mask into
the high byte. The original selects this mask from RAM `$1A80+actor` only when
`$1A00+actor` bit `$20` is set.

The native API returns ordered address/value writes without changing reference
RAM or VRAM. Caller scheduling and clearing the actor's update-request bit are
not yet native-owned. The original calls this form from the main actor pass
when flags `$1540+actor` have bits `$80`, `$20` and `$10` set, and clears `$10`
after writing. The decoder rejects zero dimensions and rectangles exceeding
125 words, which would wrap the original eight-bit descriptor index. It does
not interpret all entries in the shared pointer table as background descriptors.

The existing bounded actor trace option additionally observes `16:A1C3` entry
and `16:A24A` return, recording complete before/after VRAM and the resulting actor
flags. `background-passes.bin` uses header `ACBG0001` and 139,274-byte records:
u32 entry/end frames, one actor-index byte, 8,192 input RAM bytes, 65,536 before
VRAM bytes, 65,536 after VRAM bytes, and one final actor-flags byte. Integer
fields are little-endian. Pair/orphan/pending counts are in the trace manifest.

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --verify-background-trace out/mesen/actor-trace/background-passes.bin
```

Verification applies native writes to the captured input VRAM, compares all
32,768 output words, and checks that only the expected update-request flag was
cleared in the actor's flags byte. This catches changes outside the intended
tile region as well as incorrect tile values. The observed calls use one-word
VDC auto-increment; verification rejects upper-half VRAM addresses instead of
assuming how those accesses behave.

| Captured frame interval | Updates | Native writes | Distinct descriptors in interval |
| --- | ---: | ---: | ---: |
| 595..624 | 2 | 106 | 2 |
| 1481..1600 | 8 | 314 | 5 |

All ten complete VRAM snapshots and actor flag acknowledgements match, with no
orphan ends or pending calls. At frame 621, descriptor `$0F` at `$A9B0` updates
a 10×10 region and descriptor `$CE` at `$AF03` updates a 2×3 region. The new
624-frame capture retains identical RAM and CPU logs to the preceding overlay
capture, and its main actor/overlay verifications still pass. ROM-free tests
cover bounds, row stride, address and value wrapping, carry and attribute OR.
Truncated traces and an unexpected change to the final VRAM byte are rejected.
This validates sampled background update calls, not all scenes or a replacement
renderer. Dynamic sprite pattern transfers, palettes, HUD and native scheduling
remain to be integrated.

### Dynamic actor sprite patterns

`ActorAssets::pattern_transfer` decodes the transfer selected by `16:A0DF..A120`.
The preceding check at `$A0D7` uses actor flags bit `$02` (distinct from the
ordering bit `$04`). The destination is the 16-bit wrapped sum of
`pattern_base << 5` and the actor-type offset in `16:A123`. The source is selected
by RAM `$1A80+actor` through the 17 low/high pointer entries at `16:B2C1/B2D2`.
`00:E018` jumps to the existing word-RLE loader at `$E3DD`. The original clears
the actor's `$02` flag before calling the loader. Native code returns destination,
ROM offset, compressed length and decoded words; it does not change reference RAM.

All 17 streams decode to 384 words (six 16×16 sprite patterns). With pattern base
`$0200`, actor types 26/27 target VRAM `$7C80/$7E80`. ROM integration checks all
17 stream layouts, matching data for the two destinations, destination wrapping,
and rejection of out-of-table selectors and unsupported VRAM destinations.
Those layout checks are separate from observed execution equivalence.

Actor tracing now emits `pattern-passes.bin` (`ACPT0001`) with the same record
layout as `ACBG0001`. It pairs pending-transfer entry `$A0DF` with return to
`00:EBB1`, before the sprite wrapper restores ROM banks. The capture manifest
reports completed pairs and any pending entry. Unpaired wrapper returns are
ordinary sprite calls and do not indicate a missing transfer.

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --verify-pattern-trace out/mesen/actor-trace/pattern-passes.bin
```

Frames 1481..1600 contain four transfers: types 26/27 at frames 1555 and
1592/1593, all selecting stream 1. All 1,536 native writes reproduce complete
post-transfer VRAM, and every pending-flag acknowledgement matches. One transfer
crosses a frame boundary; no pending entry remains at capture end. Frames
595..624 contain no transfers; their empty trace is rejected as insufficient
validation, not reported as a pass. Other stream selections still need execution
coverage. The later capture retains identical RAM/CPU logs to the preceding
background trace, and its main actors, overlay and background checks all pass.
An unrelated VRAM mutation is rejected by the pattern verifier. Release checks
pass 5/5 and ROM-free Debug checks pass 2/2.

Remaining presentation work includes palette changes, HUD artwork/assembly and
native render scheduling. Pattern transfers are decoded and sampled here, but
have not yet been connected to the live renderer.

### Main-table HUD artwork and assembly

`decode_hud_assets` follows `00:EDD2..EDEA`: when RAM `$76` is zero, banks
`0F..11` map to `$8000..DFFF`, pointer `$8008` selects `$CAB7` (physical ROM
`$22AB7`), and the word-RLE loader writes at VRAM `$0500`. The 1,305-byte stream
decodes to 768 words, or 12 sprite patterns (decimal IDs 20..31). All 768 words
match the lower/upper reference samples at frames 600/720. The alternate loader
path for nonzero `$76` is not included in this main-table asset API.

`--extract-assets` now writes a ROM-hashed `table-hud-v1` directory with
`patterns.bin`, a provenance/hash manifest and `digits.bmp`. The preview shows
digits 0..9 with initial lower-table palette 15, on dark gray; it was visually
checked. Pattern data SHA-256 is
`977da766953084e148add71489da8baac80b690c87776f692ac3c5b5cf22d200`.

`assemble_table_hud` builds ten SAT entries from a settled score and stored ball
counter. The nine score digits occupy slots 0..8 at hardware Y=80 and X=48+10i;
the ball digit uses slot 9 at (264,96). All flags are `$008F`. Digit pattern words
are twice (20+digit), consistent with the lookup at `03:900E`; the ball routine
at `03:8FF0` subtracts one from the stored counter. Scores must be multiples of
100 in 0..999,999,900 and counters must be 1..10. Other states are rejected.

The existing discovery gate now compares all 40 HUD sprite words on each eligible
frame, rather than only decoding their displayed values. All 57,480 words match
on 1,437 frames. Eligibility still requires a recognizable source HUD, no pending
score update and a supported ball counter; this does not establish menu, bonus,
or transition validity. ROM-free tests cover decimal order, trailing hundreds,
zero/maximum scores, ball placement and invalid input. Release passes 5/5 and
Debug passes 2/2.

This API reconstructs settled HUD state. It does not imitate the original
incremental digit writes at `00:FA49`, own the update flag or replace live
rendering. Palette changes and native render scheduling remain open.

### Native palette fade-in

`advance_palette_fade` translates one iteration of `00:E4B7..E5CC`. The original
starts its 512-word palette buffer at RAM `$2A00` at black, then processes
thresholds 21 down to 1. A target color participates when its three channel
intensities sum to at least the threshold. One channel advances per participating
step: the channel with the largest remaining difference, with ties favoring red,
then green, then blue. Colors that have reached their target remain unchanged.
This is a discrete color-building effect, not linear brightness scaling.

The native function takes current/target palettes and a threshold, and returns
the next palette. It requires 512 nine-bit colors, thresholds 1..21, and target
channels no lower than current channels. It does not modify game RAM or emulate
the original waits/VCE upload. Tests cover brightness gating, channel tie order,
completion after 21 steps and invalid inputs.

The bounded actor tracing option now also writes `palette-fades.bin`, pairing
`00:E4B7` with `$E5CC` before the frame wait and VCE transfer. Schema `ACPF0001`
has 3,081-byte records: u32 entry/end frames, one threshold byte, then 1,024-byte
current, target and resulting palette buffers. All integers are little-endian.
The manifest records completed, orphan and pending pairs.

```powershell
.\tools\Invoke-MesenCapture.ps1 -MesenPath <pinned-Mesen.exe> -RomPath '.\Alien Crush.pce' -OutputDirectory out/mesen/palette-trace -Frames 370 -TraceActorsLastFrames 120
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --verify-palette-trace out/mesen/palette-trace/palette-fades.bin
```

This captures the complete fade beginning at frame 333: all 21 thresholds and
10,752 color words match; nine computations cross frame boundaries. No orphan
or pending pair remains. A fresh untraced 370-frame control matches all RAM and
CPU/clock logs. A deliberately corrupted output color is rejected. Release
checks pass 5/5 and ROM-free Debug checks pass 2/2. The 120-frame startup window
contains no such fade and is not treated as successful coverage.

Other palette behavior is still separate: direct level palette loads are already
extracted, while fade-out and the palette-zero pulse at `00:F64B` are not yet
translated. VCE upload timing and native renderer integration remain open.

### Native palette fade-out

`advance_palette_fade_out` translates `00:E605..E6AA`. Every nonzero red, green
and blue channel is decremented by one, without going below zero. The result
also reports how many of the 512 input colors were already black. The original
tests this count after waiting and uploading the palette; it finishes only
when the count is 512. Therefore a palette containing maximum-brightness
channels takes seven darkening passes followed by an additional all-black
completion pass. Checking the resulting palette alone would end one pass early.

The trace pairs `$E605` with `$E6AA`, before the two-frame wait and VCE upload.
`palette-fade-outs.bin` uses schema `ACFD0001` and 2,058-byte records: u32
entry/end frames, u16 already-black count, 1,024 input palette bytes and 1,024
output palette bytes, all little-endian. The existing verifier chooses the
fade-in or fade-out decoder from the header:

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --verify-palette-trace out/mesen/palette-trace/palette-fade-outs.bin
```

A 370-frame capture with the final 120 frames traced contains all eight fade-out
passes beginning at frame 301: all 4,096 color words and every already-black
counter match, including the final completion pass. Two computations cross
frame boundaries. No orphan or pending pairs remain. The same capture retains
the 21 exact fade-in steps and identical RAM/CPU logs to the untraced control.
A deliberately altered completion counter is rejected.

ROM-free tests exercise all 512 valid PCE colors, channel saturation, completion
counting and invalid colors/buffers. Release passes 5/5 and Debug passes 2/2.
This function owns no reference RAM, waits or palette uploads. Palette-zero
pulsing, VCE upload timing and native renderer integration remain open.

### Palette-zero pulse

`advance_palette_pulse` translates `00:F64B..F6A2` as a pure state transition.
Its four bytes correspond to RAM `$8A..$8D`: flags, green, red and blue. An
inactive flag (bit `$80` clear) leaves state unchanged and emits no palette write.
Ascending calls increment all three bytes; when green is already seven, the
routine replaces flags with `$C0` and starts decrementing. Descending calls clear
flags after green reaches zero. Starting from black with flags `$80`, the cycle
emits 14 colors: levels 1..7, then 6..0. The native function preserves original
byte wrapping and eight-bit packing before VCE's nine-bit color mask.

The diagnostic capture now appends pulse state to `game-observations.csv`.
A 10,000-frame canonical run found no active flag in the post-frame observations.
This route therefore does not supply active pulse execution coverage. Synthetic
tests check the complete cycle, direction/completion flags, inactive preservation
and byte underflow; these are not labeled as original-game active equivalence.

Bounded tracing pairs `00:F64B` with its caller's continuation at `00:E1D3`.
`palette-pulses.bin` uses schema `ACPZ0001` and 2,064-byte records: u32 entry/end
frames, four input state bytes, four output state bytes, then complete 1,024-byte
before/after VCE palettes. The manifest records completed, orphan and pending
pairs. The verifier checks all state fields, palette-zero output when active,
and preservation of all other colors:

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --verify-pulse-trace out/mesen/pulse-trace/palette-pulses.bin
```

The 370-frame capture's final 120 frames contain 107 paired inactive calls,
all exact, with no orphan or pending pairs. RAM and CPU/clock logs match the
untraced control. An unexpected nonzero-index palette change is rejected.
The verifier explicitly reports active/inactive counts and warns of missing
active coverage when there are no active calls. Release passes 5/5 and Debug
passes 2/2. Active gameplay coverage, VCE upload scheduling and live renderer
integration remain open.

## Diagnostic graphics and validation

`--video-every N` exports background/sprite/composite BMPs, attributes, VRAM/SAT/palette and
metadata from named save fields. Observation installs no execution hooks. Sprite
decoding handles bitplanes, sizes, alignment, flips, transparency, palette selection
and alternate 2-bit mode. `composite.bmp` combines a frozen 256x240 viewport using
the latched SAT, raw scroll origin and layer-enable bits. It preserves background
color-index opacity independently of RGB, resolves SAT priority before background
priority, wraps scroll at BAT boundaries, and clips sprites at viewport edges.
Sprite-only display uses palette entry 256 as its backdrop; both layers disabled
use entry zero, following the pinned VDC implementation.
The compositor enforces sixteen 16-pixel sprite pieces per row, counting transparent
and horizontally offscreen pieces in SAT order. A 32-pixel sprite consumes two
pieces and can lose its screen-right half, including when flipped. This follows
the pinned VDC `FetchSpriteData` implementation with sprite limits enabled.
Post-frame registers are not scanline histories: this preview omits
register/VRAM/palette changes during display and display-window timing.
It does not replace the live renderer or establish full-frame pixel equivalence.

`out/captures/compositor-1800` contains 15 exported previews, including visually
checked lower/upper tables at frames 600/720. Its complete RAM/video/audio census
retains the frozen SHA-256
`ee560dad47ece1a3c2e55a4326bcd4ef97276ad1ac0912d6c36eb29298c6572e`.

Composition timing comparison:

- `--video-every N` now saves the corresponding `reference.bmp`, exact ARGB
  `composite-difference.bmp` (magenta differs, black matches), and per-row counts
  with inclusive first/last differing x coordinates (`-1` for matching rows).
- A separate `previous-sat-composite` candidate uses the preceding frame's latched
  SAT and current-frame VRAM, palette and registers. Both candidates remain visible;
  neither is aligned, cropped or selected opportunistically. The saved
  `previous-sat.bin` and manifest identify the input epoch. Frame pixels are copied
  before save operations can invalidate the runtime's borrowed views.
- `--compare-video --capture <directory> --compact` writes compact all-frame
  `composite-epochs.csv` without requiring image exports. Its columns count complete
  viewport pixel differences for both fixed candidates. The first frame has an
  empty previous-SAT result because no preceding frame was captured.
- `out/captures/composite-all-1800`: current SAT matches 503/1,800 frames; previous
  SAT matches 1,767/1,799, including all 15 samples at 120-frame intervals. These
  results support a sprite-table timing explanation for most original differences,
  but do not establish complete scanline equivalence.
- Before adding the sprite fetch limit, previous-SAT mismatches occurred at frames
  19, 608, 622, 758, 938..958, 961..963, 966..967, 1573 and 1594. A separate frame-938
  capture has five differing pixels on row 64 (x 113..118); an independent BMP
  byte comparison verifies both candidate masks and row counts for that sample.
- `out/captures/composite-limited-1800` repeats every-frame comparison with the
  hardware sprite fetch limit: previous SAT matches 1,793/1,799 frames, current SAT
  remains 503/1,800. All 26 formerly mismatching frames in 938..967 now match exactly;
  frame 938's slot 49 was the seventeenth piece on row 64. No formerly exact frame
  regresses. The six unresolved frames and pixel counts are 19 (46,848), 608 (402),
  622 (48,783), 758 (72), 1573 (49,017), and 1594 (48,301). Their causes remain
  unverified. The complete reference census retains the same frozen hash.
- ROM-free fetch-limit tests cover transparent/offscreen budget consumption,
  truncated wide sprites with horizontal flip, exact capacity, exhausted rows,
  and reset on subsequent rows. Release passes 5/5 and Debug 2/2.
- The all-frame comparison retains the exact frozen 1,800-frame RAM/video/audio
  census hash above. Initial three-frame capture checks the missing first epoch
  and consecutive exports. Full native gameplay/presentation integration remains open.

Video component timing investigation:

- All-frame reports now retain six fixed candidates. The original current/previous
  SAT columns are preserved; four appended columns use previous SAT plus previous
  VRAM, palette, registers, or the complete previous video snapshot. Each isolated
  candidate keeps the other components current. No best-match selection occurs.
  Initial-frame previous fields remain empty. Consecutive snapshot frames are
  required. Video exports include previous VRAM/palette/SAT bytes and register
  values, plus each candidate's image, mask and row counts.
- `out/captures/composite-components-1800` retains the exact frozen census and
  original candidate results. Among 1,799 comparable frames, previous-SAT-only
  matches 1,793; previous VRAM 1,749; previous palette 909; previous registers
  1,759; complete previous video 870. Globally delaying more state is insufficient.
- Frames 622, 1573 and 1594 match exactly with previous registers. Targeted exports
  show only control changes, from 8 (layers disabled) to 200 (layers enabled);
  memory width and scroll remain zero. Their output still shows disabled layers.
  These samples establish an enable-timing difference; they do not establish a
  general register scheduling rule.
- Frame 19 matches the previous palette on rows 0..182 and the current palette
  on rows 183..239. Both have disabled layers; neither frozen palette reproduces
  the whole frame. Targeted evidence is in `composite-components-19`.
- Frame 758 mixes background map epochs. With current VRAM, 72 pixels differ on
  rows 32..42 at x 130..138. With previous VRAM, 68 pixels differ on rows 43..47
  and 115..126. The mismatch sets are disjoint: every reference pixel matches
  at least one candidate, but neither complete candidate is exact. Palette or
  register substitution alone has no effect on this mismatch. The four BAT words
  0090/0091/00B0/00B1 changed palette selection from 9 to 8. This supports a
  during-display BAT update; exact write times still need instruction/scanline traces.
- Frame 608's remaining 402 differing pixels occupy only rows 0..1, while control
  changes from 200 to 8. Older registers alone produce 49,150 mismatches. The exact
  disable/VRAM timing remains unresolved (`composite-components-608`).
- Independently checked all six frame-758 BMP masks and every CSV row against
  source/reference BMP bytes. A three-frame capture checks absent initial epochs
  and consecutive exports. Release tests pass 5/5. No renderer timing behavior or
  reference execution was changed; live integration remains open.

Native composition with supplied raster writes:

- `render::compose_raster` accepts up to 65,536 ordered writes with explicit
  effective output rows. VRAM, palette, SAT, control, scroll and memory width are
  supported. Writes on one row retain input order; row zero changes initial state.
  Bounds and ordering are checked before rendering, and caller memory/registers
  remain immutable. Rows already drawn are retained. Absolute sprite coordinates,
  full-frame scroll origin, priority and the sixteen-piece row limit are preserved.
- Composition renders bands between writes; background lookup samples only the
  required output pixels instead of decoding a complete BAT image per band.
  Bus-to-render latency, register latches, burst mode, and changes within a row
  must be handled by a future scheduler. This API does not infer effective rows.
- `--render-raster <video directory> --raster-writes <CSV> --capture <new directory>`
  loads an exported state and reference BMP without executing the reference core.
  The strict CSV schema is `row,target,address,value` using decimal uint16 fields
  and the target names from the public API. Header-only input renders the initial
  snapshot. Results include `raster.bmp`, exact difference mask, per-row counts,
  the supplied writes, and input hashes. Pixel mismatch is diagnostic, not an error.
- `out/captures/raster-palette-19`: row-zero palette entry 0 uses the previous
  value; at row 183 it takes the current value. All 61,440 pixels match.
  `out/captures/raster-bat-758`: row zero explicitly supplies previous SAT and
  previous BAT words 0090/0091/00B0/00B1; row 43 supplies their current values.
  All 61,440 pixels match. Both BMPs are byte-identical to exported references.
  These are reconstruction experiments using boundaries identified from pixels,
  not independently scheduled emulation-equivalence tests. They do not raise
  the automatic 1,793/1,799 fixed-candidate match count.
- Empty schedule frame 758 is byte-identical to the current frozen composite
  (237 mismatching pixels). Bad field counts, negative values, unknown targets,
  out-of-range rows and backwards order are rejected before output creation.
  ROM-free tests additionally cover same-row ordering, last-row writes, state
  immutability, SAT/pattern changes, scroll origin, map width and event bounds.
- `out/captures/raster-regression-1800` retains all six fixed candidate counts for
  every frame and the exact frozen RAM/video/audio census. Release passes 5/5;
  Debug passes 2/2. Live renderer integration remains open.

Observed display control scheduling:

- `cmake/ReferenceVideo.cmake` generates a copy of pinned `vdc.cpp` with an optional
  callback at its existing HPHASE_HDW draw boundary during VDW. It reports RCRCount,
  CR_cache and burst_mode only. The original dependency source is not rewritten;
  no game RAM/ROM, register, cycle count or renderer decision is changed. The
  callback is disabled at runtime unless explicitly requested and can be removed
  entirely by configuring `AC_REFERENCE_VIDEO_OBSERVER=OFF`.
- Runtime observations use a bounded buffer and do not allocate or throw inside
  the callback. Overflow is reported by the diagnostic accessor. Load, reset,
  restore and frame advance clear stale observations; close detaches the callback.
  Observer state is not serialized and does not change quick-state compatibility.
- `--compare-display` adds a seventh, reference-assisted candidate. Current VRAM,
  palette, scroll and MWR combine with previous SAT and observed per-row control.
  `display_control_schedule` emits explicit control and burst-mode values,
  including initial values, and coalesces unchanged consecutive values. Burst mode
  suppresses both layers and selects border palette entry 256. It requires
  exactly one ordered observation for each output row. Missing/duplicate/extra rows
  are unavailable, not filled from post-frame state. This translates latched
  observations; it does not independently implement hardware latch timing.
- `out/captures/display-observer-1800`: 1,795/1,798 complete comparable frames match.
  Frames 1/2 have 372/242 observations and are excluded; frames 3..1800 have 240.
  On that same comparable subset, the previous-SAT-only candidate matches 1,792.
  Observed control resolves frames 622/1573/1594 without manual row choices.
  Remaining differences: frame 19 (46,848 pixels), 608 (470), 758 (72). Frame 608
  already differed by 402 pixels in the fixed candidate; using correctly observed
  control alone exposes additional differences from frozen VRAM/other video state.
- Frame 608 observes control C8 on rows 0/1 and 08 from row 2, with burst mode off.
  Frame 622 observes control 08 on rows 0/1 then C8, but burst mode stays on, so
  effective rendering remains disabled. Exported `display-writes.csv` reproduces
  frame 622 exactly via offline `--render-raster` (`display-replay-622`).
- `display-rows.csv` retains every raw observation; `display-comparison.csv` records
  coverage and differences; `display-manifest.txt` records the hook, candidate and
  limitations. The initial three-frame capture verifies explicit missing coverage.
  With `--verify-replay`, `display-restore-1800` verifies all 900 post-checkpoint
  row observations as well as RAM/video/audio. Repeat and initial-state replay pass.
- Observer-enabled and compiled-out captures retain the exact frozen census
  (`display-observer-off-1800` for the latter). Compiled-out builds explicitly reject
  observation requests. The final workspace build has observation support enabled.
  Release passes 5/5 and Debug 2/2, including complete-coverage and burst-mode tests.
  Palette/VRAM timing and independent native scheduling remain open.

Pixel-timed palette continuation:

- Added a VCE observer after `Sync`, before each palette byte write, recording the
  complete old/new nine-bit color and next framebuffer cursor. The final display
  rectangle normalizes both coordinates; no pixel-alignment search or hand-picked
  row is used. `ReferenceVideo.cmake` composes this observer with the serialization
  extension in generated copies, preserving the optional observer-off build.
- Corrected an earlier inference about the startup split: while burst mode is
  active, the VDC emits border index 256 (its no-line-buffer path), not palette zero.
  Entry 0 changes before row 175. Entry 256 changes beyond the right edge of row
  182, so the visible transition starts at row 183. The earlier manual palette-zero
  experiment reproduced pixels under a simplified model; it did not identify the
  hardware palette entry. Native burst mode is now explicit in `Viewport` and
  raster writes, separate from both layer-enable bits being cleared.
- `compose_palette_raster` retains palette indices through background/sprite
  composition, then applies ordered palette updates before individual output pixels.
  Equal RGB values from different palette entries remain distinct, and within-row
  changes do not recolor earlier pixels. Inputs remain immutable; invalid positions,
  backwards order and ambiguous row/pixel palette timelines are rejected.
- `palette_pixel_schedule` verifies every old/new write against the previous full
  palette and requires the complete final palette to match. Pre-display writes
  affect pixel zero; writes beyond the right edge affect the next row; writes after
  frame completion only participate in final-state verification. No-op writes stay
  in raw evidence but need no pixel event. All observed writes fit these rules in
  the canonical run; missing chains or nonchronological output positions fail.
- `palette-timed-1800`: display control plus palette timing matches 1,796/1,798
  complete frames. Frame 19 now matches automatically. Only 608 (470 pixels) and
  758 (72 pixels) remain; VRAM timing is still frozen. All six original fixed
  candidate counts remain unchanged. Display-only remains 1,795/1,798.
- Exports retain `palette-writes.csv` (raw ordered observations), normalized
  `palette-pixels.csv`, `palette-initial.bin`, `palette-timed.bmp`, its exact mask
  and row counts. Frame 19's exported BMP is byte-identical to the reference.
  Evidence directories: `palette-timed-export-19` and `palette-timed-export-758`.
  These observations still depend on the reference; this is not independent device
  scheduling or live presentation integration.
- All 900 restored display and palette observations repeat, alongside existing
  RAM/video/audio replay gates. Enabled and compiled-out observers retain the
  frozen census (`palette-observer-off-1800`); the final build enables observers.
  Release passes 5/5 and Debug 2/2, including burst border selection, palette identity,
  same-pixel ordering, within-row timing, event bounds and full-palette validation.

RGB conversion follows the reference's RGB565 quantization. ROM-free tests cover
bounded field parsing, tile planes, universal background color zero, dormant patterns,
sprite geometry, RLE edge cases and strided scores. Capturing diagnostic video every
120 frames preserves the exact frozen 1,800-frame RAM/video/audio census.

Mesen can optionally trace VDC/VCE bus writes for the final 1..10 frames. Callbacks
return no replacement values. The `video-bus-timing-v1` manifest describes ports,
coordinates and limitations. The original CSV fields remain first; appended fields
record device, master clock, raw scanline/hclock/RCR, pre-write VRAM address/latch
and palette address/color. Snapshots cover the end of the preceding frame, the
trace window and the final frame. Only CPU ports 0000..0003 and 0400..0405 are
observed, including ST0/ST1/ST2; mirrored ports, DMA and delayed commits are not.

`Inspect-VideoTiming.ps1` validates schema, ROM, record count, chronology and numeric
bounds before decoding VRAM high-byte requests, control low bytes and palette
byte writes into `video-events.csv`. It preserves raw coordinates and does not
assign native update times. Evidence from `out/mesen/video-timing-*`:

- Frame 758: four BAT requests from 03:9337 target 0090/0091/00B0/00B1 with values
  80F3/80F4/8113/8114. Raw (scanline, hclock, RCR) values are (59,291,42),
  (59,753,42), (59,1353,43), (60,450,43). This supports the raster-42/43 change
  inferred from Beetle pixels, while leaving exact fetch/commit latency open.
  All 235 last requested VRAM/palette values across the two-frame window match
  final Mesen memory. The trace has 1,320 bus writes / 456 decoded data requests.
- Frame 608 disables layers at 00:E2F6, raw (18,315,1), writing CR low byte 08.
  Frame 622 enables layers at 00:E2E3, (18,408,1), writing C8. Pinned Beetle
  `vdc.cpp` sets `burst_mode` on entry to VDW from CR bits C0 and caches CR each
  scanline. Enabling after entering a disabled display therefore cannot be modeled
  by simply using post-frame CR. These traces support the transition timing
  explanation; native scheduling is still unimplemented.
- Frame 19 writes palette zero from 0B0 to 000 at 1C:4090, raw (188,291,171),
  followed by its high byte at hclock 312. Mesen's startup timing does not match
  Beetle's observed palette split at output row 183. Do not substitute Mesen's
  startup coordinates into the Beetle renderer.
- The traced and untraced 758-frame captures have identical complete RAM and
  CPU/clock logs. Their pixel differences are confined to frames 1..19, before
  trace output begins at 757; frames 20..758 match exactly. This reproduces the
  previously observed startup-only capture variation, not full pixel equality.
- A three-frame capture verifies snapshots at 1/2/3 and normal trace completion.
  Inspector rejects truncated, backwards-clock, out-of-range-value and unknown
  schema fixtures. Existing native/reference source and test binaries are unchanged
  by this tracing increment.

`Find-VideoRomMatches.ps1` locates exact runs, but repeated
artwork means a match alone does not prove the loader path.

Scanline VRAM observation (2026-09-08):

- `ReferenceVideo.cmake` now passes read-only VRAM at the existing HDW observation
  after `CheckAndCommitPending`, before background drawing. The host stores a full
  first-row image followed by deltas, bounded to 65,536 words/events per frame;
  callbacks do not allocate or mutate emulation. Incomplete row coverage fails closed.
- `vram-timed-1800` matches all 1,798 complete frames using current scroll/MWR,
  previous SAT, observed CR/burst, palette pixels and scanline VRAM. No reference
  pixels are consulted to construct schedules. Frames 1/2 remain excluded.
- Frame 758 actually observes BAT 0090/0091 at row 42 and 00B0/00B1 at row 43.
  This replaces the earlier manually inferred simultaneous row-43 schedule.
  Frame 608 also becomes exact by retaining the VRAM read during its early rows.
- `vram-timed-export` includes byte-identical native/reference BMPs at 608 and 758.
  Schema `reference-display-rows-v3` retains earlier comparison columns and adds
  `vram_different_pixels`. Exported `vram-timed/vram-rows.csv` contains full initial
  VRAM and deltas; combine with the parent display/SAT schedule and palette inputs.
- All 900 restored VRAM schedules match field-encoded SHA-256 signatures. Both
  observer-enabled and compiled-out runs retain census SHA-256
  `ee560dad47ece1a3c2e55a4326bcd4ef97276ad1ac0912d6c36eb29298c6572e`.
  This verifies the canonical route, not arbitrary raster programs or independent
  hardware timing. Live cabinet presentation still uses the reference framebuffer.

Next: scene/launcher validity, launch arithmetic, ball update epochs,
actors and palette rules; then continuous table rendering and cabinet integration.
