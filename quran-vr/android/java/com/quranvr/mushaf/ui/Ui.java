package com.quranvr.mushaf.ui;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * State and drawing for the floating glass panels (surah list, title, Earth
 * info, pager, dock and the Search / Bookmarks / Settings sheet).
 * The native renderer places the panels in 3D and forwards laser-pointer
 * events here in panel UV coordinates.
 */
public final class Ui {
    public static final int SURAHS = 0, TITLE = 1, EARTH = 2, PAGER = 3, DOCK = 4, SHEET = 5, COUNT = 6;
    public static final int[] W = {736, 1300, 480, 520, 880, 736};
    public static final int[] H = {1024, 520, 400, 104, 220, 1024};

    public static final int EV_HOVER = 0, EV_DOWN = 1, EV_UP = 2, EV_LEAVE = 3;

    static final int TAB_QURAN = 0, TAB_SEARCH = 1, TAB_BOOKMARKS = 2, TAB_SETTINGS = 3;

    static final int WHITE = 0xF7FFFFFF, SECOND = 0xA8FFFFFF, THIRD = 0x70FFFFFF;
    static final int GREEN = 0xFF34C759, GOLD = 0xFFE4C582;

    static final String[] MARKER_NAME = {"Riyadh", "Makkah", "Madinah"};
    static final double[][] MARKER_LL = {{24.7136, 46.6753}, {21.4225, 39.8262}, {24.4672, 39.6112}};

    private final Store store;
    private int dirty = (1 << COUNT) - 1;
    private int page = 1;
    private int gotoPage = -1;
    private int tab = TAB_QURAN;

    boolean liveEarth = true, station = true, night = false, haptics = true;
    int marker = 0;

    // scrolling (rows)
    private float listScroll, sheetScroll;
    private double now, lastUserScroll = -100;

    // pointer
    private int hoverPanel = -1, hoverItem = -1;
    private int pressPanel = -1, pressItem = -1;
    private float pressY, dragStartScroll;
    private boolean pressDragging;

    private int pickPage = 1;
    private final List<Integer> bookmarks = new ArrayList<>();

    public Ui(Store store) {
        this.store = store;
        liveEarth = store.getInt("liveEarth", 1) != 0;
        station = store.getInt("station", 1) != 0;
        night = store.getInt("night", 0) != 0;
        haptics = store.getInt("haptics", 1) != 0;
        marker = Math.max(0, Math.min(2, store.getInt("marker", 0)));
        String b = store.getString("bookmarks", "");
        for (String s : b.split(",")) {
            try { if (!s.isEmpty()) bookmarks.add(Integer.parseInt(s.trim())); } catch (NumberFormatException ignored) { }
        }
        page = store.getInt("page", 1);
    }

    public int lastPage() { return page; }
    public boolean haptics() { return haptics; }

    // ------------------------------------------------------------ native API

    /** Called once per frame: {dirtyMask, visibleMask, gotoPage, flags, latE4, lonE4}. */
    public int[] frame(double t) {
        now = t;
        int visible = (1 << SURAHS) | (1 << TITLE) | (1 << PAGER) | (1 << DOCK);
        if (tab == TAB_QURAN) visible |= 1 << EARTH;
        else visible |= 1 << SHEET;
        int flags = (station ? 1 : 0) | (night ? 2 : 0) | (liveEarth ? 4 : 0) | (haptics ? 8 : 0);
        int[] r = {dirty, visible, gotoPage, flags,
                (int) Math.round(MARKER_LL[marker][0] * 1e4), (int) Math.round(MARKER_LL[marker][1] * 1e4)};
        dirty = 0;
        gotoPage = -1;
        return r;
    }

    public void onPage(int rightPage) {
        if (rightPage == page && (dirty & (1 << PAGER)) == 0) {
            // no change
        }
        page = rightPage;
        store.putInt("page", page);
        if (now - lastUserScroll > 4.0) ensureSurahVisible();
        dirty |= (1 << PAGER) | (1 << SURAHS) | (1 << SHEET);
    }

    public void scroll(int panel, float rows) {
        if (panel == SURAHS) {
            listScroll = clamp(listScroll + rows, 0, maxListScroll());
            lastUserScroll = now;
            dirty |= 1 << SURAHS;
        } else if (panel == SHEET && tab == TAB_BOOKMARKS) {
            sheetScroll = clamp(sheetScroll + rows, 0, maxSheetScroll());
            dirty |= 1 << SHEET;
        }
    }

