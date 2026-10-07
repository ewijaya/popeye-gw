# Rich watchface implementation

The owner approved this expansion on 2026-10-06, including a continuously
active default animation and phone configuration through Clay. This document
records watchface scope outside the game's PRD. The game remains unchanged.

## Product direction

Keep the fixed-pose, printed-colour LCD harbour. Make time and two compact
information rows useful at a glance. All three characters participate in
connected throw, movement, catch and threat sequences, starting immediately
when the face becomes visible. Time and information remain stable while the
scene animates. No continuous smooth animation or sound.

## Feature acceptance

| Feature | Required behaviour |
|---|---|
| Information panel | Two independently selected rows; off/date/battery/weather/steps/world time/event; optional slow rotation; second-row symbols and values, with world-location labels. Battery, Weather and Steps rows can pair with a second one of those on the same line, as plain text without icons, in the chosen order (see Pair rows) |
| Calendar | Day/month or month/day or numeric; English/German/French/Spanish/Japanese weekday labels; Japanese month/day formatting; optional year and ISO week number |
| Power | Battery percent/bars/both/spinach; threshold visibility; charging shown in the battery row |
| Steps | Daily count, percentage or spinach meter; configurable goal; one brief goal celebration; honest unavailable state |
| Weather | Opt-in; automatic or manual city; Celsius/Fahrenheit; current or high/low; refresh interval; cached/stale indication; weather symbols confined to the information row |
| World time | Chosen city/time zone, custom label and time format; phone computes DST-aware offset; cached offset expires visibly |
| Event | Label/date; countdown or elapsed days; optional annual repeat; leap-date handling; brief date celebration |
| Animation | Arcade (default, crowded)/Relaxed/Still (Lively and Once per minute retired: saved Lively loads as Arcade, saved Once per minute as Still); speed and pauses; individual character activity; quiet hours and battery cutoff. Still shows a paused game frame that changes only with the minute (see below) |
| Themes | Ivory default, Original/Green/Amber alternatives; optional custom background/segment/accent; subtle Faint or Strong ghosts; colour or monochrome scenery |
| Accessibility | Larger time, high contrast, reduced motion, steady or blinking colon; no colour-only meaning |
| Clay configuration | My Apps settings entry; grouped readable controls that appear only while a row uses them; Classic/Everyday/Traveller/Large Time layout presets independent of animation activity; persistent settings; save updates the watch |

## Still scene

Still never animates: `data_animation_allowed()` stays false, so no frame timer
runs and the minute tick is the only redraw. Instead of the idle cast it shows
`face_scene_still()`, a pure function of the local hour and minute that is
identical for every second of a minute. Decided 2026-10-07; details in
`docs/watchface-still-notes.md`.

- **Paused demo frame.** The app's attract demo beat, frozen: lane = minute
  mod 4, beat 1–5 = 1 + (minute div 4) mod 5, so every lane/stage pair appears
  each 20 minutes. Popeye's pose, the catch flash on the last beat, Brutus's
  side and wind-up/strike, and Olive's throw on the first beat all come from
  `clock_scene`'s demo branch.
- **Second food.** When the first food is past stage 0, Olive's next food is in
  another lane, behind it by exactly Popeye's pose distance (the `can_launch`
  spacing), never in the lane Brutus threatens. The candidate order flips every
  20 minutes. Olive is in her throw pose when that food is at stage 0.
- **Kiss.** On the hour and all day on 14 February, one fixed frame replaces the
  action: the idle scene with Olive's kiss pose and the heart over Popeye
  (`scene_kiss` final step).
- **Decorations left out.** Mode lamp and HI stay unlit: HI sits beside the
  digits and would read as a high score; both overlap the Large Time clock.
- Other modes' static fallback (quiet hours, battery cutoff, reduced motion,
  covered, no activity) and the game's clock page are unchanged.

## Pair rows

Each row has a main item and an optional companion (`row1_then`, `row2_then`, wire keys
49 and 50). Only Battery, Weather and Steps can be a main item with a companion, or a
companion; a row never pairs with itself, and anything else is ignored. A pair row is plain
text on one line, the main item first, two spaces apart, and never draws an icon, so
`Weather` then `Battery` reads `21C  62%` and `Battery` then `Weather` reads `62%  21C`.

