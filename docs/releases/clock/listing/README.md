# Popeye G&W Clock listing assets

Listing material prepared 2026-10-06. The supplied clock icon was attached to
both App Store icon fields on 2026-10-07 (80 × 80 and 144 × 144). The Dashboard
read-back and downloaded pixels match; public catalog icon URLs remained empty
after two checks. Evidence is in `.release/popeye-gw-clock/display-cleanup/`.
These assets belong to the clock; the game's listing remains unchanged.

| Asset | Dimensions | Source |
|---|---|---|
| [Classic](01-clock.png) | 200 × 228 | Native Emery release candidate screenshot |
| [Everyday](02-everyday.png) | 200 × 228 | Native Emery release candidate screenshot |
| [Large Time](03-large-time.png) | 200 × 228 | Native Emery release candidate screenshot |
| [Japanese / Green LCD](04-japanese-green.png) | 200 × 228 | Native Emery release candidate screenshot |
| [Banner](banner-720x320.png) | 720 × 320 | Generated clock-specific marketing illustration |
| [Large icon](icon-144.png) | 144 × 144 | Generated clock-specific icon |
| [Small icon](icon-80.png) | 80 × 80 | Same icon master |
| [Alternate small icon](icon-48.png) | 48 × 48 | Same master; original game PRD size |

The four gallery screenshots came from the clean 1.0.0 release build, SHA-256
`111f8bb595f4938b841e5c030ee9af4eaaad35d92bb4221fd712311afb5fe1be`.
They are native 200×228 captures, without retouching or resizing. The Classic
capture uses 10:09; the remaining captures show current time after the SDK
resynchronized it. The prior minute-demo gallery image is archived. The owner
confirmed the shared emulator was free and authorized reinstall/captures after
the first attempt showed the launcher; that failed set was discarded.

The current banner was completely redesigned with the built-in image generator
on 2026-10-06: an early-1980s electronics-advertising layout with vermilion/navy
lettering, ivory paper, diagonal geometry and a brushed-silver watch. Its display
uses the latest native [Classic Ivory reference](../previews/classic-banner.png)
(compact 10:09, no information rows, ghosts off). The screen is marketing
illustration, not device evidence. All four approved text strings are unchanged.
The previous banner master/export are retained in `../artwork/archive/`.
The icons retain the original green/gold/ivory/red treatment. No CLI/API image
generation fallback was used.

Masters and exact prompts are retained in [../artwork/](../artwork/):
[current redesign prompt](../artwork/banner-revamp-prompt.txt),
[original banner prompt](../artwork/banner-prompt.txt),
[selected subtitle edit](../artwork/banner-subtitle-prompt.txt),
[larger footer edit](../artwork/banner-footer-size-prompt.txt),
[icon prompt](../artwork/icon-prompt.txt). Exact-size exports were produced
with the repository's existing ImageMagick approach:

```sh
magick docs/releases/clock/artwork/banner-master.png -resize '720x320!' docs/releases/clock/listing/banner-720x320.png
magick docs/releases/clock/artwork/icon-master.png -resize 144x144 docs/releases/clock/listing/icon-144.png
magick docs/releases/clock/artwork/icon-master.png -resize 80x80 docs/releases/clock/listing/icon-80.png
magick docs/releases/clock/artwork/icon-master.png -resize 48x48 docs/releases/clock/listing/icon-48.png
```

The 720 × 320 banner and 80 × 80 icon were visually checked after export.
The owner selected the banner subtitles “Old-school charm. Right on time.”
and “The Popeye watchface.” The built-in image generator applied this edit,
removing the previous device-specific footer and its decorative separators.
The footer was then enlarged at the owner's request using the built-in image
generator, retaining the selected wording.
All listing files are below 4.4 MB. Reinspect Dashboard dimensions before
registration; use 80 × 80 unless the current form requires another size.
Hash all selected PNGs and the description in the frozen release manifest.
Only PNG assets go to the store, not this README or artwork sources.
