# Handheld landscape mode

Open **Menu → Settings → View** and press Select to cycle **Portrait → Top →
Bottom**. View is the fifth item, after Demo. Its footer explains where to hold
the buttons. The choice is saved and survives app updates.

| View | Hold the watch | Buttons from left to right |
|---|---|---|
| Top | Turn counterclockwise; buttons above the display | Up / Select / Down |
| Bottom | Turn clockwise; buttons below the display | Down / Select / Up |

In both landscape views, the physical left/right buttons move Popeye left/right
and navigate menus backward/forward. Select starts or pauses; holding it starts
Game B from the clock. Back is on the opposite edge. Swap reverses game movement
as before. The platform adapter maps both press and release; game rules and
one-press movement are unchanged.

The clock, gameplay, pause/game-over cards, alarm and menus all follow the
selected orientation. Portrait remains the default.

## Rendering and storage

The original approved art remains the source. `tools/build_art.py` generates a
228 × 200 logical landscape composition with nearest-neighbour sampling, then
rotates it clockwise for Top and counterclockwise for Bottom into the native
200 × 228 framebuffer. All three orientations
have complete 82-segment atlases and matching ghost/plain backdrops. No new
image generation or network request is needed to rebuild these assets.

Only the selected atlas and backdrop are loaded. A reusable 10 KB palette
bitmap rotates native-font UI text, centred within the landscape width. Its
palette is transparent, black, white and dark green for menu title banners.
Gameplay without overlays skips this UI conversion. No image allocation
occurs per frame; assets change when orientation or ghosts change.

Settings use version 3 of the existing eight-byte record, adding a bottom-buttons
flag. Valid v1/v2 records retain preferences and alarms: v1 uses Portrait, and
v2 landscape retains Top. High-score records are unchanged.

## Bottom-button follow-up

Host and sanitizer suites pass, covering v1/v2 migration, bottom-view persistence,
both UI rotations including the green palette entry, and left/right press/release
with Swap on/off. All three atlases pass bounds/overlap checks; every bottom
segment is the exact 180-degree counterpart of its top-view segment.
Five native captures check settings, clock after reopening, A movement left/right
and Game B. Both modes retain over 40% free emulator heap. The exact build and
physical follow-up are recorded in [PT2 results](pt2-playtest-results.md).

## Validation — 5 October 2026

The notes below describe the original M4 build. The later large-menu/green-title
build passed the owner's landscape check in the [PT2 acceptance](pt2-playtest-results.md).

Implemented and tested in an isolated copy of the ongoing M4 work, with a
separate emulator and persistent storage. The main thread's emulator was
not installed into, stopped or changed.

- Normal and address/undefined-behaviour sanitizer suites passed, including
  both 10,000-seed fairness runs.
- Both atlases pass segment bounds and overlap checks. The art pipeline
  rebuilds both orientations deterministically.
- Tests cover legacy settings migration, saved landscape round-trips and
  clockwise UI pixel packing, including transparent pixels and row padding.
- Emery build passed. Final merged app image: 16,872 / 61,440 bytes; resources: 18,863 bytes.
- The emulator checks used the 16,796-byte orientation build before the
  final merge with concurrent M4 refinements. Emulator Game B: 47,092 bytes free, 67,180 used (41.2% free). Game A also
  remained above the 20% budget.
- Changing orientation, both modes, movement, pause, reinstall persistence
  and returning to portrait were exercised. Physical handheld comfort has
  not yet been checked on the watch.

Native emulator captures are rotated clockwise for landscape, exactly as
stored on the watch:

- [Landscape settings](../art/previews/landscape-settings.png)
- [Landscape clock](../art/previews/landscape-clock.png)
- [Landscape game](../art/previews/landscape-game.png)
- [Landscape retained after reinstall](../art/previews/landscape-restored.png)
- [Return to portrait](../art/previews/portrait-restored.png)
