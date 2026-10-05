# Popeye G&W — M3 art and rename audit, 2026-10-05

The complete 88-segment LCD inventory uses Popeye, Olive Oyl and Brutus.
Runtime filenames, C identifiers, package metadata, title text, maintenance
skills and release configuration use the same identity. The display name is
**Popeye G&W**; the repository, checkout directory and PBW basename are
`popeye-gw`. The app UUID and version 1.0.0 are unchanged.

This audit covers the development changes based on
`5949ecc50ebd9c1203e8aa06d8739bf1e9169251`. The tree was dirty during validation;
these results are not a frozen release candidate or store-release sign-off.
No physical-watch installation or store publication was performed.

| Check | Result |
|---|---|
| Normal host suite | Passed: rules, 88 segment mappings, atlas bounds/non-overlap, 200 rounds to game over and 20,000-seed fairness simulation |
| Address/undefined-behavior sanitizers | Passed; zero unavoidable misses, drops, hits or stalls |
| Art pipeline | All 88 segments and the 25 × 25 menu icon validate and match generated outputs |
| Clean SDK build | Passed, exit 0 and `'build' finished`; Pebble Tool 5.0.40, SDK 4.33.1, arm64 host |
| App image | 6,970 / 61,440 bytes; 54,470 bytes headroom |
| Resource pack | 10,459 / 200,000 bytes |
| PBW | 18,738 / 2,000,000 bytes; UUID, version, name, Emery-only target and watchapp identity passed |
| Emulator heap | Game A and Game B start: 66,800 bytes free, 57,296 used; **53.8% free** |
| Launcher | Actual capture confirms the Popeye G&W name and 25 × 25 anchor icon |
| Identity | Approved character names throughout source and sprite paths; mirrored skills match |
| Watch play-test | Not performed |

PBW: `build/popeye-gw.pbw`

SHA-256:
`1e70ecba743cb74a30c6f26b67ccdd8151c205f998b287e7bd73305e488de922`

The playable engine is linked: `game_start`, `game_input`, `game_advance`,
`game_step`, pause/resume and `scene_game` are present in the ELF. Unused host
convenience initialization is removed by the linker. The app-image total is
6,346 bytes of text, 32 bytes of data and 592 bytes of BSS.

The linker emits the SDK's RWX LOAD-segment warning. There were no C compiler
warnings or build errors. Local `.lock-waf*` files were removed after building.

## Emulator evidence

The renamed PBW was installed and launched explicitly by UUID. Five native
200 × 228 captures confirm the title fits, both game modes load their art,
the pause overlay renders and the launcher displays the new name and icon.
Game A was paused and quit, then Game B was started. The heap measurements
above are from the renamed build's start logs; no game-over heap measurement
is claimed for this run. No resource-load failures appeared in its logs.

- [Title](../art/previews/emulator-title.png)
- [Game A](../art/previews/emulator-game-a.png)
- [Paused Game A](../art/previews/emulator-paused.png)
- [Game B](../art/previews/emulator-game-b.png)
- [Launcher](../art/previews/emulator-launcher.png)

Offline artwork compositions (not emulator screenshots):

- [All five player poses](../art/previews/m3-player-poses.png)
- [Pose animation](../art/previews/m3-player-poses.gif)
- [Native-size composition](../art/previews/screen-native.png)

Earlier development logs and previews were preserved outside the checkout in
`../popeye-gw-development-archive-20261005/`. Their recorded names and hashes
were not rewritten to appear to describe this build.

## Implementation

`tools/prepare_art.py` creates runtime PNGs from saved imagegen sources,
mirrors right-facing poses and rasterizes simple UI geometry with ImageMagick.
`tools/build_art.py` uses only Python's standard library, packs the complete
set into a 200 × 307 atlas, bakes sparse ghost pixels, copies the launcher icon
and generates the segment table. Missing segments fail validation.

The renderer loads a black/transparent 1-bit palette atlas once and keeps
sub-bitmaps referencing its storage. Resource allocation failures are reported
visibly. The rename changes identifiers and identity without changing game
rules, timing, controls or difficulty. Clock, daily alarm, menu, settings and
persistent high scores remain the next milestone (M4).
