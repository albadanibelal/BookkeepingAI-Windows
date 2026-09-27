#!/usr/bin/env python3
"""Pre-renders the 604 pages of the Madinah Mushaf (Hafs, King Fahd Complex
QCF4 glyphs) into WebP textures used by the VR app.

Every Quranic word is a single pre-composed glyph from the King Fahd Complex
font, so no shaping is involved: glyphs are placed right-to-left and each full
line is justified to the text block width exactly like the printed Mushaf.

usage: render_pages.py <quran-qcf4 package dir> <qcf ttf dir> <out dir> [pages]
(AMIRI_TTF env var: Amiri Bold .ttf used for page numbers)
"""
import json
import math
import os
import sys
from multiprocessing import Pool

from PIL import Image, ImageDraw, ImageFilter, ImageFont

PKG, TTF, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
AMIRI = os.environ.get('AMIRI_TTF', os.path.join(TTF, '..', 'amiri-arabic-700-normal.ttf'))

SS = 2                      # supersampling factor
W, H = 1000, 1440           # design size (portrait page)
SW, SH = W * SS, H * SS
OW, OH = 760, 1094          # texture size shipped in the APK

PAPER = (250, 244, 226)
PAPER_EDGE = (236, 224, 196)
INK = (22, 20, 18)
GREEN = (38, 104, 88)
GREEN_DARK = (22, 70, 60)
GOLD = (201, 164, 84)
GOLD_LIGHT = (236, 210, 140)
RED = (170, 60, 44)

JUZ_START = [1, 22, 42, 62, 82, 102, 121, 142, 162, 182, 201, 222, 242, 262, 282,
             302, 322, 342, 362, 382, 402, 422, 442, 462, 482, 502, 522, 542, 562, 582]

_fonts = {}


def font(name, px):
    key = (name, px)
    if key not in _fonts:
        fn = name + ('_W.ttf' if 'Hafs' in name else '.ttf')
        _fonts[key] = ImageFont.truetype(os.path.join(TTF, fn), px,
                                         layout_engine=ImageFont.Layout.BASIC)
    return _fonts[key]


def advance(name, px, ch):
    return font(name, px).getlength(ch)


def juz_of(page):
    j = 1
    for i, s in enumerate(JUZ_START):
        if page >= s:
            j = i + 1
    return j


def arabic_digits(n):
    return ''.join('٠١٢٣٤٥٦٧٨٩'[int(c)] for c in str(n))


# ---------------------------------------------------------------- ornaments

def rosette(d, cx, cy, r, petals=8, fill=GOLD, inner=GREEN, dot=PAPER):
    pts = []
    for i in range(petals * 2):
        a = math.pi * i / petals
        rr = r if i % 2 == 0 else r * 0.55
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    d.polygon(pts, fill=fill)
    d.ellipse((cx - r * 0.42, cy - r * 0.42, cx + r * 0.42, cy + r * 0.42), fill=inner)
    d.ellipse((cx - r * 0.18, cy - r * 0.18, cx + r * 0.18, cy + r * 0.18), fill=dot)


def band(d, x0, y0, x1, y1, horizontal):
    """Green frame band filled with a repeating gold interlace motif."""
    d.rectangle((x0, y0, x1, y1), fill=GREEN)
    t = (y1 - y0) if horizontal else (x1 - x0)
    step = t * 1.0
    length = (x1 - x0) if horizontal else (y1 - y0)
    n = max(1, int(length / step))
    step = length / n
    for i in range(n):
        if horizontal:
            cx, cy = x0 + step * (i + 0.5), (y0 + y1) / 2
        else:
            cx, cy = (x0 + x1) / 2, y0 + step * (i + 0.5)
        r = t * 0.40
        d.polygon([(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)], outline=GOLD, width=max(2, SS * 2))
        d.ellipse((cx - r * 0.35, cy - r * 0.35, cx + r * 0.35, cy + r * 0.35), fill=GOLD)
        d.ellipse((cx - r * 0.14, cy - r * 0.14, cx + r * 0.14, cy + r * 0.14), fill=RED)
        # little links between diamonds
        if horizontal:
            e = cx + step / 2
            d.ellipse((e - r * 0.16, cy - r * 0.16, e + r * 0.16, cy + r * 0.16), fill=GOLD_LIGHT)
        else:
            e = cy + step / 2
            d.ellipse((cx - r * 0.16, e - r * 0.16, cx + r * 0.16, e + r * 0.16), fill=GOLD_LIGHT)


