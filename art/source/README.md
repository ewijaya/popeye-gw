# LCD sprite source art

Popeye G&W uses the approved Popeye, Olive Oyl and Brutus designs, with fixed black LCD segments inspired by the original wide-screen handheld.

The complete runtime set has **82 segments**, including every gameplay pose and the clock/alarm indicators. Character variants, cargo and the backdrop were made with the built-in image generation tool. The tool does not expose a model selector or identifier. Source PNGs preserve generated transparency. Runtime segments are strictly opaque black or transparent at 200 × 228.

## Reproduce the art

From the repository root:

```sh
# Maintainer preparation only; requires ImageMagick.
python3 tools/prepare_art.py
# Normal pack/check steps require Python's standard library only.
python3 tools/build_art.py
python3 tools/build_art.py --check
./tools/test.sh
```

`tools/prepare_art.py` defines the layout and rasterizes geometric UI symbols. It crops alpha bounds, resizes using area averaging, thresholds coverage, mirrors right-facing variants, and positions segments on full-screen canvases. Character feet align with the new backdrop; catching hands align with the four cargo lanes. The backdrop is reduced to Emery's four levels per RGB channel without dithering.

The renderer loads the packed atlas once with a black/transparent 1-bit palette. Sub-bitmaps reference it without duplicating pixel storage. Ghosts appear only in the score/MISS registers at half grey coverage. Character and food ghosts are omitted to keep the field clear on Emery.

## Source mapping

| Source | Runtime use |
|---|---|
| `popeye.png` | Center pose |
| `popeye-near-left.png` | Near-left / mirrored near-right catch |
| `popeye-far-left.png` | Far-left / mirrored far-right catch |
| `popeye-dizzy.png` | Left / mirrored right dizzy |
| `olive.png` | Ready beside the left car |
| `olive-throw.png` | Throw from the fixed left ledge |
| `olive-bell-0.png`, `olive-bell-1.png` | Two alarm ringing frames |
| `brutus.png`, `brutus-windup.png`, `brutus-strike.png` | Mirrored right fist phases |
| `brutus-hammer-idle.png`, `brutus-hammer-windup.png`, `brutus-hammer-strike.png` | Left hammer phases |
| `food-bottle.png`, `fish.png`, `barrel.png`, `food-can.png` | Four food arcs, five stages each: lane 0 tilted bottle, lane 1 fish, lane 2 upright barrel, lane 3 can. The barrel replaced a second, upright bottle that looked too much like lane 0 at 14 px; its prompt is in `m3-prompts.json` |
| `backdrop-vibrant.png` | Bright red car, brick ledge, orange boat, blue ship and water |
| `clock-menu-icon.png` | Companion watchface launcher: clock with sailor cap and pipe. Separate from the game's anchor icon; [conversion instructions](../../watchface/resources/images/README.md), [generation prompt](clock-menu-icon-prompt.txt). |
| Geometric artwork in `tools/prepare_art.py` | Seven-segment digits, text indicators, splashes, MISS cans, bell, catch flash and anchor launcher icon |

Runtime filenames and engine identifiers use `popeye`, `olive` and `brutus` consistently.

## Generation prompts

The new backdrop, left hammer poses and food prompts are recorded verbatim in
[pp23-prompts.json](pp23-prompts.json), generated using the built-in `image_gen`
tool. Historical animation, cargo and backdrop prompts are recorded verbatim in [m3-prompts.json](m3-prompts.json). Original approved character prompts follow.


### Popeye

