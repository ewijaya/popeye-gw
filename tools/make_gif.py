#!/usr/bin/env python3
"""Build a small animated GIF from emulator frames with the Python standard library only.

Frames are PPM (P6) or 8-bit non-interlaced PNG files (RGB or RGBA), for example
the QEMU monitor's `screendump` output or `pebble screenshot` files. Identical
consecutive frames are merged into one longer frame, each frame stores only the
rectangle that changed, and the colour table is built from the frames themselves
(at most 64 colours, which is all the Pebble Time 2 can show), so nothing is
dithered or approximated.

    make_gif.py --fps 12 --scale 2 -o docs/media/gameplay.gif frame*.ppm
"""

import argparse
import struct
import sys
import zlib

MAX_COLOURS = 64


def read_ppm(data):
    fields, pos = [], 0
    while len(fields) < 4:  # magic, width, height, maxval; '#' comments allowed
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            pos = data.index(b"\n", pos)
            continue
        end = pos
        while not data[end:end + 1].isspace():
            end += 1
        fields.append(data[pos:end])
        pos = end
    if fields[0] != b"P6" or int(fields[3]) != 255:
        raise ValueError("only 8-bit binary PPM (P6) is supported")
    width, height = int(fields[1]), int(fields[2])
    raw = data[pos + 1:pos + 1 + width * height * 3]
    if len(raw) != width * height * 3:
        raise ValueError("truncated PPM")
    return width, height, [tuple(raw[i:i + 3]) for i in range(0, len(raw), 3)]


def read_png(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")
    pos, idat, header = 8, [], None
    while pos < len(data):
        size, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + size]
        pos += 12 + size
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat.append(body)
    width, height, depth, colour, _, _, interlace = header
    if depth != 8 or colour not in (2, 6) or interlace:
        raise ValueError("only 8-bit non-interlaced RGB/RGBA PNG is supported")
    bpp = 3 if colour == 2 else 4
    stride = width * bpp
    raw = zlib.decompress(b"".join(idat))
    rows, prior = [], bytearray(stride)
    for y in range(height):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            left = line[i - bpp] if i >= bpp else 0
            up = prior[i]
            upleft = prior[i - bpp] if i >= bpp else 0
            if kind == 1:
                line[i] = (line[i] + left) & 255
            elif kind == 2:
                line[i] = (line[i] + up) & 255
            elif kind == 3:
                line[i] = (line[i] + (left + up) // 2) & 255
            elif kind == 4:
                p = left + up - upleft
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - upleft)
                line[i] = (line[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else upleft)) & 255
            elif kind != 0:
                raise ValueError("bad PNG filter")
        rows.append(line)
        prior = line
    pixels = [tuple(line[x * bpp:x * bpp + 3]) for line in rows for x in range(width)]
    return width, height, pixels


def read_frame(path):
    with open(path, "rb") as handle:
        data = handle.read()
    return read_png(data) if data[:4] == b"\x89PNG" else read_ppm(data)


def scale_frame(frame, factor):
    width, height, pixels = frame
    if factor == 1:
        return frame
    out = []
    for y in range(height):
        row = [p for p in pixels[y * width:(y + 1) * width] for _ in range(factor)]
        out.extend(row * factor)
    return width * factor, height * factor, out


def lzw(indices, min_bits):
    """GIF LZW with variable-width codes, resetting the table before it passes 4096 entries."""
    clear, end = 1 << min_bits, (1 << min_bits) + 1
    out, acc, nbits = bytearray(), 0, 0

    def emit(code, width):
        nonlocal acc, nbits
        acc |= code << nbits
        nbits += width
        while nbits >= 8:
            out.append(acc & 255)
            acc >>= 8
            nbits -= 8

    table, nxt, width = {}, end + 1, min_bits + 1
    emit(clear, width)
    prefix = indices[0]
    for symbol in indices[1:]:
        key = (prefix, symbol)
        if key in table:
            prefix = table[key]
            continue
        emit(prefix, width)
        if nxt < 4096:
            table[key] = nxt
            if nxt == (1 << width):
                width += 1
            nxt += 1
        else:
            emit(clear, width)
            table, nxt, width = {}, end + 1, min_bits + 1
        prefix = symbol
    emit(prefix, width)
    emit(end, width)
    if nbits:
        out.append(acc & 255)
    return bytes(out)