    public void pointer(int panel, float u, float v, int ev) {
        float x = u * W[panel], y = v * H[panel];
        switch (ev) {
            case EV_HOVER: {
                if (pressPanel == panel && pressPanel >= 0 && (panel == SURAHS || (panel == SHEET && tab == TAB_BOOKMARKS))) {
                    float rowH = panel == SURAHS ? ROW_H : BM_ROW_H;
                    float delta = (pressY - y) / rowH;
                    if (Math.abs(delta) > 0.12f) pressDragging = true;
                    if (pressDragging) {
                        if (panel == SURAHS) {
                            listScroll = clamp(dragStartScroll + delta, 0, maxListScroll());
                            lastUserScroll = now;
                        } else {
                            sheetScroll = clamp(dragStartScroll + delta, 0, maxSheetScroll());
                        }
                        dirty |= 1 << panel;
                    }
                }
                int item = itemAt(panel, x, y);
                if (panel != hoverPanel || item != hoverItem) {
                    if (hoverPanel >= 0) dirty |= 1 << hoverPanel;
                    hoverPanel = panel;
                    hoverItem = item;
                    dirty |= 1 << panel;
                }
                break;
            }
            case EV_LEAVE:
                if (hoverPanel >= 0) dirty |= 1 << hoverPanel;
                hoverPanel = -1;
                hoverItem = -1;
                break;
            case EV_DOWN:
                pressPanel = panel;
                pressItem = itemAt(panel, x, y);
                pressY = y;
                pressDragging = false;
                dragStartScroll = panel == SURAHS ? listScroll : sheetScroll;
                dirty |= 1 << panel;
                break;
            case EV_UP: {
                int item = u < 0 ? -1 : itemAt(panel, x, y);
                if (!pressDragging && pressPanel == panel && item == pressItem && item >= 0) click(panel, item);
                pressPanel = -1;
                pressItem = -1;
                pressDragging = false;
                dirty |= 1 << panel;
                break;
            }
        }
    }

    // ------------------------------------------------------------ actions

    private void requestPage(int p) {
        gotoPage = Math.max(1, Math.min(QuranData.PAGES, p));
    }

    private void setTab(int t) {
        tab = (tab == t) ? TAB_QURAN : t;
        if (tab == TAB_SEARCH) pickPage = page;
        sheetScroll = 0;
        dirty |= (1 << DOCK) | (1 << SHEET) | (1 << EARTH);
    }

    private void click(int panel, int item) {
        switch (panel) {
            case SURAHS:
                if (item >= 0 && item < 114) {
                    requestPage(QuranData.START_PAGE[item]);
                    lastUserScroll = now;
                }
                break;
            case PAGER:
                if (item == 1) requestPage(page + 2);        // left chevron: next (Arabic direction)
                else if (item == 2) requestPage(page - 2);   // right chevron: previous
                else if (item == 0) setTab(TAB_SEARCH);
                break;
            case DOCK:
                if (item == 0) { tab = TAB_QURAN; dirty |= (1 << DOCK) | (1 << SHEET) | (1 << EARTH); }
                else if (item >= 1 && item <= 3) setTab(item);
                break;
            case SHEET:
                clickSheet(item);
                break;
        }
    }

