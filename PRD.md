# Popeye G&W — Product Requirements Document

| | |
|---|---|
| Status | Draft 1.0 for owner review |
| Date | 5 October 2026 |
| Owner | Edward Wijaya |
| Target | Pebble Time 2 (Emery, 200 × 228, 64 colours) |
| Repository | `ewijaya/popeye-gw` (public) |
| App name | Popeye G&W |

## 1. Summary

Popeye G&W is a fan-made single-screen LCD-style catch game for Pebble
Time 2, inspired by Nintendo’s Popeye Game & Watch wide-screen handheld.
Popeye stands in a rowboat and catches cargo that Olive Oyl tosses from a
freighter’s deck. His rival Brutus lurks on the piers at the screen edges and
strikes when Popeye leans too far. Standing upright in the middle of the boat is
always safe from Brutus, but Popeye cannot catch anything there.

Like the classic handhelds, the app doubles as a clock with an alarm: when no
game is running it shows the time on the same segment display.

The approved cast is Popeye, Olive Oyl and Brutus. The watch adaptation uses
custom generated segment art and Pebble button controls; it does not claim
to be an official Nintendo release or an exact hardware emulation.

## 2. Decisions already made

| Topic | Decision |
|---|---|
| Game model | One-screen catch-and-dodge game with two modes, Game A and Game B |
| Theme | Popeye G&W: Popeye (hero), Olive Oyl (thrower), Brutus (rival) |
| Platform | Pebble Time 2 (Emery) only for version 1 |
| Look | Authentic LCD: dark segments on a pale LCD panel, faint "ghosts" of unlit segments, colour backdrop |
| Version 1 scope | Game A and Game B, idle clock and alarm, saved high scores, RePebble store release |
| Repository | Public on GitHub, fresh history, MIT licence |
| Quality bar | Lean: host unit tests for game logic, emulator screenshots, a play-test on the owner's watch before each store release |
| Phone | Not required. No PebbleKit JS, no phone settings page, no permissions |

## 3. Goals and non-goals

### Goals

1. A game that feels like a genuine 1980s LCD handheld: fixed segment
   positions, crisp on/off animation, rising tempo and quick rounds.
2. Instant play: open the app and press Select to play.
3. Fair difficulty that is hard at high scores but never asks for the
   impossible.
4. A useful clock and alarm while idle, like the originals.
5. Small, robust and battery-friendly, inside the Pebble app size limit
   with room to spare.
6. A clean, public, well-tested codebase that others can read and build.

### Non-goals (version 1)

- Other Pebble models (Pebble 2 Duo, round watches). The design keeps
  layout data separate so they can follow later.
- Sound. Pebble has no speaker; feedback is visual plus vibration.
- Online leaderboards, phone settings or any network use.
- Additional game modes beyond A and B.
- Exact emulation of the original handheld hardware.

## 4. Players and moments of use

- **Short breaks.** A round lasts one to five minutes. Players want to start
  in one press and pause instantly when interrupted.
- **Score chasing.** Players replay to beat their best score in each mode.
- **Glance at the time.** The idle clock is visible whenever the app is open
  and not playing.
- **Wake-up or reminder.** A simple daily alarm that rings even when the app
  is closed.

## 5. Game design

### 5.1 Scene and cast

| Element | Description |
|---|---|
| Popeye | Recognizable sailor with cap, pipe and oversized forearms in a rowboat at the centre of the harbour. Five poses: far-left reach, near-left reach, upright centre, near-right reach, far-right reach. |
| Olive Oyl | Tall, slender figure with a hair bun and long skirt on a freighter along the top of the screen. Walks between four throw spots and tosses cargo. |
| Cargo | Falls along one of four lanes, one per catch pose. Each lane has its own fixed drawing (lane 1 crates, lane 2 fish, lane 3 lanterns, lane 4 barrels), as LCD segments cannot change shape. |
| Brutus | Burly, bearded rival with a sailor cap. Game A: punches from the left pier. Game B: also attacks from the right pier. |
| Splash | Shown at the bottom of a lane when cargo hits the water. |
| Life rings | Miss counter, up to three. A half-ring marks one dropped cargo (see 5.6). |
| Gull | Lights up during Lucky Tide (double points, see 5.7). |

### 5.2 Screen layout (200 × 228)

