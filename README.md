# Popeye G&W

A fan-made LCD catch game for **Pebble Time 2**, based on Nintendo's
**Popeye Wide Screen PP-23 (1981)**. Olive throws food beside her car on the
left; Popeye catches it in his boat while Brutus threatens him with a hammer
from the left pier or a fist from the right ship. The PT2 scene uses crisp
black segments, a white field, and vivid red, orange, blue and turquoise.

**Status:** M5 software and store artwork are implemented: 82 segments, Game A/B,
clock with a food-catching demo, daily alarm, saved settings/high scores,
portrait/landscape, and finite visual/haptic feedback. Physical-watch play-test
sign-off and M6 release remain pending. Use the [PT2 play-test checklist](docs/pt2-playtest.md)
and [results record](docs/pt2-playtest-results.md). This is a watch adaptation, not an exact ROM recreation.
[Reference observations and remaining fidelity gaps](docs/pp23-fidelity.md)
distinguish confirmed rules from provisional timing, paths and input behavior.
The app name is **Popeye G&W**; repository/package/build slug is `popeye-gw`.
See the [PRD](PRD.md), [repository conventions](CLAUDE.md), and the
[M5 notes and artwork](docs/m5-notes.md) and [M5 audit](docs/m5-audit.md). Earlier scene/M3/M4 audits describe their
historical builds.

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
pull request; neither requires the watch SDK. The M5 suite covers every attract
food position, alarm override, finite flashing, pause/resume, delayed callbacks,
timer allocation failure and haptic priority/settings/Quiet Time.

`src/c/game.c` and `game.h` have no platform dependencies. `game_init` starts a
round; `game_input` handles press/release edges immediately; `game_advance`
consumes active milliseconds. Pause/resume preserves partial timers, and
`game_step` advances to the next boundary for simulation. State and feedback
feed the segment renderer. Pacing values live in `src/c/tuning.h`.

The [PP-23 ledger](docs/pp23-fidelity.md) records the manual and recording
observations. Catches score one point, two dropped foods or a Brutus hit make
one miss, and misses clear at 200/500. Food pace and quantity reset every 100
points. Game A keeps Brutus on the left; B has a single Brutus changing sides.
The numeric timing table, five player poses, four food arcs, spawn scheduler
and precise miss/input behavior remain provisional. The fairness bot verifies
20,000 seeded games through 1,000 points, including side changes and resets.

## Playing

| State | Up / Down | Select | Hold Select | Back |
|---|---|---|---|---|
| Clock | High scores / Menu | Game A | Game B | Exit |
| Playing | Move Popeye one pose | Pause | — | Pause |
| Paused | — | Resume | — | Quit to clock |
| Game over | Move Popeye | Play again | Other mode | Clock |
| Menu | Move selection | Choose / toggle | Choose | Return |
| Alarm time editing | Increase / decrease | Save | Save | Cancel |
| Alarm ringing while idle | Stop | Stop | Stop | Stop |

`src/c/scene.c` turns game state into the PRD 9.2 set of 82 lit segments; it
is pure C99 and host-tested. `src/c/view.c` draws each segment from a packed
sprite sheet using positions generated into `src/c/segments.h`. Only unlit digit/MISS registers have baked grey ghosts; the playfield stays
clear of overlapping character silhouettes.
`src/c/main.c` runs one `AppTimer` per step
or recovery and none while paused, over or in the clock. A separate presentation
timer finishes a bounded 1.5-second feedback sequence, including after game over,
then stops. It freezes while paused or hidden. Pausing keeps the
elapsed part of the current interval. Losing focus pauses a running game.

The clock uses all four digits, follows the watch's 12/24-hour preference,
and demonstrates a complete throw/flight/catch sequence in two-second poses.
All four food arcs appear over 48 seconds. With attract animation disabled,
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
blinks its splash/dizzy pose through recovery. Bonuses flash MISS, and game over
flashes the score and any new-best HI. Misses/bonuses give a short pulse; game
over gives a long pulse, followed by two short pulses for a new record. All
flashing finishes after 1.5 active seconds. Physical-watch feel, alarm and
Quiet Time behavior still need the owner's play-test.

## Handheld orientation

Choose **Menu → Settings → Orientation → Landscape** for a miniature
Game & Watch layout. Turn the watch so the three buttons sit along the top:
left to move left, middle to start/pause, right to move right. The preference
is saved, and clock, alarm and menus rotate too. Switch back to Portrait for
wrist use. See [landscape mode](docs/landscape.md) for details and screenshots.

## Art

The complete inventory includes five player poses, two dizzy poses, catching
feedback, fixed Olive ready/throw poses, two bell frames, one rival with
left-hammer/right-fist phases, four food arcs with five positions each,
splashes, MISS cans, digits and indicators. There are no rectangle
placeholders in the renderer.

Character animation and cargo sources were created with the built-in image
generation tool. Repeated positions and mirrored poses share those sources;
small UI symbols use geometric artwork. Prompts and preparation instructions
are in [art/source/README.md](art/source/README.md). The 1981-style PT2 advert,
store icons, source prompts and native screenshot set are linked from
[art/store/README.md](art/store/README.md).

```sh
# Only when changing source art; requires ImageMagick.
python3 tools/prepare_art.py
# Pack the committed runtime segments; Python standard library only.
python3 tools/build_art.py
python3 tools/build_art.py --check
```

The art check rejects missing, unexpected, empty or opaque full-screen
segments, and validates the 25 × 25 launcher icon. Host tests check all 82
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
