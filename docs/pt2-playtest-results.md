# PT2 play-test results — 2026-10-05

Status: **M5 and Bottom view accepted; Help and Bottom-first cycle installed**. The owner
enabled the phone's Dev Connection. CloudPebble ping returned Pong, and the
installer exited 0 with `App install succeeded.` Game A subsequently appeared
in the physical-device log. Connection, installation and runtime logs do not
establish visual/tactile acceptance.

Follow [the checklist](pt2-playtest.md). The owner reported: “LOVING IT! Perfect.
I think the controls is fine... so far so good.” This is positive subjective
control feedback, not a separate pass for every requested check. The owner
requested larger, more compact menus, a fix for the clock's top clearance and
an explanation of the A/B hint. Those changes are installed in the new
candidate below and were rechecked by the owner.
The owner subsequently confirmed “Both looks good” for larger menu text and
clock clearance, and requested a vibrant title background to distinguish it
from menu content. That additional styling is installed. The next requested
observations covered deliberate Brutus hits, dizzy/MISS/vibration feedback and
Game B's side changes. The owner answered “yes everythig loooks good”; those
specific requested checks are passed. Silent alarm previews (Vibrate Off and
Quiet Time) and a scheduled alarm with the app closed were also confirmed:
“yes... all looks good.” The owner answered **“All pass”** to in-game alarms,
pause/resume and notification focus handling. For at least ten minutes across
both modes, landscape and scores/settings after updates and reopening, the
owner answered **“yes all looks good”**. These reports establish M5 acceptance
for the installed build, not publication approval or timing-accurate PP-23 fidelity.

## Current candidate: on-watch Help and Bottom-first landscape

The owner requested a concise Help page and Bottom as the default landscape
choice. Menu → Help now contains three short screens with Select to advance,
movement buttons to browse both ways, and Back to return. Direction labels
adapt to portrait/landscape. View cycles Portrait → Bottom → Top and preserves
existing saved views. Main-menu scrolling still reaches About below Help.

- Version **1.0.0**, source commit `c2e7270d36ef6694f0f9185d375b27430dba758f`.
- Clean tree when frozen at **2026-10-05 19:03:17 JST**.
- Frozen file: `.release/pt2-playtest/c2e7270d36ef/popeye-gw.pbw`.
- SHA-256: `79c82ea63923f9f3aa23724a99f45952a7602d7c9c9106349a427e88ee05c046`.
- Physical installation: **pass**, CloudPebble exit 0, `App install succeeded.`

| Build check | Result |
|---|---|
| Host tests / ASan+UBSan | Both pass; 10,000 seeds/mode with zero unavoidable misses |
| Clean SDK build | Exit 0 and literal `'build' finished`; Tool 5.0.40 / SDK 4.33.1 |
| App image / headroom | **19,752 / 61,440 bytes**, 41,688 bytes headroom; all six gameplay entry points retained |
| PBW / resources | **48,091 / 26,040 bytes**, within budgets |
| Identity / icon | Name, UUID, version, Emery-only watchapp verified; launcher icon unchanged |
| Emulator heap, Bottom A/B | Each **45,136 free / 66,184 used**, 40.55% free, after visiting Help |
| Visual checks | Three native Help captures: controls in portrait, rules and shortcuts in Bottom; no clipping |
| Navigation | Help Select/previous-page wrap, Back to menu, About route, and Portrait → Bottom exercised |
| Physical Help readability | Not yet separately reported; earlier M5 and Bottom acceptance retained |
| Cleanup | Environment locks removed; owned emulator logger stopped |

Logs/captures are under `.release/pt2-help/`. The preceding Bottom-default check
used two native captures in `.release/pt2-bottom-default/`, for five total across
this combined update. Rules, save formats and source art are unchanged.

## Accepted follow-up: Bottom landscape view

The owner requested landscape with the controls below the display after M5
acceptance. The new View cycle is Portrait → Top → Bottom. Existing Top remains
available; native Down/Up become physical left/right in Bottom, with Swap still
applied by the game. Clock, scene, menus and overlays rotate together.

- Version **1.0.0**, source commit `de86f20421a2b71a2920443ea24a4e4156d1941b`.
- Clean tree when frozen at **2026-10-05 18:46:31 JST**.
- Frozen file: `.release/pt2-playtest/de86f20421a2/popeye-gw.pbw`.
- SHA-256: `f40a6d16327e453900aeb9872893743bcae277d8a3a72ffd4cd31457b7a9821f`.
- Physical installation: **pass**, CloudPebble exit 0, `App install succeeded.`