```
+----------------------------------------+  y=0
| GAME A   (bell)(gull)        8 8 8 8    |  status: mode, alarm, Lucky Tide, digits
|----------------------------------------|  y≈24
|  ====== freighter deck / rail ======   |
|     Olive ->  T1    T2    T3    T4       |  throw spots above lanes 1–4
|----------------------------------------|  y≈72
|        .     .     .     .             |
|       .      .     .      .            |  4 lanes × 5 steps each
|      .       .     .       .           |
|----------------------------------------|  y≈150
| [pier]  Popeye poses: L2 L1  C  R1 R2 [pier]|  Brutus on piers
|  Brutus   \______rowboat______/  Brutus|
| ~~~~ splash1  splash2  splash3  splash4 ~~|  y≈196
|  (ring)(ring)(ring)(half)                |  misses
+----------------------------------------+  y=228
```

Band positions are indicative. The art pipeline (section 9) fixes exact pixel
positions, and the game logic never depends on pixels.

### 5.3 Controls

Pebble Time 2 buttons are Up, Select and Down on the right, and Back on the left.

| Mode | Up | Down | Select | Hold Select | Back |
|---|---|---|---|---|---|
| Clock | High scores | Menu | Start Game A | Start Game B | Exit app |
| Playing | Move Popeye one pose left | Move Popeye one pose right | Pause | — | Pause |
| Paused | — | — | Resume | — | Quit to clock |
| Game over | — | — | Play again (same mode) | Switch mode and play | Clock |
| Alarm ringing | Stop | Stop | Stop | — | Stop |

- One press moves exactly one pose. Holding a button does not auto-repeat,
  as on the original hardware.
- A setting swaps Up and Down for players who think of Up as "right".
- Input takes effect immediately and redraws at once, not on the next game step.

### 5.4 Core loop

1. The game advances in **steps** at a set interval (section 5.8).
2. Each step, every airborne piece of cargo moves down one position in its lane.
3. On its fifth step a piece reaches catch height. If Popeye is in that lane's
   pose, it is **caught** (+1 point). Otherwise it **drops** into the water
   with a splash.
4. Olive walks between throw spots and launches new cargo according to the
   scheduler (section 5.9).
5. Brutus runs an independent attack cycle (section 5.5).

### 5.5 Brutus's attacks

- **Cycle:** idle for a random time, then wind-up (visible for two steps),
  then strike (one step), then idle again.
- **Hit:** if Popeye is in the far pose on that side (far-left for the left
  pier, far-right for the right pier) on the strike step, he is hit: a miss.
- **Safety:** near-left, near-right and centre poses are always safe from
  strikes.
- **Game A:** left pier only. **Game B:** both piers, each with its own cycle.
  The two never strike on the same step.

### 5.6 Drops and misses

- The **first** dropped cargo shows a half-ring. The **second** drop turns it
  into a full miss and clears the half-ring. Being hit by Brutus is always a
  full miss.
- After a miss, play pauses about 1.5 seconds: Popeye shows a dizzy pose (hit)
  or the splash flashes (drop), all airborne cargo is cleared, and play resumes.
- **Three misses** end the game.

### 5.7 Scoring and milestones

| Event | Effect |
|---|---|
| Catch | +1 point (+2 during Lucky Tide) |
| Reach 200 or 500 points | If there are misses, all misses and the half-ring are cleared. If there are none, **Lucky Tide** starts: the gull lights up and catches score double until the next miss or drop. |
| Score passes 999 | The display wraps to 000 and play continues. The 200 and 500 milestones repeat in each 1,000. |

The high score stores the true total (it can exceed 999). The game display
shows three digits, like the original handhelds; the high-score screen shows
the full number.

### 5.8 Difficulty and pacing

All values are initial and tunable in one header (`src/c/tuning.h`), set by
play-testing.

| Parameter | Game A | Game B |
|---|---|---|
| Starting step interval | 560 ms | 440 ms |
| Speed-up | −16 ms per 25 points | −16 ms per 25 points |
| Fastest step interval | 240 ms | 200 ms |
| Cargo in the air at once | 1 below 10 points, 2 below 60, then 3 | 2 below 30, then 3 |
| Brutus idle time | 4–9 s | 3–7 s per pier |
| Wind-up warning | 2 steps | 2 steps |
| Pause after a miss | 1.5 s | 1.5 s |

### 5.9 Fairness guarantees

The scheduler never creates a situation a perfect player cannot survive.

1. **Reachability:** two pieces of cargo landing in lanes *d* poses apart
   are at least *d* steps apart in landing time, since Popeye moves one pose
   per press and a player can make at least one press per step.
