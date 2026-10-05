# PT2 play-test results — 2026-10-05

Status: **initial controls accepted; readability update being prepared**. The owner is available and
enabled the phone's Dev Connection. CloudPebble ping returned Pong, and the
installer exited 0 with `App install succeeded.` Game A subsequently appeared
in the physical-device log. Connection, installation and runtime logs do not
establish visual/tactile acceptance.

Follow [the checklist](pt2-playtest.md). The owner reported: “LOVING IT! Perfect.
I think the controls is fine... so far so good.” This is positive subjective
control feedback, not a separate pass for every requested check. The owner
requested larger, more compact menus, a fix for the clock's top clearance and
an explanation of the A/B hint. These changes need a new installed candidate
and another readability check.

## Frozen candidate and local audit

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

## Physical acceptance

| Check | Status / evidence |
|---|---|
| CloudPebble connection | Pass: phone/watch connection responded to ping |
| Frozen PBW installed | Pass: CloudPebble install exited 0; physical Game A start logged |
| Clock food demonstration | Owner observation pending |
| Up/Down controls | Owner reports controls feel fine on initial build; <50 ms latency remains unmeasured |
| Catches and miss feedback | Detailed owner observation pending |
| Game A and B, ten minutes total | Owner observation pending |
| Pause/resume and focus loss | Owner observation pending |
| Portrait/landscape and readability | Owner observation pending |
| Vibration On/Off and Quiet Time | Owner observation pending |
| Alarm while app closed | Owner observation pending |
| Alarm during gameplay | Owner observation pending |
| Settings/scores after reopening | Owner observation pending |
| Settings/scores after app update | Owner observation pending |
| 200/500 bonus on watch | Not yet reached; host coverage is separate |
| <50 ms physical input latency | Unmeasured |
| Overall M5 acceptance | Pending owner report |

The owner approved the final store banner with “A whole childhood on one small
screen.” Artwork approval does not imply completion of the physical checks.
