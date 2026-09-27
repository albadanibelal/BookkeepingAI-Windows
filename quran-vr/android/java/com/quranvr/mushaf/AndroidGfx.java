package com.quranvr.mushaf;

import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.RadialGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;

import com.quranvr.mushaf.ui.Gfx;

/** android.graphics implementation of the UI drawing surface. */
final class AndroidGfx implements Gfx {
    static final class Fonts {
        final Typeface[] tf = new Typeface[6];

        Fonts(AssetManager am) {
            String[] f = {"Inter-Regular", "Inter-Medium", "Inter-SemiBold", "Inter-Bold", "Amiri-Regular", "Amiri-Bold"};
            for (int i = 0; i < 6; i++) {
                try {
                    tf[i] = Typeface.createFromAsset(am, "fonts/" + f[i] + ".ttf");
                } catch (Exception e) {
                    tf[i] = Typeface.DEFAULT;
                }
            }
        }
    }

    final Bitmap bitmap;
    private final Canvas c;
    private final Fonts fonts;
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
    private final RectF r = new RectF();
    private float shadowR;
    private int shadowC;

    AndroidGfx(int w, int h, Fonts fonts) {
        bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        c = new Canvas(bitmap);
        this.fonts = fonts;
        fill.setStyle(Paint.Style.FILL);
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeCap(Paint.Cap.ROUND);
        stroke.setStrokeJoin(Paint.Join.ROUND);
    }

    public void clear() { c.drawColor(0, PorterDuff.Mode.CLEAR); }
    public void save() { c.save(); }
    public void restore() { c.restore(); }
    public void clip(float x, float y, float w, float h) { c.clipRect(x, y, x + w, y + h); }
    public void translate(float x, float y) { c.translate(x, y); }

    public void fillRoundRect(float x, float y, float w, float h, float rad, int argb) {
        fill.setShader(null);
        fill.setColor(argb);
        r.set(x, y, x + w, y + h);
        c.drawRoundRect(r, rad, rad, fill);
    }

    public void fillRoundRectV(float x, float y, float w, float h, float rad, int top, int bottom) {
        fill.setShader(new LinearGradient(0, y, 0, y + h, top, bottom, Shader.TileMode.CLAMP));
        fill.setColor(Color.WHITE);
        r.set(x, y, x + w, y + h);
        c.drawRoundRect(r, rad, rad, fill);
        fill.setShader(null);
    }

    public void strokeRoundRect(float x, float y, float w, float h, float rad, float sw, int argb) {
        stroke.setColor(argb);
        stroke.setStrokeWidth(sw);
        r.set(x, y, x + w, y + h);
        c.drawRoundRect(r, rad, rad, stroke);
    }

    public void fillCircle(float cx, float cy, float rad, int argb) {
        fill.setShader(null);
        fill.setColor(argb);
        c.drawCircle(cx, cy, rad, fill);
    }

    public void strokeCircle(float cx, float cy, float rad, float sw, int argb) {
        stroke.setColor(argb);
        stroke.setStrokeWidth(sw);
        c.drawCircle(cx, cy, rad, stroke);
    }

    public void radial(float cx, float cy, float rad, int center, int edge) {
        fill.setShader(new RadialGradient(cx, cy, rad, center, edge, Shader.TileMode.CLAMP));
        fill.setColor(Color.WHITE);
        c.drawCircle(cx, cy, rad, fill);
        fill.setShader(null);
    }

    public void line(float x0, float y0, float x1, float y1, float sw, int argb) {
        stroke.setColor(argb);
        stroke.setStrokeWidth(sw);
        c.drawLine(x0, y0, x1, y1, stroke);
    }

    public void path(float[] xy, boolean closed, boolean doFill, float sw, int argb) {
        Path p = new Path();
        p.moveTo(xy[0], xy[1]);
        for (int i = 2; i < xy.length; i += 2) p.lineTo(xy[i], xy[i + 1]);
        if (closed) p.close();
        if (doFill) {
            fill.setShader(null);
            fill.setColor(argb);
            c.drawPath(p, fill);
        } else {
            stroke.setColor(argb);
            stroke.setStrokeWidth(sw);
            c.drawPath(p, stroke);
        }
    }

    public void arc(float cx, float cy, float rad, float startDeg, float sweepDeg, float sw, int argb) {
        stroke.setColor(argb);
        stroke.setStrokeWidth(sw);
        r.set(cx - rad, cy - rad, cx + rad, cy + rad);
        c.drawArc(r, startDeg, sweepDeg, false, stroke);
    }

    private void setup(int font, float size) {
        text.setTypeface(fonts.tf[font]);
        text.setTextSize(size);
    }

    public float text(String s, float x, float y, int font, float size, int argb, int align) {
        setup(font, size);
        float w = text.measureText(s);
        float x0 = align == CENTER ? x - w / 2 : align == RIGHT ? x - w : x;
        if (shadowR > 0) {
            text.setColor(shadowC);
            text.setMaskFilter(new BlurMaskFilter(shadowR, BlurMaskFilter.Blur.NORMAL));
            c.drawText(s, x0, y, text);
            text.setMaskFilter(null);
        }
        text.setColor(argb);
        c.drawText(s, x0, y, text);
        return w;
    }

    public float measure(String s, int font, float size) {
        setup(font, size);
        return text.measureText(s);
    }

    public void setShadow(float radius, int argb) {
        shadowR = radius;
        shadowC = argb;
    }
}