2. **No forced hits:** no cargo lands in the far-left lane during a left
   strike window, or the far-right lane during a right strike window. The
   strike window is the strike step plus the step before it.
3. **Escape time:** a wind-up always gives at least two steps' warning, enough
   to move from a far pose to a safe pose.
4. **Throw timing:** Olive only launches from a lane's throw spot after she has
   walked there, so throws are visible before the cargo moves.

A host test runs a perfect-player bot over 10,000 random seeds in both modes
up to 1,000 points and must record zero unavoidable misses.

### 5.10 Game A and Game B

| | Game A | Game B |
|---|---|---|
| Brutus | Left pier | Left and right piers |
| Tempo | Normal | Faster start and floor |
| Cargo in the air at once | Ramps from 1 | Ramps from 2 |
| High score | Separate | Separate |

### 5.11 States

```
          Select / hold Select
 CLOCK  ───────────────────────►  PLAYING ──3 misses──► GAME OVER
  ▲  ▲                             │   ▲                  │  │
  │  └──────── Back ─── PAUSED ◄───┘   └── Select ────────┘  │
  │          (quit)       │ Select resumes                   │
  └───────────────────────┴──────────── Back ────────────────┘

 Alarm time reached (any state) ─► RINGING ─ any button ─► previous state
```

- Leaving the app or losing focus (notification, incoming call) pauses a game
  automatically. Reopening the app while a game is paused offers Resume.
- A paused game is kept only in memory. Quitting the app ends it.

## 6. Clock and alarm

### 6.1 Idle clock

- The **clock** state shows the time on the four digits with a blinking
  colon. AM/PM indicators follow the watch's 12/24-hour setting.
- Attract animation: in clock mode Olive, Popeye and Brutus move through a few
  idle poses slowly, about one change every two seconds, as the classic
  handhelds did. A setting turns this off to save battery.

### 6.2 Alarm

- One daily alarm: hour, minute and on/off, set from the menu.
- Implemented with the Pebble Wakeup API, so it rings even when the app is
  closed. After it fires, the app schedules the next day's alarm.
- **Ringing:** Olive rings the ship's bell (two-pose animation), the bell icon
  flashes, and the watch vibrates in a short pattern every two seconds for up
  to 60 seconds or until any button is pressed.
- **During a game:** the bell icon flashes and one short vibration plays, but
  the game continues uninterrupted, like the originals.
- **Quiet Time:** the alarm shows visually but does not vibrate.
- **Clashes:** if Pebble refuses the time because another app's wakeup is
  within a minute, the alarm is moved one minute later and the menu shows the
  adjusted time.

## 7. Menu, settings and high scores

- **Menu** (Down in clock mode): High scores, Alarm, Settings, About.
- **High scores:** best score and date for Game A and Game B, plus a reset
  option with confirmation.
- **Settings:**
  - Swap Up and Down (off)
  - Vibration (on)
  - Ghost segments (on)
  - Attract animation (on)
- **About:** version, licence, and a one-line note identifying this as a fan-made watch adaptation.
- All settings and scores are saved on the watch and survive app updates.

## 8. Feedback

| Event | Visual | Vibration |
|---|---|---|
| Catch | Popeye's catch pose flashes | none |
| Drop (first) | Splash, then half-ring | none |
| Miss | Dizzy Popeye or flashing splash, ring lights | short pulse |
| Lucky Tide starts | Gull lights, flashes three times | double pulse |
| New high score | "HI" indicator flashes on game over | double pulse |
| Game over | Score flashes | long pulse |

Every vibration respects the Vibration setting and Quiet Time.

## 9. Visual style and art

### 9.1 LCD look

- **Panel:** pale grey-green LCD background with a thin dark frame, inside a
  coloured "printed backdrop" (sea, sky, freighter hull) in muted colours
  from the Emery palette.
- **Lit segments:** near-black.
- **Ghost segments:** when the setting is on, every segment shows faintly in a
  colour one shade darker than the panel. Ghosts are baked into the backdrop
  image at build time, so the watch only draws lit segments.
- **No in-between frames:** a segment is either on or off.

### 9.2 Segment inventory (estimate ~90)

| Group | Segments |
|---|---|
| Olive: 4 throw spots × ready/throw, plus 2 bell-ringing poses | 10 |
| Cargo: 4 lanes × 5 steps | 20 |
| Popeye: 5 poses, 2 dizzy poses, 1 catch flash | 8 |
| Splashes | 4 |
| Brutus: left idle/wind-up/strike; right idle/wind-up/strike | 6 |
| Life rings 3 + half-ring | 4 |
| Digits: 4 × 7-segment, colon, AM, PM | 31 |
| Indicators: GAME A, GAME B, bell, gull, HI | 5 |

