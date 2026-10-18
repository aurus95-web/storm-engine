#!/usr/bin/env python3
"""Draws the pictures of the touch buttons that the game has no picture for, in the style of the ability icons
of Pirates of the Caribbean: things of brass, iron, wood, canvas and parchment lit from the top left
on a dark blue background.

usage: touch_icons.py <game folder> [preview folder]
writes <game folder>/RESOURCE/Textures/INTERFACES/touch_<name>.tga (128x128, loaded as raw Targa outside Windows)
and the scaled up previews <preview folder>/touch_<name>.png
"""
import math
import os
import struct
import sys
import zlib

SIZE = 128
SS = 4  # supersampling


def mix(a, b, k):
    return tuple(a[i] + (b[i] - a[i]) * k for i in range(len(a)))


def clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


def smooth(e0, e1, v):
    t = clamp((v - e0) / (e1 - e0))
    return t * t * (3 - 2 * t)


def scale(c, k):
    return tuple(clamp(v * k) for v in c)


def add(a, b):
    return tuple(clamp(a[i] + b[i]) for i in range(3))


def normalize(v):
    n = math.sqrt(sum(c * c for c in v))
    return tuple(c / n for c in v)


# ---------------------------------------------------------------------------------------------------------------------
# noise for the materials

def hash2(ix, iy, seed=0):
    h = (ix * 374761393 + iy * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return (h ^ (h >> 16)) / 0xFFFFFFFF


def noise(x, y, seed=0):
    """value noise from 0 to 1"""
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = hash2(ix, iy, seed), hash2(ix + 1, iy, seed)
    c, d = hash2(ix, iy + 1, seed), hash2(ix + 1, iy + 1, seed)
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy


def fbm(x, y, octaves=4, seed=0):
    v, amp, total = 0.0, 0.5, 0.0
    for i in range(octaves):
        v += noise(x, y, seed + i) * amp
        total += amp
        x, y, amp = x * 2.03, y * 2.03, amp * 0.5
    return v / total


# ---------------------------------------------------------------------------------------------------------------------
# background and frame (the buttons cut the frame off and draw their own one)

def background(x, y):
    """the background of the icons of the items of the game (plate); returns (colour, is the frame): no frame, the
    engine draws the frames of the buttons"""
    return plate(x, y), False


# ---------------------------------------------------------------------------------------------------------------------
# signed distances (negative inside)

def sd_circle(x, y, cx, cy, r):
    return math.hypot(x - cx, y - cy) - r


def sd_segment(x, y, ax, ay, bx, by, r):
    """a capsule: the segment a-b thickened by r"""
    px, py, dx, dy = x - ax, y - ay, bx - ax, by - ay
    h = clamp((px * dx + py * dy) / (dx * dx + dy * dy))
    return math.hypot(px - dx * h, py - dy * h) - r


def sd_box(x, y, cx, cy, hw, hh, r=0.0):
    """a box with the half sizes hw, hh and the corner radius r"""
    qx, qy = abs(x - cx) - hw + r, abs(y - cy) - hh + r
    return math.hypot(max(qx, 0.0), max(qy, 0.0)) + min(max(qx, qy), 0.0) - r


def sd_polygon(x, y, pts):
    d = float("inf")
    inside = False
    for i in range(len(pts)):
        ax, ay = pts[i]
        bx, by = pts[(i + 1) % len(pts)]
        d = min(d, sd_segment(x, y, ax, ay, bx, by, 0.0))
        if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
            inside = not inside
    return -d if inside else d


def union(*ds):
    return min(ds)


def smooth_union(*ds, k=0.03):
    """a union without the creases where the shapes meet"""
    d = ds[0]
    for e in ds[1:]:
        h = clamp(0.5 + 0.5 * (e - d) / k)
        d = e + (d - e) * h - k * h * (1 - h)
    return d


def rotate(x, y, cx, cy, angle):
    c, s = math.cos(angle), math.sin(angle)
    dx, dy = x - cx, y - cy
    return cx + dx * c + dy * s, cy - dx * s + dy * c


# ---------------------------------------------------------------------------------------------------------------------
# lighting: from the top left, the viewer in front

LIGHT = normalize((-0.55, -0.65, 0.75))
EPS = 0.003


def relief_normal(sdf, x, y, bevel, depth=1.0):
    """the normal of a shape raised with a rounded bevel along its edge"""

    def height(px, py):
        t = clamp(-sdf(px, py) / bevel)
        return math.sin(t * math.pi / 2)

    hx = (height(x + EPS, y) - height(x - EPS, y)) / (2 * EPS) * bevel * depth
    hy = (height(x, y + EPS) - height(x, y - EPS)) / (2 * EPS) * bevel * depth
    return normalize((-hx, -hy, 1.0))


def shade_metal(base, n, shine=40.0, spec=0.9):
    """a polished metal: diffuse, the sky reflected at the top, a highlight"""
    diff = max(0.0, sum(n[i] * LIGHT[i] for i in range(3)))
    r = tuple(2 * n[2] * n[i] - (1 if i == 2 else 0) for i in range(3))  # the view reflected
    hl = max(0.0, sum(r[i] * LIGHT[i] for i in range(3))) ** shine
    env = clamp(0.5 - n[1] * 0.6)
    col = scale(base, 0.30 + 0.70 * diff + 0.25 * env)
    return add(col, (spec * hl, spec * hl * 0.95, spec * hl * 0.85))


def shade_matte(base, n, rough=0.15):
    diff = max(0.0, sum(n[i] * LIGHT[i] for i in range(3)))
    return scale(base, 0.45 + 0.65 * diff + rough * 0.0)


BRASS = (0.92, 0.66, 0.26)
IRON = (0.42, 0.44, 0.48)
INK = (0.12, 0.07, 0.04)


def drop_shadow(col, sdf, x, y, strength=0.55, dx=0.025, dy=0.03, soft=0.04):
    d = sdf(x - dx, y - dy)
    k = clamp(1 - d / soft) if d > 0 else 1.0
    return mix(col, (0.0, 0.0, 0.0), strength * k)


def metal_sign(sdf, base=BRASS, bevel=0.035, extra=None):
    """a sign cast in metal on the background"""

    def sample(x, y):
        col, frame = background(x, y)
        if frame:
            return col
        col = drop_shadow(col, sdf, x, y)
        d = sdf(x, y)
        if d < 0:
            n = relief_normal(sdf, x, y, bevel)
            # a fine patina and the light falling from the top left
            m = (0.96 + 0.08 * fbm(x * 70, y * 70, 2, 7)) * (1.12 - 0.3 * clamp((x + y) * 0.5))
            col = shade_metal(scale(base, m), n)
            if extra:
                col = extra(x, y, col)
        elif d < 0.006:
            col = mix(col, (0.05, 0.03, 0.01), 1 - d / 0.006)
        return col

    return sample


# ---------------------------------------------------------------------------------------------------------------------
# the materials

def wood(x, y, along_x=False):
    u, v = (x, y) if along_x else (y, x)
    rings = math.sin((v * 90 + fbm(u * 4, v * 30, 3, 3) * 6) ) * 0.5 + 0.5
    base = mix((0.42, 0.23, 0.11), (0.62, 0.38, 0.19), rings)
    return scale(base, 0.85 + 0.3 * fbm(u * 60, v * 8, 2, 5))


def parchment(x, y):
    f = fbm(x * 9, y * 9, 4, 11)
    fibres = fbm(x * 80, y * 12, 2, 13)
    return scale(mix((0.80, 0.68, 0.48), (0.95, 0.88, 0.70), f), 0.92 + 0.12 * fibres)


def canvas(x, y):
    weave = 0.5 + 0.25 * math.sin(x * 400) * math.sin(y * 400)
    dirt = fbm(x * 6, y * 6, 3, 21)
    return scale(mix((0.80, 0.74, 0.62), (0.97, 0.94, 0.86), dirt), 0.94 + 0.08 * weave)


# ---------------------------------------------------------------------------------------------------------------------
# the icons

def arrow(up):
    s = 1 if up else -1

    def sdf(x, y):
        y2 = 0.5 + (y - 0.5) * s
        return sd_polygon(x, y2, [(0.5, 0.14), (0.83, 0.48), (0.61, 0.48), (0.61, 0.85), (0.39, 0.85),
                                  (0.39, 0.48), (0.17, 0.48)])

    return metal_sign(sdf)


def plate(x, y):
    """the background of the icons of the items of the game (items1.tga): dark teal at the top, a grey blue glow from
    the bottom middle, darker to the edges; the touch buttons show the pictures of the game on it"""
    top, bottom = (0.0, 0.17, 0.27), (0.27, 0.35, 0.39)
    glow = clamp(1 - math.hypot((x - 0.5) * 1.2, (y - 0.95) * 1.1) / 0.9)
    col = mix(top, bottom, clamp(0.2 * y + glow * glow))

    def smooth(v):
        v = clamp(v)
        return v * v * (3 - 2 * v)

    # darker to the edges, smoothly (no seams in the corners)
    edge = smooth(min(x, 1 - x) / 0.25) * smooth(y / 0.2) * smooth((1 - y) / 0.1)
    col = scale(col, 0.2 + 0.8 * edge)
    return scale(col, 0.97 + 0.06 * fbm(x * 6, y * 6, 3, 21))


def lr_arrows():
    """the scroll of the commands of the game: brass chevrons, the left one in the left half, the right one in the
    right half (each half is shown as a narrow button: half as wide as high); transparent around them"""

    def chevron(left):
        def sdf(x, y):
            # the cell in the units of its height: x from 0 to 0.5
            u = (1 - x if left else x) * 0.5
            return sd_polygon(u, y, [(0.1, 0.16), (0.42, 0.5), (0.1, 0.84), (0.1, 0.63), (0.25, 0.5), (0.1, 0.37)])

        sign = metal_sign(sdf)

        def sample(x, y):
            d = sdf(x, y)
            if d > 0.012:
                return (0.0, 0.0, 0.0, 0.0)
            col = sign(x, y)
            return (col[0], col[1], col[2], 1.0 if d <= 0 else 1 - d / 0.012)

        return sample

    lft, rgt = chevron(True), chevron(False)
    return lambda x, y: lft(x * 2, y) if x < 0.5 else rgt(x * 2 - 1, y)


def back():
    """Esc: a brass arrow turning back"""

    def sdf(x, y):
        ring = abs(sd_circle(x, y, 0.52, 0.52, 0.24)) - 0.06
        ring = max(ring, y - 0.56, -(x - 0.30) if y > 0.5 else -1.0)
        stem = sd_box(x, y, 0.76, 0.66, 0.06, 0.14, 0.015)
        head = sd_polygon(x, y, [(0.28, 0.82), (0.08, 0.52), (0.48, 0.52)])
        return smooth_union(ring, stem, head, k=0.045)

    return metal_sign(sdf)


def skip():
    def sdf(x, y):
        t1 = sd_polygon(x, y, [(0.16, 0.25), (0.46, 0.5), (0.16, 0.75)])
        t2 = sd_polygon(x, y, [(0.43, 0.25), (0.73, 0.5), (0.43, 0.75)])
        return union(t1, t2, sd_box(x, y, 0.79, 0.5, 0.045, 0.25, 0.012))

    return metal_sign(sdf)


def magnifier(plus):
    cx, cy, r = 0.42, 0.41, 0.23
    rim = 0.032

    def ring_sdf(x, y):
        return abs(sd_circle(x, y, cx, cy, r)) - rim

    def ferrule(x, y):
        return sd_segment(x, y, cx + (r + rim) * 0.70, cy + (r + rim) * 0.70, 0.66, 0.66, 0.030)

    def handle(x, y):
        return sd_segment(x, y, 0.66, 0.66, 0.84, 0.84, 0.042)

    def symbol(x, y):
        d = sd_box(x, y, cx, cy, 0.105, 0.024, 0.01)
        if plus:
            d = union(d, sd_box(x, y, cx, cy, 0.024, 0.105, 0.01))
        return d

    def all_sdf(x, y):
        return union(ring_sdf(x, y), ferrule(x, y), handle(x, y), sd_circle(x, y, cx, cy, r))

    def sample(x, y):
        col, frame = background(x, y)
        if frame:
            return col
        col = drop_shadow(col, all_sdf, x, y, 0.5)
        # the glass: the background magnified, tinted, darker at the rim, a reflection
        g = sd_circle(x, y, cx, cy, r)
        if g < 0:
            mx, my = cx + (x - cx) * 0.7, cy + (y - cy) * 0.7
            col = mix(background(mx, my)[0], (0.55, 0.75, 0.85), 0.18)
            col = scale(col, 1.15 - 0.4 * (1 + g / r) ** 0)
            col = scale(col, 0.75 + 0.45 * clamp(-g / r))
            refl = sd_segment(x, y, cx - 0.13, cy - 0.02, cx - 0.03, cy - 0.13, 0.014)
            if refl < 0:
                col = mix(col, (1, 1, 1), 0.55 * clamp(-refl / 0.01))
            d = symbol(x, y)
            if d < 0.012:
                col = mix(col, (0, 0, 0), 0.5 * clamp(1 - (d - 0.004) / 0.012))  # its shadow on the glass
            if d < 0:
                col = shade_metal(BRASS, relief_normal(symbol, x, y, 0.02))
        # the handle: dark turned wood
        d = handle(x, y)
        if d < 0:
            px, py = x - 0.66, y - 0.66
            across = (px - py) / math.sqrt(2) / 0.042
            n = normalize((across * 0.7, -across * 0.7, math.sqrt(max(0.05, 1 - across * across))))
            col = shade_metal(wood(px + py, across * 0.1, True), n, 20, 0.35)
        # the brass ferrule and rim
        for f in (ferrule, ring_sdf):
            d = f(x, y)
            if d < 0:
                col = shade_metal(BRASS, relief_normal(f, x, y, 0.03))
            elif d < 0.005 and g >= 0:
                col = mix(col, (0.05, 0.03, 0.01), 0.7)
        return col

    return sample


def eye():
    cx, cy = 0.5, 0.52

    def opening(x, y):
        return max(sd_circle(x, y, cx, cy + 0.26, 0.40), sd_circle(x, y, cx, cy - 0.26, 0.40))

    def sample(x, y):
        col, frame = background(x, y)
        if frame:
            return col
        d = opening(x, y)
        if d < 0:
            r = math.hypot(x - cx, y - cy)
            # the white: shaded to the corners and under the upper lid
            col = mix((0.97, 0.94, 0.90), (0.75, 0.62, 0.58), clamp(abs(x - cx) / 0.32) ** 2)
            col = scale(col, 0.75 + 0.25 * smooth(0.0, 0.05, -sd_circle(x, y, cx, cy + 0.26, 0.40)))
            if r < 0.125:
                # the iris: fibres around the pupil, a dark ring at its edge
                a = math.atan2(y - cy, x - cx)
                fib = fbm(a * 6, r * 40, 3, 41)
                col = mix((0.35, 0.62, 0.78), (0.10, 0.28, 0.42), clamp(r / 0.125 * 0.8 + fib * 0.4))
                col = scale(col, 1 - 0.6 * smooth(0.10, 0.125, r))
                # the shadow of the upper lid on the iris
                col = scale(col, 0.6 + 0.4 * smooth(0.0, 0.06, -sd_circle(x, y, cx, cy + 0.26, 0.40)))
            if r < 0.052:
                col = (0.02, 0.02, 0.03)
            # the reflection of the light
            if math.hypot(x - cx + 0.04, y - cy + 0.04) < 0.022:
                col = (1, 1, 1)
        elif d < 0.018 and y < cy:
            col = mix(col, (0.06, 0.04, 0.03), 0.9)  # the upper lashes
        elif d < 0.008:
            col = mix(col, (0.45, 0.25, 0.20), 0.6)  # the lower lid
        return col

    return sample


def parchment_sheet(ink):
    """a sheet of parchment, a bit turned, with ink on it"""
    angle = -0.12

    def sheet(x, y):
        rx, ry = rotate(x, y, 0.5, 0.5, angle)
        wav = 0.012 * math.sin(ry * 40) + 0.01 * (fbm(rx * 20, ry * 20, 2, 61) - 0.5)
        return sd_box(rx + wav, ry, 0.5, 0.5, 0.30, 0.36, 0.01)

    def sample(x, y):
        col, frame = background(x, y)
        if frame:
            return col
        col = drop_shadow(col, sheet, x, y, 0.5)
        d = sheet(x, y)
        if d < 0:
            rx, ry = rotate(x, y, 0.5, 0.5, angle)
            col = parchment(rx, ry)
            # aged darker edges and a stain
            col = scale(col, 0.70 + 0.30 * smooth(0.0, 0.06, -d))
            col = scale(col, 1 - 0.15 * smooth(0.6, 0.8, fbm(rx * 5, ry * 5, 3, 71)))
            k = ink(rx, ry)
            if k > 0:
                col = mix(col, INK, k)
        return col

    return sample


def check():
    def ink(x, y):
        # a quill stroke: thin at the start, thick at the turn, thinning at the end
        d1 = sd_segment(x, y, 0.30, 0.52, 0.44, 0.68, 0.0)
        d2 = sd_segment(x, y, 0.44, 0.68, 0.74, 0.28, 0.0)
        t1 = clamp((x - 0.30) / 0.14)
        t2 = clamp((x - 0.44) / 0.30)
        w = min(d1 - (0.015 + 0.03 * t1), d2 - (0.045 - 0.035 * t2))
        return clamp(-w / 0.004) * (0.85 + 0.15 * fbm(x * 50, y * 50, 2, 81))

    return parchment_sheet(ink)


def scroll():
    """menu: written lines on parchment"""

    def ink(x, y):
        k = 0.0
        for i, (ly, end) in enumerate(((0.27, 0.72), (0.37, 0.70), (0.47, 0.73), (0.57, 0.66), (0.67, 0.71),
                                       (0.77, 0.52))):
            if 0.28 < x < end:
                # a hand written line: the loops of the letters, the gaps between the words
                if noise(x * 16, i * 7.0, 91) < 0.3:
                    continue
                yy = ly + 0.004 * math.sin(x * 25 + i) - 0.014 * max(0.0, math.sin(x * 230 + i * 3)) ** 3
                if abs(y - yy) < 0.0045 + 0.002 * math.cos(x * 230 + i * 3):
                    k = 0.85
        return k

    return parchment_sheet(ink)


def tricorn():
    """the hero: a black tricorn with a golden braid, seen from the front"""

    def crown(x, y):
        return union(sd_circle(x, y, 0.5, 0.36, 0.17), sd_box(x, y, 0.5, 0.46, 0.17, 0.10))

    def brim(x, y):
        # the brim turned up into three corners: two high at the sides, one low at the front
        top = 0.24 + 0.24 * (1 - ((x - 0.5) / 0.44) ** 2) if abs(x - 0.5) < 0.44 else 0.24
        return max(sd_polygon(x, y, [(0.03, 0.22), (0.50, 0.76), (0.97, 0.22)]), top - y)

    def hat(x, y):
        return union(crown(x, y), brim(x, y))

    def sample(x, y):
        col, frame = background(x, y)
        if frame:
            return col
        col = drop_shadow(col, hat, x, y)
        felt = scale((0.17, 0.15, 0.14), 0.85 + 0.3 * fbm(x * 70, y * 70, 3, 101))
        d = crown(x, y)
        if d < 0:
            col = shade_matte(felt, relief_normal(crown, x, y, 0.12))
        d = brim(x, y)
        if d < 0:
            col = shade_matte(scale(felt, 1.15), relief_normal(brim, x, y, 0.06))
            # the braid along the edge of the brim
            if d > -0.028:
                col = shade_metal(BRASS, relief_normal(brim, x, y, 0.028))
                if math.sin((x * 0.7 + y) * 150) > 0.55:
                    col = scale(col, 0.7)  # its twists
        elif d < 0.006:
            col = mix(col, (0.03, 0.02, 0.01), 0.8)
        # a cockade on the left side of the brim
        cc = (0.30, 0.48)
        dc = sd_circle(x, y, cc[0], cc[1], 0.055)
        if dc < 0:
            ruffle = 0.7 + 0.3 * math.cos(math.atan2(y - cc[1], x - cc[0]) * 14)
            col = shade_matte(scale((0.75, 0.10, 0.08), ruffle),
                              relief_normal(lambda px, py: sd_circle(px, py, cc[0], cc[1], 0.055), x, y, 0.05))
            button = lambda px, py: sd_circle(px, py, cc[0], cc[1], 0.02)
            if button(x, y) < 0:
                col = shade_metal(BRASS, relief_normal(button, x, y, 0.02))
        return col

    return sample


def sails(up):
    def sample(x, y):
        col, frame = background(x, y)
        if frame:
            return col
        bottom = 0.80 if up else 0.48
        belly = lambda yy: 0.05 * math.sin(clamp((yy - 0.25) / (bottom - 0.25)) * math.pi)
        sail = lambda px, py: sd_polygon(px + belly(py), py, [(0.15, 0.25), (0.57, 0.25), (0.53, bottom), (0.19, bottom)])
        mast = lambda px, py: sd_box(px, py, 0.36, 0.52, 0.022, 0.40, 0.01)
        yard = lambda px, py: sd_segment(px, py, 0.12, 0.235, 0.60, 0.235, 0.02)
        col = drop_shadow(col, lambda px, py: union(sail(px, py), mast(px, py), yard(px, py)), x, y, 0.45)
        # the mast behind the canvas
        d = mast(x, y)
        if d < 0:
            k = (x - 0.36) / 0.022
            n = normalize((k * 0.8, 0.0, math.sqrt(max(0.05, 1 - k * k))))
            col = shade_metal(wood(x, y), n, 15, 0.2)
        d = sail(x, y)
        if d < 0:
            # canvas: vertical folds of a bellied sail, the seams across, darker in the shade
            fold = math.sin((x + belly(y)) * 45) * 0.08
            shade_k = 0.85 + 0.25 * (1 - clamp((x - 0.15) / 0.45)) + fold
            col = scale(canvas(x, y), shade_k)
            if abs(((y - 0.25) * 10) % 1 - 0.5) < 0.03:
                col = scale(col, 0.82)
            if d > -0.012:
                col = scale(col, 0.8)  # the bolt rope
        elif d < 0.006:
            col = mix(col, (0.15, 0.10, 0.06), 0.8)
        d = yard(x, y)
        if d < 0:
            k = (y - 0.235) / 0.02
            n = normalize((0.0, k * 0.8, math.sqrt(max(0.05, 1 - k * k))))
            col = shade_metal(wood(x, y, True), n, 15, 0.2)
        # the brass arrow at the right
        s = 1 if up else -1
        ay = lambda v: 0.5 + (v - 0.5) * s
        arrow_sdf = lambda px, py: sd_polygon(px, py, [(0.77, ay(0.18)), (0.95, ay(0.43)), (0.825, ay(0.43)),
                                                       (0.825, ay(0.84)), (0.715, ay(0.84)), (0.715, ay(0.43)),
                                                       (0.59, ay(0.43))])
        col = drop_shadow(col, arrow_sdf, x, y, 0.4, 0.015, 0.02, 0.03)
        d = arrow_sdf(x, y)
        if d < 0:
            col = shade_metal(BRASS, relief_normal(arrow_sdf, x, y, 0.03))
        elif d < 0.005:
            col = mix(col, (0.05, 0.03, 0.01), 0.7)
        return col

    return sample


# the sword in its scabbard

TIP = (0.15, 0.89)
MOUTH = (0.61, 0.41)
POMMEL = (0.83, 0.18)
AXLEN = math.hypot(MOUTH[0] - TIP[0], MOUTH[1] - TIP[1])
DIR = ((MOUTH[0] - TIP[0]) / AXLEN, (MOUTH[1] - TIP[1]) / AXLEN)
NRM = (-DIR[1], DIR[0])
LIGHT2 = (-0.6, -0.8)  # from the top left


def axis(x, y):
    """t along the axis from the tip (in the scabbard lengths), d across it"""
    px, py = x - TIP[0], y - TIP[1]
    return (px * DIR[0] + py * DIR[1]) / AXLEN, px * NRM[0] + py * NRM[1]


def metal(base, d, w):
    """a cylinder lit from the top left: d across in -w..w"""
    k = d / w
    side = -(NRM[0] * LIGHT2[0] + NRM[1] * LIGHT2[1]) * k
    shade = 0.55 + 0.45 * math.sqrt(max(0.0, 1 - k * k)) + 0.25 * side
    hl = max(0.0, 1 - abs(k + 0.35) * 4) ** 2 * 0.6
    return tuple(clamp(c * shade + hl) for c in base)


def scabbard(x, y):
    col, frame = background(x, y)
    if frame:
        return col
    t, d = axis(x, y)
    # the shadow on the background
    ts, ds = axis(x - 0.025, y - 0.03)
    if -0.08 <= ts <= 1.0 and abs(ds) <= 0.052 + 0.020 * clamp(ts) - (max(0.0, -ts) * 0.5):
        col = mix(col, (0.0, 0.0, 0.0), 0.45)
    # the scabbard: leather, narrower to the tip, a pointed brass chape
    if -0.08 <= t <= 1.0:
        w = 0.052 + 0.020 * clamp(t) - max(0.0, -t) * 0.5
        if abs(d) <= w:
            c = metal((0.50, 0.29, 0.15), d, w)
            brass = (0.95, 0.72, 0.30)
            # the chape, the middle band and the mouth (locket)
            if t < 0.13 or 0.50 < t < 0.56 or t > 0.88:
                c = metal(brass, d, w)
                if 0.0 < t < 0.13 and abs((t * 40) % 1 - 0.5) < 0.08:
                    c = mix(c, (0.3, 0.2, 0.05), 0.5)  # engraved lines
            elif abs(abs(d) - w * 0.72) < 0.0035:
                c = mix(c, (0.85, 0.65, 0.35), 0.55)  # the stitching
            col = c
    # the guard: a bar across the mouth with round ends
    gx, gy = MOUTH[0] + DIR[0] * 0.035, MOUTH[1] + DIR[1] * 0.035
    px, py = x - gx, y - gy
    along, across = px * DIR[0] + py * DIR[1], px * NRM[0] + py * NRM[1]
    if abs(along) <= 0.027 and abs(across) <= 0.165:
        col = metal((0.80, 0.82, 0.86), along, 0.027)
    for sgn in (-1, 1):
        ex, ey = gx + NRM[0] * 0.165 * sgn, gy + NRM[1] * 0.165 * sgn
        if math.hypot(x - ex, y - ey) <= 0.038:
            col = metal((0.80, 0.82, 0.86), x - ex - (y - ey), 0.045)
    # the grip: dark leather wound with wire, from the guard to the pommel
    hl = math.hypot(POMMEL[0] - gx, POMMEL[1] - gy)
    if 0.02 <= along <= hl and abs(across) <= 0.036:
        c = metal((0.30, 0.20, 0.13), across, 0.036)
        if abs(((along + across * 0.8) * 55) % 1 - 0.5) < 0.18:
            c = metal((0.85, 0.80, 0.65), across, 0.036)
        col = c
    # the pommel: a steel ball
    dd = math.hypot(x - POMMEL[0], y - POMMEL[1])
    if dd <= 0.060:
        k = dd / 0.060
        spec = max(0.0, 1 - math.hypot(x - POMMEL[0] + 0.022, y - POMMEL[1] + 0.024) / 0.035)
        col = tuple(clamp(c * (1.0 - 0.55 * k * k) + spec * 0.8) for c in (0.82, 0.84, 0.88))
    return col


# the sticks in the style of the compass of the world map (without the cardinal points and the star): its painted rings
# around a warm inset and a knob like its medallion; transparent around

OUTLINE_DARK = (0.16, 0.09, 0.05)


def painted(base, x, y, seed, amount=0.25):
    """a painted surface: the mottle of the compass picture"""
    return scale(base, 1 - amount / 2 + amount * fbm(x * 40, y * 40, 4, seed))


def stick_base(x, y):
    dx, dy = (x - 0.5) / 0.435, (y - 0.5) / 0.435
    r = math.hypot(dx, dy)
    angle = math.atan2(dy, dx)
    turn = angle / (2 * math.pi) % 1
    if r > 1.0:
        # the outer edge: blocks of two heights in a dark border, and a soft shadow
        high = (turn * 64) % 1 < 0.5
        edge = 1.03 if high else 0.995
        if r < edge:
            col = painted((0.93, 0.84, 0.55), x, y, 141)
            if (turn * 64) % 1 < 0.04 or (turn * 64) % 1 > 0.96:
                col = OUTLINE_DARK
            return col + (1.0,)
        if r < 1.10:
            # a dark border, a bit lighter in its middle
            return mix(OUTLINE_DARK, (0.30, 0.18, 0.10), 1 - abs(r - 1.065) / 0.035) + (1.0,)
        return (0.0, 0.0, 0.0, 0.35 * clamp(1 - (r - 1.10) / 0.04))
    if r > 0.93:
        # the scale: light blocks with dark lines between them
        k = (turn * 64) % 1
        col = painted((0.95, 0.90, 0.80) if int(turn * 64) % 2 else (0.88, 0.70, 0.58), x, y, 151, 0.15)
        if k < 0.06 or abs(r - 0.93) < 0.008:
            col = OUTLINE_DARK
        return col + (1.0,)
    if r > 0.72:
        # the band of the rhumbs: greyish brown, a dark line every 32nd of the circle
        col = painted((0.52, 0.45, 0.40), x, y, 161, 0.35)
        k = (turn * 32) % 1
        if k < 0.03 or k > 0.97 or abs(r - 0.72) < 0.009:
            col = OUTLINE_DARK
        return col + (1.0,)
    # the inset: warm painted plaster with the fine rhumb lines
    col = painted((0.83, 0.60, 0.43), x, y, 171, 0.3)
    k = (turn * 16) % 1
    if (k < 0.008 or k > 0.992) and r > 0.12:
        col = mix(col, OUTLINE_DARK, 0.45)
    # shaded under the rings
    col = scale(col, 0.75 + 0.25 * smooth(0.0, 0.1, 0.72 - r))
    return col + (0.95,)


def stick_knob(x, y):
    dx, dy = x - 0.5, y - 0.5
    r = math.hypot(dx, dy) / 0.42
    rs = math.hypot(dx - 0.03, dy - 0.04) / 0.42
    if r > 1.0:
        return (0.0, 0.0, 0.0, 0.45 * clamp(1 - (rs - 0.95) / 0.2)) if rs < 1.15 else (0.0, 0.0, 0.0, 0.0)
    # a slightly domed medallion lit from the top left
    nx, ny = dx / 0.42, dy / 0.42
    light = 1.0 - 0.25 * (nx + ny) - 0.25 * r * r
    if r > 0.89 or 0.64 < r < 0.70:
        col = OUTLINE_DARK
    elif r > 0.70:
        # the ring of the medallion, without the names of the cardinal points
        col = painted((0.86, 0.68, 0.42), x, y, 181, 0.2)
    else:
        # the sky and the sea with the curls of the waves
        sea_line = 0.08 + 0.04 * math.sin(dx * 40)
        if dy / 0.42 < sea_line:
            col = mix((0.90, 0.92, 0.85), (0.55, 0.70, 0.85), clamp((dy / 0.42 + 0.6) / 0.7))
        else:
            col = mix((0.30, 0.55, 0.80), (0.10, 0.25, 0.55), clamp((dy / 0.42 - 0.1) / 0.5))
            for wave in (0.22, 0.38):
                wy = wave + 0.035 * math.sin(dx / 0.42 * 9 + wave * 20)
                if abs(dy / 0.42 - wy) < 0.035:
                    col = (0.95, 0.95, 0.92)
        col = painted(col, x, y, 191, 0.15)
    col = scale(col, light)
    # the gloss of the varnish
    if math.hypot(nx + 0.35, ny + 0.4) < 0.18:
        col = mix(col, (1, 1, 1), 0.35 * (1 - math.hypot(nx + 0.35, ny + 0.4) / 0.18))
    return col + (1.0,)


ICONS = {
    "back": back(),
    "menu": scroll(),
    "hero": tricorn(),
    "scabbard": scabbard,
    "zoom_in": magnifier(True),
    "zoom_out": magnifier(False),
    "eye": eye(),
    "skip": skip(),
    "up": arrow(True),
    "down": arrow(False),
    "check": check(),
    "sails_up": sails(True),
    "sails_down": sails(False),
    "plate": (plate, 128),
    "lr_arrows": (lr_arrows(), 256),
    "stick_base": (stick_base, 512),
    "stick_knob": (stick_knob, 256),
}


# ---------------------------------------------------------------------------------------------------------------------

def render(sample, size):
    """the RGBA pixels; the samples are (r, g, b) or (r, g, b, a)"""
    pixels = []
    for py in range(size):
        row = []
        for px in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy in range(SS):
                for sx in range(SS):
                    c = sample((px + (sx + 0.5) / SS) / size, (py + (sy + 0.5) / SS) / size)
                    a = c[3] if len(c) > 3 else 1.0
                    for i in range(3):
                        acc[i] += c[i] * a
                    acc[3] += a
            a = acc[3] / (SS * SS)
            rgb = [acc[i] / acc[3] if acc[3] > 0 else 0.0 for i in range(3)]
            row.append(tuple(int(clamp(v) * 255 + 0.5) for v in rgb + [a]))
        pixels.append(row)
    return pixels


def write_tga(path, pixels):
    # uncompressed true colour with alpha, 32 bits, top-left origin
    size = len(pixels)
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, size, size, 32, 0x28)
    body = b"".join(bytes((b, g, r, a)) for row in pixels for (r, g, b, a) in row)
    with open(path, "wb") as f:
        f.write(header + body)


def write_png(path, pixels, factor=3):
    """a preview on the background of the buttons"""
    size = len(pixels)
    back = (46, 48, 80)

    def over(p):
        a = p[3] / 255
        return bytes(int(p[i] * a + back[i] * (1 - a)) for i in range(3))

    raw = b"".join(b"\0" + b"".join(over(p) for p in row for _ in range(factor)) for row in pixels
                   for _ in range(factor))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    n = size * factor
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    folder = os.path.join(sys.argv[1], "RESOURCE", "Textures", "INTERFACES")
    names = sys.argv[3:] or list(ICONS)  # the names to draw (all by default)
    for name in names:
        sample, size = ICONS[name] if isinstance(ICONS[name], tuple) else (ICONS[name], SIZE)
        image = render(sample, size)
        write_tga(os.path.join(folder, "touch_" + name + ".tga"), image)
        if len(sys.argv) > 2 and sys.argv[2]:
            write_png(os.path.join(sys.argv[2], "touch_" + name + ".png"), image, 3 if size <= 128 else 1)
