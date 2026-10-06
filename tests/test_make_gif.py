"""Round-trip checks for tools/make_gif.py with an independent GIF decoder (standard library only)."""
import contextlib
import importlib.util
import io
from pathlib import Path
import random
import struct
import tempfile
import unittest
import zlib

SPEC = importlib.util.spec_from_file_location("make_gif", Path(__file__).resolve().parents[1] / "tools/make_gif.py")
gif = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gif)


def lzw_decode(data, min_bits):
    clear, end = 1 << min_bits, (1 << min_bits) + 1
    out, table, width, pos, prev = [], None, min_bits + 1, 0, None
    nbits = len(data) * 8
    while pos + width <= nbits:
        code = sum(((data[(pos + i) >> 3] >> ((pos + i) & 7)) & 1) << i for i in range(width))
        pos += width
        if code == clear:
            table, width, prev = [[i] for i in range(clear)] + [None, None], min_bits + 1, None
            continue
        if code == end:
            return out
        if prev is None:
            entry = table[code]
        else:
            entry = table[code] if code < len(table) else prev + [prev[0]]
            table.append(prev + [entry[0]])
            if len(table) == (1 << width) and width < 12:
                width += 1
        out.extend(entry)
        prev = entry
    raise AssertionError("no end code")


def decode(data):
    """Returns (width, height, [(delay_cs, canvas pixels)]) composing frames onto a persistent canvas."""
    assert data[:6] == b"GIF89a" and data[-1] == 0x3B
    width, height, flags = struct.unpack("<HHB", data[6:11])
    size = 2 << (flags & 7)
    table = [tuple(data[13 + 3 * i:16 + 3 * i]) for i in range(size)]
    pos, canvas, frames, delay = 13 + 3 * size, [None] * (width * height), [], 0
    while data[pos] != 0x3B:
        if data[pos] == 0x21:
            label = data[pos + 1]
            if label == 0xF9:
                delay = struct.unpack("<H", data[pos + 4:pos + 6])[0]
            pos += 2
            while data[pos]:
                pos += data[pos] + 1
            pos += 1
        else:
            left, top, w, h, flags = struct.unpack("<HHHHB", data[pos + 1:pos + 10])
            assert flags == 0
            bits, pos, body = data[pos + 10], pos + 11, bytearray()
            while data[pos]:
                body += data[pos + 1:pos + 1 + data[pos]]
                pos += data[pos] + 1
            pos += 1
            indices = lzw_decode(body, bits)
            assert len(indices) == w * h
            for i, value in enumerate(indices):
                canvas[(top + i // w) * width + left + i % w] = table[value]
            frames.append((delay, list(canvas)))
    return width, height, frames


def png_bytes(width, height, rows, kinds):
    """A PNG whose rows use the given filter types, to exercise the reader."""
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    raw, prior = bytearray(), bytes(width * 3)
    for y, row in enumerate(rows):
        kind, line = kinds[y % len(kinds)], bytearray()
        for i, value in enumerate(row):
            left = row[i - 3] if i >= 3 else 0
            up = prior[i]
            upleft = prior[i - 3] if i >= 3 else 0
            if kind == 0:
                guess = 0
            elif kind == 1:
                guess = left
            elif kind == 2:
                guess = up
            elif kind == 3:
                guess = (left + up) // 2
            else:
                p = left + up - upleft
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - upleft)
                guess = left if pa <= pb and pa <= pc else up if pb <= pc else upleft
            line.append((value - guess) & 255)
        raw += bytes([kind]) + line
        prior = bytes(row)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b""))


