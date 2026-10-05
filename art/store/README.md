# M5 store artwork

Created with the built-in image generation tool on 2026-10-05. The owner's
brief was a charming, vibrant, high-contrast 1981 advert, with a Pebble Time 2
showing this game on its screen. These assets are local listing drafts. The owner likes the banner design;
its platform-specific strip and dated footer are being replaced after a
ranked copy review. The current master retains the first draft text.

| Master | Prompt | Export |
|---|---|---|
| [banner-master.png](banner-master.png) | [banner-prompt.txt](banner-prompt.txt) | [720 × 320 banner](../../docs/releases/listing/banner-720x320.png) |
| [icon-master.png](icon-master.png) | [icon-prompt.txt](icon-prompt.txt) | [144 × 144](../../docs/releases/listing/icon-144.png), [48 × 48](../../docs/releases/listing/icon-48.png) |

The banner uses warm ivory, deep green, gold and red stripes, retro display
lettering, and the black/red PT2. Its watch reference was the
[official Pebble Time 2 product image](https://repebble.com/images/products/PWkl7yO.jpeg),
linked from [Pebble's product page](https://repebble.com/watch). That downloaded
reference stays in ignored `.release/m5/pt2-reference.jpg`.
The screen reference was [the native Game B capture](../../docs/releases/listing/03-game-b.png).
The icon used the approved [Popeye source preview](../previews/popeye-source.png)
and the banner's palette. No CLI/API fallback was used.

The banner is generated marketing artwork, including its rendered screen;
it is not a photograph of physical-watch brightness. The five separate
200 × 228 listing images are unedited native QEMU captures. Game A/B and
HI examples use debugger-staged state for repeatable composition, not earned
scores; the game-over transition itself ran through the game engine. See
[the M5 audit](../../docs/m5-audit.md) for validation and limitations.

Rebuild the exact-size exports with ImageMagick:

```sh
magick art/store/banner-master.png -resize 720x320! docs/releases/listing/banner-720x320.png
magick art/store/icon-master.png -resize 144x144 docs/releases/listing/icon-144.png
magick art/store/icon-master.png -resize 48x48 docs/releases/listing/icon-48.png
```

The existing 25 × 25 on-watch launcher icon is unchanged. Larger store art is
not bundled into the PBW and has no effect on watch memory or download size.
