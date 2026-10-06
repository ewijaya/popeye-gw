# Popeye G&W Clock watch test

Status: the original minute-demo development PBW was installed on 2026-10-06. The owner replied
“everything looks great!” to the readability, minute-demo and silence check.
This is positive normal-use feedback for that earlier build. The rich watchface
update adds continuous motion, phone settings and data services. Its development
PBW was also installed successfully on 2026-10-06, with SHA-256
`a683e7e36532e78b610fbfe66e5d327babc9ea93c8f8a16d339c9387dab2eac9`;
the owner confirmed “Settings save works and motion feels good.” Detailed edge
cases remain pending. See the
[development audit](watchface-audit.md) for separate host and emulator results.
Host tests and emulator evidence cannot fill in physical-device results.
Test on Pebble Time 2 (Emery), and repeat on the final frozen candidate.

For a development test, install `watchface/build/popeye-gw-clock.pbw`.
For release approval, repeat on the exact frozen file under
`.release/popeye-gw-clock/X.Y.Z/`, and record its SHA-256, version, date,
watch firmware and observed results in the candidate manifest.

## Original minute-demo build checklist

This table records the earlier build's intended behaviour. It does not describe
the default animation in the rich update.

| Check | Expected | Result |
|---|---|---|
| Identity | Listed as Popeye G&W Clock, separate from the game | Pending |
| Readability | Time and character art are legible, with no clipping | Pending |
| Time | Matches local time; minute and hour rollover are correct | Pending |
| Time format | Follows the watch's 12/24-hour setting, including noon/midnight | Pending |
| Minute demo | Starts once at the minute boundary, lasts about 10 seconds, then stays static | Pending |
| Covered face | Cover with a notification during the demo: it stops; clock is current on return | Pending |
| Missed minute | Keep covered across a minute; returning does not replay the missed demo | Pending |
| Quiet Time | Enable before a minute boundary: time updates without starting a demo | Pending |
| Low battery | At ≤20%, unplugged and not charging: time updates without starting a demo | Pending |
| Power connected | At ≤20%, plugged in or charging: demo may start at the next minute | Pending |
| Silence | No watchface vibration or sound on launch, ticks, demo or resume | Pending |
| Lifecycle | Switch away and back repeatedly; time and artwork recover correctly | Pending |
| Battery observation | Record start/end battery %, duration, and typical use over several hours | Pending |

Record readability and demo-duration feedback in plain words. If a low-battery
condition cannot be tested yet, leave it pending rather than claiming a pass.
In that earlier build, Quiet Time and battery are checked when a demo starts; enabling Quiet Time
mid-demo is not currently specified to interrupt it. Covering the face does
interrupt it.

This watchface has no gameplay controls, gameplay heap gate or alarm feature.
It is a fan-made companion with no official affiliation. A successful install
is not publication approval. After testing the frozen candidate, approval must
name its SHA-256, version and destinations; first registration also requires
review of the full proposed listing.

## Rich update device checklist

Install the current development PBW for these checks, then repeat relevant
checks on the exact frozen candidate before approval. Record the PBW SHA-256,
version, watch firmware, phone OS/app version, date and observed result.
Only the normal-use results explicitly confirmed by the owner are marked below;
host or emulator evidence is recorded separately.

| Check | Expected | Result |
|---|---|---|
| Identity and navigation | Popeye G&W Clock installs separately from the game; buttons retain system navigation | Pending |
| Default face | Everyday shows date and battery; Olive, Popeye and Brutus remain active through continuous fixed-pose sequences | Owner confirms motion feels good; detailed layout edge cases pending |
| Main time | Watch preference and explicit 12/24-hour overrides display correct time at noon, midnight and minute/hour rollover | Pending |
| Clay opening | My Apps settings opens a readable offline form in the real phone WebView and returns to the Pebble app on save/cancel | Owner confirms settings save works; cancel/offline variations pending |
| Save and persistence | Change several controls, save, switch faces and restart the phone app; saved choices reach the watch and remain selected | Owner confirms preset save works; extended restart persistence pending |
| Presets | Classic, Everyday, Traveller and Large Time update layout/style; animation activity, reduced motion, labels/goals and weather consent remain intact | Pending |
| Information rows | Select each row type and Hidden; labels/data fit without covering the clock or character lanes | Pending |
| Rotation | Two configured nonempty choices alternate at the chosen interval, including Still, reduced motion, quiet hours and low battery; hidden face stops updates | Pending |
| Date options | Date order and five weekday languages (including Japanese) work; year and ISO week remain visible together, including year rollover | Pending |
| Battery styles | Percent, bars, both and spinach reflect battery state; threshold visibility and charging indicator behave as selected | Pending |
| Connection | Connection indicator changes correctly; optional disconnect pulse occurs only when enabled and permitted by quiet settings | Pending |
| Steps and permissions | Daily count matches Pebble Health; percentage/meter respect the goal; denied or unavailable Health data displays `STEPS --` | Pending |
| Goal celebration | Reach the goal with motion eligible; celebration is brief and does not replay after switching faces or reopening that day | Pending |
| Weather opt-in | Weather disabled causes no location prompt or weather request; enabling it permits the selected phone/manual location workflow | Pending |
| Weather location and display | A manual city and phone location each produce plausible current/high-low data; effects follow their switch | Pending |
| Weather units and failures | C/F changes show correctly converted values; loss of Internet keeps valid cached data with `*`; a new uncached location displays `WEATHER --` | Pending |
| Weather refresh | Selected refresh interval is respected during normal use; GPS/network failures recover without repeated prompts or rapid requests | Pending |
| World time | Selected city/zone, fractional offsets, label and time format match an independent clock; phone restart refreshes the offset | Pending |
| World DST and stale data | A DST transition updates the offset; after sufficient phone disconnection, the stale marker appears without implying fresh data | Pending |
| Event dates | Future/today/elapsed displays are correct; empty or past one-time dates with elapsed off show `--`; annual repeats and February 29 behave as documented | Pending |
| Event celebration | Today's event produces a brief eligible-motion celebration once per local day; it does not show the steps-only goal label | Pending |
| Animation controls | Lively, Arcade, Relaxed, Classic and Still match their descriptions; custom speed, pauses and individual character switches work | Pending |
| Focus interruption | Cover with a notification mid-sequence and across minutes; motion stops and resumes promptly without a burst of missed frames | Pending |
| Quiet Time | Enable before and during motion; time continues, motion stops, and disconnect vibration is suppressed | Pending |
| Quiet hours | Verify normal and overnight intervals; matching start/end creates no quiet interval; eligible motion resumes at the end | Pending |
| Low battery and power | At/below chosen cutoff off power, motion stops; charging or rising above the cutoff resumes it; cutoff zero disables the limit | Pending |
| Themes and colours | All four themes (Ivory default), custom background/segment/accent colours, ghost strengths and monochrome artwork remain readable | Pending |
| Accessibility | Large Time and high contrast fit the screen; reduced motion stops character motion; the colon is steady until blinking is enabled | Pending |
| Lifecycle and silence | Repeated face switching recovers time/art/settings; launch, ordinary updates and celebrations produce no sound or vibration | Pending |
| Battery observation | Compare Lively and Still over representative use, recording duration, start/end battery %, refresh settings and notification frequency | Pending |

Leave untested conditions pending. In particular, emulator Health values,
synthetic battery changes and offline network mocks are not evidence of
physical permissions, battery consumption or mobile WebView compatibility.
