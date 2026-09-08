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
Post-frame registers are not scanline histories: this preview omits scanline sprite
limits, register/VRAM/palette changes during display, and display-window timing.
It does not replace the live renderer or establish full-frame pixel equivalence.

`out/captures/compositor-1800` contains 15 exported previews, including visually
checked lower/upper tables at frames 600/720. Its complete RAM/video/audio census
retains the frozen SHA-256
`ee560dad47ece1a3c2e55a4326bcd4ef97276ad1ac0912d6c36eb29298c6572e`.

RGB conversion follows the reference's RGB565 quantization. ROM-free tests cover
bounded field parsing, tile planes, universal background color zero, dormant patterns,
sprite geometry, RLE edge cases and strided scores. Capturing diagnostic video every
120 frames preserves the exact frozen 1,800-frame RAM/video/audio census.

Mesen can optionally trace VDC writes for the final 1..10 frames. Callbacks return
no replacement values. `Find-VideoRomMatches.ps1` locates exact runs, but repeated
artwork means a match alone does not prove the loader path.

Next: scene/launcher validity, launch arithmetic, ball update epochs,
actors and palette rules; then continuous table rendering and cabinet integration.
