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
| Information panel | Two independently selected rows; off/date/battery/weather/steps/world time/event; optional slow rotation; second-row symbols and values, with world-location labels |
| Calendar | Day/month or month/day or numeric; English/German/French/Spanish/Japanese weekday labels; Japanese month/day formatting; optional year and ISO week number |
| Power | Battery percent/bars/both/spinach; threshold visibility; charging shown in the battery row |
| Steps | Daily count, percentage or spinach meter; configurable goal; one brief goal celebration; honest unavailable state |
| Weather | Opt-in; automatic or manual city; Celsius/Fahrenheit; current or high/low; refresh interval; cached/stale indication; weather symbols confined to the information row |
| World time | Chosen city/time zone, custom label and time format; phone computes DST-aware offset; cached offset expires visibly |
| Event | Label/date; countdown or elapsed days; optional annual repeat; leap-date handling; brief date celebration |
| Animation | Arcade (default)/Relaxed/Classic/Still (Lively retired, saved Lively loads as Arcade); speed and pauses; individual character activity; quiet hours and battery cutoff. Still shows a paused game frame that changes only with the minute (see below) |
| Themes | Ivory default, Original/Green/Amber alternatives; optional custom background/segment/accent; subtle Faint or Strong ghosts; colour or monochrome scenery |
| Accessibility | Larger time, high contrast, reduced motion, steady or blinking colon; no colour-only meaning |
| Clay configuration | My Apps settings entry; grouped readable controls; Classic/Everyday/Traveller/Large Time layout presets independent of animation activity; persistent settings; save updates the watch |

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
