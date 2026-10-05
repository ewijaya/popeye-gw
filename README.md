# Popeye G&W

A fan-made LCD-style catch game for the **Pebble Time 2**, inspired by
[Nintendo’s Popeye Game & Watch](https://nintendo.fandom.com/wiki/Popeye_(Game_%26_Watch)).
Fixed black segments, faint LCD ghosts and a printed nautical backdrop bring
the wide-screen handheld feel to the watch.

Popeye catches cargo that Olive Oyl tosses from a freighter's deck while
Brutus strikes from the piers. The app is named **Popeye G&W**; the repository,
package and build artifact use `popeye-gw`.

**Status:** M4 implemented. Game A and Game B share an 88-segment LCD art set
with an idle clock, daily alarm, watch menus, saved settings and dated high
scores. M5 adds game-event vibration, flashing effects and play-test tuning.
Build measurements and emulator evidence are in the [M4 audit](docs/m4-audit.md)
and the earlier [M3 art audit](docs/m3-art-audit.md).
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
address and undefined-behavior sanitizers. The M4 suite tests versioned records,
corrupt data, write failures, 12/24-hour clock segments, calendar/DST rollover,
alarm conflicts and wakeup delivery using the real adapters with a small SDK
fake. It runs in UTC and America/New_York. Both commands run on every push and
pull request; neither requires the watch SDK.

`src/c/game.c` and `game.h` have no platform dependencies. `game_init` starts a
round; `game_input` handles press/release edges immediately; `game_advance`
consumes active milliseconds. Pause/resume preserves partial timers, and
`game_step` advances to the next boundary for simulation. State and feedback
feed the segment renderer. Pacing values live in `src/c/tuning.h`.

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
| Clock | High scores / Menu | Game A | Game B | Exit |
| Playing | Move Popeye one pose | Pause | — | Pause |
| Paused | — | Resume | — | Quit to clock |
| Game over | — | Play again | Other mode | Clock |
| Menu | Move selection | Choose / toggle | Choose | Return |
| Alarm time editing | Increase / decrease | Save | Save | Cancel |
| Alarm ringing while idle | Stop | Stop | Stop | Stop |

`src/c/scene.c` turns game state into the PRD 9.2 set of 88 lit segments; it
is pure C99 and host-tested. `src/c/view.c` draws each segment from a packed
sprite sheet using positions generated into `src/c/segments.h`. Inactive
segments are baked into the backdrop with sparse grey pixels, keeping the
overlapping poses faint on Emery's limited palette.
`src/c/main.c` runs one `AppTimer` per step
or recovery and none while paused, over or in the clock. Pausing keeps the
elapsed part of the current interval. Losing focus pauses a running game.

The clock uses all four digits, follows the watch's 12/24-hour preference,
and cycles character poses every two seconds. With attract animation disabled,
it shows static poses and a steady colon and updates once per minute. Clock
ticks stop on other pages and while the app is out of focus.

The menu contains High scores, Alarm, Settings and About. Settings toggle
swapped controls, vibration, ghost segments and attract animation. The renderer
holds one backdrop at a time and swaps it only when the ghost preference
changes, keeping free heap above the project budget.

A daily alarm uses Pebble Wakeup to launch the closed app. After firing, it
schedules the next local calendar day. A scheduling conflict gets one retry a
minute later; the Alarm page shows the adjusted time or a visible error with
Retry. Hour/minute editing uses 24-hour values: Select saves, Back cancels.
Test alarm previews the bell animation without changing the daily schedule.

While idle, an alarm animates Olive's bell and pulses every two seconds for up
to a minute; any button dismisses it and restores the previous page. During
play, the bell flashes and the watch pulses once while the game keeps running.
A press also dismisses that indicator while performing its normal game action.
Vibration respects both the app setting and Quiet Time.

Settings and high scores use separate versioned, checksummed byte records.
Missing, corrupt or unknown records fall back to defaults independently. Best
scores retain their full 32-bit totals and local dates. Writes happen at pause,
game over, focus loss and exit, avoiding a flash write for every catch. Reset
requires a second selection, with Keep scores selected by default. Save
failures are visible. The unchanged UUID preserves records across app updates.

The game score still uses the right three digits without leading zeros. A
catch lights its final cargo segment and a catch flash for one step; a miss
keeps its feedback through recovery. M5 will add game-event vibration and
flashing effects. Physical-watch alarm and Quiet Time behavior still need the
owner's release play-test.

## Handheld orientation

Choose **Menu → Settings → Orientation → Landscape** for a miniature
Game & Watch layout. Turn the watch so the three buttons sit along the top:
left to move left, middle to start/pause, right to move right. The preference
is saved, and clock, alarm and menus rotate too. Switch back to Portrait for
wrist use. See [landscape mode](docs/landscape.md) for details and screenshots.

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
