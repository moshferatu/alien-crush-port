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

Next: scene/launcher validity, launch arithmetic, ball update epochs,
actors and palette rules; then continuous table rendering and cabinet integration.
