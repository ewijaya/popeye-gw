# Popeye G&W — Product Requirements Document

> PP-23 correction approved by the owner's 2026-10-05 scene and reference
> requests. Target: Wide Screen PP-23 (1981), not Table Top/Panorama or arcade.
> [Source observations and fidelity gaps](docs/pp23-fidelity.md) distinguish
> documented rules from this app's provisional timing, paths and controls.
> This document specifies the watch implementation, not recovered Nintendo code.

| | |
|---|---|
| Status | M5 and bottom-button view accepted on PT2; M6 unreleased |
| Date | 5 October 2026 |
| Owner | Edward Wijaya |
| Target | Pebble Time 2 (Emery, 200 × 228, 64 colours) |
| Repository | `ewijaya/popeye-gw` (public) |
| App name | Popeye G&W |

## 1. Summary

Popeye G&W is a fan-made single-screen LCD-style catch game for Pebble
Time 2, inspired by Nintendo’s Popeye Game & Watch wide-screen handheld.
Popeye stands in a rowboat and catches cargo that Olive Oyl tosses from a
left ledge beside her car. His rival Brutus lurks on a left pier or right ship and
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
| Look | LCD-inspired black segments on white, vivid printed scenery, restrained score/MISS ghosts |
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
- Sound in version 1. Feedback is visual plus vibration; no audio is implemented.
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
| Olive Oyl | Tall, slender figure beside her car on the left ledge; fixed ready/throw poses launch food along authored arcs. |
| Cargo | Falls along one of four lanes, one per catch pose. Each lane has its own fixed drawing (lane 1 tilted bottle, lane 2 fish, lane 3 upright bottle, lane 4 food can), as LCD segments cannot change shape. |
| Brutus | Burly, bearded rival with a sailor cap. Game A: hammer from the left pier. Game B: one Brutus changes between the left pier and right ship (fist). |
| Splash | Shown at the bottom of a lane when cargo hits the water. |
| MISS cans | Upper-right miss counter, up to three empty cans. A half-can marks one pending dropped food (watch affordance, see 5.6). |

### 5.2 Screen layout (200 × 228)

```
+----------------------------------------+
| GAME A   (bell)              8 8 8 8   |
|                         MISS [ ][ ][ ] |
| [car] Olive -> .  .                    |
| [ledge]           .   .               |
|                         .             |
| Brutus (A/B left)       Brutus (B right)|
|       Popeye: L2 L1 C R1 R2      [ship] |
|          \____orange boat____/         |
| ~~~~~~~ blue/turquoise water ~~~~~~~~~~ |
+----------------------------------------+
```

Only one Brutus is visible. Left and right positions in this diagram show
available states, not simultaneous opponents. Portrait and rotated landscape
share segment IDs, with pixel coordinates owned by the art pipeline.

Band positions are indicative. The art pipeline (section 9) fixes exact pixel
positions, and the game logic never depends on pixels.

### 5.3 Controls

Pebble Time 2 buttons are Up, Select and Down on the right, and Back on the left.

| Mode | Up | Down | Select | Hold Select | Back |
|---|---|---|---|---|---|
| Clock | High scores | Menu | Start Game A | Start Game B | Exit app |
| Playing | Move Popeye one pose left | Move Popeye one pose right | Pause | — | Pause |
| Paused | — | — | Resume | — | Quit to clock |
| Game over | Move left | Move right | Play again (same mode) | Switch mode and play | Clock |
| Alarm ringing | Stop | Stop | Stop | — | Stop |

- Provisional watch input: one press moves one pose, without hold repeat.
  Original hold/release and recenter behavior still requires measurement.
- A setting swaps Up and Down for players who think of Up as "right".
- Landscape can place the buttons above or below the screen. Platform input
  mapping keeps the physical left button moving left in either view; Swap
  still reverses movement. Menus follow the same physical left/right ordering.
- Input takes effect immediately and redraws at once, not on the next game step.

### 5.4 Core loop

1. The game advances in **steps** at a set interval (section 5.8).
2. Each step, every airborne piece of food advances one fixed position along its arc.
3. On its fifth step a piece reaches catch height. If Popeye is in that lane's
   pose, it is **caught** (+1 point). Otherwise it **drops** into the water
   with a splash.
4. Olive stays beside the car and throws along one of four food arcs according
   to the scheduler (section 5.9).
5. Brutus runs an independent attack cycle (section 5.5).

### 5.5 Brutus's attacks

