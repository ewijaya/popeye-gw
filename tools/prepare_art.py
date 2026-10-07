#!/usr/bin/env python3
"""Prepare all 89 LCD segments from approved imagegen art and geometric UI.

Maintainer-only tool: requires ImageMagick. Normal builds and CI consume the
committed PNGs and use build_art.py, which needs only Python's standard library.
No generative model or network access is needed to reproduce runtime assets.
"""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "art" / "source"
SEGMENTS = ROOT / "art" / "segments"
MAGICK = shutil.which("magick")
LANES = (50, 75, 125, 150)
# Five visible points along each throw arc; coordinates are sprite centres.
ARCS = (
    ((56, 77), (68, 66), (76, 82), (65, 111), (50, 137)),
    ((56, 77), (78, 61), (94, 80), (86, 110), (75, 137)),
    ((56, 77), (87, 56), (113, 76), (126, 107), (125, 137)),
    ((56, 77), (91, 53), (127, 70), (146, 103), (150, 137)),
)


def run(*args):
    subprocess.run([MAGICK, *map(str, args)], check=True)


def sprite(source, name, box, mirror=False, angle=0):
    x, y, w, h = box
    args = [SOURCE / (source + ".png"), "-alpha", "extract", "-threshold", "10%",
            "-trim", "+repage"]
    if mirror:
        args += ["-flop"]
    if angle:
        args += ["-background", "black", "-rotate", str(angle), "-trim", "+repage"]
    args += ["-filter", "Box", "-resize", f"{w}x{h}", "-threshold", "45%",
             "-trim", "+repage", "-background", "black", "-gravity", "south",
             "-extent", f"{w}x{h}", "-alpha", "copy", "-channel", "RGB",
             "-evaluate", "set", "0", "+channel", "-background", "none",
             "-gravity", "northwest", "-splice", f"{x}x{y}", "-extent", "200x228",
             "PNG32:" + str(SEGMENTS / (name + ".png"))]
    run(*args)


def vector(name, drawing, temp, canvas=(200, 228), destination=None):
    """Rasterize simple UI geometry; character art always comes from imagegen."""
    width, height = canvas
    svg = Path(temp) / (name + ".svg")
    svg.write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
                   f'height="{height}" viewBox="0 0 {width} {height}">'
                   f'<g fill="black">{drawing}</g></svg>\n')
    target = destination or SEGMENTS / (name + ".png")
    run("-background", "none", svg, "-alpha", "extract", "-threshold", "50%",
        "-background", "black", "-alpha", "shape",
        "PNG32:" + str(target))


def rect(x, y, w, h):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}"/>'


FONT = {
    "A": ("010", "101", "111", "101", "101"),
    "B": ("110", "101", "110", "101", "110"),
    "E": ("111", "100", "110", "100", "111"),
    "G": ("111", "100", "101", "101", "111"),
    "H": ("101", "101", "111", "101", "101"),
    "I": ("111", "010", "010", "010", "111"),
    "M": ("10001", "11011", "10101", "10001", "10001"),
    "S": ("111", "100", "111", "001", "111"),
    "P": ("110", "101", "110", "100", "100"),
    " ": ("0", "0", "0", "0", "0"),
}


def lettering(text, x, y):
    result = ""
    for char in text:
        glyph = FONT[char]
        for row, bits in enumerate(glyph):
            for col, bit in enumerate(bits):
                if bit == "1":
                    result += rect(x + col, y + row, 1, 1)
        x += len(glyph[0]) + 1
    return result


def characters():
    sprite("popeye", "popeye-2", (78, 125, 44, 62))
    for pose, source, box, mirror in (
        (0, "popeye-far-left", (43, 129, 45, 58), False),
        (1, "popeye-near-left", (67, 129, 38, 58), False),
        (3, "popeye-near-left", (95, 129, 38, 58), True),
        (4, "popeye-far-left", (112, 129, 45, 58), True),
    ):
        sprite(source, f"popeye-{pose}", box, mirror)
    for side, x, mirror in (("left", 30, False), ("right", 130, True)):
        sprite("popeye-dizzy", "popeye-dizzy-" + side, (x, 131, 40, 56), mirror)
    sprite("olive", "olive-ready", (24, 52, 30, 57))
    sprite("olive-throw", "olive-throw", (24, 54, 33, 55))
    # Same scale as olive-ready (57 px from a 1521 px trim); feet on the ledge.
    sprite("olive-kiss", "olive-kiss", (20, 54, 38, 55))
    for frame in range(2):
        sprite(f"olive-bell-{frame}", f"olive-bell-{frame}", (24, 54, 31, 55))
    for phase, w, h in (("idle", 35, 39), ("windup", 46, 52), ("strike", 59, 43)):
        sprite("brutus-hammer-" + phase, "brutus-left-" + phase, (0, 173-h, w, h))
    for phase, source, w, h in (("idle", "brutus", 44, 62),
                                ("windup", "brutus-windup", 44, 66),
                                ("strike", "brutus-strike", 60, 59)):
        sprite(source, f"brutus-right-{phase}", (200 - w, 156 - h, w, h), True)


