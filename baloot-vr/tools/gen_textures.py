#!/usr/bin/env python3
"""Procedurally generates every bitmap used by Baloot VR.

Cards (32 faces + back), the majlis carpet, engraved table top, fabrics,
glow sprites and the flat UI icons are all drawn here so the project has no
third-party art. Run from the project root:  python3 tools/gen_textures.py
"""
import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
CARDS = ROOT / "assets" / "cards"
TEX = ROOT / "assets" / "textures"
ICONS = ROOT / "assets" / "icons"
for d in (CARDS, TEX, ICONS):
    d.mkdir(parents=True, exist_ok=True)

SS = 2  # supersampling factor
W, H = 320, 448
RADIUS = 26
FONT_DIR = Path("/usr/share/fonts/truetype/dejavu")
SERIF_B = str(FONT_DIR / "DejaVuSerif-Bold.ttf")

RED = (196, 22, 42)
BLACK = (22, 22, 26)
GOLD = (201, 164, 92)
GOLD_LIGHT = (236, 206, 140)
IVORY = (252, 249, 242)
SKIN = (244, 214, 184)

SUITS = {"S": BLACK, "H": RED, "D": RED, "C": BLACK}
RANKS = ["7", "8", "9", "10", "J", "Q", "K", "A"]


# ---------------------------------------------------------------- suit shapes
def _heart(n=90):
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((x / 34.0, -y / 34.0 + 0.06))
    return [pts]


def _circle(cx, cy, r, n=48):
    return [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]


def _diamond():
    pts = []
    corners = [(0, -0.5), (0.38, 0), (0, 0.5), (-0.38, 0)]
    for i in range(4):
        a, b = corners[i], corners[(i + 1) % 4]
        for k in range(12):
            t = k / 12
            x = a[0] + (b[0] - a[0]) * t
            y = a[1] + (b[1] - a[1]) * t
            bow = math.sin(math.pi * t) * 0.045  # slightly concave sides
            x -= math.copysign(bow, x) if abs(x) > 1e-6 else 0
            y -= math.copysign(bow, y) if abs(y) > 1e-6 else 0
            pts.append((x, y))
    return [pts]


def _spade():
    heart = [(x, -y + 0.02) for (x, y) in _heart()[0]]
    heart = [(x, y - 0.08) for (x, y) in heart]
    stem = [(-0.04, 0.12), (0.04, 0.12), (0.13, 0.5), (-0.13, 0.5)]
    return [heart, stem]


def _club():
    r = 0.19
    return [_circle(0, -0.24, r), _circle(-0.22, 0.06, r), _circle(0.22, 0.06, r),
            _circle(0, -0.02, 0.12), [(-0.04, 0.05), (0.04, 0.05), (0.14, 0.5), (-0.14, 0.5)]]


SHAPES = {"H": _heart(), "D": _diamond(), "S": _spade(), "C": _club()}


def draw_suit(d, cx, cy, size, suit, color=None, flip=False):
    color = color or SUITS[suit]
    for poly in SHAPES[suit]:
        pts = []
        for (x, y) in poly:
            if flip:
                x, y = -x, -y
            pts.append((cx + x * size, cy + y * size))
        d.polygon(pts, fill=color)


def rounded_mask(w, h, r):
    m = Image.new("L", (w, h), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, w - 1, h - 1), r, fill=255)
    return m


def finish(img, path, w=W, h=H):
    img = img.resize((w, h), Image.LANCZOS)
    img.save(path, optimize=True)


# ---------------------------------------------------------------- card faces
def draw_index(img, rank, suit):
    s = SS
    d = ImageDraw.Draw(img)
    color = SUITS[suit]
    font = ImageFont.truetype(SERIF_B, (40 if rank == "10" else 50) * s)
    x0, y0 = 30 * s, 16 * s
    bbox = d.textbbox((0, 0), rank, font=font)
    tw = bbox[2] - bbox[0]
    d.text((x0 - tw / 2 - bbox[0], y0 - bbox[1] + 4 * s), rank, font=font, fill=color)
    draw_suit(d, x0, y0 + 80 * s, 34 * s, suit)
    # rotated copy in the opposite corner
    corner = img.crop((0, 0, 64 * s, 124 * s)).rotate(180)
    mask = corner.split()[3]
    img.paste(corner, (W * s - 64 * s, H * s - 124 * s), mask)


PIP_LAYOUT = {
    "7": [(0, 0), (1, 0), (0, .5), (1, .5), (0, 1), (1, 1), (.5, .25)],
    "8": [(0, 0), (1, 0), (0, .5), (1, .5), (0, 1), (1, 1), (.5, .25), (.5, .75)],
    "9": [(0, 0), (1, 0), (0, 1 / 3), (1, 1 / 3), (0, 2 / 3), (1, 2 / 3), (0, 1), (1, 1), (.5, .5)],
    "10": [(0, 0), (1, 0), (0, 1 / 3), (1, 1 / 3), (0, 2 / 3), (1, 2 / 3), (0, 1), (1, 1), (.5, 1 / 6), (.5, 5 / 6)],
}


