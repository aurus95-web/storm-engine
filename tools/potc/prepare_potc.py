#!/usr/bin/env python3
"""Prepares the files of Pirates of the Caribbean (Корсары 2, 2003) for this engine (the port to the iPad and the Mac).

usage: prepare_potc.py <game folder> <Beyond New Horizons folder> [--screen=WxH] [--icons]

The game folder is changed in place (make a copy first):
- the texts of the scripts and of the interface (PROGRAM, RESOURCE/INI, RESOURCE/QUESTSTEXT) are recoded from
  Windows-1251 to UTF-8 (the engine reads UTF-8; the files that are UTF-8 already are kept);
- the files the engine needs and Pirates of the Caribbean does not have (the waves of the sea, the particles, a few
  textures of the sea and the world map) are copied from Beyond New Horizons, a free mod of the same game
  (https://cmdrhammie.itch.io/beyond-new-horizons), only the missing ones;
- the skies are unpacked (tools/potc-skybox-converter: sky_side -> sky_fr/lf/bk/rt);
- engine.ini for the iPad is written (the old one is kept as engine.ini.orig);
- with --icons the pictures of the touch buttons are drawn (tools/touch-icons).
Python 3 only, no other packages. The videos are converted separately: tools/video/convert-videos.sh (ffmpeg).
"""
import os
import shutil
import subprocess
import sys

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# the files and folders of Beyond New Horizons copied when the game has none of them (paths in RESOURCE)
BNH_FILES = [
    "Sea",
    "Particles",
    "Textures/Particles_list.tga.tx",
    "Textures/Particles_listnm.tga.tx",
    "Textures/sealight.tga.tx",
    "Textures/SeaTrash.tga.tx",
    "Textures/WorldMap/StormLight.tga.tx",
    "Textures/WorldMap/Lightnings.tga.tx",
    "Textures/WorldMap/CloudsLight.tga.tx",
    "Textures/WorldMap/rain.tga.tx",
    "Textures/WorldMap/Interfaces",
    "Textures/WorldMap/Sea/flare.tga.tx",
    "Textures/LocEfx/state_bars.tga.tx",
    "Textures/ShipsFlares/corona.tga.tx",
    "Textures/ShipsFlares/FlareSunroad.tga.tx",
    "Textures/WEATHER/sea_rain_drops.tga.tx",
    "Textures/WEATHER/rain_drops.tga.tx",
]
# the particles under the names of Pirates of the Caribbean: (its name, the one of Beyond New Horizons)
PARTICLE_COPIES = [
    ("cancloud.xps", "cancloud_stock.xps"),
    ("fire_mix.xps", "Fire.xps"),
    ("waterfog_fall.xps", "waterfog.xps"),
    ("stars.xps", "signstar.xps"),
]
TEXT_FOLDERS = ["PROGRAM", "RESOURCE/INI", "RESOURCE/QUESTSTEXT"]
TEXT_EXTENSIONS = (".c", ".h", ".ini", ".txt")

ENGINE_INI = """modules path = modules\\
full_screen = 0
logs_folder = .
screen_x = {width}
screen_y = {height}
msaa = 4
max_fps = 60
lockable_back_buffer = 0
screen_bpp = D3DFMT_X8R8G8B8
texture_degradation = 0
controls = pcs_controls
program_directory = PROGRAM
run = seadogs.c
show_fps = 0
safe_render = 1
texture_log = 0
geometry_log = 0
tracefilesoff = 0
mem_profile = memory.mp
startFontIniFile = resource\\ini\\fonts.ini
font = interface_normal
numoftips = 0

[compatibility]
target_version = potc

[script]
debuginfo = 1
codefiles = 0
runtimelog = 0

[sound]
sound path = resource\\sounds\\

[stats]
memory_stats = 0
update_mem_profile = 0

[controls]
mouse_move_scale = 2
"""


def find(base, relative):
    """The path of relative in base whatever the case of its names (the game was made for Windows), or None."""
    path = base
    for name in relative.replace("\\", "/").split("/"):
        if not os.path.isdir(path):
            return None
        match = next((n for n in os.listdir(path) if n.lower() == name.lower()), None)
        if match is None:
            return None
        path = os.path.join(path, match)
    return path


