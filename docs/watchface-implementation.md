# Popeye G&W Clock implementation notes

The companion watchface expansion was approved on 2026-10-06. These notes
cover that project only; the game's PRD and behaviour remain separate.

## Identity and installation

**Popeye G&W Clock** is an Emery-only watchface in `watchface/`, with package
slug `popeye-gw-clock`, version 1.0.0 and UUID
`9f808f24-978d-4e7d-9af1-16bd31e4b8c5`. It installs beside the game, has its own
persistent settings and leaves watch buttons available for system navigation.
The rich update is implemented and installed. The owner confirmed the phone
settings save and active motion work well. Battery consumption and detailed
physical edge cases remain unmeasured; release approval is still separate.

```sh
cd watchface
npm ci
./test.sh
./test.sh --sanitize
pebble clean
TERM=xterm pebble build
pebble install --emulator emery build/popeye-gw-clock.pbw
# For the requested physical-device test:
pebble install --cloudpebble build/popeye-gw-clock.pbw
```

Use Pebble Tool 5.0.40, SDK 4.33.1 and its bundled compiler. Check both the build
exit status and the literal `'build' finished`. The build checks the linked C
image against the repository's 61,440-byte budget. The phone JavaScript bundle
and artwork are separate from that C image; exact candidate measurements belong
in its audit record.

Dependencies and phone tests require a working Node runtime. The test runner
accepts `WATCHFACE_NODE=/path/to/node`, tries the verified local Node 20.19.5
installation and common executable paths, then checks the PATH runtime. It
fails with an explicit error if Clay dependencies or phone tests are missing.
The root game test runner and root SDK build do not include the watchface.

## Phone settings and presets

Open **My Apps → Popeye G&W Clock → Settings** in the Pebble phone app. Clay
opens an offline form with grouped controls and a Save button. Weather access
is optional; choosing a preset does not enable it. Saved choices persist on
the phone and watch. A save sends validated changes in small AppMessage batches;
data updates are sent separately, and failed delivery is retried with limits.

| Preset | Information rows | Style |
|---|---|---|
| Classic | Both hidden | Ivory LCD |
| Everyday, the default | Date / battery | Ivory LCD |
| Traveller | World time / date | Ivory LCD, saved world zone |
| Large Time | Date / battery | Larger clock and high contrast |

Animation activity is independent of every preset: Arcade (the default), Relaxed,
Once per minute or Still. Lively (wire value 0) was retired on 2026-10-07: saved or
sent 0 loads as Arcade on both phone and watch, and the other values keep their numbers. Presets preserve motion mode, speed, loop pauses,
active characters, reduced motion and weather effects. Retired Active preset
preferences keep their choices under Custom. Japanese date labels use compact
weekday kanji and month/day order, with a numeric form when the year is shown.

Selecting a preset updates presentation controls once. Later individual edits
keep their values. Presets preserve personal labels, event dates, step goals,
weather consent and location choices, world zone, quiet hours and battery cutoff.

## Information and appearance

Two independently selected rows support Hidden, Date, Battery, Weather, Steps,
World Time and Event. With rotation enabled, the two nonempty choices alternate
in one row every 15–120 seconds. Rotation remains available when character
motion is stopped, and pauses when the face is covered.

The main clock follows the watch's 12/24-hour preference unless overridden.
The colon is steady by default. Date controls select day/month, month/day or
numeric order, English/German/French/Spanish abbreviations, Japanese date labels,
year and ISO week.
When year and week are both enabled, a compact format preserves them together
with the weekday and date.

Battery presentation offers percent, bars, both or spinach, with an optional
visibility threshold. Connection and charging indicators remain available.
Daily steps offer count, goal percentage or a spinach meter; unavailable Health
data displays `STEPS --`. Goal and event-date celebrations are brief and
recorded separately once per local day, only when motion is eligible.

