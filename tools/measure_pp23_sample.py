#!/usr/bin/env python3
"""Reproduce one narrow observation from Old80s' PP-23 MAME recording.

Input must be its 710x480, 30 fps version, video ID 7r-XsTXNuNU. This does
not infer the game's rules, input transitions or hardware clock frequency.
Requires ffmpeg; does not download or redistribute the recording.
"""
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('video', help='Local 710x480 recording')
args = parser.parse_args()
width, height, fps, start = 330, 240, 30, 59
raw = subprocess.run([
    'ffmpeg', '-v', 'error', '-ss', str(start), '-i', args.video, '-t', '4',
    '-vf', 'fps=30,crop=330:240:200:25', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'
], check=True, stdout=subprocess.PIPE).stdout
size = width * height * 3
assert len(raw) == size * 120, 'Expected four seconds / 120 decoded frames'
# Visually identified can positions; coordinates are in the uncropped video.
boxes = [(211, 50, 240, 90), (257, 58, 288, 98), (300, 73, 341, 108),
         (351, 98, 392, 129), (397, 128, 433, 168), (428, 177, 474, 229)]
print('position,first_frame,last_frame,start_seconds,end_exclusive_seconds,duration_seconds,censored_end')
for number, (x1, y1, x2, y2) in enumerate(boxes, 1):
    active = []
    offsets = [((y - 25) * width + x - 200) * 3
               for y in range(y1, y2 + 1) for x in range(x1, x2 + 1)]
    for frame in range(120):
        pixels = raw[frame * size:(frame + 1) * size]
        if sum(max(pixels[o:o + 3]) < 85 for o in offsets) > 350:
            active.append(frame)
    assert active and active == list(range(active[0], active[-1] + 1)), 'Inspect video/ROI before interpreting'
    first, last = active[0], active[-1]
    print(f'{number},{first},{last},{start + first / fps:.3f},'
          f'{start + (last + 1) / fps:.3f},{len(active) / fps:.3f},{last == 119}')
