#!/usr/bin/env python3
"""Prepare all 88 LCD segments from approved imagegen art and geometric UI.

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


def run(*args):
    subprocess.run([MAGICK, *map(str, args)], check=True)


def sprite(source, name, box, mirror=False):
    x, y, w, h = box
    args = [SOURCE / (source + ".png"), "-alpha", "extract", "-threshold", "10%",
            "-trim", "+repage"]
    if mirror:
        args += ["-flop"]
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
    sprite("popeye", "popeye-2", (78, 130, 44, 62))
    for pose, source, box, mirror in (
        (0, "popeye-far-left", (43, 134, 45, 58), False),
        (1, "popeye-near-left", (67, 134, 38, 58), False),
        (3, "popeye-near-left", (95, 134, 38, 58), True),
        (4, "popeye-far-left", (112, 134, 45, 58), True),
    ):
        sprite(source, f"popeye-{pose}", box, mirror)
    for side, x, mirror in (("left", 30, False), ("right", 130, True)):
        sprite("popeye-dizzy", "popeye-dizzy-" + side, (x, 136, 40, 56), mirror)
    for lane, x in enumerate(LANES):
        sprite("olive", f"olive-ready-{lane}", (x - 12, 19, 24, 51))
        sprite("olive-throw", f"olive-throw-{lane}", (x - 13, 21, 27, 49))
    for frame in range(2):
        sprite(f"olive-bell-{frame}", f"olive-bell-{frame}", (7, 21, 28, 49))
    for side, mirror in (("left", False), ("right", True)):
        for phase, source, w, h in (("idle", "brutus", 44, 62),
                                    ("windup", "brutus-windup", 44, 66),
                                    ("strike", "brutus-strike", 60, 59)):
            sprite(source, f"brutus-{side}-{phase}",
                   (200 - w if mirror else 0, 154 - h, w, h), mirror)


def cargo():
    for lane, (x, source) in enumerate(zip(LANES, ("cargo", "fish", "lantern", "barrel"))):
        for stage in range(5):
            sprite(source, f"cargo-{lane}-{stage}", (x - 7, 79 + stage * 13, 14, 14))


def ui(temp):
    for lane, x in enumerate(LANES):
        drawing = (f'<path d="M {x-9} 208 l 5 -1 -2 -5 5 4 1 -6 2 6 5 -4 '
                   ' -2 5 5 1 -4 2 h -12 Z"/>')
        vector(f"splash-{lane}", drawing, temp)
    vector("popeye-catch", '<path d="M 94 128 l 4 -3 2 -5 2 5 4 3 -4 2 -2 5 -2 -5 Z"/>', temp)
    for i in range(3):
        x = 12 + 16 * i
        drawing = (f'<circle cx="{x}" cy="220" r="5" fill="none" stroke="black" '
                   f'stroke-width="2"/>{rect(x-1,214,2,3)}{rect(x-1,223,2,3)}'
                   f'{rect(x-6,219,3,2)}{rect(x+3,219,3,2)}')
        vector(f"ring-{i}", drawing, temp)
    vector("ring-half", '<path d="M 60 215 A 5 5 0 0 0 60 225" fill="none" '
           'stroke="black" stroke-width="2"/>' + rect(59,214,2,3) + rect(59,223,2,3), temp)
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
                   f'<g transform="translate({x},2)"><polygon points="{points}"/></g>', temp)
    vector("colon", rect(145,7,2,2) + rect(145,14,2,2), temp)
    for name, label, x, y in (("am","AM",186,4), ("pm","PM",186,14),
                              ("game-a","GAME A",5,4), ("game-b","GAME B",5,14),
                              ("hi","HI",88,7)):
        vector(name, lettering(label, x, y), temp)
    vector("bell", '<path d="M 50 16 L 52 13 V 9 Q 52 5 56 5 Q 60 5 60 9 V 13 '
           'L 62 16 Z M 54 18 H 58" fill="none" stroke="black" stroke-width="2"/>'
           + rect(55,3,2,2), temp)
    vector("gull", '<path d="M 67 10 Q 72 4 76 11 Q 80 4 85 10 '
           'Q 80 8 76 14 Q 72 8 67 10 Z"/>', temp)


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
    run(SOURCE / "backdrop-lcd.png", "-alpha", "off", "-filter", "Box",
        "-resize", "200x228!", "+dither", "-channel", "RGB", "-posterize", "4",
        "+channel", "PNG32:" + str(ROOT / "art" / "backdrop.png"))
    print("Prepared 88 segments, backdrop, and 25 x 25 launcher icon")


if __name__ == "__main__":
    main()
