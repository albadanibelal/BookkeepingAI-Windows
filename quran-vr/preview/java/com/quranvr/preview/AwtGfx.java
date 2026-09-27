package com.quranvr.preview;

import com.quranvr.mushaf.ui.Gfx;

import java.awt.*;
import java.awt.font.FontRenderContext;
import java.awt.font.TextLayout;
import java.awt.geom.*;
import java.awt.image.BufferedImage;
import java.io.File;
import java.util.ArrayDeque;

/** Java2D implementation of Gfx for the desktop preview. */
final class AwtGfx implements Gfx {
    final BufferedImage img;
    private Graphics2D g;
    private final ArrayDeque<Graphics2D> stack = new ArrayDeque<>();
    private final Font[] fonts = new Font[6];
    private float shadowR;
    private int shadowC;

    AwtGfx(int w, int h, String fontDir) throws Exception {
        img = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB_PRE);
        String[] f = {"Inter-Regular", "Inter-Medium", "Inter-SemiBold", "Inter-Bold", "Amiri-Regular", "Amiri-Bold"};
        for (int i = 0; i < 6; i++) fonts[i] = Font.createFont(Font.TRUETYPE_FONT, new File(fontDir, f[i] + ".ttf"));
        g = img.createGraphics();
        hints(g);
    }

    private static void hints(Graphics2D g) {
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
        g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL, RenderingHints.VALUE_STROKE_PURE);
        g.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
    }

    private static Color c(int argb) { return new Color(argb, true); }

    public void clear() {
        g.setComposite(AlphaComposite.Clear);
        g.fillRect(0, 0, img.getWidth(), img.getHeight());
        g.setComposite(AlphaComposite.SrcOver);
    }
    public void save() { stack.push(g); g = (Graphics2D) g.create(); }
    public void restore() { g.dispose(); g = stack.pop(); }
    public void clip(float x, float y, float w, float h) { g.clip(new Rectangle2D.Float(x, y, w, h)); }
    public void translate(float x, float y) { g.translate(x, y); }

    private static Shape rr(float x, float y, float w, float h, float r) {
        return new RoundRectangle2D.Float(x, y, w, h, 2 * r, 2 * r);
    }
    public void fillRoundRect(float x, float y, float w, float h, float r, int argb) { g.setColor(c(argb)); g.fill(rr(x, y, w, h, r)); }
    public void fillRoundRectV(float x, float y, float w, float h, float r, int top, int bottom) {
        g.setPaint(new GradientPaint(0, y, c(top), 0, y + h, c(bottom)));
        g.fill(rr(x, y, w, h, r));
    }
    public void strokeRoundRect(float x, float y, float w, float h, float r, float sw, int argb) {
        g.setColor(c(argb)); g.setStroke(new BasicStroke(sw)); g.draw(rr(x, y, w, h, r));
    }
    public void fillCircle(float cx, float cy, float r, int argb) { g.setColor(c(argb)); g.fill(new Ellipse2D.Float(cx - r, cy - r, 2 * r, 2 * r)); }
    public void strokeCircle(float cx, float cy, float r, float sw, int argb) {
        g.setColor(c(argb)); g.setStroke(new BasicStroke(sw)); g.draw(new Ellipse2D.Float(cx - r, cy - r, 2 * r, 2 * r));
    }
    public void radial(float cx, float cy, float r, int center, int edge) {
        g.setPaint(new RadialGradientPaint(cx, cy, r, new float[]{0, 1}, new Color[]{c(center), c(edge)}));
        g.fill(new Ellipse2D.Float(cx - r, cy - r, 2 * r, 2 * r));
    }
    public void line(float x0, float y0, float x1, float y1, float sw, int argb) {
        g.setColor(c(argb)); g.setStroke(new BasicStroke(sw, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
        g.draw(new Line2D.Float(x0, y0, x1, y1));
    }
    public void path(float[] xy, boolean closed, boolean fill, float sw, int argb) {
        Path2D.Float p = new Path2D.Float();
        p.moveTo(xy[0], xy[1]);
        for (int i = 2; i < xy.length; i += 2) p.lineTo(xy[i], xy[i + 1]);
        if (closed) p.closePath();
        g.setColor(c(argb));
        if (fill) g.fill(p);
        else { g.setStroke(new BasicStroke(sw, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND)); g.draw(p); }
    }
    public void arc(float cx, float cy, float r, float startDeg, float sweepDeg, float sw, int argb) {
        g.setColor(c(argb)); g.setStroke(new BasicStroke(sw, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
        g.draw(new Arc2D.Float(cx - r, cy - r, 2 * r, 2 * r, -startDeg, -sweepDeg, Arc2D.OPEN));
    }
    public float text(String s, float x, float y, int font, float size, int argb, int align) {
        if (s.isEmpty()) return 0;
        Font f = fonts[font].deriveFont(size);
        FontRenderContext frc = g.getFontRenderContext();
        TextLayout tl = new TextLayout(s, f, frc);
        float w = tl.getAdvance();
        float x0 = align == CENTER ? x - w / 2 : align == RIGHT ? x - w : x;
        if (shadowR > 0) {
            int a = (shadowC >>> 24) / 6;
            g.setColor(c((a << 24) | (shadowC & 0xffffff)));
            for (int i = 0; i < 12; i++) {
                double ang = Math.PI * 2 * i / 12;
                tl.draw(g, x0 + (float) Math.cos(ang) * shadowR * 0.35f, y + (float) Math.sin(ang) * shadowR * 0.35f);
            }
        }
        g.setColor(c(argb));
        tl.draw(g, x0, y);
        return w;
    }
    public float measure(String s, int font, float size) {
        if (s.isEmpty()) return 0;
        return new TextLayout(s, fonts[font].deriveFont(size), g.getFontRenderContext()).getAdvance();
    }
    public void setShadow(float radius, int argb) { shadowR = radius; shadowC = argb; }
}