def draw_pips(img, rank, suit):
    s = SS
    d = ImageDraw.Draw(img)
    x0, x1 = 0.30 * W * s, 0.70 * W * s
    y0, y1 = 0.19 * H * s, 0.81 * H * s
    for (u, v) in PIP_LAYOUT[rank]:
        cx = x0 + (x1 - x0) * u
        cy = y0 + (y1 - y0) * v
        draw_suit(d, cx, cy, 62 * s, suit, flip=v > 0.55)


def draw_ace(img, suit):
    s = SS
    d = ImageDraw.Draw(img)
    cx, cy = W * s / 2, H * s / 2
    # decorative ring behind the ace
    d.ellipse((cx - 92 * s, cy - 92 * s, cx + 92 * s, cy + 92 * s), outline=GOLD + (255,), width=3 * s)
    d.ellipse((cx - 84 * s, cy - 84 * s, cx + 84 * s, cy + 84 * s), outline=GOLD + (140,), width=1 * s)
    for i in range(16):
        a = i * math.pi / 8
        r0, r1 = 92 * s, (104 if i % 2 == 0 else 98) * s
        d.line((cx + r0 * math.cos(a), cy + r0 * math.sin(a), cx + r1 * math.cos(a), cy + r1 * math.sin(a)), fill=GOLD, width=2 * s)
    draw_suit(d, cx, cy, 130 * s, suit)


COURT_PALETTE = {
    "H": [(196, 22, 42), (36, 64, 140), (226, 178, 60)],
    "D": [(196, 22, 42), (226, 178, 60), (30, 110, 90)],
    "S": [(36, 64, 140), (196, 22, 42), (226, 178, 60)],
    "C": [(30, 30, 36), (196, 22, 42), (226, 178, 60)],
}


