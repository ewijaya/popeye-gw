# Popeye G&W Clock listing assets

Draft listing material prepared 2026-10-06. Not yet approved for publication.
These assets belong to the clock; the game's listing remains unchanged.

| Asset | Dimensions | Source |
|---|---|---|
| [Static clock](01-clock.png) | 200 × 228 | Native Emery watchface screenshot |
| [Minute demo](02-minute-demo.png) | 200 × 228 | Native Emery watchface screenshot |
| [Banner](banner-720x320.png) | 720 × 320 | Generated clock-specific marketing illustration |
| [Large icon](icon-144.png) | 144 × 144 | Generated clock-specific icon |
| [Small icon](icon-80.png) | 80 × 80 | Same icon master |
| [Alternate small icon](icon-48.png) | 48 × 48 | Same master; original game PRD size |

Screenshots came from the running 1.0.0 watchface PBW, SHA-256
`12bf420c966e08e227daba870e8a6e8fb353cd021f330ccda435586d8ed420a8`.
The emulator clock was set to 10:09:50 for the static scene, then 10:09:59
and allowed to cross the minute for the demo. Captures were not retouched,
resized or generated. No game screenshots were reused.

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
