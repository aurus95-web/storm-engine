#!/usr/bin/env python3
"""Draws the icon of the app (no art of the game: the compass of the touch sticks on the dark blue of the sea).

usage: make_icon.py <icon.png> [size]   (512 by default; tools/ios/package-adhoc.sh resizes it)
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "touch-icons"))
import touch_icons as ti  # noqa: E402


def over(top, bottom):
    a = top[3] if len(top) > 3 else 1.0
    return tuple(top[i] * a + bottom[i] * (1 - a) for i in range(3))


def sample(x, y):
    # the night sea: lighter at the top left
    r = math.hypot(x - 0.3, y - 0.25)
    col = ti.mix((0.16, 0.24, 0.42), (0.04, 0.07, 0.16), ti.clamp(r / 0.95))
    # the compass ring, a bit smaller than the icon (the corners of the icons are rounded)
    k = 0.86
    bx, by = 0.5 + (x - 0.5) / k, 0.5 + (y - 0.5) / k
    if 0.0 <= bx <= 1.0 and 0.0 <= by <= 1.0:
        col = over(ti.stick_base(bx, by), col)
    # the medallion of the sea in the middle
    m = 0.5
    kx, ky = 0.5 + (x - 0.5) / m, 0.5 + (y - 0.5) / m
    if 0.0 <= kx <= 1.0 and 0.0 <= ky <= 1.0:
        col = over(ti.stick_knob(kx, ky), col)
    return col + (1.0,)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    size = int(sys.argv[2]) if len(sys.argv) > 2 else 512
    ti.SS = 2
    ti.write_png(sys.argv[1], ti.render(sample, size), 1)


if __name__ == "__main__":
    main()
