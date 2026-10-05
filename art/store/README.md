# M5 store artwork

Created with the built-in image generation tool on 2026-10-05. The owner's
brief was a charming, vibrant, high-contrast 1981 advert, with a Pebble Time 2
showing this game on its screen. These assets are local listing drafts. The owner selected the Game & Watch strip and the first nostalgic footer option:
**BACK TO GAME & WATCH** / **A whole childhood on one small screen.**
The banner master and 720 × 320 export now use that copy. The tagline uses
large, heavy vintage slab-serif lettering with no flanking lines or separators.

| Master | Prompt | Export |
|---|---|---|
| [banner-master.png](banner-master.png) | [Original generation](banner-prompt.txt), [selected copy edit](banner-edit-prompt.txt), [larger footer](banner-footer-prompt.txt), [nostalgic wording](banner-nostalgia-prompt.txt) | [720 × 320 banner](../../docs/releases/listing/banner-720x320.png) |
| [icon-master.png](icon-master.png) | [icon-prompt.txt](icon-prompt.txt) | [144 × 144](../../docs/releases/listing/icon-144.png), [48 × 48](../../docs/releases/listing/icon-48.png) |

The banner uses warm ivory, deep green, gold and red stripes, retro display
lettering, and the black/red PT2. Its watch reference was the
[official Pebble Time 2 product image](https://repebble.com/images/products/PWkl7yO.jpeg),
linked from [Pebble's product page](https://repebble.com/watch). That downloaded
reference stays in ignored `.release/m5/pt2-reference.jpg`.
The screen reference was [the original native Game B capture](https://github.com/ewijaya/popeye-gw/blob/9285f0fd5d61444a082350793e0777c8c2b87463/docs/releases/listing/03-game-b.png).
The icon used the approved [Popeye source preview](../previews/popeye-source.png)
and the banner's palette. No CLI/API fallback was used.

The copy edit also used the built-in image generator, with the previous
approved banner as its sole input. The exact input is preserved in git at
`a6e3c52:art/store/banner-master.png`; [banner-edit-prompt.txt](banner-edit-prompt.txt)
records the two replacements and the composition-preservation instructions.
The subsequent typography edit used `d052b81:art/store/banner-master.png`
as its input and [banner-footer-prompt.txt](banner-footer-prompt.txt) as its
instructions: enlarge the tagline, use a 1981 toy-box lettering style and
remove both decorative horizontal rules. It also used the built-in generator.
The final wording edit used `d458c6c:art/store/banner-master.png` and
[banner-nostalgia-prompt.txt](banner-nostalgia-prompt.txt), keeping the enlarged
type and plain footer while replacing its sentence. The final text was
visually checked at master and 720 × 320 export sizes.

The banner is generated marketing artwork, including its rendered screen;
it is not a photograph of physical-watch brightness. The proposed 1.0.0 gallery
has five fresh captures: Bottom landscape clock, portrait Game A, Bottom
landscape Game B with Brutus striking, portrait pause controls and alarm.
The pause image replaces the earlier proposed game-over/HI image for this
gallery review. All five 200 × 228 originals are retained in
[listing/native/](../../docs/releases/listing/native/). The two landscape
presentation exports are lossless 90-degree rotations to 228 × 200, with no
resizing or retouching. Confirm portal acceptance before uploading those exports.

Game A/B and pause compositions use debugger-staged scores and food positions,
not earned scores. Clock and alarm are running app scenes. No capture code is
compiled into the app; emulator scores/settings were restored afterwards.
See [the 1.0.0 review](../../docs/releases/1.0.0.md) for the exact PBW and gallery.

Rebuild the exact-size exports with ImageMagick:

```sh
magick art/store/banner-master.png -resize 720x320! docs/releases/listing/banner-720x320.png
magick art/store/icon-master.png -resize 144x144 docs/releases/listing/icon-144.png
magick art/store/icon-master.png -resize 48x48 docs/releases/listing/icon-48.png
magick art/store/icon-master.png -resize 80x80 docs/releases/listing/icon-80.png
```

The existing 25 × 25 on-watch launcher icon is unchanged. Larger store art is
not bundled into the PBW and has no effect on watch memory or download size.