- Battery is its percentage, with a `+` while charging. With "show battery only below" set,
  an item above the threshold is left out and the other half shows alone.
- Steps are the count, or the percentage of the goal; the progress meter shows the count.
- Weather is the current temperature or the high/low, with `*` when stale. Disabled, unavailable
  or re-united data shows `--`, as single rows do.
- Rotating rows keep each row's own pair, and two rows that are identical, pair included, do not
  rotate. The text comes from `data_format_pair()` and is covered by the host tests.
- Stored in slots 26 and 27 of the saved settings record, which were reserved, so the record's length
  and version are unchanged: older records load as single rows and an older app ignores the slots.

## Settings page

Decided 2026-10-07 to trim the Clay page (47 controls) without removing saved settings.
`config.js` marks each row-driven section with `use`, `schema.groups()` turns that into data for
the page, and `config-custom.js` shows a section while a row (or a pair's second item) uses it:
Date, Battery, Daily steps, Weather (also while weather updates are on), World clock and
Event countdown. Options of switched-off features are hidden, not greyed. Hidden controls keep
their saved values. About 25 of 45 controls show at the default layout.

Retired with their wire numbers kept, pinned to the defaults: animation beat (speed), pause between
loops, active characters and artwork colour. Celebrate and the step goal now sit in "Motion and
celebrations". The page was checked in Chrome with the real Clay code and the phone's `ready`
event; a phone WebView was not available.

## Arcade scene

Arcade (600 ms beats) shows `face_scene_crowd()`: a continuous 32-beat pattern, a
pure function of the frame, with no empty beat and no Pause beats (the Pause
setting applies to Relaxed). Decided 2026-10-07 from the owner's request.

- **Foods:** 12 throws per pattern, two or three in flight on most beats and never
  more than the game's three, each lane holding one food at a time. Every lane is
  thrown to three times per pattern.
- **Popeye:** leaves each catch at once, one pose per beat, and is in place for the
  next; the catch flash shows only under food at its last position.
- **Brutus:** five idle beats, two of wind-up and one strike, alternating sides
  like Game B (strikes on beats 0, 8, 16, 24).
- **Olive:** throws as each food leaves; blows a quick kiss on two free beats
  after two of the throws, like the game's bonus nod.
- **Game rules kept** (checked by the tests from what is lit): landings are
  spaced by Popeye's travel, no food lands in Brutus's lane on the strike or the
  beat before, and Popeye is never in the strike pose.
- The hour and 14 February replace the crowd with Olive's kiss, as in the other
  modes. Character-activity switches still choose which of the three move.
- Relaxed keeps the original one-food cycle at 2,000 ms.

## Runtime requirements

- Only one animation timer; no animation while covered, in Quiet Time, in
  configured quiet hours, below the cutoff off power, or with reduced motion.
- Resume promptly when visible and eligible. No replay of elapsed animations.
- Information rows clear the background only behind their own text and icons, so
  food crossing the row area stays visible at all five positions; it passes behind
  text only where they overlap (2026-10-07: the full-width strips hid every lane's
  second position).
- Minute time updates remain independent of character animation. Per-second
  updates are allowed only for an explicitly enabled blinking colon.
- No sound. Disconnect vibration is off by default and respects Quiet Time.
- Validate phone values and persisted records. Unknown or partial settings
  preserve valid existing values; corrupt saved settings fall back safely.
- Keep cached weather readable on failures and mark stale/unavailable data.
  Changed temperature units must never relabel old values incorrectly.
- World time uses a named zone with DST rules, not a permanent UTC offset.
- Do not introduce a watchface button handler that steals system navigation.
- Shared game code and art remain referenced by path and are not modified or
  duplicated. Watchface-only additions live under `watchface/`.

## Verification

Host tests cover settings/defaults/presets, invalid inputs, persistence,
calendar and event edge cases, animation invariants and unchanged clock time.
Phone tests use mocked transport to cover queueing, weather errors and caches,
zone offsets, settings payloads and configuration behaviour. The final audit
includes a clean Emery build, PBW identity and budgets, native-size layout
inspection and a Clay-to-watch configuration round trip.

Emulator checks do not prove physical battery consumption, health-data access,
phone geolocation permission behaviour or the mobile WebView's compatibility.
Record those observations after installing the final development build.
Nothing here authorizes publication of a frozen release.