def draw_court(img, rank, suit):
    s = SS
    fx0, fx1 = int(0.19 * W * s), int(0.81 * W * s)
    fy0, fy1 = int(0.12 * H * s), int(0.88 * H * s)
    fw, fh = fx1 - fx0, fy1 - fy0
    half_h = fh // 2
    pal = COURT_PALETTE[suit]
    main, second, gold = pal
    half = Image.new("RGBA", (fw, half_h), (250, 244, 228, 255))
    d = ImageDraw.Draw(half)
    # background lattice
    for i in range(-fw, fw * 2, 18 * s):
        d.line((i, 0, i + half_h, half_h), fill=(236, 226, 204), width=1 * s)
        d.line((i, half_h, i + half_h, 0), fill=(236, 226, 204), width=1 * s)
    cx = fw / 2
    # item (sword / flower / staff) on the left side
    if rank == "K":
        d.rectangle((cx - 0.36 * fw, 0.08 * half_h, cx - 0.33 * fw, half_h), fill=(170, 176, 186))
        d.rectangle((cx - 0.42 * fw, 0.62 * half_h, cx - 0.27 * fw, 0.66 * half_h), fill=gold)
    elif rank == "Q":
        d.line((cx - 0.30 * fw, 0.35 * half_h, cx - 0.22 * fw, half_h), fill=(40, 110, 60), width=3 * s)
        for k in range(6):
            a = k * math.pi / 3
            d.ellipse((cx - 0.30 * fw + 14 * s * math.cos(a) - 9 * s, 0.33 * half_h + 14 * s * math.sin(a) - 9 * s,
                       cx - 0.30 * fw + 14 * s * math.cos(a) + 9 * s, 0.33 * half_h + 14 * s * math.sin(a) + 9 * s), fill=main)
        d.ellipse((cx - 0.30 * fw - 8 * s, 0.33 * half_h - 8 * s, cx - 0.30 * fw + 8 * s, 0.33 * half_h + 8 * s), fill=gold)
    else:
        d.rectangle((cx + 0.30 * fw, 0.05 * half_h, cx + 0.33 * fw, half_h), fill=(120, 80, 40))
        d.polygon([(cx + 0.315 * fw, 0.0), (cx + 0.40 * fw, 0.14 * half_h), (cx + 0.315 * fw, 0.22 * half_h), (cx + 0.23 * fw, 0.14 * half_h)], fill=(170, 176, 186))
    # robe
    shoulder_y = 0.60 * half_h
    d.polygon([(cx - 0.46 * fw, half_h), (cx - 0.40 * fw, shoulder_y + 10 * s), (cx - 0.12 * fw, shoulder_y - 6 * s),
               (cx + 0.12 * fw, shoulder_y - 6 * s), (cx + 0.40 * fw, shoulder_y + 10 * s), (cx + 0.46 * fw, half_h)], fill=main)
    d.polygon([(cx - 0.12 * fw, shoulder_y - 6 * s), (cx + 0.12 * fw, shoulder_y - 6 * s), (cx + 0.07 * fw, half_h), (cx - 0.07 * fw, half_h)], fill=second)
    for k in range(5):
        yy = shoulder_y + k * (half_h - shoulder_y) / 5 + 6 * s
        d.polygon([(cx, yy - 8 * s), (cx + 8 * s, yy), (cx, yy + 8 * s), (cx - 8 * s, yy)], fill=gold)
    d.line((cx - 0.40 * fw, shoulder_y + 10 * s, cx - 0.12 * fw, shoulder_y - 6 * s), fill=gold, width=5 * s)
    d.line((cx + 0.40 * fw, shoulder_y + 10 * s, cx + 0.12 * fw, shoulder_y - 6 * s), fill=gold, width=5 * s)
    for k in range(4):
        x = cx - 0.36 * fw + k * 9 * s
        d.line((x, shoulder_y + 20 * s + k * 4 * s, x, half_h), fill=second, width=3 * s)
        x2 = cx + 0.36 * fw - k * 9 * s
        d.line((x2, shoulder_y + 20 * s + k * 4 * s, x2, half_h), fill=second, width=3 * s)
    # head
    hy = 0.38 * half_h
    hr_x, hr_y = 0.15 * fw, 0.17 * half_h
    hair = (120, 70, 30) if rank != "Q" else (200, 150, 60)
    if rank == "Q":
        d.polygon([(cx - hr_x * 1.25, hy - hr_y * 0.6), (cx + hr_x * 1.25, hy - hr_y * 0.6), (cx + hr_x * 1.5, shoulder_y + 14 * s), (cx - hr_x * 1.5, shoulder_y + 14 * s)], fill=hair)
    else:
        d.ellipse((cx - hr_x * 1.18, hy - hr_y * 1.05, cx + hr_x * 1.18, hy + hr_y * 1.0), fill=hair)
    d.rectangle((cx - 0.05 * fw, hy + hr_y * 0.7, cx + 0.05 * fw, shoulder_y), fill=SKIN)
    d.ellipse((cx - hr_x, hy - hr_y, cx + hr_x, hy + hr_y), fill=SKIN, outline=(90, 60, 40), width=1 * s)
    if rank == "K":
        d.polygon([(cx - hr_x * 0.95, hy + hr_y * 0.1), (cx + hr_x * 0.95, hy + hr_y * 0.1), (cx + hr_x * 0.5, hy + hr_y * 1.55), (cx, hy + hr_y * 1.8), (cx - hr_x * 0.5, hy + hr_y * 1.55)], fill=hair)
        d.ellipse((cx - hr_x * 0.35, hy + hr_y * 0.35, cx + hr_x * 0.35, hy + hr_y * 0.6), fill=SKIN)
    # face
    for ex in (-0.4, 0.4):
        d.ellipse((cx + ex * hr_x - 3.5 * s, hy - 0.15 * hr_y - 2.5 * s, cx + ex * hr_x + 3.5 * s, hy - 0.15 * hr_y + 2.5 * s), fill=(40, 40, 50))
        d.line((cx + ex * hr_x - 6 * s, hy - 0.42 * hr_y, cx + ex * hr_x + 6 * s, hy - 0.46 * hr_y), fill=(90, 60, 40), width=2 * s)
    d.line((cx, hy - 0.05 * hr_y, cx - 3 * s, hy + 0.3 * hr_y, cx + 2 * s, hy + 0.34 * hr_y), fill=(160, 110, 80), width=2 * s)
    d.arc((cx - 9 * s, hy + 0.35 * hr_y, cx + 9 * s, hy + 0.62 * hr_y), 20, 160, fill=(170, 40, 50), width=2 * s)
    # headwear
    top = hy - hr_y * 0.9
    if rank == "K":
        pts = [(cx - hr_x * 1.1, top), (cx - hr_x * 1.15, top - 34 * s), (cx - hr_x * 0.55, top - 16 * s), (cx, top - 42 * s),
               (cx + hr_x * 0.55, top - 16 * s), (cx + hr_x * 1.15, top - 34 * s), (cx + hr_x * 1.1, top)]
        d.polygon(pts, fill=gold, outline=(120, 90, 30))
        for jx in (-0.6, 0, 0.6):
            d.ellipse((cx + jx * hr_x - 4 * s, top - 10 * s, cx + jx * hr_x + 4 * s, top - 2 * s), fill=main)
    elif rank == "Q":
        d.polygon([(cx - hr_x * 0.95, top + 2 * s), (cx - hr_x * 0.8, top - 20 * s), (cx, top - 30 * s), (cx + hr_x * 0.8, top - 20 * s), (cx + hr_x * 0.95, top + 2 * s)], fill=gold, outline=(120, 90, 30))
        d.ellipse((cx - 5 * s, top - 22 * s, cx + 5 * s, top - 12 * s), fill=main)
    else:
        d.polygon([(cx - hr_x * 1.2, top + 6 * s), (cx - hr_x * 0.9, top - 22 * s), (cx + hr_x * 0.9, top - 22 * s), (cx + hr_x * 1.2, top + 6 * s)], fill=second)
        d.rectangle((cx - hr_x * 1.2, top, cx + hr_x * 1.2, top + 6 * s), fill=gold)
        d.polygon([(cx + hr_x * 0.6, top - 20 * s), (cx + hr_x * 1.6, top - 46 * s), (cx + hr_x * 1.1, top - 14 * s)], fill=main)
    draw_suit(d, 0.15 * fw, 0.13 * half_h, 30 * s, suit)
    # assemble mirrored halves
    frame = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    frame.paste(half, (0, 0))
    frame.paste(half.rotate(180), (0, fh - half_h))
    fd = ImageDraw.Draw(frame)
    fd.line((0, fh / 2, fw, fh / 2), fill=gold, width=2 * s)
    fd.rectangle((0, 0, fw - 1, fh - 1), outline=main, width=4 * s)
    fd.rectangle((5 * s, 5 * s, fw - 6 * s, fh - 6 * s), outline=gold, width=2 * s)
    img.paste(frame, (fx0, fy0))


