#!/usr/bin/env python3
"""Build Popeye G&W LCD art (PRD 9.3) without third-party packages.

Inputs:  art/backdrop.png and art/segments/<name>.png, each a full 200 x 228
         screen. A segment PNG holds only opaque black pixels on transparency;
         its position is where it is drawn.
Outputs: resources/images/segments.png  (packed sprite sheet)
         resources/images/backdrop.png  (copy of the printed backdrop)
         resources/images/backdrop-ghosts.png (backdrop with baked ghosts)
         src/c/segments.h               (segment ID -> sheet and screen rects)

--check rebuilds into a temporary directory and fails if any committed output
differs in pixels or text from the source art.
"""

import argparse
import os
import re
import struct
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WIDTH, HEIGHT = 200, 228
SHEET_WIDTH = 200
LIT = (0, 0, 0, 255)
CLEAR = (0, 0, 0, 0)
GHOST = (0xAA, 0xAA, 0xAA, 0xFF)

# Complete inventory: missing animation frames must fail instead of silently
# rendering prototype rectangles. Keep in sync with scene.h's 82 segment IDs.
EXPECTED_SEGMENTS = {
    "olive-ready", "olive-throw",
    *[f"olive-bell-{pose}" for pose in range(2)],
    *[f"cargo-{lane}-{stage}" for lane in range(4) for stage in range(5)],
    *[f"popeye-{pose}" for pose in range(5)],
    "popeye-dizzy-left", "popeye-dizzy-right", "popeye-catch",
    *[f"splash-{lane}" for lane in range(4)],
    *[f"brutus-{side}-{phase}" for side in ("left", "right") for phase in ("idle", "windup", "strike")],
    *[f"miss-{mark}" for mark in range(3)], "miss-half", "miss-label",
    *[f"digit-{digit}-{bar}" for digit in range(4) for bar in "abcdefg"],
    "colon", "am", "pm", "game-a", "game-b", "bell", "hi",
}

SEGMENT_NAMES = [
    (r"olive-ready", lambda m: "SEG_OLIVE_READY"),
    (r"olive-throw", lambda m: "SEG_OLIVE_THROW"),
    (r"olive-bell-([01])", lambda m: "SEG_OLIVE_BELL + {}".format(m[0])),
    (r"cargo-([0-3])-([0-4])", lambda m: "SEG_CARGO + {} * 5 + {}".format(m[0], m[1])),
    (r"popeye-([0-4])", lambda m: "SEG_POPEYE + {}".format(m[0])),
    (r"popeye-dizzy-(left|right)", lambda m: "SEG_POPEYE_DIZZY_" + m[0].upper()),
    (r"popeye-catch", lambda m: "SEG_POPEYE_CATCH"),
    (r"splash-([0-3])", lambda m: "SEG_SPLASH + {}".format(m[0])),
    (r"brutus-(left|right)-(idle|windup|strike)", lambda m: "SEG_BRUTUS + {} * 3 + {}".format(
        ["left", "right"].index(m[0]), ["idle", "windup", "strike"].index(m[1]))),
    (r"miss-([0-2])", lambda m: "SEG_MISS + {}".format(m[0])),
    (r"miss-half", lambda m: "SEG_MISS_HALF"),
    (r"miss-label", lambda m: "SEG_MISS_LABEL"),
    (r"digit-([0-3])-([a-g])", lambda m: "SEG_DIGIT + {} * 7 + {}".format(m[0], "abcdefg".index(m[1]))),
    (r"(colon|am|pm|bell|hi)", lambda m: "SEG_" + m[0].upper()),
    (r"game-(a|b)", lambda m: "SEG_GAME_" + m[0].upper()),
]