    private void clickSheet(int item) {
        if (item == 900) { tab = TAB_QURAN; dirty |= (1 << DOCK) | (1 << EARTH); return; }
        if (tab == TAB_SEARCH) {
            if (item >= 100 && item < 130) requestPage(QuranData.JUZ_START[item - 100]);
            else if (item >= 200 && item <= 203) {
                int[] d = {-10, -1, 1, 10};
                pickPage = Math.max(1, Math.min(QuranData.PAGES, pickPage + d[item - 200]));
            } else if (item == 210) requestPage(pickPage);
        } else if (tab == TAB_BOOKMARKS) {
            if (item == 300) {
                Integer p = page;
                if (bookmarks.contains(p)) bookmarks.remove(p);
                else { bookmarks.add(p); Collections.sort(bookmarks); }
                saveBookmarks();
            } else if (item >= 500 && item < 500 + bookmarks.size()) {
                bookmarks.remove(item - 500);
                saveBookmarks();
                sheetScroll = clamp(sheetScroll, 0, maxSheetScroll());
            } else if (item >= 400 && item < 400 + bookmarks.size()) {
                requestPage(bookmarks.get(item - 400));
            }
        } else if (tab == TAB_SETTINGS) {
            if (item == 600) { liveEarth = !liveEarth; store.putInt("liveEarth", liveEarth ? 1 : 0); }
            if (item == 601) { station = !station; store.putInt("station", station ? 1 : 0); }
            if (item == 602) { night = !night; store.putInt("night", night ? 1 : 0); }
            if (item == 603) { haptics = !haptics; store.putInt("haptics", haptics ? 1 : 0); }
            if (item >= 610 && item <= 612) { marker = item - 610; store.putInt("marker", marker); dirty |= 1 << EARTH; }
        }
        dirty |= 1 << SHEET;
    }

    private void saveBookmarks() {
        StringBuilder sb = new StringBuilder();
        for (int p : bookmarks) { if (sb.length() > 0) sb.append(','); sb.append(p); }
        store.putString("bookmarks", sb.toString());
    }

    // ------------------------------------------------------------ layout

    static final float LIST_TOP = 136, ROW_H = 124, LIST_BOTTOM = 1004;
    static final float BM_TOP = 250, BM_ROW_H = 112, BM_BOTTOM = 1000;

    private float maxListScroll() { return Math.max(0, 114 - (LIST_BOTTOM - LIST_TOP) / ROW_H); }
    private float maxSheetScroll() { return Math.max(0, bookmarks.size() - (BM_BOTTOM - BM_TOP) / BM_ROW_H); }

    private void ensureSurahVisible() {
        int s = QuranData.surahOfPage(page) - 1;
        float vis = (LIST_BOTTOM - LIST_TOP) / ROW_H;
        if (s < listScroll + 0.2f || s > listScroll + vis - 1.2f) listScroll = clamp(s - 1.5f, 0, maxListScroll());
    }

    private static float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    private static boolean in(float x, float y, float x0, float y0, float w, float h) {
        return x >= x0 && x <= x0 + w && y >= y0 && y <= y0 + h;
    }

    int itemAt(int panel, float x, float y) {
        switch (panel) {
            case SURAHS: {
                if (y < LIST_TOP || y > LIST_BOTTOM) return -1;
                int i = (int) Math.floor((y - LIST_TOP) / ROW_H + listScroll);
                return (i >= 0 && i < 114) ? i : -1;
            }
            case PAGER:
                if (x < 110) return 1;
                if (x > W[PAGER] - 110) return 2;
                return 0;
            case DOCK: {
                int i = (int) (x / (W[DOCK] / 4f));
                return Math.max(0, Math.min(3, i));
            }
            case SHEET:
                return sheetItemAt(x, y);
        }
        return -1;
    }

    private int sheetItemAt(float x, float y) {
        float w = W[SHEET];
        if (Math.hypot(x - (w - 70), y - 66) < 36) return 900;
        if (tab == TAB_SEARCH) {
            for (int j = 0; j < 30; j++) {
                float[] r = juzRect(j);
                if (in(x, y, r[0], r[1], r[2], r[3])) return 100 + j;
            }
            for (int k = 0; k < 4; k++) {
                float[] r = stepRect(k);
                if (in(x, y, r[0], r[1], r[2], r[3])) return 200 + k;
            }
            if (in(x, y, 40, 900, w - 80, 84)) return 210;
        } else if (tab == TAB_BOOKMARKS) {
            if (in(x, y, 40, 128, w - 80, 92)) return 300;
            if (y >= BM_TOP && y <= BM_BOTTOM) {
                int i = (int) Math.floor((y - BM_TOP) / BM_ROW_H + sheetScroll);
                if (i >= 0 && i < bookmarks.size()) return x > w - 120 ? 500 + i : 400 + i;
            }
        } else if (tab == TAB_SETTINGS) {
            for (int k = 0; k < 4; k++) if (in(x, y, 30, 130 + k * 124, w - 60, 116)) return 600 + k;
            for (int k = 0; k < 3; k++) {
                float[] r = segRect(k);
                if (in(x, y, r[0], r[1], r[2], r[3])) return 610 + k;
            }
        }
        return -1;
    }