def make_face(rank, suit):
    s = SS
    img = Image.new("RGBA", (W * s, H * s), (0, 0, 0, 0))
    base = Image.new("RGBA", (W * s, H * s), IVORY + (255,))
    img.paste(base, (0, 0), rounded_mask(W * s, H * s, RADIUS * s))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((1, 1, W * s - 2, H * s - 2), RADIUS * s, outline=(200, 196, 188), width=2 * s)
    if rank in PIP_LAYOUT:
        draw_pips(img, rank, suit)
    elif rank == "A":
        draw_ace(img, suit)
    else:
        draw_court(img, rank, suit)
    draw_index(img, rank, suit)
    finish(img, CARDS / f"{rank}{suit}.png")


def make_back():
    s = SS
    w, h = W * s, H * s
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    base = Image.new("RGBA", (w, h), (20, 22, 28, 255))
    bd = ImageDraw.Draw(base)
    # subtle lattice
    for i in range(-h, w + h, 22 * s):
        bd.line((i, 0, i + h, h), fill=(34, 36, 44), width=1 * s)
        bd.line((i, h, i + h, 0), fill=(34, 36, 44), width=1 * s)
    img.paste(base, (0, 0), rounded_mask(w, h, RADIUS * s))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((14 * s, 14 * s, w - 15 * s, h - 15 * s), (RADIUS - 10) * s, outline=GOLD, width=3 * s)
    d.rounded_rectangle((22 * s, 22 * s, w - 23 * s, h - 23 * s), (RADIUS - 14) * s, outline=GOLD + (120,), width=1 * s)
    cx, cy = w / 2, h / 2
    # 8-point star medallion
    for R, width in ((96, 3), (82, 1), (40, 2)):
        d.ellipse((cx - R * s, cy - R * s, cx + R * s, cy + R * s), outline=GOLD, width=width * s)
    for rot in (0, math.pi / 4):
        sq = [(cx + 74 * s * math.cos(rot + k * math.pi / 2 + math.pi / 4), cy + 74 * s * math.sin(rot + k * math.pi / 2 + math.pi / 4)) for k in range(4)]
        d.polygon(sq, outline=GOLD, width=2 * s)
    for k in range(16):
        a = k * math.pi / 8
        r1 = (118 if k % 2 == 0 else 106) * s
        d.line((cx + 96 * s * math.cos(a), cy + 96 * s * math.sin(a), cx + r1 * math.cos(a), cy + r1 * math.sin(a)), fill=GOLD, width=2 * s)
    star = []
    for k in range(16):
        a = k * math.pi / 8 - math.pi / 2
        r = (36 if k % 2 == 0 else 16) * s
        star.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    d.polygon(star, fill=GOLD)
    # corner ornaments
    for (ox, oy) in ((52, 56), (W - 52, 56), (52, H - 56), (W - 52, H - 56)):
        ox, oy = ox * s, oy * s
        pts = []
        for k in range(8):
            a = k * math.pi / 4
            r = (16 if k % 2 == 0 else 6) * s
            pts.append((ox + r * math.cos(a), oy + r * math.sin(a)))
        d.polygon(pts, fill=GOLD)
    # top & bottom wordmark bars
    for yy in (cy - 150 * s, cy + 150 * s):
        d.line((cx - 60 * s, yy, cx + 60 * s, yy), fill=GOLD, width=2 * s)
        d.polygon([(cx, yy - 7 * s), (cx + 7 * s, yy), (cx, yy + 7 * s), (cx - 7 * s, yy)], fill=GOLD)
    finish(img, CARDS / "back.png")


# ---------------------------------------------------------------- environment
rng = np.random.default_rng(7)


