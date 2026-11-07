#!/usr/bin/env python3
"""Build a classic Workbench tool icon with normal and selected images.

SPDX-License-Identifier: EPL-2.0
"""
import struct
import sys
from pathlib import Path


def image(selected):
    width, height = 48, 32
    pixels = [[0] * width for _ in range(height)]

    def rect(x0, y0, x1, y1, pen):
        for y in range(y0, y1 + 1):
            pixels[y][x0:x1 + 1] = [pen] * (x1 - x0 + 1)

    # Three sliders, using the standard four Workbench pens.
    light, dark = (1, 2) if selected else (2, 1)
    rect(2, 2, 44, 29, 1)
    rect(2, 2, 43, 28, 0)
    rect(2, 2, 43, 2, light)
    rect(2, 2, 2, 28, light)
    rect(43, 3, 43, 28, dark)
    rect(3, 28, 43, 28, dark)
    for y, x in ((8, 16), (15, 30), (22, 22)):
        rect(7, y, 38, y, 1)
        rect(7, y + 1, 38, y + 1, 2)
        rect(x - 3, y - 2, x + 3, y + 3, 1)
        rect(x - 3, y - 2, x + 2, y + 2, 3 if selected else 0)
        rect(x - 3, y - 2, x + 2, y - 2, light)
        rect(x - 3, y - 2, x - 3, y + 2, light)

    # Intuition Image: two planes, word-aligned rows, most significant bit first.
    header = struct.pack(">5HIBBI", 0, 0, width, height, 2, 1, 3, 0, 0)
    bitmap = bytearray()
    for plane in range(2):
        for row in pixels:
            for x in range(0, width, 8):
                bitmap.append(sum(((row[x + bit] >> plane) & 1) << (7 - bit)
                                  for bit in range(8)))
    return header + bitmap


def icon():
    # DiskObject and its embedded Gadget use 68000 big-endian, 2-byte alignment.
    header = bytearray(78)
    struct.pack_into(">HH", header, 0, 0xE310, 1)
    struct.pack_into(">5H", header, 12, 48, 32, 6, 1, 1)
    struct.pack_into(">II", header, 22, 1, 1)  # Both images present.
    struct.pack_into(">I", header, 44, 1)  # Workbench 2.x icon revision.
    header[48] = 3  # WBTOOL; no DefaultTool or ToolTypes.
    struct.pack_into(">II", header, 58, 0x80000000, 0x80000000)
    struct.pack_into(">I", header, 74, 16384)
    return header + image(False) + image(True)


if __name__ == "__main__":
    Path(sys.argv[1]).write_bytes(icon())
