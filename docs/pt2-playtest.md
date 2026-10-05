# Pebble Time 2 play-test

This is the physical-device acceptance check for M5 / PRD 12.6, not store
publication approval. The candidate is v1.0.0, Emery only, with the existing
UUID and saved-data formats. Exact build identity and observations are kept
in [pt2-playtest-results.md](pt2-playtest-results.md).

## Before playing

The phone's Dev Connection must be on and the watch connected. Install the
frozen PBW named in the results, not a later file rebuilt into `build/`.
Keep the current scores and settings; do not use Reset scores for this test.
If an existing saved preference differs from a default below, note it before
changing it and restore it afterwards. The build does not reset saved data.

## First pass — clock and controls (about two minutes)

1. Open **Popeye G&W**. Leave the clock visible for about 50 seconds: Olive
   should release food from the car, food should travel through fixed positions,
   and Popeye should move to catch it. All four routes fit in a 48-second cycle.
2. Press **Select** once for Game A. **Up moves left**, **Down moves right**
   unless Swap is enabled. A press moves one pose; holding does not
   repeat. The center is safe from Brutus but cannot catch food.
3. Catch one item. Deliberately miss two: the first drop adds a half-can and
   stays silent; the second adds a full miss, blinks the splash and gives a
   short pulse. Game A's Brutus attacks with a hammer from the left.
4. Press Select to pause. Wait a few seconds, then Select to resume. Back also
   pauses; Back again from Paused quits to the clock. Report lag, accidental
   repeats, disappearing food, unreadable shapes or uncomfortable vibration.

Reply with **clock / controls / catch / miss / pause: pass or issue**, and
include the mode/score if something goes wrong. A subjective quick response
is useful but does not establish a measured <50 ms input latency.

## Both modes — at least ten minutes total

5. From the clock **hold Select for at least 0.6 seconds** to start Game B.
   Watch for a single Brutus changing between the left hammer and right fist.
   Try quick direction changes and catch food while avoiding his far-side hit.
6. Play both modes for at least ten minutes total. Note each mode's best score,
   any food you could not reasonably reach, and whether misses feel explainable.
   A new record produces HI at game over; score/HI blink briefly and then stay
   readable. Expect one long pulse, followed by two short pulses for a record.
7. During a round, cover the app with a notification/system screen if practical.
   On return it should be paused; no hidden play or continuing game vibration.
   Select resumes. Ordinary app exit ends the in-memory round.
8. In 1.0.1, from clock open Menu → Settings → Orientation. Choose Vertical or
   Horizontal and press Select. Back cancels. Confirm Buttons appears only in
   Horizontal mode and Select changes Bottom/Top independently of orientation.
   Try buttons above/below; physical left/right should still move left/right.
   Switch to Vertical, quit/reopen, then return to Horizontal: the chosen button
   position should return. Menus and clock rotate too. Restore the preferred view.
   On store version 1.0.0, Settings → View cycles Portrait → Bottom → Top.
   Toggle Swap and Ghosts once; verify the change and restore
   the previous choices. Turn Demo Off: poses and colon become static.

If naturally reached, check 200/500: MISS cans clear, MISS blinks, one short
pulse. If not reached, record **not reached**; host tests cover those events,
not a fabricated physical-watch result.

## Vibration and alarms

9. Set Vibrate Off. Preview an alarm through Down → Alarm → Test alarm;
   it should animate without vibrating. Any button dismisses it. Restore
   Vibrate On, enable the watch's Quiet Time, and repeat; it stays silent.
   Restore Quiet Time afterwards. With both enabled normally, the test alarm
   should give short pulses roughly every two seconds until dismissed.
10. Record the existing alarm setting, then set a daily alarm one or two
    minutes ahead. Leave the app. At the shown time it should launch with
    Olive ringing; dismiss it and confirm the next alarm is scheduled tomorrow.
    If scheduling reports an adjusted time, use the time shown by the app.
11. Set another alarm ahead and start a round before it fires. The bell should
    flash with one short pulse while gameplay continues. A movement press
    dismisses the indicator and still moves Popeye. Restore the original alarm
    hour/minute/on-off choice afterwards.

## Saved data and final report

12. Note the current high scores and one changed preference. Quit/reopen and
    verify both remain. The same frozen PBW can be reinstalled once to verify
    an app update preserves them; record that result separately from restart.
    Restore the original preference. Do not delete/reinstall via the launcher,
    since deletion may remove saved data.

Report:

- Game A/B scores and approximate minutes played.
- Clock/food, controls, catch/miss feedback, pause/focus and both orientations.
- Vibration On/Off, Quiet Time, closed-app alarm and in-game alarm.
- Settings/scores after reopening and, if performed, updating the app.
- Anything too small, dim, fast, slow or uncomfortable; include mode and score.
- Overall: pass, or the concrete issues to fix.

Only observed checks can be marked passed. Device install/logs are recorded
separately from the owner's visual and tactile observations. Unmeasured original
PP-23 behavior stays in the fidelity ledger regardless of this result.