| Build check | Result |
|---|---|
| Host tests / ASan+UBSan | Both pass; 10,000 seeds/mode, zero unavoidable misses |
| Settings migration | v3 remains eight bytes; v1/v2 alarms/preferences retained; v2 landscape becomes Top; Bottom round-trip passes |
| Controls / rendering | Both rotations and green palette packing pass; left/right press/release tested with Swap on/off; three complete atlases checked |
| Clean SDK build | Exit 0 and literal `'build' finished`; Tool 5.0.40 / SDK 4.33.1 |
| App image / headroom | **18,984 / 61,440 bytes**, 42,456 bytes headroom; all six gameplay entry points retained by linker |
| PBW / resources | **47,322 / 26,040 bytes**, within budgets |
| Identity / icon | Correct name, UUID, version, Emery-only watchapp; unchanged launcher icon |
| Emulator heap, Bottom A/B | Each **45,904 free / 66,184 used**, 40.95% free |
| Visual checks | Five native captures: Bottom settings, clock after reopening, A movement left/right, B |
| Physical follow-up | Pass: owner answered “Perfect!” to upright/comfortable layout, left/right + Swap, persisted Bottom and retained scores/settings |
| Cleanup | Environment locks removed; emulator logger stopped |

Logs and native captures are under `.release/pt2-bottom/`. The earlier core M5
acceptance remains attached to the prior candidate below. The owner accepted
this Bottom-view follow-up and then requested that Bottom become the first
landscape choice. That changes the cycle to Portrait → Bottom → Top;
existing saved views remain intact.

## Accepted M5 base: green title banners

- Version **1.0.0**, source commit `15ea674141ab4508f9156a0ddd338b180daeec83`.
- Clean tree when frozen at **2026-10-05 18:26:59 JST**.
- Frozen file: `.release/pt2-playtest/15ea674141ab/popeye-gw.pbw`.
- SHA-256: `4871914cca78826738c159a38b71bfc8f2c2d61d07d67183cc80ef163b27de33`.
- Physical installation: **pass**, CloudPebble exit 0, `App install succeeded.`

| Build check | Result |
|---|---|
| Host tests / ASan+UBSan | Both pass; green palette rotation covered; 10,000 seeds/mode with zero unavoidable misses |
| Clean SDK build | Exit 0, literal `'build' finished`; Tool 5.0.40 / SDK 4.33.1 |
| App image / headroom | **17,572 / 61,440 bytes**, 43,868 bytes headroom; playable engine retained |
| PBW / resources | **37,974 / 18,689 bytes**, within budgets |
| Identity / icon | Name, UUID, version, Emery-only watchapp verified; unchanged launcher icon |
| Emulator heap, A and B | **47,548 free / 65,948 used**, 41.89% free after visiting menus |
| Physical heap | Not yet sampled on this candidate |
| Visual checks | Two native captures confirm green titles and white lettering in portrait and landscape |
| Cleanup | Build environment locks removed; emulator and watch loggers stopped after testing |

Logs and captures: `.release/pt2-header/`. The title accent uses the existing
fourth palette slot; the UI bitmap remains 10 KB. The owner-accepted font size
and clock inset are unchanged. No publication approval has been given.

## Previous candidate: larger menus and clock clearance

- Version: **1.0.0**, source commit `d27d095287ddb2f851aea17e087a52d2fd8c9533`.
- Clean tree when frozen at **2026-10-05 18:19:39 JST**.
- Frozen file: `.release/pt2-playtest/d27d095287dd/popeye-gw.pbw`.
- SHA-256: `e0760a30acb5e54a68e63c2ff8d0e7a297d08641c2ffb893e57bc046ab89b9d3`.
- CloudPebble installation: **pass**, exit 0, `App install succeeded.`
- Name, UUID, version, Emery-only target and watchapp type verified.

| Build check | Result |
|---|---|
| Host tests / ASan+UBSan | Both pass; 10,000 seeds per mode, zero unavoidable misses, all four catch paths exercised |
| Clean SDK build | Exit 0 and literal `'build' finished`; Pebble Tool 5.0.40, SDK 4.33.1 |
| App image / headroom | **17,572 / 61,440 bytes**, 43,868 bytes headroom; gameplay engine functions confirmed linked |
| PBW / resources | **37,973 / 18,689 bytes**, within budgets |
| Heap, emulator A and B | Each **47,548 free / 65,948 used**, 41.89% free, sampled after visiting menus |
| Heap, physical watch | New candidate not yet sampled; previous build's observations are below |
| Visual checks | Five native captures: clock, main menu, portrait settings, landscape settings, alarm; no clipped clock digits or wrapped menu rows |
| Art / launcher | All 82 generated segments match source art; icon unchanged from prior inspected version |
| Cleanup | Environment lock files removed; emulator logger stopped |