    private float[] juzRect(int j) {
        float gw = (W[SHEET] - 80 - 4 * 14) / 5f;
        int c = j % 5, r = j / 5;
        return new float[]{40 + c * (gw + 14), 172 + r * 90, gw, 76};
    }

    private float[] stepRect(int k) {
        float x = k < 2 ? 40 + k * 124 : W[SHEET] - 40 - (4 - k) * 124 + 10;
        return new float[]{x, 776, 114, 84};
    }

    private float[] segRect(int k) {
        float w = (W[SHEET] - 80) / 3f;
        return new float[]{40 + k * w, 700, w, 76};
    }

    // ------------------------------------------------------------ drawing

    public void draw(int panel, Gfx g) {
        g.clear();
        switch (panel) {
            case SURAHS: drawSurahs(g); break;
            case TITLE: drawTitle(g); break;
            case EARTH: drawEarth(g); break;
            case PAGER: drawPager(g); break;
            case DOCK: drawDock(g); break;
            case SHEET: drawSheet(g); break;
        }
    }

    private static void glass(Gfx g, float w, float h, float r) { glass(g, w, h, r, 0x6E1A2130); }

    private static void glass(Gfx g, float w, float h, float r, int tint) {
        g.fillRoundRect(0, 0, w, h, r, tint);
        g.fillRoundRectV(0, 0, w, h, r, 0x2EFFFFFF, 0x0DFFFFFF);
        g.strokeRoundRect(1.5f, 1.5f, w - 3, h - 3, r - 1.5f, 2.5f, 0x40FFFFFF);
    }

    private boolean hovered(int panel, int item) { return hoverPanel == panel && hoverItem == item; }
    private boolean pressed(int panel, int item) { return pressPanel == panel && pressItem == item && !pressDragging; }

    private void drawSurahs(Gfx g) {
        float w = W[SURAHS], h = H[SURAHS];
        glass(g, w, h, 46);
        Icons.chevron(g, 52, 70, 22, false, WHITE, 4.5f);
        g.text("Surahs", 84, 84, Gfx.SEMIBOLD, 42, WHITE, Gfx.LEFT);
        g.text("114", w - 48, 84, Gfx.MEDIUM, 28, THIRD, Gfx.RIGHT);
        g.line(32, 124, w - 32, 124, 2, 0x1FFFFFFF);

        int current = QuranData.surahOfPage(page) - 1;
        g.save();
        g.clip(0, LIST_TOP, w, LIST_BOTTOM - LIST_TOP);
        int first = (int) Math.floor(listScroll);
        for (int i = first; i < Math.min(114, first + 9); i++) {
            float y = LIST_TOP + (i - listScroll) * ROW_H;
            boolean sel = i == current;
            if (sel) {
                g.fillRoundRect(22, y + 6, w - 44, ROW_H - 12, 26, 0x2EFFFFFF);
                g.strokeRoundRect(22, y + 6, w - 44, ROW_H - 12, 26, 2, 0x26FFFFFF);
                g.fillRoundRect(6, y + 26, 7, ROW_H - 52, 3.5f, 0xF2FFFFFF);
            } else if (hovered(SURAHS, i)) {
                g.fillRoundRect(22, y + 6, w - 44, ROW_H - 12, 26, pressed(SURAHS, i) ? 0x24FFFFFF : 0x14FFFFFF);
            }
            g.text(String.valueOf(i + 1), 66, y + 74, Gfx.MEDIUM, 34, sel ? WHITE : SECOND, Gfx.CENTER);
            g.text(QuranData.NAME[i], 122, y + 58, Gfx.MEDIUM, 36, WHITE, Gfx.LEFT);
            g.text(QuranData.VERSES[i] + " verses", 122, y + 98, Gfx.REGULAR, 27, SECOND, Gfx.LEFT);
            g.text(QuranData.NAME_AR[i], w - 52, y + 76, Gfx.ARABIC, 42, sel ? WHITE : SECOND, Gfx.RIGHT);
            if (!sel && i + 1 < 114 && i + 1 != current) g.line(122, y + ROW_H, w - 40, y + ROW_H, 1.5f, 0x14FFFFFF);
        }
        g.restore();
        // scroll indicator
        float vis = (LIST_BOTTOM - LIST_TOP) / ROW_H;
        float th = (LIST_BOTTOM - LIST_TOP) * vis / 114f;
        float ty = LIST_TOP + (LIST_BOTTOM - LIST_TOP - th) * (listScroll / Math.max(0.001f, maxListScroll()));
        g.fillRoundRect(w - 14, ty, 5, th, 2.5f, 0x40FFFFFF);
    }

