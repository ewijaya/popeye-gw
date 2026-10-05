#!/usr/bin/env python3
"""Enforce the Harbor Catch app-image budget using the active SDK's size tool."""

import argparse
import subprocess
import sys

APP_IMAGE_BUDGET = 61_440


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size-tool", required=True)
    parser.add_argument("elf")
    args = parser.parse_args()
    try:
        output = subprocess.check_output(
            [args.size_tool, "--format=berkeley", args.elf], text=True
        )
        rows = output.splitlines()
        if len(rows) != 2 or rows[0].split()[:3] != ["text", "data", "bss"]:
            raise ValueError("unexpected size output")
        text_size, data_size, bss_size = map(int, rows[1].split()[:3])
        if min(text_size, data_size, bss_size) < 0:
            raise ValueError("negative section size")
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        print("App image measurement failed: {}".format(error), file=sys.stderr)
        return 1

    total = text_size + data_size + bss_size
    print("App image: {} + {} + {} = {:,} / {:,} bytes (text + data + bss)".format(
        text_size, data_size, bss_size, total, APP_IMAGE_BUDGET
    ))
    if total > APP_IMAGE_BUDGET:
        print("App image exceeds the repository budget by {:,} bytes.".format(
            total - APP_IMAGE_BUDGET
        ), file=sys.stderr)
        return 1
    print("App image headroom: {:,} bytes.".format(APP_IMAGE_BUDGET - total))
    return 0


if __name__ == "__main__":
    sys.exit(main())
