# Popeye G&W Clock changelog

## 1.0.0 — Unreleased

- Adds an Emery-only companion watchface with separate identity and settings.
- Uses continuous Lively character motion by default; also offers Arcade,
  Relaxed, Classic minute scenes and Still, speed, pauses and character controls.
- Adds an offline Clay phone form with Classic, Everyday, Traveller and
  Large Time presets, persistent preferences and validated partial updates.
- Keeps animation activity independent of layout presets, including Classic;
  presets preserve motion controls and reduced motion. Retired Active choices
  migrate to Custom without losing their saved layout or activity.
- Adds two configurable information rows with optional rotation: date,
  battery, weather, steps, world time and event countdowns.
- Adds date order, weekday language (including Japanese), year and ISO week, battery styles and
  thresholds, connection/charging status, step goals and brief celebrations.
- Adds opt-in Open-Meteo weather, manual or phone location, temperature units,
  high/low display, cached-data markers and optional LCD weather effects.
- Adds world time with bundled IANA DST rules, custom labels and time formats,
  plus event countdown/elapsed days, annual repeats and event-date celebrations.
- Adds four LCD themes with Ivory as the default, custom colours, ghost strength, monochrome scenery,
  larger time, high contrast and reduced motion.
- Insets the large clock eight pixels from its former position to avoid bezel
  clipping, and repositions its information rows. Faint ghosts use sparse ink
  for a subtler appearance. Retired Midnight preferences migrate to Ivory.
- Tightens large-clock spacing around a centered colon, with a narrow `1` so
  times such as `14:19` have no oversized gap before the minutes.
- Pauses motion while covered, in Quiet Time or configured quiet hours, and at
  the selected low-battery cutoff off power; resumes without replaying frames.
- Keeps the colon steady unless blinking is enabled. Optional disconnect
  vibration is off by default and respects quiet settings; there is no sound.
- Reuses the game's source and artwork by path, with host and offline phone
  checks. The owner confirmed phone settings save and active motion; detailed
  physical edge cases remain pending.

- Enlarges information lettering from 7px to 11px, aligns rows and icons to the right, and keeps animated cargo behind visible information.

- Replaces stretched information lettering with native-size Noto Sans JP for cleaner English and Japanese text, retaining clock-edge alignment.

- Offers battery icon + percentage by default, icon only, percentage only, or spinach can + percentage; removes BAT wording and adds a charging bolt. Existing preferences are preserved.
