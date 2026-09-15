# Continuous cabinet handoff

Status: implemented and under cabinet acceptance, 2026-09-09.

## Implemented path

`ContinuousHandoff.cmake` adds guarded hooks to generated CPU/VDC/VCE sources.
`handoff_packets.hpp` implements the ROM's word-RLE packet contracts and the
60-slot actor-field exchange. Full VRAM streams complete synchronously, including
the original pointer carries and ADC flags. RAM packets and actor exchanges return
to the original pointer/return code. VDC transfers preserve MAWR progression and
write-latch contents, update tile caches, and commit the preceding pending write.
Active DMA declines replacement until ordinary execution can complete it.

The guard requires the ordinary table, ball actor, mapped ROM/RAM banks and
the actual handoff stack: return 40A0 from live play, or 403A from the demo loop
when the demo selector is active. Graphics calls from actor restoration are covered too.
Camera shake is permitted: nudging can change camera RAM during a valid handoff.
Only the JSRs at 89B3/89E9 and 8A1B/8A41 are bypassed. The complete E705 routine,
collision pointer installation and actor scheduler remain in their original order.
No sound register, timer, IRQ clock or audio buffer is advanced or discarded by a hook.

Cabinet mode uses the `-handoff1` runtime revision. Recordings retain this identity;
host and tools select the proper runtime before strict restore. Old recordings
remain original-timing replays. Cabinet quick states have a separate filename.
V changes presentation without changing execution semantics. The host no longer
uses `TableTransitionClock`; that helper remains only for historical diagnostics.

Verification includes 46,560 packet/actor predictions checked against original
CPU execution, including flags, scratch RAM, output words and MAWR. The video
oracle includes the final pending bus write, which can commit after the CPU has
returned. Ten genuine pre-handoff checkpoints compare collision pointers, flags
and both complete actor banks at four matching instruction boundaries, in both
directions. None ends a frame in a loading routine. Native replay checks retain
exact audio/pixel/RAM equality after restoring a native checkpoint.

Demo coverage also audits the original transfer contracts and imports genuine
pre-crossing checkpoints in both directions to compare actor banks and collision
pointers. Neither crossing ends a frame inside the loader. A separate native
cabinet demo checks the full audio timeline, natural title return, Run to exit,
and 400 frames of exact restored RAM/video/audio including a seam crossing.
`demo_handoff_host` replays this route through the cabinet renderer and writes
`demo-handoff-host.csv` for pacing checks.

`out/captures/native-handoff` contains the first crossing. The reviewed contact
sheet `out/native-seam-contact.png` shows continuous current-ball motion through
frames 606..611; `out/native-seam-table.png` retains the aligned 24-row artwork
join. The host timing CSV is `out/build/windows-vs2022/continuous-host.csv`:
120 frames, every audio batch retained, maximum crossing interval 16.7222 ms and
maximum crossing work 11.3914 ms. Queued audio peaks at 8,000 bytes / 45.4 ms.
The test exports a two-second PCM WAV at `out/build/windows-vs2022/continuous.acrec.wav`.
Native bonus return also passes exact repeated/restored replay and the stitched
host smoke check. Hardware listening and physical three-monitor cabinet acceptance remain
pending; automated audio checks alone do not establish perceived sound quality.

## Why the superseded workaround interrupted music

The previous stitched host ran multiple original emulator frames during a crossing
and discarded their audio. The next audible samples came from a later point in the
music sequence. Removing video delays therefore creates an audible time jump.
Queueing all that audio would instead accumulate sound latency at each crossing.

The user rejected this compromise. The upload/frame-wait fast-forward path has
been removed from the host.

## What Devil's Crush actually does

Read-only comparison with `D:/Workspace/devils-crush`:

- `src/app/main.cpp:20798` runs the game frame; line 20936 queues its audio, then
  advances the shared frame deadline. There is no seam-specific audio discard
  in this cabinet loop.
- `src/app/main.cpp:10946` maps the scrolling camera to table world coordinates.
  The accompanying comments explain how to align the copied SAT with the
  camera's display epoch.
- `src/render/pce_sprite_overlay.cpp:1798` explains the persistent layer of
  actors retained from different camera visits. The live viewport overlays it.

This supplies the desired presentation/clock model. Alien Crush has a different
handoff: it replaces the active table half and its assets rather than only moving
that viewport. Devil's Crush addresses and gameplay replacements cannot be copied
into Alien Crush.

## Original implementation requirements

Keep a single regular gameplay/audio clock. Make the main-table handoff complete
without spending emulated frames on display-only loading. Retain the existing
ROM-derived resident artwork, current-ball presentation and read-only snapshots.

Implement narrowly guarded native replacements for the loading work, with the
original reference path available for comparison:

1. Trace and specify both handoff paths at `03:89B3..89EF` and `03:8A0A..8A47`.
   Separate display waits/copies from gameplay operations. Preserve the original
   coordinate translation, actor-bank exchange, collision-pointer installation,
   flags and scheduler order.
2. Recover the precise contracts of `00:E705`, its `ED97`, `EB10` and `EB77`
   callees, and the table-specific loaders. `E705` installs pointers in RAM
   42..45 and changes flag 68; replacing it with a bare return would break play.
   Implement immediate, bounded transfers for the verified ROM word-RLE loader
   at `00:E3DD..E43A` and any required actor-data loading. Preserve CPU return
   state and relevant VDC state, not just the visible picture.
3. Bypass only verified handoff-specific display waits. Keep the `01:46FD`
   actor-bank exchange and gameplay setup intact unless separately replaced and
   verified. Do not advance inactive actors or introduce extra physics ticks.
4. Remove host transition-frame compression and audio discard once those
   replacements are verified. Every regular frame then supplies one continuous
   audio batch. Music and effects remain on the same game clock.

This intentionally changes the cabinet's loading timeline. Keep the original
reference replay tests unchanged; give recordings/states an explicit runtime-mode
identity before exposing a native handoff mode. Existing recordings must never
silently replay under different execution semantics.

## Acceptance evidence

Cabinet viewport entry also filters two original transition rasters. The score
loader (`1C:46FD -> 48B5`) waits a frame before disabling video, leaving the
outgoing table visible after the layout has already changed to centered scores.
The bonus loader enables video at `FE37` before `EEDF` starts its palette fade,
producing a bright raster before the fade begins. `cabinet_viewport_blank`
blacks out the score-loader wait and the bonus display-enable edge. It does not
skip emulation or audio frames, change the exit transitions, or delay the normal
fade. Score detection covers pause, practice, normal game end and demo callers.
Replay checks cover the outgoing score raster, monotonic ordinary bonus entry,
and display-enable transitions in all four practice rooms.

- Both directions, repeated crossings, bonus returns and quick-state restoration.
- Compare gameplay state at matching handoff boundaries, rather than demanding
  equal original frame numbers after removing loading time. Check ball position,
  velocity/fractions, scores, flags, collision pointers and active/inactive actors.
- Verify decoded transfers and CPU return contracts against original execution.
- No omitted audio batches, seam-triggered queue clears, music-timer jumps or
  steadily increasing audio latency. Capture and listen to crossing audio.
- Record presented frame intervals and ball movement across each crossing;
  verify no stationary loading frames or return to the old SAT ball position.
- Preserve all original reference parity tests and add distinct cabinet-mode
  recording/replay tests. Finish with physical cabinet acceptance.
