# PP-23 scene and rules audit — 2026-10-05

Development build based on `0ebc86d` plus the changes in this commit. The tree
was dirty during the local build: **unreleased development evidence**, not a
store release or owner watch sign-off. Pebble Tool 5.0.40, SDK 4.33.1, Emery.

| Check | Result |
|---|---|
| Host / ASan + UBSan | Both passed; strict C99, art regeneration check, clock/storage/wakeup tests in UTC and New York, 200 scene rounds |
| Fairness | 10,000 seeds/mode through 1,000 points; 50,619,664 steps, 20,000,000 catches; zero drops/hits/misses/stalls |
| Attack coverage | 1,894,982 left / 628,317 right strikes; A remains left, B uses one rival and changes sides |
| Rule coverage | One-point catches, 200/500 clearing, 100-point food resets with airborne food retained, post-game-over movement without advancing timers |
| Clean SDK build | Exit 0 and literal `'build' finished`; no C warnings; existing SDK linker RWX warning |
| App image | 15,352 text + 48 data + 876 bss = **16,276 bytes**, 45,164 bytes below the 61,440 budget |
| Linked engine | `game_start`, `game_input`, `game_advance`, `game_step`, `game_pause`, `game_resume` present; unused host-only `game_init` removed |
| Resources | 18,690 bytes / 200,000 budget |
| PBW | **36,698 bytes** / 2,000,000 budget |
| Portrait heap A/B | 48,844 free / 65,948 used = 42.55% free, start and game over |
| Landscape heap A/B | 48,608 free / 66,184 used = 42.34% free; A at start, B at start/game over |
| Identity | `Popeye G&W`, UUID `6b7c8c28-36f0-47ca-a073-3e8f33efcaf0`, v1.0.0, Emery-only watchapp; all PBW checks passed |
| Launcher icon | 25 × 25, pixel/byte-identical runtime asset to preceding audited build; not recaptured in this five-capture scene audit |
| Cleanup | Waf environment locks and generated Python cache removed; diagnostic logger stopped |
| Physical PT2 | Not tested; actual screen color/readability and input feel still need owner play-testing |

PBW SHA-256:
`519b1832f272176a6dcd8a57e2f7ca19186118be28f5a05b0eb572402fe71156`.

Runtime launcher icon SHA-256:
`5c848518963cf683ecdfcbbce926667d2fc1dc66039f8260c78957d771734fa6`.

## Visual evidence

Four retained native 200 × 228 captures from five capture attempts (one duplicate
clock discarded). These are actual Emery emulator screens from the PBW above:

- [Portrait clock](../art/previews/pp23-clock-portrait.png): compact start hint, unobstructed field, car/Olive, right rival.
- [Game A portrait](../art/previews/pp23-game-a-portrait.png): fixed Olive, left hammer rival, score/MISS.
- [Game B portrait](../art/previews/pp23-game-b-right.png): one rival on right, one full and one pending-drop MISS mark.
- [Game B landscape](../art/previews/pp23-game-b-landscape.png): native rotated buffer, same bright scene and right-side rival; view by turning the watch counterclockwise.

The off-screen composition check also covered left idle/wind-up/strike, right
strike, dizzy Popeye and splash. Host tests check both atlases' bounds and
non-overlap. Normal art builds require only standard-library Python; ImageMagick
is a maintainer dependency for preparing source sprites.

## Fidelity limits

See [PP-23 source ledger](pp23-fidelity.md) for the verified model/manual,
recording timestamps and remaining measurements. The tests establish the
watch implementation's behavior and fairness, **not Nintendo timing fidelity**.
Exact original poses, button holds/recentering, food paths, catch windows,
Brutus side-switch schedule and numeric timing remain provisional.

New generated artwork lives in `art/source/`: vibrant backdrop, three hammer
poses, food can and bottle. [Prompts](../art/source/pp23-prompts.json) record the
built-in image-generation requests. Approved Popeye/Olive/right-Brutus designs
are preserved. The source and runtime set now contains 82 LCD segments, with
score/MISS-only ghosts to keep the playfield clear.

Settings, score schema, UUID, version and remote name were preserved. Existing
high scores can include points from the earlier double-point rules and were
not erased. The emulator's orientation preference was returned to portrait.
