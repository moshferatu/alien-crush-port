# Alien Crush PC Port

Windows x64 port of Alien Crush for desktop and a three-monitor PinOne pinball cabinet.
The development baseline runs the original game in an independent, pinned Beetle PCE
reference runtime. Native gameplay and full-table cabinet presentation are being developed.
See [PLAN.md](PLAN.md) for the target and [PROGRESS.md](PROGRESS.md) for verified status.

## ROM

Supply your own `Alien Crush.pce`: 262,144 bytes, SHA-256
`1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f`.
Other ROM revisions are rejected. ROMs and generated game assets are not distributed.

## Build

Requires Visual Studio 2022 C++ tools, Windows SDK, and CMake 3.30+.
Initial configuration fetches the pinned SDL and Beetle sources.

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-release
ctest --preset windows-release
```

## Play

```powershell
.\out\build\windows-vs2022\Release\alien-crush.exe --rom '.\Alien Crush.pce'
```

The baseline uses the original controller behavior:

| Input | Action |
| --- | --- |
| Enter / gamepad Start | Start and pause |
| Left Shift / Left / left shoulder | Left flipper |
| Right Shift / Z / right shoulder / south button | Right flipper and original launcher |
| X / east button | Nudge |
| Backspace / gamepad Back | Select speed/music |
| F5 / F9 | Save / restore quick state |
| F11 | Borderless fullscreen |
| Escape | Exit |

Quick states live beside the executable in `portable-data/alien-crush.reference.state`.
Use `--data-dir` to select another location. Missing ROM paths default beside the executable.

Record with `--record session.acrec`, replay with `--replay session.acrec`.
Recordings embed the initial state and every sampled controller input with an integrity hash.
Quick-state operations are disabled during recording and replay to retain a linear timeline.

## Diagnostics

```powershell
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --validate-rom
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --verify-replay --frames 1800 --capture out/captures/baseline
```

The headless tools record per-frame RAM/video/audio fingerprints, periodic BMP screenshots,
RAM snapshots, raw per-frame RAM/pixels, and a replayable input recording. A 1,800-frame
capture takes about 440 MB. No game-specific execution patches are installed.
`--verify-replay` checks repeated execution, midpoint restore, and boot-state restore.
Use `--replay session.acrec` to diagnose a recorded play session instead of the canonical route.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Verify-Baseline.ps1 -RomPath '.\Alien Crush.pce'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Inspect-ReferenceState.ps1 -PayloadPath out/captures/verified-baseline/checkpoint.payload
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Disassemble-Rom.ps1 -RomPath '.\Alien Crush.pce' -Bank 00 -Pc E2FC -Count 32
```

The baseline verifier compares against a frozen census from untouched Beetle. The default
runtime extends serialization to preserve audio history and fractional timestamps; see
[provenance](third_party/REUSE.md). Set `AC_REFERENCE_STATE_EXTENSION=OFF` in a separate
build directory to reproduce the untouched source baseline. Its incomplete audio saves
are unsuitable for the deterministic restored-audio gate.

For an independent Mesen 2.1.1 comparison, supply the executable from its complete
Windows distribution. The capture script checks its hash, copies runtime binaries to an
isolated output directory, and sets the capture palette/controller configuration there.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Invoke-MesenCapture.ps1 -MesenPath '<Mesen directory>\Mesen.exe' -RomPath '.\Alien Crush.pce' -Frames 600
.\out\build\windows-vs2022\Release\alien-crush-tools.exe --rom '.\Alien Crush.pce' --frames 600 --capture out/captures/oracle
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Compare-MesenCapture.ps1 -ReferenceDirectory out/captures/oracle -MesenDirectory out/mesen/baseline
```

The comparator reports differences; it does not turn nonzero mismatches into a passing
equivalence assertion. See [PROGRESS.md](PROGRESS.md) for current results and coverage gaps.

Enable local ROM tests with `-DAC_ENABLE_ROM_TESTS=ON -DAC_ROM_PATH='<absolute ROM path>'`
at configuration. Public CI runs ROM-free tests. `windows-core-only` builds without SDL or Beetle.