- **Cycle:** idle for a random time, then wind-up (visible for two steps),
  then strike (one step), then idle again.
- **Hit:** if Popeye is in the far pose on that side (far-left for the left
  pier, far-right for the right pier) on the strike step, he is hit: a miss.
- **Safety:** near-left, near-right and centre poses are always safe from
  strikes.
- **Game A:** left hammer only. **Game B:** one rival changes sides after a
  strike. The exact original switch schedule is unmeasured; this cadence is
  provisional, as are the phase durations and vulnerable poses.

### 5.6 Drops and misses

- The **first** dropped cargo shows a half-can. The **second** drop turns it
  into a full miss and clears the half-can. Being hit by Brutus is always a
  full miss.
- After a miss, play pauses about 1.5 seconds: Popeye shows a dizzy pose (hit)
  or the splash flashes (drop), all airborne cargo is cleared, and play resumes.
- **Three misses** end the game.

### 5.7 Scoring and milestones

| Event | Effect |
|---|---|
| Catch | +1 point |
| Reach 200 or 500 points | Miss marks clear. Pending-drop clearing is provisional. No invented double-point bonus. |
| Score passes 999 | Display wraps to 000; watch high scores retain the true total. |

The manual confirms displayed rollover; exact internal restart behavior above
999 still needs observation. The app repeats bonuses in each displayed 1,000.

### 5.8 Difficulty and pacing

The PP-23 manual establishes food quantity/speed resets every 100 points.
Numeric values below are **provisional watch tuning**, not original timings.
They live in `src/c/tuning.h`. Existing airborne food finishes across a reset;
the smaller cap applies to new launches.

| Parameter | Both modes |
|---|---|
| Food step at cycle scores 0 / 25 / 50 / 75 | 560 / 440 / 340 / 240 ms |
| Food capacity within each 100 points | 1 below 10, 2 below 60, then 3 |
| Brutus idle | 4–9 s |
| Wind-up / strike | 2 steps / 1 step |
| Miss recovery | 1.5 s |

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
4. **Throw timing:** Olive shows a ready frame before switching food arcs. A
   launch lights its first segment immediately. Distinct silhouettes identify
   destinations at launch; no continuous gravity or interpolated movement.

A host test runs a perfect-player bot over 10,000 random seeds in both modes
up to 1,000 points and must record zero unavoidable misses.

### 5.10 Game A and Game B

| | Game A | Game B |
|---|---|---|
| Brutus | Left pier, hammer | Switches left hammer / right fist |
| Tempo | Same provisional food table | Same provisional food table |
| Cargo in the air at once | 1–3, resets every 100 | 1–3, resets every 100 |
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
- The clock/score register has an eight-pixel top inset. Clock hints spell out
  **Tap Select: A** and **Hold Select: B** beside the digits.
- Attract animation: a silent throw/flight/catch demonstration uses all four
  authored food arcs over 48 seconds, with one pose change every two seconds.
  Olive throws from her car, Popeye moves to catch, and Brutus threatens the
  opposite side. These are watch presentation timings, not measured PP-23
  timings. Alarm ringing replaces the demonstration with Olive’s bell.
- Attract Off uses static poses and a steady colon with minute-only updates.

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

- **Menu** (Down in clock mode): High scores, Alarm, Settings, Help, About.
- Menu titles and rows use 28-pixel bold text with compact, single-line labels;
  unusually long values use a smaller font to fit. Footers use 18-pixel text.
- A dark green banner with white lettering distinguishes page titles from
  black-on-white menu content and the black selected row in both orientations.
- **High scores:** best score and date for Game A and Game B, plus a reset
  option with confirmation.
- **Settings:**
  - Swap: swap Up and Down (off)
  - Vibrate: vibration (on)
  - Ghosts: ghost segments (on)
  - Demo: attract animation (on)
  - View: Portrait / Bottom / Top (Portrait); Bottom is the first landscape
    choice. Bottom and Top place buttons below or above. Rotate all game,
    clock and menu UI.
- **Help:** three short screens covering A/B controls, catches and misses, and
  clock/menu shortcuts. Select advances; movement buttons browse both ways;
  Back returns to the menu. Direction labels follow the selected orientation.
- **About:** version, licence, and a one-line note identifying this as a fan-made watch adaptation.
- All settings and scores are saved on the watch and survive app updates.

## 8. Feedback

