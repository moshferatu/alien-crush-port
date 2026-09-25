# Music voice overlay

Alien Crush's sound effects take PSG channels from the music. The overlay plays
the music's voice on those channels through a second PSG, so the music keeps all
of its parts. It is Devil's Crush's feature (its `src/audio/music_voice_overlay.cpp`)
ported to this game's older build of the same Compile sound driver. It is on by
default in normal play (`audio.music_voice_overlay`, `--no-music-voice-overlay`).

## The driver

The driver is in bank `$19`, run with MPR5=`$19` and MPR6=`$1A`. Bank 00 maps it at
`00:E9B8`. The jump table is at `19:A000`: init `A7D3`, stop `A7F7`, start a sound
`A821`, and the per-frame tick `A015`. Its RAM is Devil's Crush's plus `$EEC`.
The six channels' latches are:

| Latch | Address |
| --- | --- |
| Period low, high | `$3D02`, `$3D08` |
| Amplitude | `$3D0E` |
| Balance | `$3D14` |
| Waveform (`$FF`: none, noise) | `$3D1A` |
| Noise | `$3D20` |
| Waveform loaded in the PSG | `$3D26` |
| Envelope scratch | `$3D23` |

There are ten slots. Songs use slots 0–5, one voice each, on the channel
`$3D6E,X` names. Sound effects use slots 6–8, mostly on channels 3, 5 and 2.
Song `$00` uses slot 9 and plays alone.

Each tick runs in this order:

1. `19:A6AE` clears the six amplitude latches.
2. It walks slots 0 to 8, upward. `19:A6C3` commits each slot's voice into the
   latches. X is the slot, Y is the channel, and A is the value at every store:

   | PC | Store |
   | --- | --- |
   | `A6F5` | Amplitude 0 (no period) |
   | `A70B` | Noise |
   | `A710` | Waveform `$FF` |
   | `A738`, `A7C2` | Noise 0 |
   | `A73E` / `A744` | Period low / high |
   | `A7C7` / `A7CC` | Period low / high (arpeggio path) |
   | `A74A` | Waveform |
   | `A750` | Amplitude |
   | `A756` | Balance |

   A slot with state bit `$20` is exclusive: it calls `A6AE` from inside the
   commit (`A6CA`), which clears every earlier slot's latches.
3. `19:A139` pushes the latches to the PSG. For each channel, an amplitude of
   0 switches it off. Otherwise the push writes period, balance and noise,
   uploads the waveform when it changed, and then writes the amplitude with
   bit 7 set. The waveform upload is `$0804`=`$40`, then `$00`, then 32 bytes
   to `$0806`, from `19:A9DF` + 32×number (ROM file offset `0x329DF`). The
   stop routine also calls the push (`A818`).

Because the walk goes upward, effects commit after the music, and the last
writer wins. An effect overwrites its channel's latches and silences the
music's voice there, even when the effect itself is silent: a zero amplitude
is stored all the same. Devil's Crush gets the same result the other way
round. Its driver walks downward and skips latches that are already claimed,
so effects win there too.

## The overlay

`src/runtime/music_voice_overlay.hpp` watches the driver's instructions through
the CPU hook (`src/runtime/handoff_hook.inc`: MPR5=`$19`, PC `A139`–`A7D2`).
It keeps a second latch bank that holds only the music slots' stores (slots
0–5). It notes each channel that an effect slot stored to (slots 6 and up). At
the push, it programs the second PSG from that second bank for those channels
only, the way the push programs the real PSG. It switches every other channel
off. `A6AE` clears the second bank's amplitudes as it clears the real ones,
and so does the exclusive slot's clear, which mutes the music as on hardware.

The second PSG is added in a generated copy of Beetle's `pce.cpp`
(`cmake/MusicVoiceOverlay.cmake`). It shares the physical PSG's output
buffers, so its sound adds to the physical PSG's. It copies the physical PSG's
main volume (`$0801`). It stays idle unless the host turns it on
(`ReferenceRuntime::music_voice_overlay`).

It doesn't touch the machine, the physical PSG or save states. A power-on or
a restored state resets it to silent, and the next push rebuilds it, since
every active slot commits every tick.

## Limits

- The overlay's channels are switched off while no effect holds them, so
  their waveform phase is not kept in step with the music's. Devil's Crush
  keeps its overlay channels running muted.
- The overlay doesn't use the LFO.
- Effects `$22`, `$26`, `$2F`, `$34` and `$37` load into music slot 5
  itself, which replaces that voice. The overlay can't bring it back.

## Checks

`tests/music_voice_overlay_tests.cpp` checks the model against hand-built
driver sequences. With the ROM, it also runs 1,500 frames of the canonical
input with the overlay off and then on. Every frame's RAM must be identical,
and the sound must differ only after the first effect plays. Given a folder,
the test writes `original.wav`, `overlay.wav` and `added.wav` for listening.
