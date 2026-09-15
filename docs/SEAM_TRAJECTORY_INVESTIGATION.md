# Seam trajectory investigation

The cabinet ball had a confirmed presentation discontinuity at the seam.
The investigation below records the original fault; the subsequent correction
is described at the end. Gameplay physics remains unchanged.

## Coordinate mismatch

The original ROM adds 224 to the ball's local Y when entering the upper half
(`03:89B9..89C7`), and subtracts 224 on entering the lower half
(`03:8A0A..8A18`). A continuous game-world Y is therefore local Y plus 224
in the lower half, or local Y in the upper half.

The artwork is stitched with a 216-pixel offset. `TablePlayfield::transition_ball`
also applies that artwork offset to the lower-half ball. For the normal ball
descriptor, its rendered top edge is world Y minus 78 in the lower half,
but world Y minus 70 in the upper half (rounded to whole pixels).
Changing halves consequently introduces an 8-pixel display displacement:
downward on entering the upper half, upward on entering the lower half.
Slow or nearly horizontal trajectories make this especially visible.

## Replay evidence

The `continuous_handoff` test now records fractional coordinates, signed
velocities and the actual `transition_ball` output. In addition to the ordinary
1,800-frame route, it records 20 frames after each of ten genuine original
pre-handoff checkpoints imported into cabinet execution.

Example from checkpoint 4, near the left side (Y increases downward):

| Frame | Half | World Y | Y velocity | Rendered X | Rendered Y |
| --- | --- | ---: | ---: | ---: | ---: |
| 1380 | Lower | 305.645 | -2.49609 | 50 | 227 |
| 1381 | Upper | 300.965 | -2.46094 | 52 | 230 |

The ball moves upward by 4.68 game pixels, yet its displayed top edge moves
downward by 3 pixels. This is the 8-pixel mapping change plus integer rounding,
not a reversal of its vertical velocity.

Checkpoint 5 shows the reverse: world Y increases from 287.266 to 289.078,
while rendered Y decreases from 217 to 211 on entering the lower half.

The same test passed its original-execution comparisons at all ten handoff
entries: collision pointers, flags and both complete actor banks match at four
instruction boundaries per entry. This supports preservation of the original
handoff physics in the tested cases; it does not claim that every apparent
deflection is artificial. Original collisions can also occur near the seam.

The trace additionally shows uneven motion sampling around some handoffs:
more movement in the crossing frame followed by a repeated position on the
next frame. This is separate from the coordinate jump and can amplify it.

Artifacts (generated under the Windows release build directory):

- `continuous.acrec.csv`: ordinary cabinet route with trajectory columns.
- `continuous.acrec.entries.csv`: checkpoint trajectories in both directions.
- `out/trajectory-tests.log`: passing original handoff comparisons.

## Correction scope

The ball needs a continuous presentation mapping through the seam. Simply
changing the entire stitched offset to 224 would undo the verified artwork
alignment; simply shifting the ball everywhere in one half would displace it
from visible collision surfaces. A correction must remove the half-dependent
jump while checking ball-to-art alignment near the join, and address any
remaining motion sampling issue separately. The original physics should remain
the reference when verifying that correction.

## Implemented correction

`transition_ball` now reconstructs fractional world Y using the original
224-pixel origin change, then applies one mapping independent of the active
half. World Y 256..320 smoothly absorbs the eight-pixel artwork difference
using `8 * t*t*(3-2*t)`. The mapping is monotonic, with the normal slope at
both endpoints. It preserves the upper native alignment before this band and
the lower native alignment after it; the artwork itself stays at offset 216.
Only ball presentation changes, with no accumulated offset or crossing state.

The test samples quarter-pixel positions over world Y 224..352 at four X
positions. Equivalent upper/lower coordinates must produce identical screen
positions, Y must remain monotonic, and both native alignments must hold
outside the join. These checks cover cold projection and either crossing
direction without requiring a previous frame. The original-timing replay also
checks equivalent coordinate systems during both actual handoff waits.
Mouth occlusion and full-table animation checks remain part of verification.

This corrects the coordinate jump. It does not change the separate uneven
sampling of motion around some handoffs documented above.