class ArtError(Exception):
    pass


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_png(path):
    """Return (width, height, rows of RGBA tuples) for 8-bit or palette PNGs."""
    with open(path, "rb") as handle:
        data = handle.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ArtError("{}: not a PNG".format(path))
    pos, idat, palette, trns = 8, b"", None, None
    header = None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    width, height, depth, colour, _, _, interlace = header
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(colour)
    if channels is None or interlace or (colour != 3 and depth != 8) or depth not in (1, 2, 4, 8):
        raise ArtError("{}: use 8-bit RGB(A), grey(+alpha) or palette PNG, not interlaced".format(path))
    stride = (width * channels * depth + 7) // 8
    step = max(1, channels * depth // 8)
    raw = zlib.decompress(idat)
    rows, previous = [], bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            left = line[i - step] if i >= step else 0
            up = previous[i]
            corner = previous[i - step] if i >= step else 0
            if kind == 1:
                line[i] = (line[i] + left) & 0xFF
            elif kind == 2:
                line[i] = (line[i] + up) & 0xFF
            elif kind == 3:
                line[i] = (line[i] + (left + up) // 2) & 0xFF
            elif kind == 4:
                line[i] = (line[i] + _paeth(left, up, corner)) & 0xFF
        previous = line
        row = []
        for x in range(width):
            if colour == 3:
                bit = x * depth
                index = (line[bit // 8] >> (8 - depth - bit % 8)) & ((1 << depth) - 1)
                alpha = trns[index] if trns is not None and index < len(trns) else 255
                row.append(palette[index] + (alpha,))
            else:
                px = line[x * channels:(x + 1) * channels]
                if colour == 0:
                    row.append((px[0], px[0], px[0], 255))
                elif colour == 2:
                    row.append((px[0], px[1], px[2], 255))
                elif colour == 4:
                    row.append((px[0], px[0], px[0], px[1]))
                else:
                    row.append(tuple(px))
        rows.append(row)
    return width, height, rows


def write_png(path, rows):
    """Write deterministic 8-bit RGBA PNG (no metadata, filter 0)."""
    height, width = len(rows), len(rows[0])
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n")
        handle.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
        handle.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        handle.write(chunk(b"IEND", b""))


def segment_id(name):
    for pattern, expression in SEGMENT_NAMES:
        match = re.fullmatch(pattern, name)
        if match:
            return expression(match.groups() or (name,))
    raise ArtError("art/segments/{}.png: unknown segment name".format(name))


def load_segments(art_dir):
    seg_dir = os.path.join(art_dir, "segments")
    found = {name[:-4] for name in os.listdir(seg_dir) if name.endswith(".png")}
    if found != EXPECTED_SEGMENTS:
        raise ArtError("segment inventory mismatch: missing {}; unexpected {}".format(
            sorted(EXPECTED_SEGMENTS - found), sorted(found - EXPECTED_SEGMENTS)))
    segments = []
    for filename in sorted(os.listdir(seg_dir)):
        if not filename.endswith(".png"):
            continue
        name = filename[:-4]
        path = os.path.join(seg_dir, filename)
        width, height, rows = read_png(path)
        if (width, height) != (WIDTH, HEIGHT):
            raise ArtError("{}: must be {} x {}".format(path, WIDTH, HEIGHT))
        lit = [(x, y) for y in range(HEIGHT) for x in range(WIDTH) if rows[y][x][3] != 0]
        if not lit:
            raise ArtError("{}: no lit pixels".format(path))
        if len(lit) == WIDTH * HEIGHT:
            raise ArtError("{}: segment must have a transparent background".format(path))
        if any(rows[y][x] != LIT for x, y in lit):
            raise ArtError("{}: pixels must be opaque black or fully transparent".format(path))
        x0 = min(x for x, _ in lit)
        y0 = min(y for _, y in lit)
        x1 = max(x for x, _ in lit) + 1
        y1 = max(y for _, y in lit) + 1
        crop = [[rows[y][x] if rows[y][x][3] else CLEAR for x in range(x0, x1)] for y in range(y0, y1)]
        segments.append({"name": name, "id": segment_id(name), "x": x0, "y": y0,
                         "w": x1 - x0, "h": y1 - y0, "pixels": crop, "lit": lit})
    ids = [s["id"] for s in segments]
    if len(set(ids)) != len(ids):
        raise ArtError("duplicate segment art")
    # The scheduler/bot assumes a throw's destination is readable immediately.
    # Shared launch coordinates therefore require distinct food silhouettes.
    launches = [next(s for s in segments if s["name"] == f"cargo-{lane}-0") for lane in range(4)]
    if len({tuple(s["lit"]) for s in launches}) != 4:
        raise ArtError("each food path must be distinguishable on its launch frame")
    return segments


def pack(segments):
    """Shelf-pack crops, tallest first, with a 1-pixel gap. Deterministic."""
    order = sorted(segments, key=lambda s: (-s["h"], -s["w"], s["name"]))
    x = y = shelf = 0
    for seg in order:
        if seg["w"] > SHEET_WIDTH:
            raise ArtError("{}: wider than the sheet".format(seg["name"]))
        if x + seg["w"] > SHEET_WIDTH:
            x, y, shelf = 0, y + shelf + 1, 0
        seg["sx"], seg["sy"] = x, y
        x += seg["w"] + 1
        shelf = max(shelf, seg["h"])
    height = y + shelf
    sheet = [[CLEAR] * SHEET_WIDTH for _ in range(height)]
    for seg in segments:
        for dy, row in enumerate(seg["pixels"]):
            sheet[seg["sy"] + dy][seg["sx"]:seg["sx"] + seg["w"]] = row
    return sheet


def header_text(segments, sheet_height):
    lines = [
        "/* Generated by tools/build_art.py from art/. Do not edit. */",
        "#ifndef POPEYE_GW_SEGMENTS_H",
        "#define POPEYE_GW_SEGMENTS_H",
        "",
        "#include <stdint.h>",
        "",
        '#include "scene.h"',
        "",
        "/* Complete LCD inventory; every entry has black-on-transparent art. */",
        "typedef struct {",
        "  int16_t sheet_x, sheet_y, x, y, w, h;",
        "} SegmentArt;",
        "",
        "#define SEGMENT_ART_COUNT {}".format(len(segments)),
        "#define SEGMENT_SHEET_WIDTH {}".format(SHEET_WIDTH),
        "#define SEGMENT_SHEET_HEIGHT {}".format(sheet_height),
        "",
        "static const SegmentArt segment_art[SEG_COUNT] = {",
    ]
    for seg in sorted(segments, key=lambda s: s["name"]):
        lines.append("  [{}] = {{ {}, {}, {}, {}, {}, {} }}, /* {} */".format(
            seg["id"], seg["sx"], seg["sy"], seg["x"], seg["y"], seg["w"], seg["h"], seg["name"]))
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines)


def landscape(rows, buttons_bottom=False):
    """228 x 200 handheld canvas, turned clockwise into the native framebuffer.

    Nearest-neighbour sampling keeps binary LCD segments crisp. Holding the
    watch counterclockwise puts Up/Select/Down along the top, left to right.
    """
    turned = [[rows[(WIDTH - 1 - x) * HEIGHT // WIDTH][y * WIDTH // HEIGHT]
               for x in range(WIDTH)] for y in range(HEIGHT)]
    # The opposite holding direction uses exactly the same sampled pixels.
    return [row[::-1] for row in turned[::-1]] if buttons_bottom else turned


def landscape_segments(segments, buttons_bottom=False):
    result = []
    for segment in segments:
        rows = [[CLEAR] * WIDTH for _ in range(HEIGHT)]
        for x, y in segment["lit"]:
            rows[y][x] = LIT
        rows = landscape(rows, buttons_bottom)
        lit = [(x, y) for y in range(HEIGHT) for x in range(WIDTH) if rows[y][x][3]]
        if not lit:
            raise ArtError(segment["name"] + ": lost in landscape conversion")
        x0, y0 = min(x for x, _ in lit), min(y for _, y in lit)
        x1, y1 = max(x for x, _ in lit) + 1, max(y for _, y in lit) + 1
        result.append(dict(name=segment["name"], id=segment["id"], x=x0, y=y0,
                           w=x1-x0, h=y1-y0, pixels=[row[x0:x1] for row in rows[y0:y1]], lit=lit))
    return result


def build(out_root):
    art_dir = os.path.join(ROOT, "art")
    width, height, backdrop = read_png(os.path.join(art_dir, "backdrop.png"))
    if (width, height) != (WIDTH, HEIGHT):
        raise ArtError("art/backdrop.png: must be {} x {}".format(WIDTH, HEIGHT))
    if any(px[3] != 255 for row in backdrop for px in row):
        raise ArtError("art/backdrop.png: must be fully opaque")
    segments = load_segments(art_dir)
    sheet = pack(segments)
    ghosts = [list(row) for row in backdrop]
    for seg in segments:
        # Restrict unlit ghosts to the score and MISS register. Full character
        # and cargo unions make a speckled mass on Emery's four grey levels.
        if not (seg["name"].startswith("digit-") or re.fullmatch(r"miss-[0-2]", seg["name"])):
            continue
        for x, y in seg["lit"]:
            if (x + y) % 2 == 0:
                ghosts[y][x] = GHOST
    images = os.path.join(out_root, "resources", "images")
    os.makedirs(images, exist_ok=True)
    os.makedirs(os.path.join(out_root, "src", "c"), exist_ok=True)
    write_png(os.path.join(images, "segments.png"), sheet)
    write_png(os.path.join(images, "backdrop.png"), backdrop)
    write_png(os.path.join(images, "backdrop-ghosts.png"), ghosts)
    for bottom in (False, True):
        slug = "landscape-bottom" if bottom else "landscape"
        symbol = slug.replace("-", "_")
        turned = landscape_segments(segments, bottom)
        turned_sheet = pack(turned)
        write_png(os.path.join(images, f"segments-{slug}.png"), turned_sheet)
        write_png(os.path.join(images, f"backdrop-{slug}.png"), landscape(backdrop, bottom))
        # Transform the same ghost composite, so lit segments and ghosts stay aligned.
        write_png(os.path.join(images, f"backdrop-ghosts-{slug}.png"), landscape(ghosts, bottom))
        turned_header = header_text(turned, len(turned_sheet))
        turned_header = turned_header.replace("POPEYE_GW_SEGMENTS_H", f"POPEYE_GW_SEGMENTS_{symbol.upper()}_H")
        turned_header = turned_header.replace("#include \"scene.h\"", "#include \"segments.h\"")
        turned_header = turned_header.replace("typedef struct {\n  int16_t sheet_x, sheet_y, x, y, w, h;\n} SegmentArt;\n", "")
        turned_header = turned_header.replace("SEGMENT_ART_COUNT", f"{symbol.upper()}_ART_COUNT")
        turned_header = turned_header.replace("SEGMENT_SHEET_", f"{symbol.upper()}_SHEET_")
        turned_header = turned_header.replace("segment_art[", f"{symbol}_art[")
        with open(os.path.join(out_root, "src", "c", f"segments_{symbol}.h"), "w", newline="\n") as handle:
            handle.write(turned_header)
    icon_w, icon_h, icon = read_png(os.path.join(art_dir, "menu-icon.png"))
    if (icon_w, icon_h) != (25, 25):
        raise ArtError("art/menu-icon.png: must be 25 x 25")
    if not any(px == LIT for row in icon for px in row):
        raise ArtError("art/menu-icon.png: must contain visible black pixels")
    if any(px[3] != 0 and px != LIT for row in icon for px in row):
        raise ArtError("art/menu-icon.png: use opaque black or transparency")
    write_png(os.path.join(images, "menu-icon.png"), icon)
    with open(os.path.join(out_root, "src", "c", "segments.h"), "w", newline="\n") as handle:
        handle.write(header_text(segments, len(sheet)))
    return len(segments)


OUTPUTS = [
    "resources/images/segments.png",
    "resources/images/backdrop.png",
    "resources/images/backdrop-ghosts.png",
    "resources/images/menu-icon.png",
    "src/c/segments.h",
    "src/c/segments_landscape.h",
    "resources/images/segments-landscape.png",
    "resources/images/backdrop-landscape.png",
    "resources/images/backdrop-ghosts-landscape.png",
    "src/c/segments_landscape_bottom.h",
    "resources/images/segments-landscape-bottom.png",
    "resources/images/backdrop-landscape-bottom.png",
    "resources/images/backdrop-ghosts-landscape-bottom.png",
]


def check():
    with tempfile.TemporaryDirectory() as temp:
        count = build(temp)
        stale = []
        for rel in OUTPUTS:
            fresh, committed = os.path.join(temp, rel), os.path.join(ROOT, rel)
            if not os.path.exists(committed):
                stale.append(rel)
            elif rel.endswith(".png"):
                if read_png(fresh) != read_png(committed):
                    stale.append(rel)
            else:
                with open(fresh) as a, open(committed) as b:
                    if a.read() != b.read():
                        stale.append(rel)
    if stale:
        print("Generated art is stale; run tools/build_art.py: " + ", ".join(stale), file=sys.stderr)
        return 1
    print("Art pipeline check passed: {} segments match art/".format(count))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if outputs differ from art/")
    args = parser.parse_args()
    try:
        if args.check:
            return check()
        count = build(ROOT)
    except ArtError as error:
        print(error, file=sys.stderr)
        return 1
    print("Built {} segments into resources/images and src/c/segments.h".format(count))
    return 0


if __name__ == "__main__":
    sys.exit(main())