def sub_blocks(data):
    out = bytearray()
    for i in range(0, len(data), 255):
        chunk = data[i:i + 255]
        out.append(len(chunk))
        out += chunk
    out.append(0)
    return bytes(out)


def merge_identical(frames, delays):
    """Merge runs of identical frames; each kept frame carries the sum of their delays."""
    kept, kept_delays = [], []
    for frame, delay in zip(frames, delays):
        if kept and kept[-1][2] == frame[2]:
            kept_delays[-1] += delay
        else:
            kept.append(frame)
            kept_delays.append(delay)
    return kept, kept_delays


def encode_gif(frames, delays_cs, loop=0):
    """frames: [(width, height, [(r, g, b), ...])] of equal size; delays_cs: hundredths of a second."""
    width, height, _ = frames[0]
    if any(f[0] != width or f[1] != height for f in frames):
        raise ValueError("frames differ in size")
    palette = sorted({p for f in frames for p in f[2]})
    if len(palette) > MAX_COLOURS:
        raise ValueError("%d colours; the limit is %d" % (len(palette), MAX_COLOURS))
    index = {p: i for i, p in enumerate(palette)}
    bits = max(2, (len(palette) - 1).bit_length())
    palette += [(0, 0, 0)] * ((1 << bits) - len(palette))
    out = bytearray(b"GIF89a")
    out += struct.pack("<HHBBB", width, height, 0x80 | (bits - 1), 0, 0)
    out += b"".join(bytes(p) for p in palette)
    out += b"\x21\xff\x0bNETSCAPE2.0\x03\x01" + struct.pack("<H", loop) + b"\x00"
    previous = None
    for frame, delay in zip(frames, delays_cs):
        pixels = frame[2]
        left, top, right, bottom = 0, 0, width, height
        if previous is not None:
            changed = [i for i in range(width * height) if pixels[i] != previous[i]]
            if changed:
                xs = [i % width for i in changed]
                left, right = min(xs), max(xs) + 1
                top, bottom = changed[0] // width, changed[-1] // width + 1
            else:
                left, top, right, bottom = 0, 0, 1, 1
        data = [index[pixels[y * width + x]] for y in range(top, bottom) for x in range(left, right)]
        out += b"\x21\xf9\x04\x04" + struct.pack("<HBB", max(delay, 2), 0, 0)  # leave in place
        out += b"\x2c" + struct.pack("<HHHHB", left, top, right - left, bottom - top, 0)
        out += bytes([bits]) + sub_blocks(lzw(data, bits))
        previous = pixels
    out += b"\x3b"
    return bytes(out)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("frames", nargs="+", help="PPM or PNG frames in playback order")
    parser.add_argument("-o", "--output", required=True)
    parser.add_argument("--fps", type=float, default=12.0, help="capture rate of the input frames")
    parser.add_argument("--scale", type=int, default=1, help="integer nearest-neighbour scale")
    parser.add_argument("--hold-last", type=float, default=0.0, help="extra seconds on the final frame")
    args = parser.parse_args(argv)
    frames = [scale_frame(read_frame(path), args.scale) for path in args.frames]
    # Round cumulative time so merged runs do not drift against the capture clock.
    delays = [round((i + 1) * 100 / args.fps) - round(i * 100 / args.fps) for i in range(len(frames))]
    delays[-1] += round(args.hold_last * 100)
    frames, delays = merge_identical(frames, delays)
    data = encode_gif(frames, delays)
    with open(args.output, "wb") as handle:
        handle.write(data)
    print("%s: %d frames, %d bytes" % (args.output, len(frames), len(data)))


if __name__ == "__main__":
    sys.exit(main())