Local captures and audit logs are in `.release/pt2-menu/`. One emulator
recovery reset was needed after a screenshot timeout; its previous state was
backed up locally first. This did not reset the physical watch. No gameplay
timing, saved-data format or catch mapping changed.

## Initial candidate and audit (superseded)

- Version: **1.0.0**, Emery-only watchapp, Popeye G&W.
- Source commit: `2270a3d23177774a1778c05552b2716ad376a399`; clean tree when frozen.
- Frozen at **2026-10-05 17:58:49 JST**.
- Local file: `.release/pt2-playtest/2270a3d23177/popeye-gw.pbw`.
- SHA-256: `2d3234cbdcada14c8210016a77985b2fab56622d40be7f27376b328fae41ce76`.
- UUID: `6b7c8c28-36f0-47ca-a073-3e8f33efcaf0`.
- Toolchain: Pebble Tool 5.0.40, SDK 4.33.1.

| Build check | Result |
|---|---|
| Host tests and ASan/UBSan | Both pass; each run covers 10,000 seeds per mode through 1,000 points with zero unavoidable misses |
| Clean SDK build | Exit 0 and literal `'build' finished` |
| App image | 16,528 text + 48 data + 908 bss = **17,484 / 61,440 bytes**; 43,956 bytes headroom |
| PBW / resources | **37,872 / 18,690 bytes**, within repository budgets |
| Identity | Name, UUID, version, Emery-only target and watchapp type verified |
| Emulator heap, A and B | Each logged **47,636 free / 65,948 used**, 41.94% free |
| Physical watch heap, A | Start at 18:03:14 JST: **47,636 free / 65,948 used**, 41.94% free; one sample, not a peak trace |
| Physical watch heap, B | Not yet observed |
| Engine / icon | Bundle is byte-identical to the [M5 audited bundle](m5-audit.md); linked gameplay and prior unchanged-icon inspection apply |
| Cleanup | Build environment lock files removed; emulator audit logger stopped |

The local manifest and test/build/device logs are under `.release/pt2-playtest/`.
This frozen play-test candidate is not a store publication approval.

## Core M5 physical acceptance (candidate `15ea674`)

| Check | Status / evidence |
|---|---|
| CloudPebble connection | Pass: phone/watch connection responded to ping |
| Frozen PBW installed | Pass: updated candidate's CloudPebble install exited 0; initial candidate also ran Game A on watch |
| Larger menus and clock top clearance | Pass: owner confirmed “Both looks good” on candidate `d27d095` |
| Colored title banner | Installed in accepted `15ea674`; native portrait/landscape checks pass |
| Brutus hit feedback and B side changes | Pass: owner confirmed dizzy Popeye, one MISS, short vibration and both sides in B look good on `15ea674` |
| Clock food demonstration | Overall play-test accepted; individual routes in the full 48-second cycle were not separately reported |
| Up/Down controls | Owner reports controls feel fine on initial build; <50 ms latency remains unmeasured |
| Catches and miss feedback | Overall play-test accepted; Brutus miss/dizzy/pulse explicitly confirmed |
| Game A and B, ten minutes total | Pass: owner confirmed at least ten minutes across both modes; scores not provided |
| Pause/resume and focus loss | Pass: owner answered “All pass” to Select pause/resume and paused return from notification |
| Portrait/landscape and readability | Pass: larger text/clock clearance and final landscape check confirmed |
| Vibration On/Off and Quiet Time | Pass: hit vibration and silent alarm previews confirmed by owner |
| Alarm while app closed | Pass: owner confirmed scheduled alarm launches and rings |
| Alarm during gameplay | Pass: owner confirmed alarm leaves gameplay running |
| Settings/scores after reopening | Pass: owner confirmed retention |
| Settings/scores after app update | Pass: owner confirmed retention after the installed updates |
| 200/500 bonus on watch | Not separately reported; host coverage is separate |
| <50 ms physical input latency | Unmeasured |
| Overall M5 acceptance | **Accepted** on installed candidate `15ea674`, PBW SHA-256 above |

The owner also approved the final store banner with “A whole childhood on one
small screen.” M6 still needs final-build store screenshots and separately
approved GitHub/store publication. No release tag or store listing was published.
