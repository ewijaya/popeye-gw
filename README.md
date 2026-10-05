# Harbor Catch

An original LCD-style catch game for the **Pebble Time 2**, in the spirit of
early-1980s wide-screen LCD handhelds.

Finn, a young sailor in a rowboat, catches cargo that Mae tosses from a
freighter's deck while Grizzle, a harbour pirate, strikes from the piers.
Game A and Game B, saved high scores, and a clock with a daily alarm when
you're not playing.

**Status:** M2 playable prototype. Game A and Game B play start to finish with
placeholder rectangles for every segment; original art is M3, and the clock,
alarm, menu and saved scores are M4. See the
[Product Requirements Document](PRD.md) and [repository conventions](CLAUDE.md).

## Host tests

```sh
./tools/test.sh
./tools/test.sh --sanitize
```

The runner uses `/usr/bin/cc`, strict C99 warnings, and a temporary output
directory. It covers the game rules, the mapping from game state to lit
segments (`src/c/scene.c`), and an independent perfect-player bot over
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

## Prototype (M2)

| State | Up / Down | Select | Hold Select | Back |
|---|---|---|---|---|
| Title | — | Game A | Game B | Exit |
| Playing | Move Finn one pose | Pause | — | Pause |
| Paused | — | Resume | — | Quit to title |
| Game over | — | Play again | Other mode | Title |

`src/c/scene.c` turns game state into the PRD 9.2 set of 88 lit segments; it
is pure C99 and host-tested. `src/c/view.c` draws each segment as a
placeholder rectangle, with unlit segments as faint outlines, and is the only
file that knows pixel positions. `src/c/main.c` runs one `AppTimer` per step
or recovery and none while paused, over or on the title. Pausing keeps the
elapsed part of the current interval. Losing focus pauses a running game.

M2 interpretations, all open to change in M3–M5:

- Until the M4 clock exists, the idle state is a title card showing Finn
  upright and the controls. Up and Down do nothing there.
- The score uses the right three digits without leading zeros, so 1,005 shows
  as `5`.
- Paused, game over and title are shown as a text card over the lane area.
  The final LCD treatment of these states is decided with the art.
- A catch lights cargo segment 5 and the catch flash for one step. A hit shows
  the dizzy pose on Finn's side. Drop and hit feedback stays through recovery.
- High scores are kept in memory only; HI lights at game over after a new best.
  Saved scores are M4, and vibration and flashing effects are M5.
- Each game start and game over logs `heap_bytes_free()` and
  `heap_bytes_used()` for the build audit.

## Build

Requires Pebble Tool 5.0.40 and SDK 4.33.1. Use the bundled compiler.

```sh
pebble clean
pebble build
pebble install --emulator emery build/harbor-catch.pbw
```

Check the exit status and the literal `'build' finished` in the build output.
Every SDK build checks `.text + .data + .bss` against the **61,440-byte** app
budget and fails if it exceeds that limit. The M2 prototype links the whole
engine; art resources arrive in M3. The bundle is named `harbor-catch.pbw`
regardless of the checkout directory name, so pass its path when installing.

## Licence

MIT, for both code and original art. See [LICENSE](LICENSE).
