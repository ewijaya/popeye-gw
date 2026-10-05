# Harbor Catch

An original LCD-style catch game for the **Pebble Time 2**, in the spirit of
early-1980s wide-screen LCD handhelds.

Finn, a young sailor in a rowboat, catches cargo that Mae tosses from a
freighter's deck while Grizzle, a harbour pirate, strikes from the piers.
Game A and Game B, saved high scores, and a clock with a daily alarm when
you're not playing.

**Status:** M1 game logic is implemented and host-testable. The watch app is
still the foundation shell; playable controls and rendering are M2. See the
[Product Requirements Document](PRD.md) and [repository conventions](CLAUDE.md).

## Host tests

```sh
./tools/test.sh
./tools/test.sh --sanitize
```

The runner uses `/usr/bin/cc`, strict C99 warnings, and a temporary output
directory. It covers the game rules and an independent perfect-player bot over
10,000 seeds in each mode through 1,000 points. The second command enables
address and undefined-behavior sanitizers. Both commands run on every push and
pull request; neither requires the watch SDK.

`src/c/game.c` and `game.h` have no platform dependencies. `game_init` starts a
round; `game_input` handles press/release edges immediately; `game_advance`
consumes active milliseconds. Pause/resume preserves partial timers, and
`game_step` advances to the next boundary for simulation. State and feedback
are exposed for the later renderer. Pacing values live in `src/c/tuning.h`.

The PRD leaves a few details open. The engine uses these interpretations:

- A launch displays cargo segment 1; four later steps advance through segments
  2–5, with the catch or drop on segment 5. Mae's arrival at a throw spot is
  visible for a step before launching.
- Milestones trigger when the true score crosses 200 or 500 within each 1,000,
  including a double-point catch that skips the exact value. Difficulty and
  cargo limits use the true total, so wrapping the display does not slow play.
- A half-ring alone is not a full miss: a milestone starts Lucky Tide and keeps
  that half-ring. A hit adds a full miss without consuming a half-ring. Every
  drop or hit ends Lucky Tide.
- Grizzle's idle countdown uses active milliseconds, with transitions on step
  boundaries. A due attack waits if its warning/strike would conflict with
  cargo already in flight or the other pier's strike.
- Hits resolve before cargo on a step. A full miss ends that step and clears
  all cargo; attacks freeze during the 1,500 ms recovery. The third miss ends
  the round immediately. Manual pause also freezes recovery.
- True scores and mode-specific high scores use 32-bit unsigned totals and
  saturate at their maximum representable value.

## Build

Requires Pebble Tool 5.0.40 and SDK 4.33.1. Use the bundled compiler.

```sh
pebble clean
pebble build
pebble install --emulator emery build/harbor-catch.pbw
```

Check the exit status and the literal `'build' finished` in the build output.
Every SDK build checks `.text + .data + .bss` against the **61,440-byte** app
budget and fails if it exceeds that limit. The M1 shell does not call the engine,
so the linker removes unused engine code from its app image; this measurement
does not predict M2's playable app size. The bundle is named `harbor-catch.pbw`
regardless of the checkout directory name, so pass its path when installing.

## Licence

MIT, for both code and original art. See [LICENSE](LICENSE).