def frame(d, x0, y0, x1, y1, bw):
    lw = SS * 2
    # outer hairlines
    d.rectangle((x0 - 5 * SS, y0 - 5 * SS, x1 + 5 * SS, y1 + 5 * SS), outline=GOLD, width=lw)
    band(d, x0, y0, x1, y0 + bw, True)
    band(d, x0, y1 - bw, x1, y1, True)
    band(d, x0, y0, x0 + bw, y1, False)
    band(d, x1 - bw, y0, x1, y1, False)
    d.rectangle((x0, y0, x1, y1), outline=GOLD, width=lw)
    d.rectangle((x0 + bw, y0 + bw, x1 - bw, y1 - bw), outline=GOLD, width=lw)
    # inner red + gold rules
    g = 6 * SS
    d.rectangle((x0 + bw + g, y0 + bw + g, x1 - bw - g, y1 - bw - g), outline=RED, width=SS)
    d.rectangle((x0 + bw + g * 2, y0 + bw + g * 2, x1 - bw - g * 2, y1 - bw - g * 2), outline=GOLD, width=SS)
    for (cx, cy) in ((x0 + bw / 2, y0 + bw / 2), (x1 - bw / 2, y0 + bw / 2),
                     (x0 + bw / 2, y1 - bw / 2), (x1 - bw / 2, y1 - bw / 2)):
        d.rectangle((cx - bw / 2, cy - bw / 2, cx + bw / 2, cy + bw / 2), fill=GREEN_DARK, outline=GOLD, width=lw)
        rosette(d, cx, cy, bw * 0.42)


def cartouche(d, cx, cy, w, h):
    r = h / 2
    d.rounded_rectangle((cx - w / 2 - 4 * SS, cy - r - 4 * SS, cx + w / 2 + 4 * SS, cy + r + 4 * SS),
                        radius=r + 4 * SS, fill=GREEN, outline=GOLD, width=SS * 2)
    d.rounded_rectangle((cx - w / 2 + 4 * SS, cy - r + 4 * SS, cx + w / 2 - 4 * SS, cy + r - 4 * SS),
                        radius=r - 4 * SS, fill=PAPER, outline=GOLD, width=SS * 2)


def surah_banner(d, x0, x1, cy, h, glyph):
    """Ornamented surah title panel like the printed Mushaf."""
    w = x1 - x0
    d.rectangle((x0, cy - h / 2, x1, cy + h / 2), fill=GREEN, outline=GOLD, width=SS * 2)
    inset = h * 0.14
    band(d, x0 + inset, cy - h / 2 + inset * 0.5, x1 - inset, cy - h / 2 + inset * 1.6, True)
    band(d, x0 + inset, cy + h / 2 - inset * 1.6, x1 - inset, cy + h / 2 - inset * 0.5, True)
    cw = w * 0.46
    ch = h * 0.66
    d.rounded_rectangle((x0 + (w - cw) / 2, cy - ch / 2, x0 + (w + cw) / 2, cy + ch / 2),
                        radius=ch / 2, fill=PAPER, outline=GOLD, width=SS * 3)
    for sx in (x0 + h * 0.55, x1 - h * 0.55):
        rosette(d, sx, cy, h * 0.3)
    f = font('QCF4_QBSML', int(ch * 1.05))
    tw = f.getlength(glyph)
    bb = f.getbbox(glyph)
    d.text((x0 + w / 2 - tw / 2, cy - (bb[1] + bb[3]) / 2), glyph, font=f, fill=INK)


# ------------------------------------------------------------------- pages

