# M4 development audit — 2026-10-05

Popeye G&W now includes the idle clock, daily alarm, menus, settings and saved
high scores. The app UUID and version 1.0.0 are unchanged. This is a development
audit of changes based on `a54f19eaf7c0d01f334c03dbb04315ab51fc3471`, built from a
dirty tree, not a frozen release candidate or physical-watch sign-off.

Landscape-orientation work from another session arrived during final validation.
It was preserved, and the current combined tree was rebuilt and tested with
its updated test runner. The build measurements below cover that combined
build. The detailed alarm lifecycle runs predate the orientation integration;
see [landscape notes](landscape.md) for that session's separate evidence.

| Check | Result |
|---|---|
| Host rules and fairness | Passed; 10,000 seeds per mode through 1,000 points, zero unavoidable misses |
| Address/undefined-behavior sanitizers | Passed, including the M4 suite |
| M4 host checks | Passed in UTC and America/New_York: corrupt/unknown records, persistence failures, dates, full-width scores, clock segments, calendar/DST recurrence, wakeup lifecycle and conflict retry |
| Art pipeline | All 88 segments match the generated atlas and backdrops |
| Clean SDK build | Exit 0 and `'build' finished`; Pebble Tool 5.0.40, SDK 4.33.1, arm64 host |
| App image | 16,872 / 61,440 bytes; 44,568 bytes headroom |
| Resource pack | 18,863 / 200,000 bytes |
| PBW | 37,427 / 2,000,000 bytes; configured name, UUID, version, watchapp type and Emery-only platform verified |
| Final emulator heap | Game A and B: 46,856 bytes free, 67,336 used; **41.0% free** |
| Launcher icon | Unchanged from M3, where the 25 × 25 icon was verified in the launcher |
| Identity | Popeye G&W with the approved Popeye, Olive Oyl and Brutus art |
| Physical watch | Not installed or tested; no store publication |

Final PBW: `build/popeye-gw.pbw`

SHA-256:
`c0800d46c4822a293dfe018df0c3d804898935b655b194384cf9883059604ea2`

The app-image total is 15,908 bytes of text, 48 bytes of data and 916 bytes of
BSS. The playable engine, clock and persistence/wakeup adapters are linked;
the unused host-only `game_init` convenience entry point is removed. The SDK
linker still emits its RWX LOAD-segment warning; there are no C compiler
warnings or build errors. Environment-bearing `.lock-waf*` files were removed.

## Emulator checks

- Changed all four settings through the UI, reinstalled the PBW and launched
  again. The settings survived. Debugger reads confirmed minute-only ticks
  with attract disabled and the plain backdrop selected with ghosts disabled.
- Played Game A with swapped controls, caught two items and reached game over.
  Reinstalled an updated build: the score of 2 and date 2026-10-05 survived.
  The score screenshot below records the real result, not injected data.
- Opened Reset, accepted its default Keep scores choice and confirmed the
  score remained. Later explicitly reset both test scores; cleanup confirmed
  both saved totals and dates were zero.
- Edited alarm minutes, saved and restored them; cancelled an unfinished hour
  edit without changing the saved hour. The Alarm page displays its next fire
  time. Scheduling conflicts and retry failures are covered by the host fake.
- Scheduled a real 16:00 alarm through the UI, then exited the app. At 16:00
  the emulator launched it and logged `alarm fired from closed app` with
  `alarm ringing passive 0`. Olive's bell animation and dismissal card were
  captured. The next day's wakeup was scheduled before ringing.
- Scheduled 16:01, started Game A, paused while waiting and resumed before the
  deadline. At 16:01 it logged `alarm fired while open` and `passive 1`.
  Gameplay continued and reached game over at 16:01:13, with its normal heap
  reading. The alarm did not force a pause or replace the game screen.
- Tested the alarm preview with the daily alarm disabled. Dismissal returned
  to the prior page. Cleanup restored default preferences and 07:00 with the
  alarm off, no wakeup ID, no ringing and normal clock ticks.

An early attempt to accelerate alarm checks by changing emulator time made
button delivery unreliable. One emulator restart, without wiping its saved
data, restored normal behavior. The alarm results above use real elapsed time
after that restart. Alarm lifecycle checks preceded the final error-message
wording, Brutus attract-pose refinement and orientation integration; the current
PBW was installed and both modes
were started again after the clean build.

## Memory correction

The first M4 build held both full-screen backdrop variants and left only about
12% free heap, below budget. The renderer now loads one variant and replaces it
only when the ghost preference changes. During the real alarm/play checks it
used 57,296 bytes and left 59,472 bytes free, about 51%, in both modes. Final
build measurements with the orientation renderer's reusable UI buffer were
46,856 free and 67,336 used in both modes, **41.0% free**, above the 20% budget.

No per-frame bitmap allocation is introduced. App unload logs reported zero
outstanding allocations. An alarm has a separate one-second animation timer;
clock ticks stop when their screen is hidden, and ordinary game timers stop
while paused or out of focus.

## Screenshots

Five native 200 × 228 screenshots were captured during M4 development:

- [Clock](../art/previews/m4-clock.png)
- [Changed settings](../art/previews/m4-settings.png)
- [Persisted score and date](../art/previews/m4-scores.png)
- [Daily alarm settings](../art/previews/m4-alarm-settings.png)
- [Alarm after launching the closed app](../art/previews/m4-alarm-ringing.png)

The earlier settings/clock captures predate the memory correction, which does
not change their appearance. The alarm settings capture shows a different
scheduled time from the subsequent near-term delivery tests.

## Remaining release validation

Physical-watch vibration, Quiet Time, actual device timezone/DST behavior and
the full owner play-test remain unverified. Game-event vibration, flashing
effects, tuning and store artwork belong to M5. No release or store listing
was published. See [M4 implementation notes](m4-notes.md) for record formats,
timing choices and failure handling.
