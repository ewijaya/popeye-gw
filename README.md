# Popeye G&W

A fan-made LCD catch game for **Pebble Time 2**, based on Nintendo's
**Popeye Wide Screen PP-23 (1981)**. Olive throws food beside her car on the
left; Popeye catches it in his boat while Brutus threatens him with a hammer
from the left pier or a fist from the right ship. The PT2 scene uses crisp
black segments, a white field, and vivid red, orange, blue and turquoise.

**Status:** Version 1.0.2 is published on GitHub and the Pebble App Store.
It fixes compressed clock and score digits in Horizontal mode, with aligned
ghost outlines and more edge spacing. See the [release record](docs/releases/1.0.2.md).

**Download:** [Pebble App Store](https://apps.repebble.com/e9cb2950ca21440798fb1db8) ·
[GitHub release v1.0.2](https://github.com/ewijaya/popeye-gw/releases/tag/v1.0.2)
([PBW](https://github.com/ewijaya/popeye-gw/releases/download/v1.0.2/popeye-gw.pbw)).
Both downloads match the approved SHA-256 recorded in the release checks.

Review the [1.0.0 store description, screenshot gallery and release checks](docs/releases/1.0.0.md).
See the [PT2 play-test checklist](docs/pt2-playtest.md)
and [results record](docs/pt2-playtest-results.md). This is a watch adaptation, not an exact ROM recreation.
[Reference observations and remaining fidelity gaps](docs/pp23-fidelity.md)
distinguish confirmed rules from provisional timing, paths and input behavior.
The app name is **Popeye G&W**; repository/package/build slug is `popeye-gw`.
See the [PRD](PRD.md), [repository conventions](CLAUDE.md), and the
[M5 notes and artwork](docs/m5-notes.md) and [M5 audit](docs/m5-audit.md). Earlier scene/M3/M4 audits describe their
historical builds.

## Help

The same quick reference is available on the watch under **Menu → Help**.
In portrait, Select is the middle button on the right. In landscape it is
the middle of the three buttons above or below the screen.

| From / topic | Button or rule | What happens |
|---|---|---|
| Clock: Game A | Tap Select | Start Game A. Brutus attacks from the left. |
| Clock: Game B | Hold Select | Start Game B. Brutus changes sides. |
| Clock: best score | Press and hold Select | Pressing shows Game A's best with HI; holding past 0.6 s switches to Game B's best. Releasing starts the mode shown, so a tap is A and a hold is B. |
| Game over | Leave it alone | After 5 minutes with no button press the app returns to the clock. |
| Playing: move | Up / Down in portrait; Left / Right in landscape | Move one pose per press. Swap reverses movement. |
| Playing: pause / resume | Select | Pause; press again to resume. |
| Catch food | Reach its matching catch pose | Add one point. |
| Center pose | Stand upright | Safe from Brutus, but cannot catch food. |
| Dropped food | Two drops | Add one MISS; the first drop shows a half-can. |
| Brutus hit | Get hit | Add one MISS. |
| Game over | Three MISS | The round ends. |
| Quit a running game | Back twice | First pause, then return to the clock. |
| Clock: high scores | Up in portrait; Left in landscape | Show saved Game A/B scores. |
| Clock: menu | Down in portrait; Right in landscape | Open High scores, Stats, Alarm, Settings, Help or About. |
| Menu: Daily (1.1) | Select Daily | A 60 s round of Game B rules with the same food and Brutus pattern for everyone on today's date. Every attempt counts toward today's best. |
| Menu: Sprint (1.1) | Select Sprint | A 60 s round of Game B rules with a random pattern. |
| Daily / Sprint: time | Pause | The pause title shows the seconds left. Paused and MISS-recovery time do not count. A beep and pulse warn at 10 s; at 0 the round ends with "Time up!". |
| Menu: High scores (1.1) | Select; movement buttons | Page 1 shows Game A and B; page 2 shows the Sprint best, the Daily best and today's Daily best. Reset clears all of them. |
| Menu: Stats (1.1) | Select Stats; Select; movement buttons | Three pages of lifetime totals (the last has Sprint and Daily rounds). Select on the last page offers Reset stats; high scores are untouched. |
| Settings: Orientation (1.0.1) | Select Orientation, choose Vertical or Horizontal, then Select | Change screen orientation. Back cancels. |
| Settings: Buttons (1.0.1, Horizontal only) | Select Buttons | Toggle Bottom / Top. The choice is remembered in Vertical mode. |
| Settings: Swap | Select Swap | Reverse game movement. |
| Settings: Sound (1.1) | Select Sound | Turn the LCD-style beeps On or Off (default On). |
| Help pages | Select; movement buttons; Back | Select advances, movement buttons browse both ways, Back returns to the menu. |

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
| Clock | High scores / Menu | Game A (shows best A while held) | Game B (best B shown) | Exit |
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

The menu contains Daily, Sprint, High scores, Stats, Alarm, Settings, Help and About. **Menu → Help**
provides four short screens for controls, catches/misses, clock shortcuts and
screen setup in 1.0.1.
Press Select for the next screen or Back to return to the menu; the movement
buttons also browse Help in either direction. Direction labels adapt to the view.
Settings toggle
swapped controls, vibration, sound, ghost segments and attract animation. The renderer
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

Sound (1.1) plays short square-wave LCD beeps through the watch speaker: a high
blip for a catch, a lower tone for a drop or MISS, two notes for a 200/500
bonus, a short falling motif at game over and a rising one for a new best. One
beep plays per game step, so the third MISS and game over give only the
game-over sound. The alarm beeps in step with its pulses while idle. Sound
follows the Settings → Sound switch and the watch's speaker mute / Quiet Time;
it stops on pause, focus loss, restart and exit. Melodies are original.

In the launcher, the app's glance (1.1) reads like "Best A 214 / B 187 - 07:00":
both best scores plus the alarm time while the alarm is on. It is refreshed
once as the app closes, with no timer or background work.

After game over, the app returns to the clock if no button is pressed for five
minutes; that is the only timer on the game-over screen.

Daily and Sprint (1.1) are 60 seconds of Game B rules, counted in active play
time: the clock runs only while steps run, so pausing, leaving the app and MISS
recovery do not use it. Three MISS still end a round early. The tick that lands
exactly on the limit is resolved first, then the round ends ("Time up!"). Sprint
uses a random seed. Daily seeds the game from the local date, so everyone gets the
same food and Brutus pattern that day; every attempt counts toward today's best, and
the all-time Daily best is kept as well. The best Daily round of the day is saved as
an input replay (below) for a future online leaderboard; nothing is sent from the watch.
The replay format and persist keys are in [docs/v1.1-notes.md](docs/v1.1-notes.md).

Lifetime stats (1.1, **Menu → Stats**) keep games played per mode (counted once a
round scores or ends), catches, drops, Brutus hits, 200/500 bonus clears, the
longest run of catches without a MISS, and active play time (running steps and
MISS recovery only; paused, hidden or menu time is excluded). They live in their
own record and are saved at pause, game over, focus loss and exit.

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
flashing finishes after 1.5 active seconds. The owner confirmed physical-watch
controls, hit feedback, alarms and Quiet Time in the recorded PT2 play-test.

## Handheld orientation

In **1.0.1**, open **Menu → Settings → Orientation**. Choose **Vertical** or
**Horizontal**, then press Select to save; Back cancels. Orientation is the first
Settings item. **Buttons: Bottom / Top** appears beneath it only in Horizontal
mode. Select toggles button position, which is remembered when switching to
Vertical and back, including after reopening. Bottom is the initial horizontal
preference; existing horizontal Top/Bottom selections are preserved on upgrade.

Turn the watch clockwise for Bottom (buttons below) or counterclockwise for Top
(buttons above). Left moves left, middle starts/pauses, right moves right
(unless Swap is on). The clock, alarm and menus follow the chosen orientation.

On older **1.0.0** installations, use **Settings → View** and cycle
**Portrait → Bottom → Top**. Portrait is vertical; Bottom and Top are horizontal.
See [landscape mode](docs/landscape.md) for details and screenshots.

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

## Companion watchface

`watchface/` is a separate Pebble project, **Popeye G&W Clock** (`popeye-gw-clock`,
Emery only): the game's LCD clock scene as a watchface, portrait, ghosts on, with
AM/PM following the watch's 12/24h setting. Olive's food demonstration plays for
about ten seconds after each minute change, and not at all in Quiet Time or on a
low battery. It shares the game's `scene.c`, `clock.c`, `game.c` and artwork by
path. Details: [v1.1 notes](docs/v1.1-notes.md).

```sh
cd watchface
TERM=xterm pebble build
pebble install --emulator emery build/popeye-gw-clock.pbw
./test.sh
```

## Licence

See [LICENSE](LICENSE) for the repository licence.