Use case: style-transfer. Create one isolated production game sprite based on the supplied vintage Popeye handheld reference. The reference is for the actual named character identity AND the LCD artwork aesthetic. The user explicitly wants the recognizable actual character, not an original substitute. Pure opaque black ink shapes on genuine transparent background, with transparent internal negative spaces (not white fill). Bold economical LCD segment drawing, expressive silhouette, very simple broad cutouts, no shading, no hatching, no greys, no text, no branding, no frame, no scenery, no platform, no extra character. All anatomy fully inside the canvas with a small clear margin. It will be reduced to watch resolution, so eliminate tiny details and use thick shapes. Subject: POPEYE THE SAILOR himself. Immediately recognizable classic face: huge jutting rounded cleft chin, squinting single eye, bulbous nose, corncob pipe sticking out of his mouth to the LEFT, classic white sailor cap rendered as a black outline with transparent inside, black short-sleeved sailor shirt with broad collar, slender upper arms and enormous bulbous forearms with one simple anchor tattoo if readable, sailor trousers and big shoes. Full body three-quarter view facing LEFT, upright standing sailor pose with bent knees and both hands held forward/up ready to catch. Make face, pipe, cap and massive forearms the dominant recognition cues. As in the vintage LCD character, use large outlined face and forearms with transparent interiors and a solid black shirt. Compact aspect ratio about 44 wide by 62 tall. Not a young generic sailor, not a knitted hat, no beard, no boat, no crate. Single Popeye sprite only.

### Olive

Use case: style-transfer. Create one isolated production game sprite based on the supplied vintage Popeye handheld reference. The reference is for the actual named character identity AND the LCD artwork aesthetic. The user explicitly wants the recognizable actual character, not an original substitute. Pure opaque black ink shapes on genuine transparent background, with transparent internal negative spaces (not white fill). Bold economical LCD segment drawing, expressive silhouette, very simple broad cutouts, no shading, no hatching, no greys, no text, no branding, no frame, no scenery, no platform, no extra character. All anatomy fully inside the canvas with a small clear margin. It will be reduced to watch resolution, so eliminate tiny details and use thick shapes. Subject: OLIVE OYL herself, recognizable classic slender cartoon woman: tiny tight black hair bun behind her head, long narrow face and projecting nose, very thin long neck, black long-sleeved top with prominent rounded/scalloped pale collar represented by transparent negative space, long black skirt and oversized flat shoes. Full body side profile facing RIGHT, leaning slightly forward with skinny arms extended in front as though about to toss a small cargo item, one hand lifted. Her long angular limbs and tall narrow silhouette must read instantly as Olive Oyl. Head and collar must be clean and bold. Narrow sprite aspect ratio about 24 wide by 51 tall. No ponytail, no overalls, no hat, no crate, no boat. Single Olive Oyl sprite only.

### Brutus

Use case: style-transfer. Create one isolated production game sprite based on the supplied vintage Popeye handheld reference. The reference is for the actual named character identity AND the LCD artwork aesthetic. The user explicitly wants the recognizable actual character, not an original substitute. Pure opaque black ink shapes on genuine transparent background, with transparent internal negative spaces (not white fill). Bold economical LCD segment drawing, expressive silhouette, very simple broad cutouts, no shading, no hatching, no greys, no text, no branding, no frame, no scenery, no platform, no extra character. All anatomy fully inside the canvas with a small clear margin. It will be reduced to watch resolution, so eliminate tiny details and use thick shapes. Subject: BRUTUS (BLUTO), Popeye's classic hulking bearded rival, recognizable large rounded body, massive shoulders and arms, black full beard surrounding a big grinning mouth, broad nose, heavy eyebrows, small black sailor cap, black short-sleeved sailor shirt with broad light sailor collar represented by transparent space, thick trousers and heavy shoes. Full body in three-quarter side view facing RIGHT toward the player, idle threatening stance with one gigantic fist held forward at waist level and the other at his hip, slightly hunched, knees bent. Match the burly bearded rival shown on the right side of the reference's LCD display, but facing right for the game's left pier. Chunky comedic villain, clear broad face and grin. Sprite aspect ratio about 44 wide by 72 tall. No raincoat, no pointed wizard hat, no hook, no weapon, no extra object. Single Brutus sprite only.