def target(base, relative):
    """The path of relative in base: the existing folders whatever their case, the rest as given."""
    path = base
    parts = relative.replace("\\", "/").split("/")
    for i, name in enumerate(parts):
        match = None
        if os.path.isdir(path):
            match = next((n for n in os.listdir(path) if n.lower() == name.lower()), None)
        if match is None:
            return os.path.join(path, *parts[i:])
        path = os.path.join(path, match)
    return path


def recode_texts(game):
    recoded = 0
    for folder in TEXT_FOLDERS:
        root = find(game, folder)
        if not root:
            continue
        for directory, _, files in os.walk(root):
            for name in files:
                if not name.lower().endswith(TEXT_EXTENSIONS):
                    continue
                path = os.path.join(directory, name)
                with open(path, "rb") as f:
                    data = f.read()
                try:
                    data.decode("utf-8")
                    continue
                except UnicodeDecodeError:
                    pass
                with open(path, "wb") as f:
                    f.write(data.decode("cp1251").encode("utf-8"))
                recoded += 1
    print(f"texts recoded to UTF-8: {recoded}")


def copy_missing(source, destination):
    """Copies the file or the folder source to destination, only the files destination has not (whatever the case)."""
    copied = 0
    if os.path.isfile(source):
        if find(os.path.dirname(destination), os.path.basename(destination)) is None:
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            shutil.copy2(source, destination)
            copied += 1
        return copied
    for directory, _, files in os.walk(source):
        relative = os.path.relpath(directory, source)
        for name in files:
            rel = name if relative == "." else os.path.join(relative, name)
            if find(destination, rel) is None:
                path = target(destination, rel)
                os.makedirs(os.path.dirname(path), exist_ok=True)
                shutil.copy2(os.path.join(directory, name), path)
                copied += 1
    return copied


def copy_bnh(game, bnh):
    game_resource = find(game, "RESOURCE")
    bnh_resource = find(bnh, "RESOURCE")
    if not bnh_resource:
        sys.exit(f"no RESOURCE folder in {bnh}: is it Beyond New Horizons?")
    copied = 0
    for relative in BNH_FILES:
        source = find(bnh_resource, relative)
        if source is None:
            print(f"not in Beyond New Horizons: {relative}")
            continue
        copied += copy_missing(source, target(game_resource, relative))
    particles = find(game_resource, "Particles")
    for name, original in PARTICLE_COPIES:
        source = find(particles, original) if particles else None
        if source and find(particles, name) is None:
            shutil.copy2(source, os.path.join(particles, name))
            copied += 1
    print(f"files copied from Beyond New Horizons: {copied}")


def unpack_skies(game):
    skies = find(game, "RESOURCE/Textures/WEATHER/SKIES")
    if not skies:
        print("no skies: RESOURCE/Textures/WEATHER/SKIES")
        return
    sys.path.insert(0, os.path.join(TOOLS, "potc-skybox-converter"))
    import sky_convert

    unpacked = 0
    for directory, _, files in os.walk(skies):
        side = next((n for n in files if n.lower() == "sky_side.tga.tx"), None)
        if side and not any(n.lower() == "sky_fr.tga.tx" for n in files):
            sky_convert.convert(os.path.join(directory, side))
            unpacked += 1
    print(f"skies unpacked: {unpacked}")


def write_engine_ini(game, width, height):
    path = find(game, "engine.ini") or os.path.join(game, "engine.ini")
    if os.path.exists(path) and not os.path.exists(path + ".orig"):
        shutil.copy2(path, path + ".orig")
    with open(path, "w", newline="\r\n") as f:
        f.write(ENGINE_INI.format(width=width, height=height))
    print(f"engine.ini written: {width}x{height}")


def draw_icons(game):
    for script in ("touch_icons.py", "game_icons.py"):
        subprocess.run([sys.executable, os.path.join(TOOLS, "touch-icons", script), game], check=True)
    print("pictures of the touch buttons drawn")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        sys.exit(__doc__)
    game, bnh = args
    if not find(game, "PROGRAM") or not find(game, "RESOURCE"):
        sys.exit(f"no PROGRAM and RESOURCE folders in {game}: is it the folder of the game?")
    width, height = 1366, 1024
    for a in sys.argv[1:]:
        if a.startswith("--screen="):
            width, height = (int(v) for v in a.split("=", 1)[1].lower().split("x"))
    recode_texts(game)
    copy_bnh(game, bnh)
    unpack_skies(game)
    write_engine_ini(game, width, height)
    if "--icons" in sys.argv[1:]:
        draw_icons(game)


if __name__ == "__main__":
    main()
