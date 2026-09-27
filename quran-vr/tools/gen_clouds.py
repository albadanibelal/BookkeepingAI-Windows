# Procedural, horizontally tileable cloud cover for the Earth globe.
# Domain-warped fBm on a periodic lattice, modulated by climate latitude bands
# (ITCZ, subtropical clear belts, mid-latitude storm tracks).
import sys, numpy as np
from PIL import Image
W, H = 2048, 1024
rng = np.random.default_rng(1405)

def periodic_noise(fx, fy, x, y):
    # value noise on a lattice with period fx horizontally
    lat = rng.random((fy + 2, fx))
    gx = x * fx; gy = y * fy
    x0 = np.floor(gx).astype(int); y0 = np.floor(gy).astype(int)
    tx = gx - x0; ty = gy - y0
    tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty)
    x1 = (x0 + 1) % fx; x0 %= fx; y1 = np.minimum(y0 + 1, fy + 1)
    a = lat[y0, x0] * (1 - tx) + lat[y0, x1] * tx
    b = lat[y1, x0] * (1 - tx) + lat[y1, x1] * tx
    return a * (1 - ty) + b * ty

def fbm(x, y, base, octaves):
    s = np.zeros_like(x); amp = 0.5; norm = 0
    for o in range(octaves):
        f = base * 2 ** o
        s += amp * periodic_noise(int(f * 2), int(f), x, y); norm += amp; amp *= 0.5
    return s / norm

xs, ys = np.meshgrid(np.arange(W) / W, np.arange(H) / H)
warp_x = fbm(xs, ys, 3, 4) - 0.5
warp_y = fbm(xs, ys, 3, 4) - 0.5
n = fbm((xs + warp_x * 0.08) % 1.0, np.clip(ys + warp_y * 0.05, 0, 0.999), 4, 8)
lat = 90 - ys * 180
band = (0.55 * np.exp(-(lat / 9) ** 2)
        + 0.75 * np.exp(-((np.abs(lat) - 55) / 14) ** 2)
        + 0.35 * np.exp(-((np.abs(lat) - 75) / 10) ** 2)
        - 0.25 * np.exp(-((np.abs(lat) - 25) / 8) ** 2))
t = n + band * 0.16
c = np.clip((t - 0.53) / 0.2, 0, 1) ** 1.3
detail = fbm(xs, ys, 32, 3)
c = np.clip(c * (0.75 + 0.5 * detail), 0, 1)
Image.fromarray((c * 255).astype(np.uint8), 'L').save(sys.argv[1], optimize=True)
print('clouds', c.mean())
