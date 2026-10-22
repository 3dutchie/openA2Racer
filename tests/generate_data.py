#!/usr/bin/env python3
"""Generate wholly procedural game-format fixtures for OpenA2 application tests.

No original game file is read, copied, or required. The resulting coloured cubes,
checkerboards, tones and fictional route are test data, not replacement game
content. Uses only the Python standard library.

Usage: python3 tests/generate_data.py build/synthetic-data
       opena2 --data build/synthetic-data --headless --demo --frames 240
"""
from pathlib import Path
import argparse
import io
import math
import struct
import wave

MARKER = "OPENA2_SYNTHETIC_FIXTURE.txt"
OBJECTS = [
    ("sky", "Sky"), ("bovw", "50"), ("arena", "Arena"),
    ("viaduct", "VIADUCT"), ("molent", "MOLEN"), ("shell", "Shell"),
    ("roadsgn", "ROADSGN"), ("uit", "UIT"), ("zaltboml", "zaltboml"),
    ("lekbrug", "Lekbrug"), ("gebouwg", "GEBOUW"), ("gebouw2g", "TILE"),
    ("gebouw3g", "TILE2"), ("trees", "Trees"), ("wegresg2", "wegresg2"),
    ("elek4", "elek4"), ("huisje", "huisje"), ("rijtjes", "rijtjes"),
    ("werkweg", "werkweg"), ("werkweg2", "werkweg2"), ("flits", "flits"),
    ("praatp", "Praatp"), ("start", "Start"), ("finish", "finish"),
    ("geld", "Geld"), ("benzine", "BENZine"), ("tijd", "Tijd"),
    ("lantaarn", "LANTAARN"), ("eindh", "eindh"), ("amsterd", "Amsterd"),
    ("denbosch", "Denbosch"), ("maastr", "maastr"), ("utrecht", "Utrecht"),
    ("s_maastr", "s_maastr"), ("s_utr", "s_utr"), ("s_denb", "s_denb"),
    ("s_adam", "s_adam"), ("s_eindho", "s_eindho"),
    *[("number" + str(n), "number" + str(n)) for n in range(1, 7)],
    ("hek", "hek"), ("koe", "koe"), ("100", "100"), ("babe", "babe"),
]
VEHICLES = [
    ("porche", "porche"), ("camaro", "camaro"), ("bmw", "bmw"),
    ("jaguar", "jaguar"), ("lotus", "lotus"), ("viper", "viper"),
    ("police", "police"), ("golf", "golf"), ("panda", "panda"),
    ("cx", "cx"), ("merceds", "mercedes"), ("van", "vanmap"),
    ("vracht", "vracht"), ("interl", "il"), ("volvo", "volvo"),
]
SOUNDS = [
    "motor", "sirene", "speed", "energy", "jammin", "moody", "disco", "rock",
    "gearup", "geardwn", "set", "go", "mstart", "bump", "bump2",
    "sp_p1", "sp_b1", "sp_b2", "sp_b3", "bonusg", "bonusb", "bonust", "finish", "loser",
]
MENUS = ["hoofd", "OPTIES", "Bedien", "Nspel_1", "Tstand"]


def rle(row):
    encoded = bytearray()
    pos = 0
    while pos < len(row):
        count = 1
        while pos + count < len(row) and row[pos + count] == row[pos] and count < 63:
            count += 1
        value = row[pos]
        if count > 1 or value >= 192:
            encoded.extend((192 | count, value))
        else:
            encoded.append(value)
        pos += count
    return encoded