| Event | Visual | Vibration |
|---|---|---|
| Catch | Catch sparkle and final food position for one game step | none |
| Drop (first) | Splash, then half-can | none |
| Miss | Dizzy Popeye or splash blinks; MISS cans stay lit | short pulse |
| Bonus threshold | MISS marks clear; MISS label blinks | short pulse |
| New high score at game over | HI and score blink, then remain visible | game-over long pulse followed by two short pulses |
| Game over | Score flashes | long pulse |

Every vibration respects the Vibration setting and Quiet Time. A new best
catch does not trigger a vibration. Combined third-miss/game-over events
produce one end-of-round cue, not stacked miss and game-over pulses.

Miss, bonus and end-of-round blinking lasts 1.5 active seconds with 250 ms
phases, then ends on a readable scene. Pause, focus loss or a full-screen
alarm freezes visual feedback; resume does not replay its vibration.
Restart/quit clears feedback. A failed presentation-timer allocation leaves
a readable, non-flashing scene.

## 9. Visual style and art

### 9.1 LCD look

- **Panel:** clean white field with vivid red car, orange ledge/boat and
  cobalt/turquoise ship/water. Use Emery's 64-colour palette at high contrast.
- **Lit segments:** opaque black. Fixed character poses and discrete food arcs.
- **Ghost segments:** optional sparse grey score/MISS registers, baked at build
  time. Omit overlapping character/food ghosts for PT2 readability.
- **No in-between frames:** a segment is either on or off.

### 9.2 Segment inventory (82)

| Group | Segments |
|---|---|
| Olive: fixed ready/throw, 2 bell poses | 4 |
| Food: 4 arcs × 5 steps | 20 |
| Popeye: 5 poses, 2 dizzy poses, 1 catch flash | 8 |
| Splashes | 4 |
| Brutus: left hammer / right fist, 3 phases each | 6 |
| MISS: 3 cans, half-can, label | 5 |
| Digits: 4 × 7-segment, colon, AM, PM | 31 |
| Indicators: GAME A, GAME B, bell, HI | 4 |

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

The owner-approved banner uses **BACK TO GAME & WATCH** and
**A whole childhood on one small screen.** The footer is large vintage type
without flanking rules. Source prompts, masters and exact-size exports live
in `art/store/` and `docs/releases/listing/`. Banner wording does not expand
the Emery-only platform target.

## 10. Technical architecture

### 10.1 Modules

| File | Role | Pebble APIs |
|---|---|---|
| `src/c/game.c/.h` | Pure game rules: state, step, input, scoring, misses, scheduler, Brutus cycle, fairness rules, deterministic xorshift RNG | none (host-testable) |
| `src/c/tuning.h` | All tunable numbers (section 5.8) | none |
| `src/c/scene.c/.h` | Pure game state → active segment bitset | none |
| `src/c/view.c/.h` | Backdrop, segments, menus, overlays and orientation | Graphics |
| `src/c/segments.h`, `segments_landscape*.h` | Generated portrait/top/bottom landscape segment tables | none |
| `src/c/orientation.c/.h` | Pack the reusable rotated UI buffer | none |
| `src/c/clock.c/.h` | Pure clock/attract scene and local-calendar alarm calculation | none |
| `src/c/alarm.c/.h` | Daily alarm scheduling and wakeup lifecycle | Wakeup, Persist |
| `src/c/store.c/.h` | Versioned settings/high-score codecs and defaults | none |
| `src/c/storage.c/.h` | Read/write those records on the watch | Persist |
| `src/c/feedback.c/.h` | Pure finite visual feedback and event priority | none |
| `src/c/feedback_service.c/.h` | Feedback timer, haptics and Quiet Time | AppTimer, vibes |
| `src/c/main.c` | App lifecycle, buttons, state machine, focus, clock ticks and alarm ringing | App, window, focus, TickTimer, AppTimer, vibes |

### 10.2 Timing

- **Game steps:** one `AppTimer` rescheduled per step or miss recovery; it
  stops while paused, in the background and immediately at game over.
- **Feedback:** a separate presentation timer runs only for the finite
  1.5-second effect, including the end-of-round flash after gameplay stops.
  It is cancelled while paused/hidden/backgrounded and resumes the remaining
  phase without repeating a pulse. No feedback timer remains after the effect.
- **Alarm:** a separate one-second foreground timer animates ringing, for at
  most 60 seconds. Idle clock ticks are unsubscribed during alarm rendering.
- **Clock:** a per-second tick for the colon blink while the clock is
  visible; per-minute when attract animation is off.
- **Input:** presses change state immediately and request one redraw.