### 9.3 Art pipeline

- Source art is drawn at full-screen coordinates: one PNG per segment under
  `art/segments/`, plus `art/backdrop.png`. Exact positions come from where
  each segment is drawn, so no coordinate tables are maintained by hand.
- `tools/build_art.py`:
  - crops each segment;
  - packs them into one 2-colour sprite sheet;
  - bakes the ghosts into a backdrop variant;
  - writes `resources/` images and `src/c/segments.h` (segment ID → sheet
    rectangle → screen position).
- The tool is deterministic and checked by a test, so the generated files can
  never drift from the source art.
- Use the approved Popeye, Olive Oyl and Brutus source designs. Final pixel
  art is reviewed by the owner before release.

### 9.4 Store artwork

Menu icon 25 × 25; store icons 48 × 48 and 144 × 144; a 720 × 320 banner;
4–5 native emulator screenshots: clock, Game A, Game B with Brutus
striking, game over with "HI", and alarm ringing.

## 10. Technical architecture

### 10.1 Modules

| File | Role | Pebble APIs |
|---|---|---|
| `src/c/game.c/.h` | Pure game rules: state, step, input, scoring, misses, scheduler, Brutus cycle, fairness rules, deterministic xorshift RNG | none (host-testable) |
| `src/c/tuning.h` | All tunable numbers (section 5.8) | none |
| `src/c/view.c/.h` | Turns game, clock and alarm state into the set of lit segments, then draws backdrop plus lit segments | Graphics |
| `src/c/segments.h` | Generated segment table | none |
| `src/c/clock.c/.h` | Clock mode, colon blink, attract animation | Tick timer |
| `src/c/alarm.c/.h` | Alarm settings, wakeup scheduling, ringing | Wakeup, vibes |
| `src/c/store.c/.h` | Settings and high scores with versioned records | Persist |
| `src/c/main.c` | App lifecycle, window, buttons, state machine, focus handling | App, window, focus |

### 10.2 Timing

- **Game steps:** one `AppTimer` rescheduled each step. No timers run while
  paused, in the background or game over.
- **Clock:** a per-second tick for the colon blink while the clock is
  visible; per-minute when attract animation is off.
- **Input:** presses change state immediately and request one redraw.

### 10.3 Rendering

- One full-screen layer.
- The update procedure draws the backdrop bitmap (with or without ghosts),
  then each lit segment as a sub-bitmap of the sprite sheet using transparent
  compositing.
- Redraws only when state changes: at most one per step, input or clock tick.

### 10.4 Saved data

| Key | Record | Fields |
|---|---|---|
| 1 | Settings, version 1 | swap buttons, vibration, ghosts, attract, alarm on, alarm hour, alarm minute |
| 2 | High scores, version 1 | Game A best + date, Game B best + date |

Each record has a version byte. Unknown or corrupt records fall back to
defaults without crashing.

### 10.5 Budgets

| Item | Budget | Why |
|---|---|---|
| App image (.text + .data + .bss) | ≤ 61,440 bytes | The SDK refuses images above 65,535 bytes; keep 4 KiB headroom (lesson from KasugaBus) |
| Free heap while playing | ≥ 20% | Room for bitmaps and future features |
| Resources | ≤ 200 KB | Backdrop variants plus sprite sheet |
| PBW | ≤ 2 MB | Store guard |

The release build checks the app image size and fails above budget.

## 11. Performance, battery and reliability

- No work at all while paused or in the background.
- The clock redraws at most once per second, only while visible.
- Input to redraw within 50 ms.
- No heap allocation per frame; bitmaps are loaded once.
- The game never crashes on missing or corrupt saved data.
- Wakeup scheduling failures are visible in the Alarm menu, never silent.

## 12. Testing and quality (lean)

1. **Host unit tests** (`tests/test_game.c`, built with the system C compiler):
   - catch and drop rules, the half-ring and misses;
   - Brutus cycle and hits;
   - milestone clearing and Lucky Tide;
   - score wrap and high-score total;
   - speed curve and cargo limits;
   - pause and resume;
   - deterministic replay from a seed.
2. **Fairness simulation:** the perfect-player bot from section 5.9, over
   10,000 seeds per mode.
3. **Store tests:** saved-data round trip, corrupt and old records, defaults.
4. **Art pipeline test:** the generated files match the source art.
5. **CI:** a GitHub Action runs the host tests on every push. The Pebble SDK
   build stays local.
