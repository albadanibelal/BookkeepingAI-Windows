package com.quranvr.mushaf.ui;

/** Line icons in the style of SF Symbols, drawn with Gfx primitives. */
final class Icons {
    private Icons() {}

    static void chevron(Gfx g, float cx, float cy, float s, boolean right, int col, float sw) {
        float d = right ? 1 : -1;
        g.path(new float[]{cx - d * s * 0.45f, cy - s, cx + d * s * 0.45f, cy, cx - d * s * 0.45f, cy + s}, false, false, sw, col);
    }

    static void book(Gfx g, float cx, float cy, float s, int col) {
        float w = s * 0.46f, h = s * 0.34f, sw = s * 0.065f;
        float[] left = {cx, cy - h * 0.78f, cx - w * 0.45f, cy - h * 1.02f, cx - w, cy - h * 0.92f,
                cx - w, cy + h * 0.92f, cx - w * 0.45f, cy + h * 0.80f, cx, cy + h * 1.02f};
        float[] right = new float[left.length];
        for (int i = 0; i < left.length; i += 2) { right[i] = 2 * cx - left[i]; right[i + 1] = left[i + 1]; }
        g.path(left, true, false, sw, col);
        g.path(right, true, false, sw, col);
        g.line(cx, cy - h * 0.78f, cx, cy + h * 1.02f, sw, col);
    }

    static void search(Gfx g, float cx, float cy, float s, int col) {
        float r = s * 0.25f, sw = s * 0.07f;
        g.strokeCircle(cx - s * 0.06f, cy - s * 0.06f, r, sw, col);
        g.line(cx + s * 0.12f, cy + s * 0.12f, cx + s * 0.32f, cy + s * 0.32f, sw * 1.2f, col);
    }

    static void bookmark(Gfx g, float cx, float cy, float s, int col, boolean fill) {
        float w = s * 0.24f, h = s * 0.36f;
        float[] p = {cx - w, cy - h, cx + w, cy - h, cx + w, cy + h, cx, cy + h * 0.52f, cx - w, cy + h};
        if (fill) g.path(p, true, true, 0, col);
        g.path(p, true, false, s * 0.065f, col);
    }

    static void gear(Gfx g, float cx, float cy, float s, int col) {
        int teeth = 8;
        float ro = s * 0.36f, ri = s * 0.27f;
        float[] p = new float[teeth * 4 * 2];
        int k = 0;
        for (int i = 0; i < teeth * 4; i++) {
            double a = Math.PI * 2 * (i - 0.5) / (teeth * 4);
            float r = (i % 4 == 1 || i % 4 == 2) ? ro : ri;
            p[k++] = cx + (float) Math.cos(a) * r;
            p[k++] = cy + (float) Math.sin(a) * r;
        }
        g.path(p, true, false, s * 0.06f, col);
        g.strokeCircle(cx, cy, s * 0.11f, s * 0.06f, col);
    }

    static void globe(Gfx g, float cx, float cy, float r, int col) {
        float sw = r * 0.12f;
        g.strokeCircle(cx, cy, r, sw, col);
        float[] mer = new float[2 * 33];
        for (int i = 0; i <= 32; i++) {
            double a = Math.PI * 2 * i / 32;
            mer[i * 2] = cx + (float) Math.cos(a) * r * 0.42f;
            mer[i * 2 + 1] = cy + (float) Math.sin(a) * r;
        }
        g.path(mer, true, false, sw, col);
        g.line(cx - r, cy, cx + r, cy, sw, col);
        g.line(cx, cy - r, cx, cy + r, sw, col);
        float yy = r * 0.5f, xx = (float) Math.sqrt(r * r - yy * yy);
        g.line(cx - xx, cy - yy, cx + xx, cy - yy, sw * 0.8f, col);
        g.line(cx - xx, cy + yy, cx + xx, cy + yy, sw * 0.8f, col);
    }

    static void pin(Gfx g, float cx, float cy, float r, int col) {
        float sw = r * 0.12f;
        float rr = r * 0.62f, top = cy - r * 0.35f;
        float[] p = new float[2 * 26];
        int k = 0;
        // teardrop: arc over the top, meeting at the tip below
        for (int i = 0; i <= 24; i++) {
            double a = Math.toRadians(-35 + 250.0 * i / 24);   // lower right, over the top, lower left
            p[k++] = cx + (float) Math.cos(a) * rr;
            p[k++] = top - (float) Math.sin(a) * rr;
        }
        p[k++] = cx;
        p[k] = cy + r;
        g.path(p, true, false, sw, col);
        g.strokeCircle(cx, top, rr * 0.36f, sw, col);
    }

    static void toggle(Gfx g, float x, float y, boolean on, int green) {
        float w = 92, h = 54;
        g.fillRoundRect(x, y, w, h, h / 2, on ? green : 0x3DFFFFFF);
        float kx = on ? x + w - h / 2 : x + h / 2;
        g.fillCircle(kx, y + h / 2, h / 2 - 4, 0xFFFFFFFF);
    }

    /** Eight-pointed star (rub el hizb) used above the title. */
    static void ornament(Gfx g, float cx, float cy, float r, int col) {
        g.radial(cx, cy, r * 2.2f, 0x40E4C582, 0x00E4C582);
        for (int s = 0; s < 2; s++) {
            float[] p = new float[8];
            for (int i = 0; i < 4; i++) {
                double a = Math.PI / 4 * s + Math.PI / 2 * i;
                p[i * 2] = cx + (float) Math.cos(a) * r;
                p[i * 2 + 1] = cy + (float) Math.sin(a) * r;
            }
            g.path(p, true, false, 3.2f, col);
        }
        g.strokeCircle(cx, cy, r * 0.52f, 3f, col);
        g.fillCircle(cx, cy, r * 0.18f, col);
        g.line(cx - r * 3.2f, cy, cx - r * 1.35f, cy, 2f, 0x99E4C582);
        g.line(cx + r * 1.35f, cy, cx + r * 3.2f, cy, 2f, 0x99E4C582);
        g.fillCircle(cx - r * 3.35f, cy, 3.5f, 0x99E4C582);
        g.fillCircle(cx + r * 3.35f, cy, 3.5f, 0x99E4C582);
    }
}
