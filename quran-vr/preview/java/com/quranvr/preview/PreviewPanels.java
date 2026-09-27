package com.quranvr.preview;

import com.quranvr.mushaf.ui.Store;
import com.quranvr.mushaf.ui.Ui;

import javax.imageio.ImageIO;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.util.HashMap;

/** Renders the UI panels with Java2D into raw RGBA files for the desktop
 *  scene preview:  PreviewPanels <fontDir> <outDir> <page> [tab] */
public final class PreviewPanels {
    public static void main(String[] a) throws Exception {
        String fontDir = a[0], out = a[1];
        int page = Integer.parseInt(a[2]);
        int tab = a.length > 3 ? Integer.parseInt(a[3]) : 0;
        HashMap<String, String> m = new HashMap<>();
        Store st = new Store() {
            public int getInt(String k, int d) { return m.containsKey(k) ? Integer.parseInt(m.get(k)) : d; }
            public void putInt(String k, int v) { m.put(k, String.valueOf(v)); }
            public String getString(String k, String d) { return m.getOrDefault(k, d); }
            public void putString(String k, String v) { m.put(k, v); }
        };
        st.putString("bookmarks", "50,187,293");
        Ui ui = new Ui(st);
        ui.frame(10);
        ui.onPage(page);
        if (tab > 0) {  // open a sheet through the dock like a user would
            ui.pointer(Ui.DOCK, (tab + 0.5f) / 4f, 0.5f, Ui.EV_DOWN);
            ui.pointer(Ui.DOCK, (tab + 0.5f) / 4f, 0.5f, Ui.EV_UP);
        }
        ui.pointer(Ui.SURAHS, 0.5f, 0.62f, Ui.EV_HOVER);
        int[] f = ui.frame(11);
        new File(out).mkdirs();
        for (int p = 0; p < Ui.COUNT; p++) {
            AwtGfx g = new AwtGfx(Ui.W[p], Ui.H[p], fontDir);
            ui.draw(p, g);
            int w = Ui.W[p], h = Ui.H[p];
            int[] px = g.img.getRGB(0, 0, w, h, null, 0, w);   // non-premultiplied ARGB
            try (DataOutputStream o = new DataOutputStream(new FileOutputStream(new File(out, "panel" + p + ".rgba")))) {
                o.writeInt(w);
                o.writeInt(h);
                byte[] row = new byte[w * 4];
                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < w; x++) {
                        int c = px[y * w + x];
                        int al = c >>> 24;
                        // premultiply like an Android ARGB_8888 bitmap
                        row[x * 4] = (byte) (((c >> 16) & 255) * al / 255);
                        row[x * 4 + 1] = (byte) (((c >> 8) & 255) * al / 255);
                        row[x * 4 + 2] = (byte) ((c & 255) * al / 255);
                        row[x * 4 + 3] = (byte) al;
                    }
                    o.write(row);
                }
            }
            ImageIO.write(g.img, "png", new File(out, "panel" + p + ".png"));
        }
        System.out.println("visible=" + f[1] + " flags=" + f[3]);
    }
}
