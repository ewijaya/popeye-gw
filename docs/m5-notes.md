# M5 implementation notes

M5 is accepted on the owner's PT2 as of 2026-10-05. The owner approved the final
banner and confirmed the guided physical checks after the readability updates.
The subsequent requested Bottom landscape view also passed the owner's focused
physical follow-up. At the owner's request, it is the first landscape choice
when cycling from Portrait; Top remains selectable.
Use the [guided checklist](pt2-playtest.md) and [results record](pt2-playtest-results.md).
Nothing has been published to the store. [Validation](m5-audit.md) covers the
local development build; [reference gaps](pp23-fidelity.md) remain explicit.

## Visible and tactile feedback

| Event | Display | Vibration |
|---|---|---|
| Catch | Existing catch sparkle and food at catch height for one step | None |
| First dropped food | Existing splash and pending half-can | None |
| Full miss | Splash or dizzy pose blinks; miss cans remain readable | Short pulse |
| 200 / 500 bonus | Cleared cans stay cleared; MISS label blinks | Short pulse |
| Game over | Score blinks, then stays readable | Long pulse |
| New best at game over | HI blinks with the score, then stays visible | Long pulse followed by two short pulses |

Blinking lasts 1,500 active milliseconds, alternating every 250 ms. Pause,
focus loss or a full-screen alarm freezes it; resume continues the remaining
time without repeating the vibration. Restart/quit clears it. If the SDK
cannot allocate a feedback timer, the scene remains readable without flashing.
New-best catches do not pulse repeatedly. Settings and Quiet Time suppress
vibration without suppressing visual feedback.

`feedback.c` is a pure scene-mask/elapsed-time helper. `feedback_service.c`
contains the Pebble timer and haptic calls; `main.c` connects game events and
app lifecycle. The game engine, score format, UUID and gameplay timing tables
are unchanged. The same segment mask works in portrait and landscape.

**PRD reconciliation:** sections 8 and 10 now explicitly describe the finite
feedback timer. The gameplay timer still stops immediately;
a separate finite presentation timer finishes the 1.5-second end sequence,
then stops too. During these effects there can be up to four presentation
redraws per second in addition to game/input redraws. There is no perpetual
post-game animation or timer while paused/backgrounded. These are presentation
durations, not claimed Nintendo timing. The owner authorized this PRD cleanup
before beginning the physical play-test.

## Clock demonstration

Attract mode now actually shows Olive releasing food, its five authored
positions, Popeye moving to catch it, and Brutus threatening the opposite side.
Each pose lasts two seconds; all four routes appear over 48 seconds. The time
continues to display normally, with no gameplay score or vibration. Alarm
ringing replaces this with Olive's bell. Attract Off remains static and only
needs the existing minute tick. No extra clock timer is introduced.

## Watch feedback: menu and clock readability

The owner's first PT2 report was positive about the controls. Following that
report, menu titles/rows now use 28-pixel bold text, compact one-line settings
labels and 18-pixel footers. Long values step down in font size to stay readable
without wrapping. The four-row viewport still scrolls to the fifth setting;
portrait and landscape share the layout.
The owner accepted the larger text and clock clearance, then requested a
clearer title hierarchy. White title text now sits on a dark green banner,
recalling the handheld casing. Landscape retains the green in the existing
fourth palette slot, without enlarging the reusable UI bitmap.

The clock/score register moved down six pixels to an eight-pixel top inset;
AM/PM and both ghost backdrops move with it. Clock hints now say “Tap Select: A”
and “Hold Select: B”. The alarm indicator sits at the right edge, clear of the
hints and MISS cans. These are presentation changes; game rules are unchanged.
Native captures cover clock, menu, settings, landscape settings and alarm.
Store screenshots predate this readability update and need refreshing against
the final accepted build before publication; approved banner artwork is unchanged.

## Bottom-button landscape follow-up

The View setting now cycles Portrait / Bottom / Top. Bottom turns the entire
interface the opposite way, putting the three buttons underneath, and maps
native Down/Up to physical left/right before menu/game input. Existing Swap
behavior remains available. Settings v3 retains the eight-byte record size and
migrates v1/v2 preferences and alarms. See [landscape mode](landscape.md).

## On-watch Help

The owner's requested Help entry sits between Settings and About. Three screens
cover starting A/B from the clock, movement and pause, catches and misses, and
clock/menu shortcuts. Select advances and wraps to the first screen; movement
buttons browse both directions. Back returns to the menu. Up/Down labels become
Left/Right in either landscape view. Help uses the existing large-text panel,
green title banner and rotation buffer without adding timers or saved state.

## Store artwork

The owner's revised brief is implemented as a 1981-style green/gold/ivory advert
with a red-strap PT2 and the app's Game B scene on its screen. Matching Popeye
portrait icons keep their face/pipe readable at small sizes. The owner selected
**BACK TO GAME & WATCH** / **A whole childhood on one small screen.**
The master and store export now use those lines, preserving the liked design.

- [Banner, 720 × 320](releases/listing/banner-720x320.png)
- [Store icon, 144 × 144](releases/listing/icon-144.png) and [48 × 48](releases/listing/icon-48.png)
- [Five native screenshots](releases/listing/): clock, A, B striking, game over/HI, alarm
- [Masters, complete prompts and references](../art/store/README.md)
- [Draft listing description](releases/store-description.txt)

## Owner PT2 acceptance

The accepted frozen PBW is identified in [the results record](pt2-playtest-results.md),
including its source commit and SHA-256. The owner confirmed at least ten minutes
across A/B, controls, larger menus and clock clearance, landscape, Brutus hits
and side changes, vibration suppression, alarms while closed and playing,
pause/focus behavior and settings/scores across restarts and app updates.

Physical input latency, per-route clock observations and 200/500 bonus events
were not separately measured or reported. Existing host coverage remains
separate from those physical observations. Exact original PP-23 timing/input
questions remain in the fidelity ledger; the play-test does not resolve them.

M6 covers final-build store screenshots, the frozen release build,
owner-approved destinations and store/GitHub publication. None was published.
