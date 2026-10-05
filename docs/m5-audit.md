# M5 development audit — 2026-10-05

Historical development audit. The subsequent clean frozen builds and the
owner's completed PT2 acceptance are recorded in [play-test results](pt2-playtest-results.md).

**Unreleased development build.** Audited as the M5 working-tree changes on
base `cfc88b9573ef75e7216d135e949f05400c7f0361`; the tree was dirty during the
build, so these results are **unreleasable**, not a frozen release approval.
Pebble Tool 5.0.40, active SDK 4.33.1, Emery. Gameplay engine/tuning and runtime
image resources are unchanged from the base.

| Check | Result |
|---|---|
| Host suite | Pass: strict C99, art/scene/rules, M4 UTC + New York, M5 feedback/demo |
| Sanitizers | Pass: AddressSanitizer + UndefinedBehaviorSanitizer |
| Fairness | Both runs: 10,000 seeds/mode through 1,000 points, zero unavoidable misses; 50,619,664 steps and 20,000,000 catches |
| Clean SDK build | Exit 0 and literal `'build' finished`; bundled linker emits its existing RWX LOAD-segment warning |
| App image | 16,528 text + 48 data + 908 bss = **17,484 / 61,440 bytes** |
| Image headroom | **43,956 bytes** |
| Engine linked | `game_start`, `game_input`, `game_advance`, `game_step`, `game_pause`, `game_resume`; feedback engine/service also linked |
| PBW | **37,872 bytes**, under 2,000,000-byte budget |
| PBW SHA-256 | `2d3234cbdcada14c8210016a77985b2fab56622d40be7f27376b328fae41ce76` |
| Resources | **18,690 bytes**, under 200,000-byte budget |
| Identity | Popeye G&W; `popeye-gw.pbw`; v1.0.0; Emery-only watchapp; configured UUID matches |
| Heap, A | Logged on emulator start: **47,636 free / 65,948 used**, 41.94% free |
| Heap, B | Start and game-over logs: **47,636 free / 65,948 used**, 41.94% free |
| Heap caveat | Logs are sampled start/over measurements, not a continuous peak-allocation trace; physical watch unmeasured |
| Launcher icon | Existing 25 × 25 PNG unchanged; art validation passes; prior native launcher inspection applies to identical icon bytes |
| Store art | Banner 720 × 320; icons 48 × 48 / 144 × 144; five native 200 × 228 captures |
| Cleanup | `.lock-waf*` deleted without exposing contents; debugger detached, log process stopped |

## Emulator checks

Installed this PBW and explicitly launched its UUID. Entered Game A and B,
paused/resumed, captured the idle demo, A, right-side Brutus strike in B,
game over with HI, and the Alarm menu's test alarm. Dismissed the alarm and
returned to the clock. Game A/B/start/over and alarm entries were observed in
the runtime log.

For repeatable screenshots, the A/B compositions and pre-game-over state were
staged through debugger field writes. The third dropped-food miss and the
199→200 bonus then ran through the normal engine and event handler; these are
fixture checks, not a claim to have earned those scores. No debug path or
fixture is compiled into the app. Original emulator scores (both zero), dates
and settings were restored, including portrait orientation and alarm off.

Debugger traces observed bonus (`events=8`), miss (`events=4`) and final
miss/game-over (`events=36`) feedback callbacks. After the end sequence,
`main.c::s_timer` and `feedback_service.c::s_timer` were both null and feedback
state was cleared. They were also null after returning to the clock.
Host adapter tests additionally cover partial-phase pause/resume, delayed
callbacks, timer allocation failure, restart cancellation, haptic priority,
Vibration Off and Quiet Time. Actual motor feel cannot be judged in QEMU.

Only five native screenshots were captured for this audit; the raw files in
[listing/](releases/listing/) are unedited. Landscape uses the same scene-mask
feedback and the unchanged, host-checked segment atlas/compositor; a new
landscape screenshot was not taken within this five-image set.

## Artwork and remaining acceptance

The banner uses the official PT2 hardware image as a shape reference and the
native B capture as its screen reference. It is generated marketing artwork,
not a measured photograph of the display. Source prompts and the two masters
are in [art/store/](../art/store/README.md); large store assets are not in the
PBW. The owner liked the visual treatment and subsequently selected
**BACK TO GAME & WATCH** / **A whole childhood on one small screen.**
The master and 720 × 320 export were updated with the built-in image editor,
and the two lines were visually checked at both sizes. This listing-only
edit does not change the audited PBW or its checksum.

[The reference ledger](pp23-fidelity.md) now includes a reproducible four-second
PP-23 recording sample. Full hardware timing, original input/hold behavior,
catch windows, side-switch cadence, and physical PT2 usability remain open.
No numeric game tuning was changed based on that short sample.

M5 acceptance still needs the [owner's watch play-test and art review](m5-notes.md).
M6 publishing has not been performed. There is no store registration, release
tag, version bump or claim of support for other Pebble models in this build.
