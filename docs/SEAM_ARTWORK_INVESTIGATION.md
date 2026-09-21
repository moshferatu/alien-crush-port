# Main-table artwork join

Rechecked 2026-09-20 against the ROM-decoded background BMPs and the current
stitcher. The lower image begins at global row 216; each half contributes a
240-row viewport, producing a 256x456 table. Upper background wins the overlap
(rows 216..239); lower sprites can still cover it. Lower background begins at
global row 240. This ownership does not switch when the ball changes halves.

Comparing lower rows 0..23 against upper rows offset by 208, 216 or 224 gives:

| Offset | Matching side-wall pixels | Matching central pixels |
| --- | --- | --- |
| 208 | 308 / 576 | 753 / 2304 |
| 216 (current) | 576 / 576 | 1320 / 2304 |
| 224 | 314 / 576 | 761 / 2304 |

Side-wall columns are 0..7 and 232..247. Central columns are 80..175. These
measurements use the initial ROM palettes, without actors or animation. The
side walls align exactly at the current offset, but 984 central pixels still
differ in that 24-row overlap. Inspection of both complete source backgrounds
and the stitched preview shows different central texture/detail at the join.
This is source-art mismatch, not evidence of an eight-pixel alignment error.
Moving the whole lower half by a tile makes the side walls worse.

Devil's Crush's `src/app/main.cpp` decodes a single 32x96-tile background map
from ROM offset `0x14 * 0x2000 + 0x000A` and projects actors into that continuous
table. Alien Crush reconstructs two separately loaded viewport maps. Thus the
ports do not face equivalent artwork joins: Devil's Crush can display a
continuous background already present in its ROM.

No seam artwork was changed in this fix. A less conspicuous Alien Crush join
would require choosing or retouching texture through the overlap (or blending
pixels), beyond simply correcting the placement. This check establishes the
static join geometry; it does not claim every possible animated palette state
has been visually checked on the physical cabinet.