    private void drawTitle(Gfx g) {
        float w = W[TITLE];
        float cx = w / 2;
        g.radial(cx, 250, 520, 0x1A6FA8FF, 0x00000000);
        Icons.ornament(g, cx, 70, 44, GOLD);
        g.setShadow(18, 0x99000814);
        g.text("القرآن الكريم", cx, 248, Gfx.ARABIC_BOLD, 112, 0xFFF3E7C9, Gfx.CENTER);
        g.text("The Holy Quran", cx, 382, Gfx.MEDIUM, 58, WHITE, Gfx.CENTER);
        g.text("Mushaf Al-Madina", cx, 446, Gfx.REGULAR, 38, 0xE6FFFFFF, Gfx.CENTER);
        g.text("The authentic Quran, as it was revealed", cx, 496, Gfx.REGULAR, 30, 0xB8FFFFFF, Gfx.CENTER);
        g.setShadow(0, 0);
    }

    private void drawEarth(Gfx g) {
        float w = W[EARTH], h = H[EARTH];
        glass(g, w, h, 42);
        Icons.globe(g, 72, 104, 30, WHITE);
        g.text("Earth", 128, 98, Gfx.SEMIBOLD, 40, WHITE, Gfx.LEFT);
        g.fillCircle(136, 128, 7, GREEN);
        g.text("Live view", 152, 139, Gfx.REGULAR, 29, SECOND, Gfx.LEFT);
        g.line(32, 200, w - 32, 200, 2, 0x1FFFFFFF);
        Icons.pin(g, 72, 296, 30, WHITE);
        g.text("Position", 128, 284, Gfx.SEMIBOLD, 36, WHITE, Gfx.LEFT);
        g.text(MARKER_NAME[marker], w - 36, 284, Gfx.MEDIUM, 27, THIRD, Gfx.RIGHT);
        g.text(coords(), 128, 330, Gfx.REGULAR, 28, SECOND, Gfx.LEFT);
    }

    private String coords() {
        double lat = MARKER_LL[marker][0], lon = MARKER_LL[marker][1];
        return String.format(java.util.Locale.US, "%.4f° %s, %.4f° %s",
                Math.abs(lat), lat >= 0 ? "N" : "S", Math.abs(lon), lon >= 0 ? "E" : "W");
    }

    private void drawPager(Gfx g) {
        float w = W[PAGER], h = H[PAGER];
        glass(g, w, h, h / 2, 0xC2141A26);
        if (hovered(PAGER, 1)) g.fillCircle(56, h / 2, 38, pressed(PAGER, 1) ? 0x33FFFFFF : 0x1FFFFFFF);
        if (hovered(PAGER, 2)) g.fillCircle(w - 56, h / 2, 38, pressed(PAGER, 2) ? 0x33FFFFFF : 0x1FFFFFFF);
        boolean canNext = page + 2 <= QuranData.PAGES, canPrev = page > 1;
        Icons.chevron(g, 56, h / 2, 16, false, canNext ? WHITE : THIRD, 4.5f);
        Icons.chevron(g, w - 56, h / 2, 16, true, canPrev ? WHITE : THIRD, 4.5f);
        int left = Math.min(QuranData.PAGES, page + 1);
        String label = "Pages " + page + "–" + left + " of " + QuranData.PAGES;
        g.text(label, w / 2, h / 2 + 12, Gfx.MEDIUM, 33, hovered(PAGER, 0) ? WHITE : 0xE6FFFFFF, Gfx.CENTER);
    }

