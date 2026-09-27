package com.quranvr.mushaf.ui;

/** Minimal 2D drawing surface, implemented with android.graphics on the headset
 *  and with Java2D for the desktop preview tool. Colors are 0xAARRGGBB. */
public interface Gfx {
    int REGULAR = 0, MEDIUM = 1, SEMIBOLD = 2, BOLD = 3, ARABIC = 4, ARABIC_BOLD = 5;
    int LEFT = 0, CENTER = 1, RIGHT = 2;

    void clear();
    void save();
    void restore();
    void clip(float x, float y, float w, float h);
    void translate(float x, float y);

    void fillRoundRect(float x, float y, float w, float h, float r, int argb);
    void fillRoundRectV(float x, float y, float w, float h, float r, int top, int bottom);
    void strokeRoundRect(float x, float y, float w, float h, float r, float sw, int argb);
    void fillCircle(float cx, float cy, float r, int argb);
    void strokeCircle(float cx, float cy, float r, float sw, int argb);
    void radial(float cx, float cy, float r, int center, int edge);
    void line(float x0, float y0, float x1, float y1, float sw, int argb);
    /** Poly-line / polygon through xy pairs. */
    void path(float[] xy, boolean closed, boolean fill, float sw, int argb);
    /** Angles in degrees, 0 = 3 o'clock, positive = clockwise (screen space). */
    void arc(float cx, float cy, float r, float startDeg, float sweepDeg, float sw, int argb);

    /** Draws text with its baseline at y; returns the advance width. */
    float text(String s, float x, float y, int font, float size, int argb, int align);
    float measure(String s, int font, float size);
    /** Soft shadow/glow behind subsequent text (radius 0 disables). */
    void setShadow(float radius, int argb);
}