def pcx(width=16, height=16, seed=0):
    header = bytearray(128)
    header[:4] = bytes((10, 5, 1, 8))
    struct.pack_into("<4H", header, 4, 0, 0, width - 1, height - 1)
    struct.pack_into("<2H", header, 12, width, height)
    header[65] = 1
    stride = (width + 1) & ~1
    struct.pack_into("<2H", header, 66, stride, 1)
    pixels = bytearray()
    for y in range(height):
        row = bytes(1 + ((x // 8 + y // 8 + seed) % 6) for x in range(width))
        pixels.extend(rle(row + bytes(stride - width)))
    palette = bytearray(768)
    colours = [(0, 0, 0), (215, 70, 75), (65, 180, 115), (70, 130, 215),
               (225, 180, 70), (145, 95, 195), (80, 195, 200)]
    for index, colour in enumerate(colours):
        palette[index * 3:index * 3 + 3] = bytes(colour)
    return bytes(header + pixels + bytes((12,)) + palette)


def mesh():
    vertices = [(-5, 0, -11), (5, 0, -11), (5, 7, -11), (-5, 7, -11),
                (-5, 0, 11), (5, 0, 11), (5, 7, 11), (-5, 7, 11)]
    triangles = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7),
                 (0, 1, 5), (0, 5, 4), (3, 7, 6), (3, 6, 2),
                 (0, 4, 7), (0, 7, 3), (1, 2, 6), (1, 6, 5)]
    data = bytearray(struct.pack("<II", len(vertices), len(triangles)))
    for index, position in enumerate(vertices):
        data.extend(struct.pack("<8fB", *position, 0, 1, 0,
                                float(index & 1), float((index >> 1) & 1), 1))
    for triangle in triangles:
        data.extend(struct.pack("<3H3B", *triangle, 0, 7, 1))
    return bytes(data)


def sound(index):
    out = io.BytesIO()
    with wave.open(out, "wb") as wav:
        wav.setparams((1, 1, 11025, 0, "NONE", "not compressed"))
        wav.writeframes(bytes(int(128 + 24 * math.sin(i * (220 + index * 11) * 2 * math.pi / 11025))
                              for i in range(11025 // 8)))
    return out.getvalue()


def archive(destination, stem, entries):
    index = bytearray(struct.pack("<H", len(entries)))
    blob = bytearray()
    for name, data in sorted(entries.items()):
        raw_name = name.encode("ascii")
        if len(raw_name) > 12:
            raise ValueError("Fixture archive name exceeds 12 bytes: " + name)
        index.extend(raw_name.ljust(12, b"\0") + bytes(8) + struct.pack("<I", len(blob)))
        blob.extend(data)
    (destination / (stem + ".IND")).write_bytes(index)
    (destination / (stem + ".IMG")).write_bytes(blob)


def sprites(destination, stem):
    count = 2
    blob = bytearray(struct.pack("<H", count))
    index = bytearray(struct.pack("<H", count))
    for number in range(count):
        index.extend(struct.pack("<I", len(blob)))
        blob.extend(struct.pack("<4H", 8, 8, 0, 0))
        blob.extend(bytes(1 + (i // 8 + i % 8 + number) % 6 for i in range(64)))
    (destination / (stem + ".IND")).write_bytes(index)
    (destination / (stem + ".IMG")).write_bytes(blob)


def track(destination, number):
    count = 32
    data = bytearray(struct.pack("<I", count))
    for point in range(count):
        fields = [0, 0, 11] + [8 * ((lane + point // 4) % 7) for lane in range(22)]
        data.extend(struct.pack("<2f25I", 400 + math.sin(point * .13) * 100,
                                point * 165, *fields))
    placements = [(4, 0, 1, 16, 110), (8, 0, 2, 17, 120),
                  (15, 0, 1, 13, 150), (20, 1, 2, 27, 100)]
    objects = bytearray(struct.pack("<I", len(placements)))
    for placement in placements:
        objects.extend(struct.pack("<4If", *placement))
    (destination / ("ETAPPE%d.TRK" % number)).write_bytes(data)
    (destination / ("ETAPPE%d.OMP" % number)).write_bytes(objects)


def generate(destination):
    if destination.exists() and any(destination.iterdir()) and not (destination / MARKER).is_file():
        raise ValueError("Refusing to overwrite a non-fixture directory: " + str(destination))
    destination.mkdir(parents=True, exist_ok=True)
    (destination / MARKER).write_text(
        "OpenA2 procedural test data. No original game assets.\n"
        "Generated from tests/generate_data.py; safe to regenerate.\n", encoding="utf8")
    pairs = OBJECTS + VEHICLES
    data = {model + ".d3d": mesh() for model, _ in pairs}
    data.update({name + ".pcx": pcx(seed=i) for i, (_, name) in enumerate(pairs)})
    data.update({name + ".pcx": pcx(seed=i) for i, name in enumerate(
        ["grass2", "GRASS", "asfalt", "2baans", "SURSCRN"])})
    data.update({name + ".wav": sound(i) for i, name in enumerate(SOUNDS)})
    archive(destination, "RCS", data)
    archive(destination, "A2MRCS", {name + ".pcx": pcx(640, 480, i) for i, name in enumerate(MENUS)})
    for stem in ["A2MRC", "GAMESPR"]:
        sprites(destination, stem)
    for number in range(1, 4):
        track(destination, number)
    print("Generated independent procedural fixture:", destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    options = parser.parse_args()
    generate(options.destination)