def render(page):
    data = json.load(open(os.path.join(PKG, 'pages', '%03d.json' % page)))
    img = Image.new('RGB', (SW, SH), PAPER)
    d = ImageDraw.Draw(img)

    # paper: subtle darkening towards the edges
    edge = Image.new('L', (SW, SH), 0)
    ed = ImageDraw.Draw(edge)
    ed.rectangle((0, 0, SW, SH), outline=255, width=40 * SS)
    edge = edge.filter(ImageFilter.GaussianBlur(40 * SS))
    img.paste(Image.new('RGB', (SW, SH), PAPER_EDGE), (0, 0), edge)
    d = ImageDraw.Draw(img)

    right_page = page % 2 == 1
    m = 34 * SS
    bw = 26 * SS
    fx0, fy0, fx1, fy1 = m, m + 34 * SS, SW - m, SH - m - 30 * SS
    frame(d, fx0, fy0, fx1, fy1, bw)

    # side medallion on the outer edge (like the printed Mushaf margins)
    mx = fx1 + (SW - fx1) / 2 + 2 * SS if right_page else fx0 / 2 - 2 * SS
    rosette(d, mx, SH / 2, 13 * SS, petals=12)

    # header: surah name (left) and juz (right), above the frame
    first = data['surahs'][0]['id'] if data['surahs'] else 1
    hf = font('QCF4_QBSML', 36 * SS)
    hy = m + 10 * SS
    sname = chr(0xF1F8 + first - 1)      # style B surah name
    jname = chr(0xF2D0 + juz_of(page) - 1)
    for glyph, cx in ((sname, fx0 + 130 * SS), (jname, fx1 - 130 * SS)):
        tw = hf.getlength(glyph)
        cartouche(d, cx, hy, max(tw + 40 * SS, 200 * SS), 38 * SS)
        bb = hf.getbbox(glyph)
        d.text((cx - tw / 2, hy - (bb[1] + bb[3]) / 2), glyph, font=hf, fill=INK)

    # page number medallion at the bottom
    py = fy1 + 14 * SS
    d.ellipse((SW / 2 - 34 * SS, py - 22 * SS, SW / 2 + 34 * SS, py + 22 * SS), fill=GREEN, outline=GOLD, width=SS * 2)
    d.ellipse((SW / 2 - 28 * SS, py - 17 * SS, SW / 2 + 28 * SS, py + 17 * SS), fill=PAPER, outline=GOLD, width=SS)
    nf = ImageFont.truetype(AMIRI, 26 * SS,
                            layout_engine=ImageFont.Layout.RAQM)
    d.text((SW / 2, py + 1 * SS), arabic_digits(page), font=nf, fill=INK, anchor='mm', direction='rtl')

    # text block
    pad = 22 * SS
    tx0, tx1 = fx0 + bw + pad, fx1 - bw - pad
    ty0, ty1 = fy0 + bw + pad, fy1 - bw - pad
    opening = page <= 2
    if opening:
        # Al-Fatiha / start of Al-Baqarah: centred inside an ornamental medallion
        cx, cy = SW / 2, (ty0 + ty1) / 2
        rw, rh = (tx1 - tx0) * 0.47, (ty1 - ty0) * 0.40
        d.ellipse((cx - rw - 30 * SS, cy - rh - 30 * SS, cx + rw + 30 * SS, cy + rh + 30 * SS), fill=GREEN, outline=GOLD, width=SS * 3)
        for i in range(48):
            a = 2 * math.pi * i / 48
            rosette(d, cx + (rw + 15 * SS) * math.cos(a), cy + (rh + 15 * SS) * math.sin(a), 9 * SS, petals=6)
        d.ellipse((cx - rw, cy - rh, cx + rw, cy + rh), fill=PAPER, outline=GOLD, width=SS * 3)
        ty0, ty1 = cy - rh * 0.86, cy + rh * 0.86
        tx0, tx1 = cx - rw * 0.80, cx + rw * 0.80

    lines = data['lines']
    nlines = 15 if not opening else len(lines)
    lh = (ty1 - ty0) / nlines
    block_w = tx1 - tx0
    # glyph size: a full line of the Mushaf is ~40600 font units (2500 upm)
    px = int(block_w / 41300 * 2500) if not opening else int(block_w / 34000 * 2500)

    for ln in lines:
        idx = ln['line'] - 1
        cy = ty0 + lh * (idx + 0.5)
        words = ln['words']
        kind = words[0]['type']
        if kind == 'surah_header':
            bx0, bx1 = tx0, tx1
            if opening:
                bx0, bx1 = tx0 + block_w * 0.12, tx1 - block_w * 0.12
            surah_banner(d, bx0, bx1, cy, lh * 0.86, chr(0xF1F8 + words[0]['sura'] - 1))
            continue
        chars = [(w['char'], w['font']) for w in words]
        size = px
        widths = [advance(fn, size, ch) for ch, fn in chars]
        total = sum(widths)
        if total > block_w:          # never overflow
            size = int(size * block_w / total)
            widths = [advance(fn, size, ch) for ch, fn in chars]
            total = sum(widths)
        justify = (not opening) and kind != 'bismillah' and total > block_w * 0.86 and len(chars) > 1
        gap = (block_w - total) / (len(chars) - 1) if justify else 0
        x = tx1 if justify else (tx0 + tx1) / 2 + total / 2
        base = cy + size * 0.18
        for (ch, fn), w in zip(chars, widths):
            x -= w
            d.text((x, base), ch, font=font(fn, size), fill=INK, anchor='ls')
            x -= gap

    img = img.resize((OW, OH), Image.LANCZOS)
    img.save(os.path.join(OUT, '%03d.webp' % page), 'WEBP', quality=70, method=6)
    return page


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    pages = range(1, 605)
    if len(sys.argv) > 4:
        pages = [int(p) for p in sys.argv[4].split(',')]
    with Pool(os.cpu_count()) as pool:
        for p in pool.imap_unordered(render, pages):
            if p % 50 == 0:
                print('page', p, flush=True)
