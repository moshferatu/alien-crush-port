# State and asset discovery

All addresses apply only to ROM SHA-256
`1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f`.
CPU work RAM at `$2000..$3FFF` corresponds to capture offsets `$0000..$1FFF`.
ROM offsets are physical file byte offsets; `bank:PC` uses 8-KiB banks.
Generated disassemblies, traces and artwork remain in ignored local output.

## Established observations

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

## Diagnostic graphics and validation

`--video-every N` exports background/sprite BMPs, attributes, VRAM/SAT/palette and
metadata from named save fields. Observation installs no execution hooks. Sprite
decoding handles bitplanes, sizes, alignment, flips, transparency, palette selection
and alternate 2-bit mode. It does not implement scanline limits or composition.
Post-frame registers are not scanline histories.

RGB conversion follows the reference's RGB565 quantization. ROM-free tests cover
bounded field parsing, tile planes, universal background color zero, dormant patterns,
sprite geometry, RLE edge cases and strided scores. Capturing diagnostic video every
120 frames preserves the exact frozen 1,800-frame RAM/video/audio census.

Mesen can optionally trace VDC writes for the final 1..10 frames. Callbacks return
no replacement values. `Find-VideoRomMatches.ps1` locates exact runs, but repeated
artwork means a match alone does not prove the loader path.

Next: scene/launcher validity, launch arithmetic, ball update epochs,
actors and palette rules; then continuous table rendering and cabinet integration.
