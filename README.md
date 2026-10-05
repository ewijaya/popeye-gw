# Popeye G&W

A fan-made LCD-style catch game for the **Pebble Time 2**, inspired by
[Nintendo’s Popeye Game & Watch](https://nintendo.fandom.com/wiki/Popeye_(Game_%26_Watch)).
Fixed black segments, faint LCD ghosts and a printed nautical backdrop bring
the wide-screen handheld feel to the watch.

Popeye catches cargo that Olive Oyl tosses from a freighter's deck while
Brutus strikes from the piers. The app is named **Popeye G&W**; the repository,
package and build artifact use `popeye-gw`.

**Status:** M3 art implementation. Game A and Game B use a complete 88-segment
LCD art set, a printed backdrop, faint ghost segments and a launcher icon.
The clock, alarm, menu and persistent high scores are the next milestone (M4).
Build measurements and emulator evidence are in the [M3 art audit](docs/m3-art-audit.md).
See the
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
  2–5, with the catch or drop on segment 5. Olive's arrival at a throw spot is
  visible for a step before launching.
- Milestones trigger when the true score crosses 200 or 500 within each 1,000,
  including a double-point catch that skips the exact value. Difficulty and
  cargo limits use the true total, so wrapping the display does not slow play.
- A half-ring alone is not a full miss: a milestone starts Lucky Tide and keeps
  that half-ring. A hit adds a full miss without consuming a half-ring. Every
  drop or hit ends Lucky Tide.
- Brutus's idle countdown uses active milliseconds, with transitions on step
  boundaries. A due attack waits if its warning/strike would conflict with
  cargo already in flight or the other pier's strike.
- Hits resolve before cargo on a step. A full miss ends that step and clears
  all cargo; attacks freeze during the 1,500 ms recovery. The third miss ends
  the round immediately. Manual pause also freezes recovery.
- True scores and mode-specific high scores use 32-bit unsigned totals and
  saturate at their maximum representable value.

## Playing

| State | Up / Down | Select | Hold Select | Back |
|---|---|---|---|---|
| Title | — | Game A | Game B | Exit |
| Playing | Move Popeye one pose | Pause | — | Pause |
| Paused | — | Resume | — | Quit to title |
| Game over | — | Play again | Other mode | Title |

`src/c/scene.c` turns game state into the PRD 9.2 set of 88 lit segments; it
is pure C99 and host-tested. `src/c/view.c` draws each segment from a packed
sprite sheet using positions generated into `src/c/segments.h`. Inactive
segments are baked into the backdrop with sparse grey pixels, keeping the
overlapping poses faint on Emery's limited palette.
`src/c/main.c` runs one `AppTimer` per step
or recovery and none while paused, over or on the title. Pausing keeps the
elapsed part of the current interval. Losing focus pauses a running game.

Current presentation and remaining work:

- Until the M4 clock exists, the idle state is a title card showing Popeye
  upright and the controls. Up and Down do nothing there.
- The score uses the right three digits without leading zeros, so 1,005 shows
  as `5`.
- Paused, game over and title are shown as a text card over the lane area.
  These overlays retain readable button instructions over the LCD scene.
- A catch lights cargo segment 5 and the catch flash for one step. A hit shows
  the dizzy pose on Popeye's side. Drop and hit feedback stays through recovery.
- High scores are kept in memory only; HI lights at game over after a new best.
  Saved scores are M4, and vibration and flashing effects are M5.
- Each game start and game over logs `heap_bytes_free()` and
  `heap_bytes_used()` for the build audit.

## Art

The complete inventory includes five player poses, two dizzy poses, catching
feedback, four throw positions with ready/throw frames, two bell frames,
both rivals' idle/wind-up/strike phases, four cargo types with five positions
each, splashes, life rings, digits and indicators. There are no rectangle
placeholders in the renderer.

Character animation and cargo sources were created with the built-in image
generation tool. Repeated positions and mirrored poses share those sources;
small UI symbols use geometric artwork. Prompts and preparation instructions
are in [art/source/README.md](art/source/README.md).

```sh
# Only when changing source art; requires ImageMagick.
python3 tools/prepare_art.py
# Pack the committed runtime segments; Python standard library only.
python3 tools/build_art.py
python3 tools/build_art.py --check
```

The art check rejects missing, unexpected, empty or opaque full-screen
segments, and validates the 25 × 25 launcher icon. Host tests check all 88
generated entries, screen bounds, atlas bounds and non-overlapping atlas crops.

## Build

Requires Pebble Tool 5.0.40 and SDK 4.33.1. Use the bundled compiler.

```sh
pebble clean
pebble build
pebble install --emulator emery build/popeye-gw.pbw
```

Check the exit status and the literal `'build' finished` in the build output.
Every SDK build checks `.text + .data + .bss` against the **61,440-byte** app
budget and fails if it exceeds that limit. The playable app links the game
engine and loads the complete art set. The bundle is named `popeye-gw.pbw`
regardless of the checkout directory name, so pass its path when installing.

## Licence

See [LICENSE](LICENSE) for the repository licence.
