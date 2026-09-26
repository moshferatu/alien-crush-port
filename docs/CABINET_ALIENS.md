# Cabinet aliens

In cabinet mode the port runs the lower table's aliens itself, in both halves
of the table. What the cabinet draws for them is the state the ball collides
with. Desktop mode still runs the cartridge's own routines.

## Why

The cartridge runs only the half of the table the ball is in; the other half
is frozen until the ball returns. The cabinet shows both halves at once, so it
used to animate a private copy of the frozen aliens and then keep drawing that
copy after the ball came back. The copy drifted from the game's aliens: over a
scripted 12,000-frame game, the drawn orange aliens disagreed with the game's
on whether they were solid 30% of the time, including about a third of the
frames the ball was near one. The ball went through drawn aliens and bounced
off hidden ones.

The original game has one such window of its own. An orange or green alien
can't be hit while it climbs out of its hole or ducks back in: the ball
physics (03:84D4) skips actors with bit 6 of `$3540` set, and the aliens clear
it only once fully out. That stays as it was.

## What the port runs

`gameplay::CabinetAliens` (`src/gameplay/cabinet_aliens.cpp`) translates each
routine, with the services they call: the random number generator (F9A2,
F993), motion (FADA), sprite animation (FD2B), actor allocation and removal
(46DE, 46AD), palette requests (EF04..EF79) and scoring (FA27).

| Type | Actor | ROM |
| ---: | --- | --- |
| 15 | Orange alien | 01:5F82..02:60DE |
| 16 | Silver orange alien | 02:60E1..619C |
| 21 | Green alien | 02:619D..62C4 |
| 22 | Silver green alien | 02:630C..63CC |
| 25 | Green alien's base | 02:63CD |
| 67 | Orange group controller | 01:5DA5..5E6F |
| 68 | Green group controller | 01:5E70..5F4A |
| 72 | Orange defeat effect | 02:6552..65AA |
| 73 | Green defeat effect (takes its base) | 02:65AB..65FC |

The state stays in the actor banks of work RAM: `$3500` for the ball's half
and `$2200` for the other, which 01:46FD exchanges. The original ball physics
still collides with it there, and quick states and recordings save it with
everything else.

In the ball's half, a CPU hook (`src/runtime/handoff_hook.inc`) catches the
dispatcher (01:45A8) entering one of these routines. It runs the port's update
instead and returns to the dispatcher. Two outcomes still continue in ROM code
that isn't the aliens': a defeat effect's sound (00:EA62), and a cleared
group's rules (03:917F, 03:9168), which award progress and request the next
controller. An update in this half is the ROM routine's to the byte, down to
its game RNG calls and zero-page temporaries.

The other half runs when one of the table's game loops starts the dispatcher:
01:4098 (play), 01:4035 (demo), 00:FDD0 and 00:FEC7. So it pauses and waits
between balls exactly as the ball's half does. There the port changes nothing
the original would not:

- It draws random numbers from a hash of RAM, without advancing the game's
  generator. The upper half's randomness is the original's.
- It writes no zero-page temporaries and plays no sound.
- It ignores `$69`. That byte carries events for the ball's half (hide, game
  over). In the original, 01:4815 clears it before the lower aliens run
  again, so they never see events set while the ball was upstairs.
- An alien with an unprocessed hit waits until its half is active again.

An actor updated this way is not updated again when the ball comes down in the
same frame and the table's second dispatch (03:89D8) runs its new half.

The presentation (`TableAnimation`, `ActorAssets::advance_animation`) no
longer animates these types. It draws them from the banks as they are.

## Differences from the original

- Everything that follows from the lower aliens moving while the ball is
  upstairs: they are wherever their cycle has taken them when the ball returns.
  The first upstairs trip is where cabinet play and the original part ways.
- A group cleared just before the ball goes up respawns 300 ticks later, while
  it is still up, instead of waiting for it to come down.
- Runtimes with cabinet aliens carry `-aliens1` in their revision. Cabinet
  recordings made before this replay without them: their lower aliens now stay
  frozen while the ball is upstairs, since the cosmetic copy is gone. Earlier
  cabinet quick states don't load.

## Verification

`cabinet_aliens` (`tests/cabinet_aliens_tests.cpp`):

- Each routine, one state at a time, including the parts replays can't reach
  on demand: both halves, hits, silver reveals, group clears, respawns in the
  stored bank, defeat effects and their bases, and the one-update-per-frame
  guard.
- Six scripted routes, native against the original, until the ball first goes
  upstairs: 5,291 frames with identical RAM (the stack page aside) and
  pictures. Frames that end with either game still working are not compared.
  The native updates take fewer CPU cycles, so such a frame catches the two at
  different points; the next frame matches again.
- Hits, the last of a group, the controller's respawn, a silver respawn and a
  silver hit, for both groups. Each is edited into a saved state the way the
  ball physics leaves a collision, and both games run from it: 1,560 frames,
  identical.
- 12,000 frames of native play: every drawn lower alien equals the game's, in
  both halves (36,848 checks). The stored half keeps moving (6,978 updates
  while upstairs), a quick state replays 1,500 frames identically, and a group
  cleared before an upstairs trip respawns and shows while the ball is up.
