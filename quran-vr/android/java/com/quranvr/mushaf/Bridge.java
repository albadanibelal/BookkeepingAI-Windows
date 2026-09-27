package com.quranvr.mushaf;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import com.quranvr.mushaf.ui.Store;
import com.quranvr.mushaf.ui.Ui;

import java.io.InputStream;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Everything the native render thread needs from Java. All methods are called
 *  from the render thread except the page decoder, which runs on a worker. */
final class Bridge {
    private final AssetManager assets;
    private final Ui ui;
    private final AndroidGfx[] gfx = new AndroidGfx[Ui.COUNT];

    private final ExecutorService decoder = Executors.newSingleThreadExecutor();
    private final HashSet<Integer> pending = new HashSet<>();
    private final LinkedHashMap<Integer, Bitmap> pages = new LinkedHashMap<Integer, Bitmap>(16, 0.75f, true) {
        @Override
        protected boolean removeEldestEntry(Map.Entry<Integer, Bitmap> e) { return size() > 16; }
    };

    Bridge(Context ctx) {
        assets = ctx.getAssets();
        final SharedPreferences prefs = ctx.getSharedPreferences("quranvr", Context.MODE_PRIVATE);
        ui = new Ui(new Store() {
            public int getInt(String k, int d) { return prefs.getInt(k, d); }
            public void putInt(String k, int v) { prefs.edit().putInt(k, v).apply(); }
            public String getString(String k, String d) { return prefs.getString(k, d); }
            public void putString(String k, String v) { prefs.edit().putString(k, v).apply(); }
        });
        AndroidGfx.Fonts fonts = new AndroidGfx.Fonts(assets);
        for (int i = 0; i < Ui.COUNT; i++) gfx[i] = new AndroidGfx(Ui.W[i], Ui.H[i], fonts);
    }

    // ---- UI
    int startPage() { return ui.lastPage(); }
    int[] frame(double t) { return ui.frame(t); }
    void pointer(int panel, float u, float v, int ev) { ui.pointer(panel, u, v, ev); }
    void scroll(int panel, float amount) { ui.scroll(panel, amount); }
    void onPage(int page) { ui.onPage(page); }

    Bitmap panelBitmap(int panel) {
        AndroidGfx g = gfx[panel];
        ui.draw(panel, g);
        return g.bitmap;
    }

    // ---- assets
    Bitmap loadAsset(String name) {
        try (InputStream in = assets.open(name)) {
            BitmapFactory.Options o = new BitmapFactory.Options();
            o.inPreferredConfig = Bitmap.Config.ARGB_8888;
            o.inPremultiplied = false;
            return BitmapFactory.decodeStream(in, null, o);
        } catch (Exception e) {
            android.util.Log.e("QuranVR", "asset " + name, e);
            return null;
        }
    }

    /** Returns the decoded page if ready; otherwise schedules decoding and returns null. */
    Bitmap pageBitmap(final int page) {
        synchronized (pages) {
            Bitmap b = pages.get(page);
            if (b != null) return b;
            if (pending.contains(page)) return null;
            pending.add(page);
        }
        decoder.execute(new Runnable() {
            @Override
            public void run() {
                Bitmap b = loadAsset(String.format(java.util.Locale.US, "pages/%03d.webp", page));
                synchronized (pages) {
                    pending.remove(page);
                    if (b != null) pages.put(page, b);
                }
            }
        });
        return null;
    }

    void shutdown() { decoder.shutdownNow(); }
}