    private void drawDock(Gfx g) {
        float w = W[DOCK], h = H[DOCK];
        glass(g, w, h, 64, 0xC2141A26);
        String[] labels = {"Quran", "Search", "Bookmarks", "Settings"};
        float cw = w / 4f;
        for (int i = 0; i < 4; i++) {
            float cx = cw * i + cw / 2, cy = 86;
            boolean active = tab == i;
            if (active) {
                g.fillCircle(cx, cy, 54, 0x3DFFFFFF);
                g.strokeCircle(cx, cy, 54, 2.5f, 0x59FFFFFF);
            } else if (hovered(DOCK, i)) {
                g.fillCircle(cx, cy, 54, pressed(DOCK, i) ? 0x2EFFFFFF : 0x1AFFFFFF);
            }
            int col = active ? WHITE : 0xD9FFFFFF;
            switch (i) {
                case 0: Icons.book(g, cx, cy, 58, col); break;
                case 1: Icons.search(g, cx, cy, 58, col); break;
                case 2: Icons.bookmark(g, cx, cy, 58, col, false); break;
                case 3: Icons.gear(g, cx, cy, 58, col); break;
            }
            g.text(labels[i], cx, 184, Gfx.MEDIUM, 27, active ? WHITE : SECOND, Gfx.CENTER);
        }
    }

    private void button(Gfx g, int panel, int item, float x, float y, float w, float h, float r,
                        boolean selected, String label, int font, float size) {
        int bg = selected ? 0x4DFFFFFF : (pressed(panel, item) ? 0x33FFFFFF : hovered(panel, item) ? 0x24FFFFFF : 0x14FFFFFF);
        g.fillRoundRect(x, y, w, h, r, bg);
        if (selected) g.strokeRoundRect(x, y, w, h, r, 2, 0x66FFFFFF);
        g.text(label, x + w / 2, y + h / 2 + size * 0.36f, font, size, WHITE, Gfx.CENTER);
    }