def sample_frames():
    palette = [(0, 0, 0), (255, 255, 255), (255, 0, 0), (0, 170, 255)]
    rng = random.Random(3)
    base = [palette[(x // 5 + y // 7) % 4] for y in range(30) for x in range(40)]
    out = [(40, 30, list(base))]
    for _ in range(4):
        nxt = list(out[-1][2])
        for _ in range(60):
            nxt[rng.randrange(40 * 30)] = rng.choice(palette)
        out.append((40, 30, nxt))
    return out


class LzwTests(unittest.TestCase):
    def test_round_trip_across_width_changes_and_table_resets(self):
        rng = random.Random(7)
        for bits, count in ((2, 50), (4, 9000), (6, 60000), (6, 300)):
            data = [rng.randrange(1 << bits) if rng.random() < 0.5 else (i // 7) % (1 << bits) for i in range(count)]
            self.assertEqual(lzw_decode(gif.lzw(data, bits), bits), data, (bits, count))

    def test_long_flat_run(self):
        data = [3] * 20000
        packed = gif.lzw(data, 2)
        self.assertEqual(lzw_decode(packed, 2), data)
        self.assertLess(len(packed), 400)


class EncodeTests(unittest.TestCase):
    def test_frames_round_trip_with_cropping_and_delays(self):
        frames = sample_frames()
        data = gif.encode_gif(frames, [10, 20, 5, 7, 30])
        width, height, decoded = decode(data)
        self.assertEqual((width, height), (40, 30))
        self.assertEqual([d for d, _ in decoded], [10, 20, 5, 7, 30])
        for (_, canvas), (_, _, pixels) in zip(decoded, frames):
            self.assertEqual(canvas, pixels)

    def test_netscape_loop_extension_and_palette_size(self):
        data = gif.encode_gif(sample_frames(), [10] * 5)
        self.assertIn(b"NETSCAPE2.0", data)
        self.assertEqual(data[10] & 7, 1)  # four colours -> two bits per pixel

    def test_more_than_64_colours_is_rejected(self):
        pixels = [(i, 0, 0) for i in range(65)]
        with self.assertRaises(ValueError):
            gif.encode_gif([(65, 1, pixels)], [10])

    def test_identical_frames_are_merged(self):
        a, b = sample_frames()[:2]
        frames, delays = gif.merge_identical([a, a, a, b, b], [8, 8, 9, 8, 8])
        self.assertEqual(delays, [25, 16])
        self.assertEqual(len(frames), 2)

    def test_scale_is_nearest_neighbour(self):
        width, height, pixels = gif.scale_frame((2, 2, [(1, 1, 1), (2, 2, 2), (3, 3, 3), (4, 4, 4)]), 2)
        self.assertEqual((width, height), (4, 4))
        self.assertEqual(pixels[:4], [(1, 1, 1)] * 2 + [(2, 2, 2)] * 2)
        self.assertEqual(pixels[4:8], pixels[:4])
        self.assertEqual(pixels[8:12], [(3, 3, 3)] * 2 + [(4, 4, 4)] * 2)


class ReaderTests(unittest.TestCase):
    def test_ppm_with_comment(self):
        data = b"P6\n# comment\n2 1\n255\n" + bytes([1, 2, 3, 4, 5, 6])
        self.assertEqual(gif.read_ppm(data), (2, 1, [(1, 2, 3), (4, 5, 6)]))

    def test_png_filters(self):
        rng = random.Random(11)
        rows = [bytes(rng.randrange(256) for _ in range(5 * 3)) for _ in range(6)]
        width, height, pixels = gif.read_png(png_bytes(5, 6, rows, [0, 1, 2, 3, 4, 4]))
        self.assertEqual((width, height), (5, 6))
        expected = [tuple(row[x * 3:x * 3 + 3]) for row in rows for x in range(5)]
        self.assertEqual(pixels, expected)

    def test_command_line_end_to_end(self):
        frames = sample_frames()
        with tempfile.TemporaryDirectory() as tmp:
            paths = []
            for i, (w, h, pixels) in enumerate([frames[0], frames[0], frames[1]]):
                path = Path(tmp) / ("f%d.ppm" % i)
                path.write_bytes(b"P6\n%d %d\n255\n" % (w, h) + bytes(c for p in pixels for c in p))
                paths.append(str(path))
            out = Path(tmp) / "out.gif"
            with contextlib.redirect_stdout(io.StringIO()):
                gif.main(["--fps", "10", "--scale", "2", "--hold-last", "1", "-o", str(out)] + paths)
            width, height, decoded = decode(out.read_bytes())
            self.assertEqual((width, height), (80, 60))
            self.assertEqual([d for d, _ in decoded], [20, 110])


if __name__ == "__main__":
    unittest.main()
