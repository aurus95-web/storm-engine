#!/usr/bin/env python3
"""The pictures of the battle interface on the touch buttons and the commands with the background of the icons of
the items (the plate of touch_icons.py, as the potion of the cure has): in the textures of the commands and the
actions the slate background of the normal pictures (the left half; the right one is the selected pictures, their
orange glow stays) is cut out; the cannon of Cannons ("Broadside") is transparent around.

usage: game_icons.py <game folder> [<preview folder>]
writes <game folder>/RESOURCE/Textures/touch/<the path of the texture> (the engine loads them instead of the
textures, DX9RENDER::TextureCreate) and RESOURCE/Textures/INTERFACES/touch_pic_broadside.tga. The game files are
only read.
"""

import os
import struct
import sys
import zlib

import touch_icons as ti

SIZE = 128


def find(folder, relative):
    """the path of the game file, the case of the names ignored"""
    path = folder
    for part in relative.replace("\\", "/").split("/"):
        names = {n.lower(): n for n in os.listdir(path)}
        if part.lower() not in names:
            sys.exit("not found: " + os.path.join(path, part))
        path = os.path.join(path, names[part.lower()])
    return path


def read_tx(path):
    """the first mip of a .tx texture: rows of (r, g, b, a) from 0 to 1"""
    data = open(path, "rb").read()
    _, w, h, _, fmt, _ = struct.unpack("<6i", data[:24])
    px = data[24:]

    def pixel(i):
        if fmt == 21:  # A8R8G8B8
            b, g, r, a = px[i * 4:i * 4 + 4]
            return r / 255, g / 255, b / 255, a / 255
        v = struct.unpack_from("<H", px, i * 2)[0]
        if fmt == 23:  # R5G6B5
            return ((v >> 11) & 31) / 31, ((v >> 5) & 63) / 63, (v & 31) / 31, 1.0
        if fmt == 25:  # A1R5G5B5
            return ((v >> 10) & 31) / 31, ((v >> 5) & 31) / 31, (v & 31) / 31, float(v >> 15)
        if fmt == 26:  # A4R4G4B4
            return ((v >> 8) & 15) / 15, ((v >> 4) & 15) / 15, (v & 15) / 15, (v >> 12) / 15
        sys.exit("unsupported format %d of %s" % (fmt, path))

    return [[pixel(y * w + x) for x in range(w)] for y in range(h)]


