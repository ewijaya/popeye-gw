# Watchface Still scene notes

Owner decision, 2026-10-07: make the existing **Still** animation option
richer rather than add a new option. No new enum value, Clay control or
settings migration; `settings.c/.h` and `pkjs/config.js` are untouched.

## Callers of the static path

| Caller | Path | Change |
|---|---|---|
| Game clock page, `src/c/main.c` `render()` | `clock_scene(attract=setting)` | none |
| Watchface Arcade/Relaxed, not animating (quiet hours, Quiet Time, battery cutoff, reduced motion, covered, timer failure, no activity) | `face_scene_active(frame 0, activity 0)` → `face_scene(-1)` → `clock_scene(attract=false)` | none |
| Watchface **Still** | was as above; now `face_scene_still()` | new |
| Tests: `tests/test_m4.c`, `tests/test_m5.c`, `watchface/tests/test_face.c` | `clock_scene`, `face_scene`, `face_scene_active` | new tests only |

`face_view_draw()` selects `face_scene_still()` whenever the saved mode is
Still, whatever else blocks motion: the scene is static, so quiet hours,
reduced motion and the battery cutoff have nothing to suppress. Every other
mode keeps the idle fallback. A full-day checksum test pins the game's
`clock_scene(attract=false)`, `face_scene(-1)` and the non-Still fallback to
their output from before this change.

## Scene rules

All logic lives in `watchface/src/c/face.c`; shared `src/c` code is unchanged.
These are presentation choices, not game pacing, so nothing went to `tuning.h`.

1. Kiss minutes (`clock_kiss_time()`): idle scene + `scene_kiss(7)`. No food.
2. Otherwise `clock_scene(attract=true)` at second `2 × beat`, where
   lane = minute mod 4 and beat = 1 + (minute div 4) mod 5.
3. Second food (beats 2–5): try the two lanes that are neither the first
   food's lane nor Brutus's threatened lane (3 when he is on the right, 0 on
   the left), starting with the second of them when (minute div 20) is odd.
   Light it at `stage − |pose distance|` if that is ≥ 0; at stage 0 Olive
   switches to her throw pose.

Two food items never share a lane or stage, keep the game's landing
separation, and never meet a rival wind-up/strike lane, so the frame is one
the game could produce (score ≥ 10 permits two items).

## Not included

Mode lamp (`SEG_GAME_A/B`, 5,4 and 5,14) and `SEG_HI` (80,7). HI sits just
left of the small clock digits and would label the time as a high score. Both
fall inside the Large Time clock band (y 0–40, digits from x 8), where
`face_view` draws segments over the large clock. The face already gains food,
throw, catch and strike detail, so the lamps were left out.
Character-activity toggles are not applied to Still; it shows all three.

## Verification, 2026-10-07

Development build at `335d275`..`43aec30`, not a release; no version change.

- `watchface/test.sh` and `--sanitize`, and `tools/test.sh` (fairness bot zero
  unavoidable misses in every mode) passed.
- Clean Emery build: exit 0 with `'build' finished`; only the existing SDK RWX
  warning. App image 22,254 + 337 + 1,016 = 23,607 / 61,440 bytes. From
  `game.c` the linker kept only `game_lane_pose`.
- PBW 1,141,412 bytes, resources 12,954 bytes, identity checks pass.
- Emulator previews in `docs/releases/clock/previews/still-*.png` (10:23 throw,
  11:00 kiss, 14 February kiss). Three captures across 14 s of one minute were
  byte-identical. Not yet seen on a physical watch.