Events accept a label and calendar date. A future date shows days remaining,
today shows `TODAY`, and elapsed mode shows days since the event. Without
elapsed mode a past one-time event displays `--`. Annual countdowns target the
next occurrence; annual elapsed mode uses the most recent occurrence. A
February 29 anniversary uses February 28 in a common year. Blank dates disable
the event display.

Ivory is the default; Original, Green LCD and Amber are also available, alongside
custom background, active-segment and accent colours. Ghost strength can be off,
faint or strong; artwork can retain its printed colours or use monochrome.
Larger time, high contrast and reduced motion support different readability
preferences. These settings recolour the shared scenery as well as indicators.

## Weather and world time

Weather is disabled by default. When enabled, the phone obtains its location
or geocodes a manually entered city, then requests Open-Meteo over HTTPS.
No mandatory API key is required. The form offers Celsius/Fahrenheit, current
temperature or today's high/low, a 15–180 minute refresh interval, and optional
LCD weather effects. GPS and requests have timeouts; requests are rate limited.

Successful weather data keeps its source timestamp. Errors retain available
cached values and mark them with `*`; missing data displays `WEATHER --`.
Changing units converts the phone's cached Celsius values before sending them,
so old Celsius values are never relabelled as Fahrenheit. A new location with
no matching cache clears the previous location's availability.

World time offers 29 curated cities/zones, an eight-character label and its own
time format. The phone computes offsets using bundled IANA 2026e rules for
2020–2040, including daylight saving time and fractional offsets, without
requiring Internet or `Intl`. It refreshes daily, at a DST transition and after
a zone change. The watch marks an offset older than 48 hours with `*`; an
unavailable offset displays `--:--` rather than inventing a new one.

## Animation and power

Arcade uses 600 ms beats, Relaxed 2,000 ms (the retired Lively was 1,000 ms). Custom beats range
from 500–4,000 ms; pause beats and individual character activity are selectable.
The default motion continues through linked throws, flights, catches and rival
poses. Classic runs the original five 2,000 ms demonstration beats and then
returns to a static scene; Still shows a static cast.

One presentation timer schedules the next animation beat or panel rotation.
Motion stops while covered, in system Quiet Time, during configured quiet hours,
with reduced motion, or at/below the selected battery cutoff off power. The
cutoff defaults to 20%; zero disables it. Matching quiet start/end means no
quiet interval. Quiet Time is checked during each beat and minute, because the
SDK does not expose a change subscription. Focus and power changes resume
eligible motion promptly without replaying elapsed frames. Allocation failure
leaves a safe static display, and the minute clock continues updating.

Seconds ticks are subscribed only when colon blinking is explicitly enabled.
There is no sound. Optional disconnect vibration is off by default and respects
both system Quiet Time and configured quiet hours.

## Shared files and verification

`watchface/wscript` references `../src/c/scene.c`, `clock.c` and `game.c`, plus
the generated segment header. Package resources reference
`../../resources/images/backdrop.png` and `segments.png`; the project contains
no copied game source or PNG art. The linker removes unused game engine code.
The test runner rejects duplicate shared sources or PNGs anywhere under
`watchface/`, excluding its build output and dependencies.

Watchface code separates pure scene selection, settings/date/data helpers,
Pebble rendering and runtime services. Settings use a 235-byte versioned,
checksummed record that fits one persistent-storage write. Data caches have
independent validation. Missing keys preserve existing preferences; invalid
stored records fall back safely.

Host checks cover scene invariants, presets, settings clamps, corrupted records,
calendar/leap/ISO-week cases, world offsets, data availability and unit changes,
rotation, timer lifecycle and request retries. Offline phone checks cover the
actual Clay form, payloads, queued delivery, geolocation/network failures,
weather caches and IANA offsets. These checks and emulator screenshots do not
prove physical battery consumption, Health permission behaviour, phone location
permissions or mobile WebView compatibility. Use the
[device checklist](watchface-playtest.md) for those observations, and the
[feature scope](watchface-features.md) for acceptance criteria.
