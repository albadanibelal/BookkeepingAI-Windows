#!/usr/bin/env python3
"""Draws the launcher icon: a gold rub el hizb over a night-sky gradient."""
import math
import sys

from PIL import Image, ImageDraw, ImageFilter

out = sys.argv[1]
S = 4 * 192
img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
bg = Image.new('RGBA', (S, S))
px = bg.load()
for y in range(S):
    t = y / S
    for x in range(0, S):
        px[x, y] = (int(8 + 18 * t), int(16 + 34 * t), int(40 + 60 * t), 255)
mask = Image.new('L', (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, S - 1, S - 1), radius=S // 5, fill=255)
img.paste(bg, (0, 0), mask)
d = ImageDraw.Draw(img)
# earth glow
glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
ImageDraw.Draw(glow).ellipse((S * 0.55, S * 0.55, S * 1.25, S * 1.25), fill=(60, 140, 255, 150))
glow = glow.filter(ImageFilter.GaussianBlur(S // 16))
img = Image.alpha_composite(img, Image.composite(glow, Image.new('RGBA', (S, S)), mask))
d = ImageDraw.Draw(img)
gold = (230, 196, 120, 255)
cx, cy, r = S / 2, S * 0.46, S * 0.30
for k in range(2):
    pts = [(cx + r * math.cos(math.pi / 4 * k + math.pi / 2 * i), cy + r * math.sin(math.pi / 4 * k + math.pi / 2 * i)) for i in range(4)]
    d.polygon(pts, outline=gold, width=S // 40)
d.ellipse((cx - r * 0.5, cy - r * 0.5, cx + r * 0.5, cy + r * 0.5), outline=gold, width=S // 45)
d.ellipse((cx - r * 0.16, cy - r * 0.16, cx + r * 0.16, cy + r * 0.16), fill=gold)
img.resize((192, 192), Image.LANCZOS).save(out)
