# Popeye G&W listing assets

Current store gallery, updated 2026-10-07 at the owner's request ("approve the
3 screenshots"): three upright screenshots, Ivory first, to show the nostalgic
LCD look. It replaced the five-screenshot gallery from 1.0.0, which is kept in
[archive/](archive/).

| # | File | Scene | SHA-256 |
|---|---|---|---|
| 1 | [01-ivory-game.png](01-ivory-game.png) | Ivory theme, Game A, score 6: Brutus swings, Popeye catches | `3091a70953922b6eb8c4ea06b0897de8e5f60402bbcccd28daa583d6a425b91d` |
| 2 | [02-classic-game.png](02-classic-game.png) | Classic theme, Game A, score 6: Brutus winds up, food in flight | `b88a13d5e3779b71d157ef79a08a12a92fc21fdc21b70cdabd94f08814230ccc` |
| 3 | [03-ivory-clock.png](03-ivory-clock.png) | Ivory clock 18:05 with the food demonstration | `397c0ad3344d3869ad3ea1c07e2094b3328e169ee211669f1786193f0a46c6b0` |

All three are native 200 × 228 `pebble screenshot --emulator emery` captures of
the published 1.3.0 build, unretouched. The game rounds were real: a local
script read emulator frames and pressed Up/Down to catch food. The Ivory colours
are identical to the Clock listing's (background 255,246,211; ink 74,22,27).

Uploaded through Dashboard Edit Listing, which the release helper does not
cover. The new images were added and saved first, then the five old images were
deleted by exact asset ID and saved. Dashboard read-backs before and after
showed only the screenshot list changed (plus the live hearts count). The
description, banner, icons, metadata and all six releases were preserved. The
general and Emery public catalogs list exactly these three. The store re-encodes
uploads as palette PNGs, so pixel values differ slightly (about 1.5% of pixels
by one or two levels), and stored hashes differ from the files here.

Icons and the banner are unchanged.
