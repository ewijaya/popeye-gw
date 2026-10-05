# Handheld landscape mode

Open **Menu → Settings → View** and press Select to switch between
Portrait and Landscape. Orientation is the fifth settings item; scroll past
Demo to reach it. The setting is saved and survives app updates.

For Landscape, turn the watch counterclockwise until the three buttons are
along the top edge. They become **left / select / right**. The left and right
buttons move Popeye; the middle button starts or pauses a game. Hold the middle
button for Game B from the clock. Back is on the opposite edge. The existing
Swap controls preference continues to reverse the movement buttons if enabled.

The clock, gameplay, pause/game-over cards, alarm and menus all follow the
selected orientation. Portrait remains the default.

## Rendering and storage

The original approved art remains the source. `tools/build_art.py` generates a
228 × 200 logical landscape composition with nearest-neighbour sampling, then
rotates it clockwise into the native 200 × 228 framebuffer. Both orientations
have complete 82-segment atlases and matching ghost/plain backdrops. No new
image generation or network request is needed to rebuild these assets.

Only the selected atlas and backdrop are loaded. A reusable 10 KB palette
bitmap rotates native-font UI text, centred within the landscape width. Its
palette is transparent, black, white and dark green for menu title banners.
Gameplay without overlays skips this UI conversion. No image allocation
occurs per frame; assets change when orientation or ghosts change.

Settings use version 2 of the existing eight-byte record, adding the
landscape flag. Valid version 1 records retain all preferences and alarm
values and default to portrait. High-score records are unchanged.

## Validation — 5 October 2026

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
