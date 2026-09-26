# Cabinet actors

In cabinet mode the port runs the stored half of the table itself: every actor
there that moves on its own, and the half's palette scripts. The cabinet draws
both halves straight from the game's actor banks, so what it shows is what the
ball meets. Desktop mode runs the cartridge alone.

## Why

The cartridge runs only the half of the table the ball is in; the other half
is frozen until the ball returns. The cabinet shows both halves at once, so it
used to animate a private copy of the frozen half, and kept drawing that copy
after the ball came back. The copy drifted from the game's state. Over 12,000
scripted frames, in the half the ball was in:

- the lower table's bouncing orbs were drawn in the wrong place 89% of the
  time, and the upper table's 87%;
- the brain's orbs 27% of the time;
- the orange aliens were drawn solid or not wrongly 30% of the time;
- the mouths, eyes and lane sensors showed the wrong pose now and then.

The ball went through drawn actors and bounced off hidden ones. The first fix
(`-aliens1`) moved only the lower aliens to native code; this one covers the
rest, and the copy is gone.

## What the port runs

`gameplay::CabinetActors` (`src/gameplay/cabinet_actors*.cpp`) translates each
routine with the services it calls: the random number generator (F9A2, F993),
motion (FADA with FB03, FB59, FBB6, FC10), sprite and pattern animation (FD2B,
FD30, FD1D), headings (03:8F52 with F726/F81B, 03:9022), actor allocation and
removal (46DE, 46CE, 46AD), palette requests (EF04..EF91), table lamps
(03:925F, 9277) and scoring (FA27).

| Table | Types | Actors |
| --- | --- | --- |
| Lower | 15, 16, 21, 22, 25, 67, 68, 72, 73 | Orange and green aliens, silver variants, bases, group controllers, defeat effects |
| Lower | 20, 56 | Central mouth and the flyers it releases |
| Lower | 26, 27, 40, 41, 52, 53 | Cocoons, the aliens that hatch from them, slingshot flyers |
| Lower | 29, 30, 33 | Rising blockers and the skull guard |
| Lower | 45, 46, 47, 70, 75, 76, 77 | Progress indicators and eyes, bouncing orbs, lane sensors |
| Upper | 7, 55 | The brain and its flyers |
| Upper | 8, 37, 38, 39, 9, 10, 51 | Targets, rebound, bumpers and their travel |
| Upper | 11, 12, 13, 34 | The four mouths |
| Upper | 14, 19, 66, 71 | Skull guard, brain orbs and their controller, bouncing orbs |
| Upper | 24, 32, 35, 42, 44, 48, 74 | Animated decorations, riser, indicators, timer lamp, lane sensor |
| Both | 50, 59, 60 | Explosions |

Flippers (2, 3) follow the controls: the stored half's are drawn in the pose
of the ball's half's. The end-of-ball sequence's actors (17, 18, 23, 43, 58,
61, 62) never outlive a visit to the other half, so they stay the ROM's.

The palette scheduler (00:F292, each vblank) steps only the ball's half's 32
slots. In cabinet play the port steps the other half's scripts at the same
moment. Their colors are uploaded only when the half is shown, so the cabinet
draws the stored half's colors from each running script's frame.

## Who runs what

The ball's half runs the cartridge's own routines, untouched. The port's
updates take no emulated time, and the upper table normally runs its game loop
over two frames; running them there would take that slowdown away and make
the upper table play faster than the original.

The stored half runs when one of the table's game loops starts the dispatcher
(01:45A8): 01:4098 (play), 01:4035 (demo), 00:FDD0 and 00:FEC7. So it pauses
and waits between balls exactly as the ball's half does. There the port
changes nothing the original would not:

- It draws random numbers from a hash of RAM, without advancing the game's
  generator.
- It writes no zero-page temporaries and plays no sound.
- It ignores `$69`, which carries events for the ball's half (hide, game
  over).
- There is no ball in the stored half. Ball checks (03:8585) find none, and the
  push off a moving actor (03:852F) has nothing to push.
- An actor that would score, touch the ball, run the game's rules (03:911B,
  9415, 94DD, 01:499E and others) or draw on the ball's table waits, unchanged,
  until its half is the ball's again. The ROM then carries on from where it
  waited. So does an actor with an unprocessed hit.
- Lamps of the stored table change only their bits (`$D6..$D9`); the table
  redraws them from those bits when it is loaded (03:9415).

The table's second dispatch in a crossing frame (03:89D8 / 8A33) skips the
actors the stored-half pass has already updated that frame.

## Differences from the original

- Everything that follows from the stored half moving: the lower aliens,
  cocoons, mouths, risers, orbs and indicators are wherever their cycles have
  taken them when the ball returns. The first upstairs trip is where cabinet
  play and the original part ways.
- Some finished sequences settle while the ball is away. A mouth whose
  600-tick hold ends closes; a skull guard reset by the ball leaving the upper
  table (`$70` bit 1) resets at once instead of on the next visit. A cleared
  group of aliens or brain orbs respawns 300 ticks later, while the ball is
  still away. The upper table's first five orbs are there from the start.
- Actors that would score or call the game's rules wait for the ball instead:
  a riser lowering after its flag clears, the brain relighting the lamps, the
  lower mouth releasing its flyers.
- Runtimes with cabinet actors carry `-actors1` in their revision. Recordings
  made with `-aliens1` replay with only the lower aliens native, as they were
  made; older cabinet recordings replay with none. Either way, the stored
  half's other actors are drawn frozen. Earlier cabinet quick states don't load.

## Verification

`cabinet_actors` (`tests/cabinet_actors_tests.cpp`):

- The translations against the ROM. In verification (`ActorMode::verify`),
  play is the original's; at each owned routine the port predicts the update
  from the RAM it starts with, and when the ROM reaches the point where the
  port would stop (01:45C8, or where it hands the routine to the ROM), its RAM
  must match to the byte, the stack aside. Three routes (36,000 frames, 8,485
  upstairs) and 21 edited states that set off the rarer sequences (10,020
  frames): 144 type and action states over 50 types, no difference.
- The stored half on synthetic RAM: cocoons hatch aliens that fly off, the
  lower mouth holds and closes with its lamp off, a guard and a riser wait for
  the ball rather than score, a pending hit waits, the orbs' controller brings
  five back, bouncing and brain orbs keep moving, palette scripts step only the
  stored half. Nothing touches the ball's bank, the game's RNG, score, sound or
  temporaries.
- 12,000 frames of cabinet play: every drawn actor equals the game's, in both
  halves (1,355,044 checks). The stored half keeps moving, its palette scripts
  step, a quick state replays 1,500 frames identically, and a group cleared
  before an upstairs trip respawns and shows while the ball is up.
- `-aliens1` runtimes still play the original's game until the ball first goes
  upstairs (2,441 frames compared).

`inactive_animation` renders cabinet play: both stored halves are drawn moving,
the picture doesn't depend on which half holds the ball, rendering never
changes the game, pause freezes the image, and replays stay exact.