### 10.3 Rendering

- One full-screen layer.
- The update procedure draws the backdrop bitmap (with or without ghosts),
  then each lit segment as a sub-bitmap of the sprite sheet using transparent
  compositing.
- Redraws follow game steps, input, visible clock ticks and alarm ticks.
  Finite feedback adds a redraw at each 250 ms phase boundary. There is no
  continuous frame loop.

### 10.4 Saved data

| Key | Record | Fields |
|---|---|---|
| 1 | Settings, version 3, 8 bytes | swap buttons, vibration, ghosts, attract, alarm on, landscape, buttons bottom, alarm hour/minute, checksum |
| 2 | High scores, version 1, 21 bytes | Game A/B full 32-bit best totals and local dates, checksum |
| 3 | Wakeup ID, signed integer | OS wakeup identifier for the daily alarm; reconciled on launch |

Settings and score records have a version byte and checksum. Settings v1/v2
remain readable: v1 defaults to Portrait and existing v2 landscape uses Top. Unknown/corrupt
records fall back independently without crashing. Writes occur at pause,
game over, focus loss and exit; failures stay visible. Wakeup scheduling
verifies its persisted ID against the OS rather than trusting stale data.

### 10.5 Budgets

| Item | Budget | Why |
|---|---|---|
| App image (.text + .data + .bss) | ≤ 61,440 bytes | The SDK refuses images above 65,535 bytes; keep 4 KiB headroom (lesson from KasugaBus) |
| Free heap while playing | ≥ 20% | Room for bitmaps and future features |
| Resources | ≤ 200 KB | Backdrop variants plus sprite sheet |
| PBW | ≤ 2 MB | Store guard |

The release build checks the app image size and fails above budget.

## 11. Performance, battery and reliability

- No gameplay or feedback timer runs while paused or backgrounded. Explicit
  input/focus transitions may save state or redraw, and an alarm can wake the
  app into the foreground. A foreground paused game can still receive an alarm.
- The clock redraws at most once per second, only while visible.
- Input-to-redraw target: within 50 ms. Immediate state changes are implemented;
  physical input/display latency remains a measurement to validate, not a
  claimed result from host tests.
- No heap allocation in the renderer update loop; bitmaps and the rotated
  UI buffer are reused. Orientation/ghost changes may replace loaded resources.
- The game never crashes on missing or corrupt saved data.
- Wakeup scheduling failures are visible in the Alarm menu, never silent.

## 12. Testing and quality (lean)

1. **Host unit tests** (`tests/test_game.c`, built with the system C compiler):
   - catch and drop rules, the half-can and misses;
   - Brutus cycle and hits;
   - milestone clearing and one-point catches;
   - score wrap and high-score total;
   - speed curve and cargo limits;
   - pause and resume;
   - deterministic replay from a seed.
2. **Fairness simulation:** the perfect-player bot from section 5.9, over
   10,000 seeds per mode.
3. **Store/adapter tests:** saved-data round trip, corrupt/old records, write
   failures, defaults, clock/calendar/DST and wakeup lifecycle. M5 tests cover
   all demo food positions, finite feedback, pause/resume, delayed callbacks,
   timer failure and haptic priority/settings/Quiet Time.
4. **Art pipeline test:** the generated files match the source art.
5. **CI:** a GitHub Action runs the host tests on every push. The Pebble SDK
   build stays local.
6. **Before a store release:**
   - clean `pebble build` within budgets;
   - emulator screenshots of the five store scenes;
   - 10-minute play-test on the owner's Pebble Time 2, covering both modes,
     pause and resume, alarm firing while closed and while playing, and the
     settings round trip. Record observations against the frozen PBW hash in
     [the PT2 checklist](docs/pt2-playtest.md); emulator results do not count as
     physical-watch passes.

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
  vivid printed nautical colours, white panel, restrained HUD ghosts, seven-segment
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
| M5 Polish | Vibration, bonus feedback, reference timing and play-test tuning, store artwork | Owner play-test sign-off on the watch |
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
2. **Tuning and fidelity:** section 5.8 is the watch adaptation’s provisional
   table. Watch play-testing can settle usability, not prove Nintendo timing.
   Original hold/release, catch windows, path counts and attack cadence remain
   trace-dependent in [the fidelity ledger](docs/pp23-fidelity.md). Do not mark
   these resolved solely because watch tests pass.
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
| Pending drop | Shown after one dropped cargo; the second drop becomes a miss |