def cargo():
    for lane, source in enumerate(("food-bottle", "fish", "barrel", "food-can")):
        for stage, (x, y) in enumerate(ARCS[lane]):
            sprite(source, f"cargo-{lane}-{stage}", (x - 7, y - 7, 14, 14), angle=-35 if lane == 0 else 0)


# Pixel hearts for Olive's kiss: rows of a small and a large heart.
HEART_SMALL = (".##.##.", "#######", "#######", ".#####.", "..###..", "...#...")
HEART_LARGE = (".###.###.", "#########", "#########", "#########", ".#######.",
               "..#####..", "...###...", "....#....")
# Heart centres from Olive's flung hand down to above Popeye's centre pose.
KISS_PATH = ((60, 58), (72, 55), (84, 61), (93, 75), (99, 92))


def heart(rows, cx, cy):
    x0, y0 = cx - len(rows[0]) // 2, cy - len(rows) // 2
    return "".join(rect(x0 + col, y0 + row, 1, 1)
                   for row, bits in enumerate(rows) for col, bit in enumerate(bits) if bit == "#")


def ui(temp):
    for stage, (x, y) in enumerate(KISS_PATH):
        vector(f"heart-{stage}", heart(HEART_SMALL, x, y), temp)
    vector("popeye-heart", heart(HEART_LARGE, 100, 114), temp)
    for lane, x in enumerate(LANES):
        drawing = (f'<path d="M {x-9} 208 l 5 -1 -2 -5 5 4 1 -6 2 6 5 -4 '
                   ' -2 5 5 1 -4 2 h -12 Z"/>')
        vector(f"splash-{lane}", drawing, temp)
    vector("popeye-catch", '<path d="M 94 128 l 4 -3 2 -5 2 5 4 3 -4 2 -2 5 -2 -5 Z"/>', temp)
    # Empty cans echo the PP-23 MISS display; half can is a watch affordance.
    for i in range(3):
        x = 139 + 13 * i
        vector(f"miss-{i}", rect(x,30,7,2) + rect(x,39,7,2) + rect(x,31,2,9) + rect(x+5,31,2,9), temp)
    vector("miss-half", rect(178,30,4,2) + rect(178,39,4,2) + rect(178,31,2,9), temp)
    vector("miss-label", lettering("MISS", 114, 35), temp)
    # True seven-segment bars with clipped corners, shared by game score and clock.
    bars = (
        "2,0 12,0 10,2 4,2", "12,1 14,3 14,8 12,9 11,8 11,3",
        "12,10 14,11 14,16 12,18 11,16 11,11", "2,19 4,17 10,17 12,19",
        "0,11 2,10 3,11 3,16 2,18 0,16", "0,3 2,1 3,3 3,8 2,9 0,8",
        "2,9 4,8 10,8 12,9 10,11 4,11",
    )
    for digit, x in enumerate((108, 126, 150, 168)):
        for bar, points in zip("abcdefg", bars):
            vector(f"digit-{digit}-{bar}",
                   f'<g transform="translate({x},8)"><polygon points="{points}"/></g>', temp)
    vector("colon", rect(145,13,2,2) + rect(145,20,2,2), temp)
    for name, label, x, y in (("am","AM",186,10), ("pm","PM",186,20),
                              ("game-a","GAME A",5,4), ("game-b","GAME B",5,14),
                              ("hi","HI",80,7)):
        vector(name, lettering(label, x, y), temp)
    # Right-edge alarm indicator leaves room for the explicit Select hints.
    vector("bell", '<g transform="translate(136,26)"><path d="M 50 16 L 52 13 V 9 Q 52 5 56 5 Q 60 5 60 9 V 13 '
           'L 62 16 Z M 54 18 H 58" fill="none" stroke="black" stroke-width="2"/>'
           + rect(55,3,2,2) + '</g>', temp)


def main():
    if MAGICK is None:
        raise SystemExit("ImageMagick (magick) is required to prepare source art")
    SEGMENTS.mkdir(parents=True, exist_ok=True)
    characters()
    cargo()
    with tempfile.TemporaryDirectory() as temp:
        ui(temp)
        # Geometric anchor icon is recognizable at the launcher's native 25 px.
        vector("menu-icon", '<circle cx="12" cy="5" r="3" fill="none" '
               'stroke="black" stroke-width="2"/>' + rect(11,8,2,13) + rect(6,10,13,2)
               + '<path d="M 3 14 L 3 19 Q 12 29 21 19 L 21 14 L 16 18 H 19 '
               'Q 12 24 5 18 H 8 Z"/>', temp, (25,25), ROOT / "art" / "menu-icon.png")
    run(SOURCE / "backdrop-vibrant.png", "-alpha", "off", "-filter", "Box",
        "-resize", "200x228!", "+dither", "-channel", "RGB", "-posterize", "4",
        "+channel", "PNG32:" + str(ROOT / "art" / "backdrop.png"))
    print("Prepared 89 segments, backdrop, and 25 x 25 launcher icon")


if __name__ == "__main__":
    main()