def noise(size, scale, octaves=4):
    out = np.zeros((size, size))
    amp = 1.0
    for o in range(octaves):
        n = max(2, int(size / scale * (2 ** o)))
        small = rng.random((n, n))
        img = Image.fromarray((small * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC)
        out += np.asarray(img, dtype=np.float32) / 255 * amp
        amp *= 0.5
    return out / out.max()


def carpet(path, field, border, accent, ivory, size=1024):
    s = 2
    n = size * s
    img = Image.new("RGB", (n, n), field)
    d = ImageDraw.Draw(img)
    # borders
    for (inset, color) in ((0, border), (60, accent), (70, border), (150, ivory), (158, field)):
        d.rectangle((inset * s, inset * s, n - inset * s, n - inset * s), fill=color)
    # border motif
    for k in range(0, n, 40 * s):
        for (x, y) in ((k, 105 * s), (k, n - 105 * s), (105 * s, k), (n - 105 * s, k)):
            d.polygon([(x, y - 18 * s), (x + 18 * s, y), (x, y + 18 * s), (x - 18 * s, y)], fill=ivory)
            d.polygon([(x, y - 9 * s), (x + 9 * s, y), (x, y + 9 * s), (x - 9 * s, y)], fill=accent)
    # field rosettes
    for gx in range(6):
        for gy in range(6):
            x = (230 + gx * 113) * s
            y = (230 + gy * 113) * s
            if abs(x - n / 2) < 250 * s and abs(y - n / 2) < 250 * s:
                continue
            for k in range(8):
                a = k * math.pi / 4
                d.ellipse((x + 18 * s * math.cos(a) - 8 * s, y + 18 * s * math.sin(a) - 8 * s,
                           x + 18 * s * math.cos(a) + 8 * s, y + 18 * s * math.sin(a) + 8 * s), fill=accent)
            d.ellipse((x - 9 * s, y - 9 * s, x + 9 * s, y + 9 * s), fill=ivory)
    # central medallion
    c = n / 2
    for (r, color) in ((250, border), (236, ivory), (224, accent), (180, field), (120, border), (100, ivory), (60, accent)):
        pts = []
        for k in range(32):
            a = k * math.pi / 16
            rr = r * (1.0 if k % 2 == 0 else 0.86)
            pts.append((c + rr * s * math.cos(a), c + rr * s * math.sin(a)))
        d.polygon(pts, fill=color)
    img = img.resize((size, size), Image.LANCZOS)
    arr = np.asarray(img, dtype=np.float32)
    grain = noise(size, 4, 3)[..., None]
    fine = rng.random((size, size, 1)) * 0.12
    arr = arr * (0.82 + 0.22 * grain + fine - 0.06)
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(path, optimize=True)


def table_top(path, size=1024):
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    c = size / 2
    r = np.sqrt((x - c) ** 2 + (y - c) ** 2) / c
    ang = np.arctan2(y - c, x - c)
    n = noise(size, 16, 4)
    grain = np.sin((x * 0.02 + n * 9.0) * 6.0) * 0.5 + 0.5
    base = np.stack([0.40, 0.25, 0.14]) * 255
    col = base[None, None, :] * (0.72 + 0.28 * grain[..., None]) * (0.9 + 0.2 * n[..., None])
    engrave = np.zeros_like(r)
    for rr, w in ((0.97, 0.008), (0.93, 0.004), (0.66, 0.006), (0.62, 0.003), (0.30, 0.005)):
        engrave = np.maximum(engrave, np.clip(1 - np.abs(r - rr) / w, 0, 1))
    # ring of arabesque lobes between 0.66 and 0.93
    lobes = np.cos(ang * 24) * 0.5 + 0.5
    band = ((r > 0.70) & (r < 0.89)).astype(np.float32)
    shape = np.clip(1 - np.abs(r - (0.795 + 0.07 * (lobes - 0.5))) / 0.006, 0, 1) * band
    petals = np.clip(1 - np.abs(r - (0.46 + 0.12 * np.abs(np.cos(ang * 4)))) / 0.006, 0, 1) * (r < 0.62)
    star = np.clip(1 - np.abs(r - (0.18 + 0.08 * np.abs(np.cos(ang * 4 + 0.785)))) / 0.006, 0, 1)
    dots = ((np.abs(np.sin(ang * 48)) > 0.97) & (np.abs(r - 0.95) < 0.012)).astype(np.float32)
    engrave = np.clip(engrave + shape + petals + star + dots, 0, 1)
    col = col * (1 - 0.55 * engrave[..., None]) + np.array([60, 36, 20]) * 0.0
    # soft highlight towards centre, darker rim
    col *= (1.08 - 0.25 * r ** 3)[..., None]
    Image.fromarray(np.clip(col, 0, 255).astype(np.uint8)).save(path, optimize=True)


def tileable_wood(path, size=512, tint=(0.35, 0.21, 0.12)):
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    n = noise(size, 32, 4)
    g = np.sin((y / size * 2 * np.pi * 7) + n * 6) * 0.5 + 0.5
    col = np.array(tint)[None, None, :] * 255 * (0.7 + 0.3 * g[..., None]) * (0.9 + 0.2 * n[..., None])
    Image.fromarray(np.clip(col, 0, 255).astype(np.uint8)).save(path, optimize=True)


def shemagh(path, size=256):
    img = Image.new("RGB", (size, size), (246, 244, 240))
    d = ImageDraw.Draw(img)
    step = 16
    for i in range(0, size, step):
        for j in range(0, size, step):
            if (i // step + j // step) % 2 == 0:
                d.rectangle((i + 2, j + 2, i + step - 3, j + step - 3), fill=(190, 30, 40))
            else:
                d.rectangle((i + 6, j + 6, i + step - 7, j + step - 7), fill=(190, 30, 40))
    img.save(path, optimize=True)


def kilim(path, size=512):
    img = Image.new("RGB", (size, size), (120, 40, 36))
    d = ImageDraw.Draw(img)
    colors = [(120, 40, 36), (40, 48, 80), (190, 140, 70), (150, 60, 40), (60, 30, 30)]
    y = 0
    k = 0
    while y < size:
        hgt = [40, 12, 26, 8, 40, 12][k % 6]
        d.rectangle((0, y, size, y + hgt), fill=colors[k % len(colors)])
        if hgt >= 26:
            for x in range(0, size, 32):
                d.polygon([(x, y + hgt / 2), (x + 16, y + 3), (x + 32, y + hgt / 2), (x + 16, y + hgt - 3)], fill=colors[(k + 2) % len(colors)])
        y += hgt
        k += 1
    arr = np.asarray(img, dtype=np.float32) * (0.85 + 0.3 * noise(size, 8, 3)[..., None])
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(path, optimize=True)


def plaster(path, size=512):
    n = noise(size, 24, 5)
    base = np.array([196, 160, 118], dtype=np.float32)
    arr = base[None, None, :] * (0.82 + 0.25 * n[..., None])
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    # carved 8-point star lattice (tileable: 4x4 cells of 128)
    for gx in range(4):
        for gy in range(4):
            cx, cy = gx * 128 + 64, gy * 128 + 64
            for rot in (0, math.pi / 4):
                sq = [(cx + 44 * math.cos(rot + k * math.pi / 2 + math.pi / 4), cy + 44 * math.sin(rot + k * math.pi / 2 + math.pi / 4)) for k in range(4)]
                d.polygon(sq, outline=(150, 115, 80), width=3)
            d.ellipse((cx - 10, cy - 10, cx + 10, cy + 10), outline=(150, 115, 80), width=3)
            d.line((gx * 128, gy * 128, gx * 128 + 128, gy * 128), fill=(165, 130, 92), width=2)
            d.line((gx * 128, gy * 128, gx * 128, gy * 128 + 128), fill=(165, 130, 92), width=2)
    img.save(path, optimize=True)


def glow(path, size=128, color=(255, 190, 110)):
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    r = np.sqrt((x - size / 2) ** 2 + (y - size / 2) ** 2) / (size / 2)
    a = np.clip(1 - r, 0, 1) ** 2.2
    arr = np.zeros((size, size, 4), dtype=np.float32)
    arr[..., 0], arr[..., 1], arr[..., 2] = color
    arr[..., 3] = a * 255
    Image.fromarray(arr.astype(np.uint8), "RGBA").save(path, optimize=True)


def card_glow(path):
    """Soft rounded-rectangle halo drawn behind a highlighted card."""
    w, h = 240, 300
    img = Image.new("L", (w, h), 0)
    ImageDraw.Draw(img).rounded_rectangle((40, 40, w - 40, h - 40), 22, fill=255)
    img = img.filter(ImageFilter.GaussianBlur(16))
    out = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    out.putalpha(img)
    out.save(path, optimize=True)


def lantern_glass(path, size=256):
    """Pierced-metal lantern panel: warm light shining through star cut-outs."""
    img = Image.new("RGBA", (size, size), (70, 45, 20, 255))
    d = ImageDraw.Draw(img)
    for gx in range(4):
        for gy in range(4):
            cx, cy = gx * 64 + 32, gy * 64 + 32
            pts = []
            for k in range(16):
                a = k * math.pi / 8
                r = 24 if k % 2 == 0 else 11
                pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
            d.polygon(pts, fill=(255, 196, 110, 255))
            d.ellipse((cx - 4, cy - 4, cx + 4, cy + 4), fill=(255, 240, 200, 255))
    img = img.filter(ImageFilter.GaussianBlur(1.2))
    img.save(path, optimize=True)


# ---------------------------------------------------------------- UI icons
def icon(name, draw_fn, size=128):
    s = 4
    img = Image.new("RGBA", (size * s, size * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    draw_fn(d, size * s)
    img.resize((size, size), Image.LANCZOS).save(ICONS / f"{name}.png", optimize=True)


WHITE = (255, 255, 255, 255)


def i_spade(d, n):
    draw_suit(d, n / 2, n / 2, n * 0.78, "S", WHITE)


def i_trophy(d, n):
    w = n * 0.07
    d.arc((n * .16, n * .18, n * .42, n * .5), 90, 270, fill=WHITE, width=int(w))
    d.arc((n * .58, n * .18, n * .84, n * .5), 270, 90, fill=WHITE, width=int(w))
    d.chord((n * .28, n * -0.1, n * .72, n * .62), 0, 180, fill=WHITE)
    d.rectangle((n * .28, n * .13, n * .72, n * .27), fill=WHITE)
    d.rectangle((n * .46, n * .6, n * .54, n * .76), fill=WHITE)
    d.rounded_rectangle((n * .3, n * .74, n * .7, n * .86), n * .03, fill=WHITE)


def i_people(d, n):
    d.ellipse((n * .18, n * .2, n * .46, n * .48), outline=WHITE, width=int(n * .06))
    d.arc((n * .06, n * .52, n * .58, n * 1.05), 180, 360, fill=WHITE, width=int(n * .06))
    d.ellipse((n * .54, n * .26, n * .78, n * .5), outline=WHITE, width=int(n * .06))
    d.arc((n * .5, n * .56, n * .94, n * 1.0), 200, 360, fill=WHITE, width=int(n * .06))


def i_gear(d, n):
    c = n / 2
    pts = []
    for k in range(48):
        a = k * 2 * math.pi / 48
        r = n * (0.42 if (k // 3) % 2 == 0 else 0.33)
        pts.append((c + r * math.cos(a), c + r * math.sin(a)))
    d.polygon(pts, fill=WHITE)
    d.ellipse((c - n * .14, c - n * .14, c + n * .14, c + n * .14), fill=(0, 0, 0, 0))


def i_chat(d, n):
    d.rounded_rectangle((n * .12, n * .18, n * .88, n * .7), n * .12, outline=WHITE, width=int(n * .06))
    d.polygon([(n * .28, n * .66), (n * .44, n * .66), (n * .26, n * .86)], fill=WHITE)
    for x in (.35, .5, .65):
        d.ellipse((n * x - n * .04, n * .44 - n * .04, n * x + n * .04, n * .44 + n * .04), fill=WHITE)


def i_mic(d, n):
    d.rounded_rectangle((n * .36, n * .1, n * .64, n * .6), n * .14, fill=WHITE)
    d.arc((n * .24, n * .3, n * .76, n * .76), 0, 180, fill=WHITE, width=int(n * .06))
    d.rectangle((n * .47, n * .75, n * .53, n * .88), fill=WHITE)
    d.rectangle((n * .34, n * .86, n * .66, n * .91), fill=WHITE)


def i_wifi(d, n):
    for k, r in enumerate((.42, .3, .18)):
        d.arc((n / 2 - n * r, n * .78 - n * r, n / 2 + n * r, n * .78 + n * r), 225, 315, fill=WHITE, width=int(n * .07))
    d.ellipse((n * .45, n * .72, n * .55, n * .82), fill=WHITE)


def i_battery(d, n):
    d.rounded_rectangle((n * .08, n * .3, n * .82, n * .7), n * .08, outline=WHITE, width=int(n * .05))
    d.rounded_rectangle((n * .15, n * .37, n * .66, n * .63), n * .04, fill=WHITE)
    d.rectangle((n * .84, n * .42, n * .9, n * .58), fill=WHITE)


def i_profile(d, n):
    d.ellipse((n * .32, n * .14, n * .68, n * .5), outline=WHITE, width=int(n * .06))
    d.arc((n * .14, n * .56, n * .86, n * 1.18), 180, 360, fill=WHITE, width=int(n * .06))


def i_sun(d, n):
    c = n / 2
    d.ellipse((c - n * .18, c - n * .18, c + n * .18, c + n * .18), fill=WHITE)
    for k in range(8):
        a = k * math.pi / 4
        d.line((c + n * .27 * math.cos(a), c + n * .27 * math.sin(a), c + n * .4 * math.cos(a), c + n * .4 * math.sin(a)), fill=WHITE, width=int(n * .06))


def i_hokm(d, n):
    draw_suit(d, n * .36, n * .38, n * .42, "H", WHITE)
    draw_suit(d, n * .64, n * .62, n * .42, "S", WHITE)


def i_ashkal(d, n):
    c = n / 2
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        d.line((c, c, c + n * .36 * math.cos(a), c + n * .36 * math.sin(a)), fill=WHITE, width=int(n * .07))
        d.ellipse((c + n * .36 * math.cos(a) - n * .07, c + n * .36 * math.sin(a) - n * .07, c + n * .36 * math.cos(a) + n * .07, c + n * .36 * math.sin(a) + n * .07), fill=WHITE)
    d.ellipse((c - n * .09, c - n * .09, c + n * .09, c + n * .09), fill=WHITE)


def i_pass(d, n):
    d.ellipse((n * .16, n * .16, n * .84, n * .84), outline=WHITE, width=int(n * .07))
    d.line((n * .3, n * .7, n * .7, n * .3), fill=WHITE, width=int(n * .07))


def i_recenter(d, n):
    d.arc((n * .16, n * .16, n * .84, n * .84), 40, 330, fill=WHITE, width=int(n * .07))
    d.polygon([(n * .78, n * .1), (n * .9, n * .36), (n * .62, n * .34)], fill=WHITE)


def i_home(d, n):
    d.polygon([(n * .5, n * .14), (n * .88, n * .48), (n * .12, n * .48)], fill=WHITE)
    d.rectangle((n * .22, n * .46, n * .78, n * .86), fill=WHITE)
    d.rectangle((n * .43, n * .6, n * .57, n * .86), fill=(0, 0, 0, 0))


def avatar_portrait():
    s = 4
    n = 128 * s
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse((0, 0, n - 1, n - 1), fill=(64, 92, 120, 255))
    d.ellipse((n * .1, n * .64, n * .9, n * 1.3), fill=(236, 236, 232, 255))
    d.ellipse((n * .29, n * .18, n * .71, n * .66), fill=(214, 168, 128, 255))
    d.chord((n * .27, n * .1, n * .73, n * .5), 180, 360, fill=(30, 24, 20, 255))
    d.chord((n * .3, n * .4, n * .7, n * .72), 0, 180, fill=(40, 30, 24, 255))
    d.ellipse((n * .43, n * .52, n * .57, n * .58), fill=(214, 168, 128, 255))
    d.rounded_rectangle((n * .3, n * .34, n * .48, n * .44), n * .03, fill=(20, 20, 24, 255))
    d.rounded_rectangle((n * .52, n * .34, n * .7, n * .44), n * .03, fill=(20, 20, 24, 255))
    d.line((n * .48, n * .37, n * .52, n * .37), fill=(20, 20, 24, 255), width=6)
    mask = Image.new("L", (n, n), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, n - 1, n - 1), fill=255)
    out = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    out.paste(img, (0, 0), mask)
    out.resize((128, 128), Image.LANCZOS).save(ICONS / "avatar_belal.png", optimize=True)


def app_icon():
    s = 4
    n = 512 * s
    img = Image.new("RGBA", (n, n), (20, 22, 28, 255))
    d = ImageDraw.Draw(img)
    for i in range(0, n, 12):
        t = i / n
        d.line((0, i, n, i), fill=(int(60 + 140 * t), int(40 + 60 * t), int(60 - 20 * t), 255))
    d.rounded_rectangle((n * .22, n * .16, n * .62, n * .74), n * .04, fill=IVORY + (255,), outline=GOLD, width=12)
    draw_suit(d, n * .42, n * .45, n * .3, "S")
    d.rounded_rectangle((n * .42, n * .28, n * .82, n * .86), n * .04, fill=(20, 22, 28, 255), outline=GOLD, width=12)
    draw_suit(d, n * .62, n * .57, n * .24, "S", GOLD)
    img.resize((512, 512), Image.LANCZOS).convert("RGB").save(ROOT / "icon.png", optimize=True)


if __name__ == "__main__":
    random.seed(3)
    for suit in SUITS:
        for rank in RANKS:
            make_face(rank, suit)
    make_back()
    carpet(TEX / "carpet_red.png", (118, 26, 30), (28, 36, 64), (196, 150, 72), (232, 220, 190))
    carpet(TEX / "carpet_navy.png", (30, 40, 72), (110, 26, 30), (196, 150, 72), (232, 220, 190), size=512)
    table_top(TEX / "table_top.png")
    tileable_wood(TEX / "wood.png")
    tileable_wood(TEX / "wood_dark.png", tint=(0.2, 0.12, 0.07))
    shemagh(TEX / "shemagh.png")
    kilim(TEX / "kilim.png")
    plaster(TEX / "plaster.png")
    glow(TEX / "glow.png")
    glow(TEX / "glow_white.png", color=(255, 255, 255))
    lantern_glass(TEX / "lantern_panel.png")
    card_glow(TEX / "card_glow.png")
    for name, fn in (("spade", i_spade), ("trophy", i_trophy), ("people", i_people), ("gear", i_gear),
                     ("chat", i_chat), ("mic", i_mic), ("wifi", i_wifi), ("battery", i_battery),
                     ("profile", i_profile), ("sun", i_sun), ("hokm", i_hokm), ("ashkal", i_ashkal),
                     ("pass", i_pass), ("recenter", i_recenter), ("home", i_home)):
        icon(name, fn)
    avatar_portrait()
    app_icon()
    print("textures generated")
