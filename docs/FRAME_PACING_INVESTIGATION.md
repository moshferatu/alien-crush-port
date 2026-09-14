# Frame pacing investigation — 2026-09-14

Alien Crush maintained its intended game speed on the development PC, but
cabinet rendering produced reproducible hitches. These measurements do not
establish the performance of the separate cabinet PC or the number of unique
frames actually scanned out by its monitors.

## Method

Used the Windows Release SDL host with real video/audio output, the same
1,800-frame `continuous.acrec` replay, isolated settings under
`out/performance-investigation`, and `--timing-log`. Ran desktop once and
cabinet twice, sequentially. Repeated cabinet with stage timestamps to locate
the expensive work. The replay crosses the seam at frames 607 and 1292.

Statistics below use frames 361–1800, excluding initial startup and focus
clock adjustments. Frame rate is elapsed frame-start time, not the reciprocal
of the time spent doing work. Work includes simulation, composition, audio
submission, and all window presentations; it excludes the deliberate pacing
wait. Frame 361 onward also includes the route's end-of-ball transitions.

| Measurement | Desktop | Cabinet run 1 | Cabinet run 2 |
| --- | ---: | ---: | ---: |
| Average game frames/second | 59.8261 | 59.8261 | 59.8261 |
| Median work/frame | 1.30 ms | 7.46 ms | 7.21 ms |
| 95th percentile work/frame | 1.71 ms | 13.82 ms | 13.49 ms |
| 99th percentile work/frame | 2.02 ms | 17.82 ms | 15.44 ms |
| Longest frame-start gap | 17.72 ms | 66.70 ms | 65.82 ms |
| Frame-start gaps above 25 ms | 0 | 3 | 3 |

The nominal frame budget is about 16.715 ms. Both cabinet runs hit the same
three expensive frames: 1687, 1693, and 1785; their delays appear in the next
frame's start gap. They are not the two seam crossings. Work within five
frames of either crossing peaked at 13.64/13.22 ms in run 1 and
8.04/14.78 ms in run 2. This does not exclude hitches on other routes.

## Where the time goes

Stage-instrumented cabinet run:

| Frame | Runtime | Snapshot/copies/stitched composition | Playfield presentation | Audio + auxiliary displays |
| --- | ---: | ---: | ---: | ---: |
| 1687 | 4.73 ms | 56.67 ms | 0.34 ms | 0.75 ms |
| 1693 | 2.69 ms | 26.60 ms | 0.28 ms | 0.86 ms |
| 1785 | 2.96 ms | 24.82 ms | 0.30 ms | 0.41 ms |

The measured bottleneck is the composition stage. Further profiling should
separate its internal routines before selecting an optimization. One source
candidate is `raster_layer` in `src/render/pce_graphics.cpp`: it calls
`compose_rows` for each band between writes, and each band decodes the entire
sprite layer again. Reusing unaffected sprite decoding or decoding only
required rows could reduce work while preserving exact raster behavior.
This candidate has not yet been isolated by an internal profiler.

## Comparison with Devil's Crush

Source inspected in `D:\Workspace\devils-crush`:

- Devil's Crush uses 59.8261 game frames/second in `src/app/main.cpp`.
  Alien Crush takes the pinned runtime's 7159090.90909090 / 455 / 263 FPS,
  which agrees to the precision relevant here.
- Devil's Crush requests VSync on the playfield renderer and disables it on
  auxiliary renderers. Its host advances up to four due simulation frames.
- Alien Crush disables VSync on every renderer and presents one game frame
  per timer iteration using `SDL_DelayPrecise` and a cumulative deadline.
  Late iterations can run back-to-back to catch up.

The source confirms different presentation pacing, not a lower configured
game rate. Unsynchronized presentation and catch-up bursts are plausible
additional contributors to perceived uneven motion. This investigation did
not collect Devil's Crush presentation telemetry or scan-out measurements,
so it does not quantify the difference in displayed smoothness between ports.

## Reproduction and cabinet measurements

The existing `--timing-log` option now appends optional stage timestamps and
the table half. Its original five columns remain first and unchanged.
Additional timestamp reads occur only when logging is enabled. To measure
normal play on the cabinet, run from the published directory:

```powershell
.\alien-crush.exe --cabinet --timing-log cabinet-timing.csv
```

Play through the behavior, then exit normally to flush the log. Use frame
start differences for cadence; subtract adjacent stage timestamps for work.
`cabinet_end_ns` includes audio submission and backglass/DMD presentation.
`compose_end_ns` equals `runtime_end_ns` outside cabinet mode. Logs contain
host timestamps, not physical monitor scan-out timing.

Raw runs, per-crossing/per-hitch details, and the summary script are in
`out/performance-investigation`. No frame pacing or rendering behavior was
changed during this investigation.

## Implemented follow-up — 2026-09-14

The raster renderer now decodes only the sprite rows consumed by each band.
Sprite dimensions, SAT ordering, fetch-limit accounting, and image indexing
remain intact. Mid-frame writes still take effect at their original bands;
there is no cache that can retain obsolete pattern data.

Playfield VSync is enabled by default, with auxiliary renderers still using
VSync off. Before another simulation frame is due, refreshes redraw the
completed texture. They do not consume replay input, tick actors, or generate
audio. Overdue simulation frames can defer primary presentation to catch up,
bounded at four, avoiding one VSync wait per overdue frame. Original timer
pacing is available with `--no-vsync` and is the fallback if VSync cannot be
enabled. Game speed remains independent of monitor refresh.

Using the same replay and frames 361–1800:

| Measurement | Optimized timer | Optimized VSync |
| --- | ---: | ---: |
| Game frames/second | 59.8261 | 59.8200 |
| Median composition | 3.96 ms | 4.06 ms |
| 99th percentile composition | 4.82 ms | 4.84 ms |
| Maximum composition | 6.30 ms | 6.55 ms |
| Maximum frame-start gap | 18.24 ms | 28.58 ms |
| Frame-start gaps above 25 ms | 0 | 1 |

The previously expensive frame 1687 took 6.30 ms to compose with timer pacing,
down from 56.67 ms. The repeatable raster hitches are removed on this route.
VSync still had an isolated longer interval; these results do not establish
perfect physical scan-out cadence. With VSync enabled, presentation timing
includes intentional refresh waits and should not be interpreted as CPU
rendering cost. Test both modes on the cabinet to judge perceived smoothness.

All 1,800 frame numbers, audio batch sizes, and table-half values match the
baseline in both modes. Graphics, table replay, inactive animation, mouth
layering, and ordinary bonus replay checks passed. Measurements are in
`optimized-timer.csv`, `optimized-vsync.csv`, and `optimized-summary.json`
under `out/performance-investigation`.
