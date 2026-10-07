# Clock launcher icon

`menu-icon.png` is the watchface's 25 × 25 launcher icon: an analog clock with
Popeye's sailor cap and pipe. It is distinct from the game's anchor icon.
`watchface/package.json` declares it as `MENU_ICON` with `menuIcon: true`.
App Store artwork is managed separately and does not supply this on-watch icon.

The master is `art/source/clock-menu-icon.png`, generated with the built-in
image generation tool. The exact prompt is in
[`art/source/clock-menu-icon-prompt.txt`](../../../art/source/clock-menu-icon-prompt.txt).
The runtime PNG contains only opaque black and transparent pixels, with a
at least a one-pixel outer margin. Regenerate it from the repository root with ImageMagick:

```sh
magick art/source/clock-menu-icon.png \
  -alpha extract -trim +repage -filter Box -resize 23x23 -threshold 45% \
  -background black -gravity center -extent 25x25 \
  -background black -alpha shape -strip \
  PNG32:watchface/resources/images/menu-icon.png
```

The [enlarged preview](../../../art/previews/clock-launcher-icon/menu-icon-large.png)
uses nearest-neighbor scaling on white so the actual launcher pixels are visible.

Validation (2026-10-07): a clean Emery build with SDK 4.33.1 exited 0 and
reported `'build' finished successfully`. The generated app metadata assigns
`RESOURCE_ID_MENU_ICON` (1), and the built PBW contains the matching resource
pack. The linked watchface image is 22,859 / 61,440 bytes; unused gameplay
functions are stripped (only `game_lane_pose` remains). This build includes
the current local watchface changes. On-device launcher appearance has not
been verified; no publication or version change was made.
