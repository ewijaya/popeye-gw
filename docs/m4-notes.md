# M4 implementation notes

The PRD remains the requirements source. These choices settle its implementation
details without changing game rules or the approved character art.

- The clock's four digits include leading zeroes in 24-hour mode; 12-hour mode
  displays 12 at midnight/noon and lights AM or PM. Attract-off uses a steady
  colon and minute ticks, following the battery rule in PRD 10.2.
- One full-screen layer draws menus, keeping the menu state and unfinished
  hour/minute edit intact while an idle alarm is ringing. Changes save only on
  Select; Back discards an unfinished time edit. The About version comes from
  `package.json` through the build.
- `store.c` is a pure C99 codec. Settings are 8 bytes, scores 21 bytes, with a
  version byte and a 32-bit checksum. The concurrent orientation work adds
  settings version 2 with migration from valid version 1 records. Date fields use YYYYMMDD. `storage.c`
  implements Pebble keys 1 and 2. An unknown version, wrong size, bad checksum
  or invalid value defaults only the affected record.
- Scores update in RAM on each record-breaking catch and persist on pause,
  game over, focus loss and exit. Equal scores preserve the original date.
  Failed writes keep the in-memory score available to retry. Reset writes the
  cleared record before replacing RAM, so a failed reset does not lose a score.
- Key 3 holds the wakeup ID; zero is a valid ID. All wakeups owned by this app
  belong to its single daily alarm. Reconciliation cancels orphan events and
  preserves a valid existing event, including an adjusted event waiting in its
  extra minute. No recurring timer is needed while the app is closed.
- Next-day recurrence uses local calendar arithmetic with `mktime` and
  `tm_isdst = -1`, rather than adding 86,400 seconds. Nonexistent or ambiguous
  local times follow the platform C library's normalization. Scheduling errors
  are visible in the Alarm menu. One E_RANGE retry adds exactly 60 seconds.
- During active play/recovery an alarm is a passive notification. It never
  changes the game timer or player input; idle/menu/paused alarms consume the
  dismissing press and return to the same page. Ringing expires after 60
  seconds, including time spent out of focus. Backgrounding cancels its timer
  and vibration; focus return resumes remaining visual feedback.
- The plain and ghosted backdrops are both bundled, but only one is loaded at a
  time. A preference change replaces that bitmap outside the drawing callback.
  No per-frame allocation is introduced.

`tests/test_m4.c` tests the actual storage and alarm adapters against an
in-memory Pebble API fake. It covers malformed records, persistence failures,
score ties and full-width totals, clock segments, calendar/DST recurrence,
alarm conflicts, due-event races, cancellation, app relaunch and wakeup launch.
The existing 20,000-seed fairness suite remains unchanged.