def cell(img, cols, rows, index):
    h, w = len(img), len(img[0])
    cw, ch = w // cols, h // rows
    x0, y0 = (index % cols) * cw, (index // cols) * ch
    return [row[x0:x0 + cw] for row in img[y0:y0 + ch]]


def sample(img, x, y):
    """bilinear, x and y from 0 to 1"""
    h, w = len(img), len(img[0])
    fx, fy = min(max(x * w - 0.5, 0), w - 1), min(max(y * h - 0.5, 0), h - 1)
    x0, y0 = int(fx), int(fy)
    x1, y1 = min(x0 + 1, w - 1), min(y0 + 1, h - 1)
    tx, ty = fx - x0, fy - y0
    top = [a + (b - a) * tx for a, b in zip(img[y0][x0], img[y0][x1])]
    bottom = [a + (b - a) * tx for a, b in zip(img[y1][x0], img[y1][x1])]
    return [a + (b - a) * ty for a, b in zip(top, bottom)]


def smoothstep(a, b, v):
    t = min(max((v - a) / (b - a), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def compose(fn):
    """the 128x128 pixels of fn(x, y, plate colour) -> (r, g, b) from 0 to 1"""
    pixels = []
    for py in range(SIZE):
        row = []
        for px in range(SIZE):
            x, y = (px + 0.5) / SIZE, (py + 0.5) / SIZE
            c = fn(x, y, ti.plate(x, y))
            row.append(tuple(int(round(min(max(v, 0.0), 1.0) * 255)) for v in c) + (255,))
        pixels.append(row)
    return pixels


def broadside(folder):
    """the cannon of Cannons (8x1, the first one), transparent around"""
    img = cell(read_tx(find(folder, "RESOURCE/Textures/INTERFACES/Cannons.tga.tx")), 8, 1, 0)

    def fn(x, y, plate):
        r, g, b, a = sample(img, x, y)
        return [p + (c - p) * a for c, p in zip((r, g, b), plate)]

    return compose(fn)


PICTURES = {"broadside": broadside}

# the textures of the commands (scripts: CommandTextures, ActiveActions): the columns and the rows of the normal
# pictures, whether the selected ones follow them in the right half
TEXTURES = [
    ("battle_interface/LandCommands.tga", 2, 4, True),
    ("battle_interface/LandCommands2.tga", 1, 1, True),
    ("battle_interface/LandTarget4.tga", 4, 1, True),
    ("battle_interface/LandTarget1.tga", 1, 1, True),
    ("battle_interface/Cancel.tga", 1, 1, True),
    ("battle_interface/land_fast_commands.tga", 8, 1, False),
    ("battle_interface/cicons_command.tga", 4, 4, True),
    ("battle_interface/cicons_charge.tga", 1, 4, True),
    ("battle_interface/cicons_locators.tga", 4, 1, True),
    ("battle_interface/SailControls.tga", 4, 1, True),
    ("battle_interface/sea_abilitys.tga", 4, 2, True),
    ("battle_interface/ships_16.tga", 4, 4, True),
    ("battle_interface/ships_1.tga", 1, 1, True),
    ("battle_interface/ships_2.tga", 1, 2, True),
    ("WorldMap/Interfaces/BigIcons.tga", 8, 1, False),
]
# the portraits of the characters keep their golden frame
PORTRAITS = "INTERFACES/PORTRAITS/64"


def gold_frame(u, v):
    """the golden frame of the icons of the items: (colour, coverage) or None inside"""
    edge = min(u, v, 1 - u, 1 - v)
    if edge < 0.035:
        k = ti.math.sin(edge / 0.035 * ti.math.pi)
        return ti.mix((0.45, 0.32, 0.10), (0.92, 0.75, 0.35), k)
    return None


def cut_background(img, cols, rows, selected, keep_frame=False):
    """the pixels (0..255) of the texture: in each normal picture the background (the median of a ring inside its
    frame) is replaced by the plate in the golden frame of the items (or its own frame is kept)"""
    h, w = len(img), len(img[0])
    total_cols = cols * (2 if selected else 1)
    cw, ch = w // total_cols, h // rows
    out = [[tuple(int(round(v * 255)) for v in p) for p in row] for row in img]
    for r in range(rows):
        for c in range(cols):
            x0, y0 = c * cw, r * ch
            pic = [row[x0:x0 + cw] for row in img[y0:y0 + ch]]
            n = min(cw, ch)
            ring = [pic[y][x] for y in range(ch) for x in range(cw)
                    if 0.06 * n <= min(x, y, cw - 1 - x, ch - 1 - y) <= 0.12 * n]
            bg = [sorted(p[i] for p in ring)[len(ring) // 2] for i in range(3)]
            # a picture over all the cell (the eye, the lips of the talk) has no background to cut out: little of the
            # ring is of the colour of the background
            near = sum(1 for p in ring if sum((p[i] - bg[i]) ** 2 for i in range(3)) ** 0.5 < 0.08)
            if near < 0.35 * len(ring):
                # only the golden frame, as the others have
                if not keep_frame:
                    for y in range(ch):
                        for x in range(cw):
                            frame = gold_frame((x + 0.5) / cw, (y + 0.5) / ch)
                            if frame:
                                out[y0 + y][x0 + x] = tuple(int(round(c * 255)) for c in frame) + (255,)
                continue
            for y in range(ch):
                for x in range(cw):
                    u, v = (x + 0.5) / cw, (y + 0.5) / ch
                    p = pic[y][x]
                    if keep_frame and min(u, v, 1 - u, 1 - v) < 0.07:
                        continue
                    frame = None if keep_frame else gold_frame(u, v)
                    if frame:
                        out[y0 + y][x0 + x] = tuple(int(round(c * 255)) for c in frame) + (255,)
                        continue
                    dist = sum((p[i] - bg[i]) ** 2 for i in range(3)) ** 0.5
                    a = smoothstep(0.1, 0.25, dist) * smoothstep(0.035, 0.045, min(u, v, 1 - u, 1 - v)) * p[3]
                    plate = ti.plate(u, v)
                    out[y0 + y][x0 + x] = tuple(
                        int(round(min(max(plate[i] + (p[i] - plate[i]) * a, 0.0), 1.0) * 255)) for i in range(3)) + (255,)
    return out


def portrait(img):
    """a portrait: its dark blue background (bluer than the shadows of the faces and the hair), where it joins the top
    and the sides above the shoulders, becomes the plate; the golden frame stays"""
    n = len(img)
    blue = [[smoothstep(0.03, 0.1, p[2] - max(p[0], p[1])) for p in row] for row in img]
    # the background joined to the edges of the upper part (not the blue clothes below)
    inside = [[False] * n for _ in range(n)]
    stack = [(x, y) for y in range(n) for x in range(n)
             if 0.07 * n <= min(x, y, n - 1 - x) <= 0.09 * n and y < 0.6 * n and blue[y][x] > 0.5]
    while stack:
        x, y = stack.pop()
        if inside[y][x] or blue[y][x] <= 0.5 or min(x, y, n - 1 - x, n - 1 - y) < 0.07 * n:
            continue
        inside[y][x] = True
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    out = []
    for y in range(n):
        row = []
        for x in range(n):
            p = img[y][x]
            a = 0.0
            if not min(x, y, n - 1 - x, n - 1 - y) < 0.07 * n:
                # the soft edge: the blue pixels next to the background too
                near = any(inside[yy][xx] for yy in range(max(y - 1, 0), min(y + 2, n))
                           for xx in range(max(x - 1, 0), min(x + 2, n)))
                a = blue[y][x] if near else 0.0
            plate = ti.plate((x + 0.5) / n, (y + 0.5) / n)
            row.append(tuple(int(round(min(max(p[i] + (plate[i] - p[i]) * a, 0.0), 1.0) * 255)) for i in range(3)) +
                       (255,))
        out.append(row)
    return out


def write_tga(path, pixels):
    """uncompressed 32 bits, top-left origin, any size"""
    h, w = len(pixels), len(pixels[0])
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, 0x28)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(header + b"".join(bytes((b, g, r, a)) for row in pixels for (r, g, b, a) in row))


def write_preview(path, pixels):
    h, w = len(pixels), len(pixels[0])
    raw = b"".join(b"\0" + b"".join(bytes(p[:3]) for p in row) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))

if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = os.path.join(sys.argv[1], "RESOURCE", "Textures", "INTERFACES")
    portraits = find(sys.argv[1], "RESOURCE/Textures/" + PORTRAITS)
    for name in sorted(os.listdir(portraits)):
        if name.lower().startswith("face_") and name.lower().endswith(".tga.tx"):
            pixels = portrait(read_tx(os.path.join(portraits, name)))
            write_tga(os.path.join(sys.argv[1], "RESOURCE", "Textures", "touch", *PORTRAITS.split("/"), name[:-3]),
                      pixels)
    for texture, cols, rows, selected in TEXTURES:
        pixels = cut_background(read_tx(find(sys.argv[1], "RESOURCE/Textures/" + texture + ".tx")), cols, rows, selected)
        write_tga(os.path.join(sys.argv[1], "RESOURCE", "Textures", "touch", *texture.split("/")), pixels)
        if len(sys.argv) > 2:
            write_preview(os.path.join(sys.argv[2], "touch_" + os.path.basename(texture) + ".png"), pixels)
    for name, make in PICTURES.items():
        pixels = make(sys.argv[1])
        ti.write_tga(os.path.join(out, "touch_pic_" + name + ".tga"), pixels)
        if len(sys.argv) > 2:
            ti.write_png(os.path.join(sys.argv[2], "touch_pic_" + name + ".png"), pixels, 2)