    private void drawSheet(Gfx g) {
        float w = W[SHEET], h = H[SHEET];
        glass(g, w, h, 46);
        String title = tab == TAB_SEARCH ? "Go to" : tab == TAB_BOOKMARKS ? "Bookmarks" : "Settings";
        g.text(title, 44, 84, Gfx.SEMIBOLD, 42, WHITE, Gfx.LEFT);
        g.fillCircle(w - 70, 66, 30, hovered(SHEET, 900) ? 0x33FFFFFF : 0x1FFFFFFF);
        g.line(w - 81, 55, w - 59, 77, 4, WHITE);
        g.line(w - 59, 55, w - 81, 77, 4, WHITE);
        g.line(32, 124, w - 32, 124, 2, 0x1FFFFFFF);

        if (tab == TAB_SEARCH) {
            g.text("Juz’", 44, 160, Gfx.MEDIUM, 28, SECOND, Gfx.LEFT);
            int cur = QuranData.juzOfPage(page) - 1;
            for (int j = 0; j < 30; j++) {
                float[] r = juzRect(j);
                button(g, SHEET, 100 + j, r[0], r[1], r[2], r[3], 20, j == cur, String.valueOf(j + 1), Gfx.MEDIUM, 32);
            }
            g.text("Page", 44, 756, Gfx.MEDIUM, 28, SECOND, Gfx.LEFT);
            String[] st = {"−10", "−1", "+1", "+10"};
            for (int k = 0; k < 4; k++) {
                float[] r = stepRect(k);
                button(g, SHEET, 200 + k, r[0], r[1], r[2], r[3], 22, false, st[k], Gfx.MEDIUM, 30);
            }
            g.text(String.valueOf(pickPage), w / 2, 834, Gfx.SEMIBOLD, 54, WHITE, Gfx.CENTER);
            String sn = QuranData.NAME[QuranData.surahOfPage(pickPage) - 1];
            g.text(sn, w / 2, 872, Gfx.REGULAR, 24, SECOND, Gfx.CENTER);
            int bg = pressed(SHEET, 210) ? 0xFF2A9D4A : hovered(SHEET, 210) ? 0xFF3AD063 : GREEN;
            g.fillRoundRect(40, 900, w - 80, 84, 42, bg);
            g.text("Open page " + pickPage, w / 2, 954, Gfx.SEMIBOLD, 32, 0xFFFFFFFF, Gfx.CENTER);
        } else if (tab == TAB_BOOKMARKS) {
            boolean marked = bookmarks.contains(page);
            button(g, SHEET, 300, 40, 128, w - 80, 92, 46, false, "", Gfx.MEDIUM, 30);
            Icons.bookmark(g, 104, 174, 40, marked ? GOLD : WHITE, marked);
            g.text(marked ? "Remove bookmark · page " + page : "Bookmark page " + page, 140, 185, Gfx.MEDIUM, 31, WHITE, Gfx.LEFT);
            if (bookmarks.isEmpty()) {
                g.text("No bookmarks yet", w / 2, 520, Gfx.MEDIUM, 32, SECOND, Gfx.CENTER);
                g.text("Save your place to return to it later.", w / 2, 566, Gfx.REGULAR, 26, THIRD, Gfx.CENTER);
            }
            g.save();
            g.clip(0, BM_TOP, w, BM_BOTTOM - BM_TOP);
            int first = (int) Math.floor(sheetScroll);
            for (int i = first; i < Math.min(bookmarks.size(), first + 9); i++) {
                float y = BM_TOP + (i - sheetScroll) * BM_ROW_H;
                int p = bookmarks.get(i);
                if (hovered(SHEET, 400 + i)) g.fillRoundRect(24, y + 6, w - 48, BM_ROW_H - 12, 24, 0x17FFFFFF);
                Icons.bookmark(g, 70, y + BM_ROW_H / 2, 34, GOLD, true);
                g.text("Page " + p, 110, y + 52, Gfx.MEDIUM, 34, WHITE, Gfx.LEFT);
                int s = QuranData.surahOfPage(p) - 1;
                g.text(QuranData.NAME[s] + " · Juz’ " + QuranData.juzOfPage(p), 110, y + 90, Gfx.REGULAR, 26, SECOND, Gfx.LEFT);
                float cx = w - 70, cy = y + BM_ROW_H / 2;
                g.fillCircle(cx, cy, 26, hovered(SHEET, 500 + i) ? 0x33FFFFFF : 0x14FFFFFF);
                g.line(cx - 9, cy - 9, cx + 9, cy + 9, 3.5f, SECOND);
                g.line(cx + 9, cy - 9, cx - 9, cy + 9, 3.5f, SECOND);
            }
            g.restore();
        } else {
            String[][] rows = {
                    {"Live Earth", "Real-time day, night & rotation"},
                    {"Space station", "Show the observation deck"},
                    {"Night reading", "Warm, dimmed pages"},
                    {"Haptics", "Controller vibration"}};
            boolean[] on = {liveEarth, station, night, haptics};
            for (int k = 0; k < 4; k++) {
                float y = 130 + k * 124;
                if (hovered(SHEET, 600 + k)) g.fillRoundRect(24, y + 4, w - 48, 112, 24, 0x14FFFFFF);
                g.text(rows[k][0], 48, y + 54, Gfx.MEDIUM, 34, WHITE, Gfx.LEFT);
                g.text(rows[k][1], 48, y + 92, Gfx.REGULAR, 25, SECOND, Gfx.LEFT);
                Icons.toggle(g, w - 130, y + 36, on[k], GREEN);
                if (k < 3) g.line(48, y + 122, w - 48, y + 122, 1.5f, 0x14FFFFFF);
            }
            g.text("Earth marker", 48, 680, Gfx.MEDIUM, 28, SECOND, Gfx.LEFT);
            for (int k = 0; k < 3; k++) {
                float[] r = segRect(k);
                button(g, SHEET, 610 + k, r[0] + 3, r[1], r[2] - 6, r[3], 22, marker == k, MARKER_NAME[k], Gfx.MEDIUM, 29);
            }
            g.text("Turning pages", 48, 836, Gfx.MEDIUM, 28, SECOND, Gfx.LEFT);
            g.text("Point at a page, hold the trigger and swipe,", 48, 878, Gfx.REGULAR, 25, THIRD, Gfx.LEFT);
            g.text("or grab it with the grip. Thumbstick ←/→ and", 48, 912, Gfx.REGULAR, 25, THIRD, Gfx.LEFT);
            g.text("A/X (next) · B/Y (back) also turn pages.", 48, 946, Gfx.REGULAR, 25, THIRD, Gfx.LEFT);
            g.text("Mushaf text: King Fahd Complex (Hafs ‘an ‘Asim)", 48, 994, Gfx.REGULAR, 21, THIRD, Gfx.LEFT);
        }
    }
}