6. **Before a store release:**
   - clean `pebble build` within budgets;
   - emulator screenshots of the five store scenes;
   - 10-minute play-test on the owner's Pebble Time 2, covering both modes,
     pause and resume, alarm firing while closed and while playing, and the
     settings round trip.

## 13. Release and distribution

- **Versions:** semantic versioning, starting at 1.0.0. Tag `vX.Y.Z`, with a
  GitHub release that attaches the PBW.
- **Store:** RePebble App Store, category Games, as a watchapp for Emery only.
  - Listing name: Popeye G&W.
  - Description: short, about 500 characters, using the approved app name and cast.
  - Changelog per release.
- **Licence:** MIT for the project code. Existing character rights are not
  granted by the code licence. A LICENSE file sits in the repository root.
- **Approval:** the owner approves each store release after the play-test.

## 14. Identity and nostalgic presentation

- Display the app name as **Popeye G&W**. Use `popeye-gw` for the repository,
  package, directory and PBW basename. Keep the existing app UUID for updates.
- Use Popeye, Olive Oyl and Brutus in artwork, documentation and internal
  identifiers. Preserve their approved recognizable character designs.
- Follow the wide-screen LCD reference: fixed black segment poses, restrained
  printed nautical colours, pale panel, faint unlit ghosts, seven-segment
  numerals and Game A / Game B labels. Avoid smooth animation and gradients.
- Describe the app as a fan-made adaptation, without official affiliation
  claims. Reference: [Popeye (Game & Watch)](https://nintendo.fandom.com/wiki/Popeye_(Game_%26_Watch)).
- Check new art and listing text against this identity before merging.

## 15. Milestones and acceptance criteria

| Milestone | Deliverables | Done when |
|---|---|---|
| M0 Foundation | Public repo, PRD, README, LICENSE, project skeleton, CI for host tests | Repo is public with a clean history; CI is green |
| M1 Game logic | `game.c`, `tuning.h`, unit and fairness tests | All tests pass; the fairness bot reports zero unavoidable misses |
| M2 Playable prototype | `view.c` with placeholder rectangles for every segment, buttons, pause, game over | Both modes playable start to finish in the emulator; app image within budget |
| M3 Art | Approved character segment art, backdrop, ghosts, art pipeline and test | Owner approves the art; screenshots match the LCD look |
| M4 Clock, alarm, menu | Clock mode, attract animation, alarm with wakeup, menu, settings, high scores, saved data | Alarm rings with the app closed and during play; settings and scores survive an app update |
| M5 Polish | Vibration, Lucky Tide effects, tuning from play-tests, store artwork | Owner play-test sign-off on the watch |
| M6 Release 1.0.0 | GitHub release, RePebble listing | Listing live and verified; PBW hash matches the GitHub release |

## 16. Risks

| Risk | Mitigation |
|---|---|
| Identity drifts away from the approved handheld reference | Follow section 14 and review art at native watch resolution |
| Difficulty feels unfair or dull | Fairness rules plus bot test; all numbers in `tuning.h`; play-test before release |
| Button presses missed at high speed | Immediate input handling; no auto-repeat; check by play-test on the watch |
| App image size limit | Budget check in the build; generated tables instead of code; reuse of digit segments for score and time |
| Alarm not firing (wakeup clash, app deleted) | Visible status in the Alarm menu; reschedule on every launch |
| Battery drain | No timers while paused or idle beyond the clock tick; attract animation can be turned off |

## 17. Open questions

1. **Art production:** approved generated character sources feed the
   deterministic segment pipeline; the owner reviews final native-size art.
2. **Tuning:** the numbers in section 5.8 are starting points to settle by
   play-testing.
3. **Identity (resolved):** Popeye G&W, starring Popeye, Olive Oyl and Brutus.
4. **Later platforms:** Pebble 2 Duo (black-and-white, 144 × 168) is the
   most likely second target after 1.0.

## Appendix A. Glossary

| Term | Meaning |
|---|---|
| Segment | A fixed drawing on the LCD that is either on or off |
| Ghost | The faint outline of an unlit segment, as seen on real LCD panels |
| Step | One tick of the game clock; all movement happens on steps |
| Pose | One of Popeye's five positions |
| Lane | One of the four paths cargo falls along |
| Half-ring | Shown after one dropped cargo; the second drop becomes a miss |
| Lucky Tide | Double points earned by reaching a milestone with no misses |
