# Nudge force multiplier

`--nudge-multiplier` follows Devil's Crush's 0–16 force option, default 1.
The accelerator detection threshold remains a separate existing cabinet setting.

Alien Crush's original routine at ROM bank 01, 438E starts a nudge on a
button-II edge after the launcher, ball/game-over and scene gates. It increments
the original tilt counter outside bonus rooms, then sets a two-update positive
vector of 0100. It reverses that vector to FF00 and runs until the screen offset
returns to zero. The visual offset and phase state remain untouched.

At 01:4416 the ball-present/action gates exclude absent, captured and otherwise
ineligible balls. 4422–442F arithmetically halves the signed vector into scratch
04/05. 4433–4440 adds it to ball Y velocity at 3780/37C0. The host hook at the
4443 return recovers the preceding velocity and substitutes the scaled impulse.
It repeats the original early-return gates because those branches also reach
4443, and excludes attract demos. CPU registers, input timing, sound, tilt count,
screen offsets and the vector used by the next phase are unchanged. Modified
velocities saturate at signed 16-bit bounds; factor 1 bypasses the hook entirely,
including preserving original overflow behavior.

Nondefault factors are encoded losslessly in the runtime identity used by saves
and recordings. Those identities abbreviate the pinned backend hash to eight
hex characters to fit the existing 64-byte header. Default identities remain
unchanged. Host and tooling replay infer the recorded factor; quick states with
a different factor are incompatible rather than silently changing physics.

`nudge_multiplier` exercises the actual ROM at factors 0, 0.5, 1, 2 and 16,
comparing RAM/video/audio fingerprints against an independently calculated
impulse at the instruction boundary after restoring the session. It verifies
both positive and negative phases, saturation, parsing and recording metadata.
`nudge_recording` verifies the captured 2x session through the tools replay path.
`table_replay` checks the unmodified default against the original reference.
