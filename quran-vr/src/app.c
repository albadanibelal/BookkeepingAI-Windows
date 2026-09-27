// Holy Quran VR — reader logic and user interface.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gfx.h"
#include "pages.h"
#include "platform.h"
#include "quran.h"
#include "scene.h"
#include "ui.h"

enum { MODE_SURAHS, MODE_SEARCH, MODE_BOOKMARKS, MODE_SETTINGS };

// Colors (0xRRGGBBAA, sRGB)
#define C_WHITE 0xFFFFFFFFu
#define C_TEXT 0xF4F7FBFFu
#define C_DIM 0xB9C3D3D0u
#define C_FAINT 0x9AA6B880u
#define C_ACCENT 0x8FD3FFFFu
#define C_GOLD 0xE6C97BFFu
#define C_GLASS_T 0x273149C8u
#define C_GLASS_B 0x121828DCu
#define C_EDGE 0xFFFFFF30u
#define C_HOVER 0xFFFFFF1Cu

static ReaderState st;
static SceneState sc;
static Panel p_side, p_toolbar, p_pill, p_card, p_title;
static Panel* panels[] = {&p_title, &p_card, &p_side, &p_pill, &p_toolbar};
#define NPANELS (int)(sizeof(panels) / sizeof(panels[0]))

static int mode = MODE_SURAHS;
static float list_scroll, list_vel;
static bool list_dragging[2];
static float drag_last_y[2];
static char query[24];
static SearchResult results[24];
static int nresults;
static bool ui_visible = true;
static float ui_fade = 1;
static bool centered;
static int pending_turn;  // queued page turns (+ forward / - backward)
static int jump_target;   // queued jump (right page) or 0
static float stick_cool[2];
static bool prev_btn_a[2], prev_btn_b[2], prev_menu, prev_trig[2];
static float save_timer = -1;
static int img_icon[32];
static bool scroll_to_current = true;

enum { I_BOOK, I_SEARCH, I_BOOKMARK, I_BOOKMARK_FILL, I_SETTINGS, I_CHEV_L, I_CHEV_R, I_CHEV_U, I_CHEV_D, I_GLOBE, I_PIN,
       I_WAVE, I_CLOSE, I_CHECK, I_PLUS, I_MINUS, I_TRASH, I_BACKSPACE, I_SUN, I_MOON, I_RECENTER, I_EYE, I_LIST, I_INFO,
       I_ROSETTE, I_KAABA, I_DOT, I_RING, I_GLOW, I_TITLE, I_COUNT };
static const char* icon_names[I_COUNT] = {"book", "search", "bookmark", "bookmark_fill", "settings", "chev_left", "chev_right",
                                          "chev_up", "chev_down", "globe", "pin", "wave", "close", "check", "plus", "minus",
                                          "trash", "backspace", "sun", "moon", "recenter", "eye", "list", "info", "rosette",
                                          "kaaba", "dot", "ring", "glow", "title"};
static int img_sura[QURAN_SURAHS + 1];

static void mark_dirty(void) { save_timer = 1.0f; }

// ------------------------------------------------------------ page navigation
static int clamp_right(int p) {
  if (p < 1) p = 1;
  if (p > QURAN_PAGES) p = QURAN_PAGES;
  return (p & 1) ? p : p - 1;
}

static void request_spread(int right, int prio) {
  pages_request(right, prio);
  pages_request(right + 1, prio);
}

static void start_flip(int target_right) {
  int cur = st.page;
  if (target_right == cur) return;
  Flip* f = &sc.flip;
  f->active = true;
  f->t = 0;
  if (target_right > cur) {
    f->dir = 1;
    f->front_page = cur + 1;      // old left page lifts
    f->back_page = target_right;  // lands on the right
    sc.right_page = cur;
    sc.left_page = target_right + 1;
  } else {
    f->dir = -1;
    f->front_page = cur;              // old right page lifts
    f->back_page = target_right + 1;  // lands on the left
    sc.right_page = target_right;
    sc.left_page = cur + 1;
  }
  st.page = target_right;
  request_spread(target_right, 10);
  mark_dirty();
}

static void finish_flip(void) {
  sc.flip.active = false;
  sc.right_page = st.page;
  sc.left_page = st.page + 1;
}

static void go_page(int page) {
  int r = clamp_right(page);
  if (sc.flip.active) jump_target = r;
  else start_flip(r);
  scroll_to_current = true;
}

static void turn(int dir) {
  if (sc.flip.active) {
    pending_turn += dir;
    return;
  }
  int r = clamp_right(st.page + 2 * dir);
  if (r != st.page) start_flip(r);
}

// ------------------------------------------------------------ layout helpers
static void place_facing(Panel* p, v3 pos) {
  v3 head = V3(0, sc.head_h, 0);
  v3 d = v3_sub(head, pos);
  float yaw = atan2f(d.x, d.z);
  float pitch = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
  p->model = m4_mul(sc.content, m4_mul(m4_translate(pos), m4_mul(m4_rot_y(yaw), m4_rot_x(-pitch))));
}

static void glass(Rect r, float radius) {
  ui_glow(R(r.x, r.y + 10, r.w, r.h), radius, 26, 0x0000004Cu);
  ui_rect(r, radius, C_GLASS_T, C_GLASS_B);
  ui_rect(R(r.x + 2, r.y + 2, r.w - 4, r.h * 0.45f), radius - 2, 0xFFFFFF14u, 0xFFFFFF00u);
  ui_border(r, radius, 1.6f, C_EDGE);
}

static void icon(int id, float cx, float cy, float size, uint32_t color) {
  ui_image(img_icon[id], R(cx - size * 0.5f, cy - size * 0.5f, size, size), color);
}

static bool button(const char* key, int n, Rect r, float radius, bool active) {
  uint32_t id = ui_id(key, n);
  bool hov = ui_hovered(r);
  float h = ui_anim(id, hov ? 1.0f : 0.0f, 14);
  if (active) ui_rect(r, radius, 0xFFFFFF30u, 0xFFFFFF22u);
  if (h > 0.01f) ui_rect(r, radius, (0xFFFFFF00u | (uint32_t)(h * 34)), (0xFFFFFF00u | (uint32_t)(h * 26)));
  return ui_clicked(r);
}

static void fmt_verses(char* buf, int n, const Surah* s) { snprintf(buf, n, "%d verses \xC2\xB7 %s", s->ayahs, s->meccan ? "Meccan" : "Medinan"); }

// ------------------------------------------------------------ side panel modes
static void side_header(const char* title, const char* sub) {
  icon(I_CHEV_L, 40, 50, 30, C_DIM);
  ui_text(66, 62, title, 34, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
  if (sub) ui_text(p_side.w - 36, 60, sub, 21, FONT_REGULAR, C_FAINT, ALIGN_RIGHT);
  ui_rect(R(28, 96, p_side.w - 56, 1.2f), 0, 0xFFFFFF22u, 0xFFFFFF22u);
}

static void scroll_list(Rect area, float content_h, int hovered_hand) {
  float max_scroll = fmaxf(0, content_h - area.h);
  for (int h = 0; h < 2; h++) {
    Pointer* p = &ui_ptr[h];
    bool over = p->valid && p->panel == &p_side && rect_has(area, p->x, p->y);
    if (over && fabsf(p->scroll) > 0.15f) list_vel = -p->scroll * 1400.0f;
    if (over && p->pressed) {
      list_dragging[h] = true;
      drag_last_y[h] = p->y;
    }
    if (list_dragging[h]) {
      if (!p->down || !p->valid || p->panel != &p_side) list_dragging[h] = false;
      else {
        float dy = p->y - drag_last_y[h];
        list_scroll -= dy;
        list_vel = -dy * 30.0f;
        drag_last_y[h] = p->y;
      }
    }
  }
  (void)hovered_hand;
  list_scroll = clampf(list_scroll, 0, max_scroll);
  // scrollbar
  if (max_scroll > 0) {
    float frac = area.h / content_h;
    float bh = fmaxf(40, area.h * frac);
    float by = area.y + (area.h - bh) * (list_scroll / max_scroll);
    ui_rect(R(area.x + area.w + 6, by, 5, bh), 2.5f, 0xFFFFFF40u, 0xFFFFFF40u);
  }
}

static void mode_surahs(float dt) {
  side_header("Surahs", "114");
  Rect area = R(16, 108, p_side.w - 44, p_side.h - 124);
  const float row_h = 84;
  int cur_surah = quran_surah_at_page(sc.flip.active ? st.page : sc.right_page);
  if (scroll_to_current) {
    list_scroll = (cur_surah - 1) * row_h - area.h * 0.35f;
    scroll_to_current = false;
  }
  list_scroll += list_vel * dt;
  list_vel = approachf(list_vel, 0, 5, dt);
  scroll_list(area, QURAN_SURAHS * row_h, 0);
  ui_push_clip(area);
  for (int i = 1; i <= QURAN_SURAHS; i++) {
    float y = area.y + (i - 1) * row_h - list_scroll;
    if (y + row_h < area.y || y > area.y + area.h) continue;
    Rect row = R(area.x + 6, y + 4, area.w - 12, row_h - 8);
    bool sel = i == cur_surah;
    if (sel) {
      ui_rect(row, 18, 0xFFFFFF30u, 0xFFFFFF20u);
      ui_border(row, 18, 1.2f, 0xFFFFFF30u);
      ui_rect(R(row.x - 2, row.y + 16, 4, row.h - 32), 2, C_ACCENT, C_ACCENT);
    }
    if (button("surah", i, row, 18, false)) go_page(Q.surah[i].start_page);
    char num[8], sub[48];
    snprintf(num, sizeof(num), "%d", i);
    ui_text(row.x + 34, y + 50, num, 24, FONT_REGULAR, sel ? C_TEXT : C_DIM, ALIGN_CENTER);
    ui_text(row.x + 72, y + 40, Q.surah[i].translit, 26, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
    fmt_verses(sub, sizeof(sub), &Q.surah[i]);
    ui_text(row.x + 72, y + 66, sub, 18, FONT_REGULAR, C_FAINT, ALIGN_LEFT);
    float ah = 30;
    float aw = ah * ui_image_aspect(img_sura[i]);
    ui_image(img_sura[i], R(row.x + row.w - 56 - aw, y + row_h * 0.5f - ah * 0.5f, aw, ah), sel ? C_GOLD : 0xD9C48CC8u);
    if (sel) icon(I_WAVE, row.x + row.w - 26, y + row_h * 0.5f, 26, C_ACCENT);
    else icon(I_CHEV_R, row.x + row.w - 26, y + row_h * 0.5f, 18, 0xFFFFFF50u);
  }
  ui_pop_clip();
}

static const char* KEYROWS[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL:", "ZXCVBNM"};

static void run_search(void) { nresults = quran_search(query, results, 24); }

static void mode_search(float dt) {
  (void)dt;
  side_header("Search", NULL);
  // Query field
  Rect q = R(28, 116, p_side.w - 56, 58);
  ui_rect(q, 16, 0x0A0F1A80u, 0x0A0F1A90u);
  ui_border(q, 16, 1.4f, 0xFFFFFF38u);
  icon(I_SEARCH, q.x + 30, q.y + 29, 26, C_DIM);
  if (query[0]) {
    float w = ui_text(q.x + 56, q.y + 38, query, 25, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
    if (fmod(sc.time, 1.0) < 0.55) ui_rect(R(q.x + 60 + w, q.y + 15, 2.5f, 30), 1, C_ACCENT, C_ACCENT);
  } else {
    ui_text(q.x + 56, q.y + 37, "Surah, number, page or 2:255", 21, FONT_REGULAR, C_FAINT, ALIGN_LEFT);
  }
  // Keyboard
  float ky = 190, kw = 46, kh = 50, gap = 5;
  for (int r = 0; r < 4; r++) {
    int n = (int)strlen(KEYROWS[r]);
    float extra = r == 3 ? 2 * (kw + gap) + 30 : 0;
    float x0 = (p_side.w - (n * (kw + gap) - gap + extra)) * 0.5f;
    for (int k = 0; k < n; k++) {
      Rect kr = R(x0 + k * (kw + gap), ky + r * (kh + gap), kw, kh);
      ui_rect(kr, 10, 0xFFFFFF1Au, 0xFFFFFF12u);
      char ch[2] = {KEYROWS[r][k], 0};
      if (button("key", r * 16 + k, kr, 10, false) && strlen(query) < sizeof(query) - 1) {
        size_t l = strlen(query);
        query[l] = ch[0] >= 'A' && ch[0] <= 'Z' ? (char)(ch[0] - 'A' + 'a') : ch[0];
        query[l + 1] = 0;
        run_search();
      }
      ui_text(kr.x + kw * 0.5f, kr.y + 34, ch, 23, FONT_SEMIBOLD, C_TEXT, ALIGN_CENTER);
    }
    if (r == 3) {
      Rect bk = R(x0 + n * (kw + gap) + 8, ky + r * (kh + gap), kw * 2 + gap, kh);
      ui_rect(bk, 10, 0xFFFFFF22u, 0xFFFFFF18u);
      if (button("bksp", 0, bk, 10, false) && query[0]) {
        query[strlen(query) - 1] = 0;
        run_search();
      }
      icon(I_BACKSPACE, bk.x + bk.w * 0.5f, bk.y + kh * 0.5f, 28, C_TEXT);
    }
  }
  float y = ky + 4 * (kh + gap) + 14;
  ui_rect(R(28, y, p_side.w - 56, 1.2f), 0, 0xFFFFFF22u, 0xFFFFFF22u);
  y += 14;
  if (!query[0]) {
    ui_text(30, y + 22, "Go to Juz", 21, FONT_SEMIBOLD, C_DIM, ALIGN_LEFT);
    y += 36;
    float bw = (p_side.w - 56 - 5 * 8) / 6.0f, bh = 44;
    for (int j = 1; j <= QURAN_JUZ; j++) {
      int cx = (j - 1) % 6, cy = (j - 1) / 6;
      Rect b = R(28 + cx * (bw + 8), y + cy * (bh + 8), bw, bh);
      bool cur = Q.page[st.page].juz == j;
      ui_rect(b, 12, cur ? 0x8FD3FF55u : 0xFFFFFF14u, cur ? 0x8FD3FF40u : 0xFFFFFF0Cu);
      if (button("juz", j, b, 12, false)) go_page(Q.juz[j].page);
      char t[8];
      snprintf(t, sizeof(t), "%d", j);
      ui_text(b.x + bw * 0.5f, b.y + 30, t, 21, FONT_SEMIBOLD, C_TEXT, ALIGN_CENTER);
    }
    return;
  }
  if (nresults == 0) {
    ui_text(p_side.w * 0.5f, y + 50, "No matches", 22, FONT_REGULAR, C_FAINT, ALIGN_CENTER);
    return;
  }
  for (int i = 0; i < nresults && y < p_side.h - 70; i++) {
    SearchResult* r = &results[i];
    Rect row = R(24, y, p_side.w - 48, 62);
    if (button("res", i, row, 14, false)) go_page(r->page);
    char a[64], b[64];
    switch (r->kind) {
      case RES_SURAH:
        snprintf(a, sizeof(a), "%s", Q.surah[r->value].translit);
        snprintf(b, sizeof(b), "Surah %d \xC2\xB7 %s \xC2\xB7 page %d", r->value, Q.surah[r->value].meaning, r->page);
        break;
      case RES_PAGE:
        snprintf(a, sizeof(a), "Page %d", r->value);
        snprintf(b, sizeof(b), "%s \xC2\xB7 Juz %d", Q.surah[quran_surah_at_page(r->value)].translit, Q.page[r->value].juz);
        break;
      case RES_JUZ:
        snprintf(a, sizeof(a), "Juz %d", r->value);
        snprintf(b, sizeof(b), "Starts at %d:%d \xC2\xB7 page %d", Q.juz[r->value].surah, Q.juz[r->value].ayah, r->page);
        break;
      default:
        snprintf(a, sizeof(a), "%s %d:%d", Q.surah[r->value].translit, r->value, r->value2);
        snprintf(b, sizeof(b), "Ayah %d \xC2\xB7 page %d", r->value2, r->page);
        break;
    }
    ui_text(row.x + 18, row.y + 28, a, 23, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
    ui_text(row.x + 18, row.y + 52, b, 17, FONT_REGULAR, C_FAINT, ALIGN_LEFT);
    icon(I_CHEV_R, row.x + row.w - 22, row.y + 31, 18, 0xFFFFFF60u);
    y += 66;
  }
}

static void mode_bookmarks(float dt) {
  (void)dt;
  char sub[16];
  snprintf(sub, sizeof(sub), "%d", st.nbookmarks);
  side_header("Bookmarks", sub);
  bool marked = state_is_bookmarked(&st, st.page);
  Rect b = R(28, 118, p_side.w - 56, 64);
  ui_rect(b, 18, marked ? 0xE6C97B40u : 0x8FD3FF38u, marked ? 0xE6C97B28u : 0x8FD3FF22u);
  ui_border(b, 18, 1.3f, marked ? 0xE6C97B70u : 0x8FD3FF60u);
  if (button("bm_toggle", 0, b, 18, false)) {
    state_toggle_bookmark(&st, st.page);
    mark_dirty();
  }
  icon(marked ? I_BOOKMARK_FILL : I_BOOKMARK, b.x + 36, b.y + 32, 28, marked ? C_GOLD : C_ACCENT);
  char t[64];
  snprintf(t, sizeof(t), marked ? "Bookmarked \xC2\xB7 pages %d\xE2\x80\x93%d" : "Bookmark pages %d\xE2\x80\x93%d", st.page, st.page + 1);
  ui_text(b.x + 66, b.y + 40, t, 22, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);

  Rect area = R(16, 200, p_side.w - 44, p_side.h - 216);
  if (st.nbookmarks == 0) {
    icon(I_BOOKMARK, p_side.w * 0.5f, 330, 64, 0xFFFFFF40u);
    ui_text(p_side.w * 0.5f, 400, "No bookmarks yet", 23, FONT_SEMIBOLD, C_DIM, ALIGN_CENTER);
    ui_text(p_side.w * 0.5f, 432, "Save a page to return to it later", 18, FONT_REGULAR, C_FAINT, ALIGN_CENTER);
    return;
  }
  float row_h = 78;
  for (int i = 0; i < st.nbookmarks; i++) {
    float y = area.y + i * row_h;
    if (y + row_h > area.y + area.h) break;
    int pg = st.bookmarks[i];
    Rect row = R(area.x + 6, y + 3, area.w - 12, row_h - 6);
    Rect del = R(row.x + row.w - 58, row.y + 12, 46, row.h - 24);
    if (button("bm_del", pg, del, 12, false)) {
      state_toggle_bookmark(&st, pg);
      mark_dirty();
      break;
    }
    icon(I_TRASH, del.x + 23, del.y + del.h * 0.5f, 24, 0xFFB4B4C0u);
    Rect go = R(row.x, row.y, row.w - 64, row.h);
    if (button("bm_go", pg, go, 16, pg == st.page || pg == st.page + 1)) go_page(pg);
    int s = quran_surah_at_page(pg);
    char a[48], bsub[48];
    snprintf(a, sizeof(a), "Page %d", pg);
    snprintf(bsub, sizeof(bsub), "%s \xC2\xB7 Juz %d", Q.surah[s].translit, Q.page[pg].juz);
    icon(I_BOOKMARK_FILL, row.x + 30, row.y + row.h * 0.5f, 24, C_GOLD);
    ui_text(row.x + 60, row.y + 32, a, 24, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
    ui_text(row.x + 60, row.y + 56, bsub, 18, FONT_REGULAR, C_FAINT, ALIGN_LEFT);
  }
}

static bool segmented(const char* key, float y, const char** labels, int n, int* value) {
  Rect all = R(28, y, p_side.w - 56, 52);
  ui_rect(all, 16, 0x0A0F1A70u, 0x0A0F1A80u);
  float w = (all.w - 8) / n;
  bool changed = false;
  for (int i = 0; i < n; i++) {
    Rect r = R(all.x + 4 + i * w, y + 4, w, 44);
    if (*value == i) {
      ui_rect(r, 13, 0xFFFFFFE0u, 0xE8EEF6E0u);
    }
    if (button(key, i, r, 13, false)) {
      *value = i;
      changed = true;
    }
    ui_text(r.x + w * 0.5f, r.y + 29, labels[i], 20, FONT_SEMIBOLD, *value == i ? 0x14203AFFu : C_TEXT, ALIGN_CENTER);
  }
  return changed;
}

static void mode_settings(float dt) {
  (void)dt;
  side_header("Settings", NULL);
  float y = 126;
  ui_text(30, y, "Page theme", 21, FONT_SEMIBOLD, C_DIM, ALIGN_LEFT);
  static const char* themes[] = {"Classic", "Sepia", "Night"};
  if (segmented("theme", y + 14, themes, 3, &st.theme)) mark_dirty();
  y += 100;

  ui_text(30, y, "Environment light", 21, FONT_SEMIBOLD, C_DIM, ALIGN_LEFT);
  Rect bar = R(96, y + 30, p_side.w - 192, 12);
  ui_rect(bar, 6, 0xFFFFFF22u, 0xFFFFFF22u);
  float f = (st.env_light - 0.2f) / 0.8f;
  ui_rect(R(bar.x, bar.y, bar.w * f, bar.h), 6, C_ACCENT, 0x5FB0F0FFu);
  ui_rect(R(bar.x + bar.w * f - 11, bar.y - 5, 22, 22), 11, C_WHITE, 0xE0E6F0FFu);
  Rect mn = R(28, y + 14, 52, 44), pl = R(p_side.w - 80, y + 14, 52, 44);
  ui_rect(mn, 12, 0xFFFFFF18u, 0xFFFFFF10u);
  ui_rect(pl, 12, 0xFFFFFF18u, 0xFFFFFF10u);
  if (button("light-", 0, mn, 12, false)) { st.env_light = clampf(st.env_light - 0.1f, 0.2f, 1.0f); mark_dirty(); }
  if (button("light+", 0, pl, 12, false)) { st.env_light = clampf(st.env_light + 0.1f, 0.2f, 1.0f); mark_dirty(); }
  icon(I_MOON, mn.x + 26, mn.y + 22, 24, C_TEXT);
  icon(I_SUN, pl.x + 26, pl.y + 22, 26, C_TEXT);
  y += 92;

  ui_text(30, y, "Book distance", 21, FONT_SEMIBOLD, C_DIM, ALIGN_LEFT);
  static const char* dists[] = {"Near", "Normal", "Far"};
  int di = st.book_dist < 0.57f ? 0 : st.book_dist < 0.69f ? 1 : 2;
  if (segmented("dist", y + 14, dists, 3, &di)) {
    static const float dv[] = {0.52f, 0.62f, 0.76f};
    st.book_dist = dv[di];
    mark_dirty();
  }
  y += 100;

  ui_text(30, y, "Book height", 21, FONT_SEMIBOLD, C_DIM, ALIGN_LEFT);
  Rect dn = R(28, y + 14, (p_side.w - 66) * 0.5f, 48), up = R(38 + (p_side.w - 66) * 0.5f, y + 14, (p_side.w - 66) * 0.5f, 48);
  ui_rect(dn, 14, 0xFFFFFF18u, 0xFFFFFF10u);
  ui_rect(up, 14, 0xFFFFFF18u, 0xFFFFFF10u);
  if (button("h-", 0, dn, 14, false)) { st.book_height = clampf(st.book_height - 0.04f, -0.3f, 0.3f); mark_dirty(); }
  if (button("h+", 0, up, 14, false)) { st.book_height = clampf(st.book_height + 0.04f, -0.3f, 0.3f); mark_dirty(); }
  icon(I_CHEV_D, dn.x + 40, dn.y + 24, 22, C_TEXT);
  ui_text(dn.x + 66, dn.y + 31, "Lower", 20, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
  icon(I_CHEV_U, up.x + 40, up.y + 24, 22, C_TEXT);
  ui_text(up.x + 66, up.y + 31, "Raise", 20, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
  y += 96;

  Rect tg = R(28, y, p_side.w - 56, 54);
  if (button("earthcard", 0, tg, 14, false)) { st.show_earth_card = !st.show_earth_card; mark_dirty(); }
  icon(I_GLOBE, tg.x + 22, tg.y + 27, 26, C_TEXT);
  ui_text(tg.x + 50, tg.y + 34, "Earth info card", 21, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
  float on = ui_anim(ui_id("earthsw", 0), st.show_earth_card ? 1.0f : 0.0f, 12);
  Rect sw = R(tg.x + tg.w - 70, tg.y + 12, 58, 30);
  ui_rect(sw, 15, on > 0.5f ? 0x5FC08CFFu : 0xFFFFFF30u, on > 0.5f ? 0x4AA877FFu : 0xFFFFFF24u);
  ui_rect(R(sw.x + 3 + on * 28, sw.y + 3, 24, 24), 12, C_WHITE, 0xE6EAF0FFu);
  y += 66;

  Rect rc = R(28, y, p_side.w - 56, 54);
  ui_rect(rc, 14, 0x8FD3FF30u, 0x8FD3FF20u);
  if (button("recenter", 0, rc, 14, false)) centered = false;
  icon(I_RECENTER, rc.x + 26, rc.y + 27, 26, C_ACCENT);
  ui_text(rc.x + 54, rc.y + 34, "Recenter view", 21, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
  y += 76;
  ui_text(30, y, "Madina Mushaf (1405 AH) \xC2\xB7 Riwayah of Hafs from Asim", 16, FONT_REGULAR, C_FAINT, ALIGN_LEFT);
  ui_text(30, y + 22, "604 pages \xC2\xB7 KFGQPC Hafs font", 16, FONT_REGULAR, C_FAINT, ALIGN_LEFT);
}

// ------------------------------------------------------------ panels
static void build_side(float dt) {
  ui_begin_panel(&p_side);
  glass(R(0, 0, p_side.w, p_side.h), 34);
  switch (mode) {
    case MODE_SURAHS: mode_surahs(dt); break;
    case MODE_SEARCH: mode_search(dt); break;
    case MODE_BOOKMARKS: mode_bookmarks(dt); break;
    default: mode_settings(dt); break;
  }
  // header back chevron returns to the surah list
  if (mode != MODE_SURAHS && button("back", 0, R(10, 14, 200, 72), 16, false)) mode = MODE_SURAHS;
  ui_end_panel();
}

static void build_toolbar(void) {
  ui_begin_panel(&p_toolbar);
  glass(R(0, 0, p_toolbar.w, p_toolbar.h), 44);
  static const char* labels[4] = {"Quran", "Search", "Bookmarks", "Settings"};
  static const int icons[4] = {I_BOOK, I_SEARCH, I_BOOKMARK, I_SETTINGS};
  float bw = p_toolbar.w / 4;
  for (int i = 0; i < 4; i++) {
    float cx = bw * i + bw * 0.5f, cy = 58;
    Rect hit = R(bw * i + 6, 6, bw - 12, p_toolbar.h - 12);
    bool active = mode == i && ui_visible;
    float hov = ui_anim(ui_id("tb", i), ui_hovered(hit) ? 1.0f : 0.0f, 14);
    if (active) {
      ui_glow(R(cx - 34, cy - 34, 68, 68), 34, 12, 0x8FD3FF40u);
      ui_rect(R(cx - 34, cy - 34, 68, 68), 34, 0xFFFFFFF0u, 0xDCE6F2F0u);
    } else if (hov > 0.01f) {
      ui_rect(R(cx - 34, cy - 34, 68, 68), 34, 0xFFFFFF00u | (uint32_t)(hov * 50), 0xFFFFFF00u | (uint32_t)(hov * 40));
    }
    ui_border(R(cx - 34, cy - 34, 68, 68), 34, 1.2f, active ? 0xFFFFFF00u : 0xFFFFFF30u);
    icon(icons[i], cx, cy, 34, active ? 0x1B2742FFu : C_TEXT);
    ui_text(cx, 128, labels[i], 19, active ? FONT_SEMIBOLD : FONT_REGULAR, active ? C_TEXT : C_DIM, ALIGN_CENTER);
    if (ui_clicked(hit)) {
      if (!ui_visible) ui_visible = true;
      mode = i;
      if (i == MODE_SURAHS) scroll_to_current = true;
    }
  }
  ui_end_panel();
}

static void build_pill(void) {
  ui_begin_panel(&p_pill);
  glass(R(0, 0, p_pill.w, p_pill.h), p_pill.h * 0.5f);
  Rect l = R(8, 8, 70, p_pill.h - 16), r = R(p_pill.w - 78, 8, 70, p_pill.h - 16);
  // Arabic Mushaf reads right-to-left: the left arrow moves forward.
  if (button("pill_next", 0, l, (p_pill.h - 16) * 0.5f, false)) turn(1);
  if (button("pill_prev", 0, r, (p_pill.h - 16) * 0.5f, false)) turn(-1);
  icon(I_CHEV_L, l.x + 35, p_pill.h * 0.5f, 26, st.page < QURAN_PAGES - 1 ? C_TEXT : C_FAINT);
  icon(I_CHEV_R, r.x + 35, p_pill.h * 0.5f, 26, st.page > 1 ? C_TEXT : C_FAINT);
  char t[48];
  snprintf(t, sizeof(t), "Page %d\xE2\x80\x93%d of %d", st.page, st.page + 1, QURAN_PAGES);
  ui_text(p_pill.w * 0.5f, 34, t, 23, FONT_SEMIBOLD, C_TEXT, ALIGN_CENTER);
  char s[64];
  snprintf(s, sizeof(s), "%s \xC2\xB7 Juz %d", Q.surah[quran_surah_at_page(st.page)].translit, Q.page[st.page].juz);
  ui_text(p_pill.w * 0.5f, 58, s, 16, FONT_REGULAR, C_DIM, ALIGN_CENTER);
  ui_end_panel();
}

static void build_card(void) {
  ui_begin_panel(&p_card);
  glass(R(0, 0, p_card.w, p_card.h), 28);
  icon(I_GLOBE, 44, 50, 34, C_TEXT);
  ui_text(80, 46, "Earth", 26, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
  time_t now = (time_t)sc.utc;
  struct tm tmv;
  gmtime_r(&now, &tmv);
  char t[48];
  snprintf(t, sizeof(t), "Live view \xC2\xB7 %02d:%02d UTC", tmv.tm_hour, tmv.tm_min);
  ui_text(80, 72, t, 17, FONT_REGULAR, C_DIM, ALIGN_LEFT);
  ui_rect(R(24, 98, p_card.w - 48, 1.2f), 0, 0xFFFFFF26u, 0xFFFFFF26u);
  icon(I_PIN, 44, 140, 30, C_GOLD);
  ui_text(80, 134, "Makkah Al-Mukarramah", 21, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
  ui_text(80, 160, "21.4225\xC2\xB0 N, 39.8262\xC2\xB0 E", 17, FONT_REGULAR, C_DIM, ALIGN_LEFT);
  bool day;
  scene_earth_info(&sc, &day);
  Rect pill = R(24, 184, 138, 34);
  ui_rect(pill, 17, day ? 0xFFD98A40u : 0x8FA8FF38u, day ? 0xFFD98A28u : 0x8FA8FF24u);
  icon(day ? I_SUN : I_MOON, pill.x + 22, pill.y + 17, 20, day ? 0xFFE3A0FFu : 0xC8D4FFFFu);
  ui_text(pill.x + 40, pill.y + 23, day ? "Daytime" : "Night", 16, FONT_SEMIBOLD, C_TEXT, ALIGN_LEFT);
  ui_text(p_card.w - 26, pill.y + 23, "Qibla marked", 15, FONT_REGULAR, C_FAINT, ALIGN_RIGHT);
  ui_end_panel();
}

static void build_title(void) {
  ui_begin_panel(&p_title);
  float cx = p_title.w * 0.5f;
  ui_image_fit(img_icon[I_GLOW], cx, 52, 110, 0xE6C97B30u);
  icon(I_ROSETTE, cx, 52, 58, C_GOLD);
  int t = img_icon[I_TITLE];
  float h = 150, w = h * ui_image_aspect(t);
  ui_image2(t, R(cx - w * 0.5f, 88, w, h), 0xF6E3A8FFu, 0xC99A45FFu);
  ui_text(cx, 292, "The Holy Quran", 46, FONT_REGULAR, C_WHITE, ALIGN_CENTER);
  ui_text(cx, 346, "Mushaf Al-Madina", 30, FONT_REGULAR, 0xE8EEF6FFu, ALIGN_CENTER);
  ui_text(cx, 384, "The authentic Quran, as it was revealed", 22, FONT_REGULAR, C_DIM, ALIGN_CENTER);
  ui_end_panel();
}

// ------------------------------------------------------------ app lifecycle
void app_init(void) {
  if (!quran_load_meta()) plat_log("failed to load meta.txt");
  state_load(&st);
  ui_init();
  scene_init();
  pages_init();
  for (int i = 0; i < I_COUNT; i++) img_icon[i] = ui_img(icon_names[i]);
  for (int i = 1; i <= QURAN_SURAHS; i++) {
    char n[16];
    snprintf(n, sizeof(n), "sura%d", i);
    img_sura[i] = ui_img(n);
  }
  memset(&sc, 0, sizeof(sc));
  sc.content = m4_identity();
  sc.head_h = 1.25f;
  sc.right_page = st.page;
  sc.left_page = st.page + 1;
  request_spread(st.page, 10);
  request_spread(st.page + 2, 5);

  p_side = (Panel){.w = 560, .h = 780, .mpp = 0.00075f, .alpha = 1, .visible = true, .interactive = true};
  p_toolbar = (Panel){.w = 520, .h = 150, .mpp = 0.00062f, .alpha = 1, .visible = true, .interactive = true};
  p_pill = (Panel){.w = 440, .h = 74, .mpp = 0.00062f, .alpha = 1, .visible = true, .interactive = true};
  p_card = (Panel){.w = 400, .h = 236, .mpp = 0.00075f, .alpha = 1, .visible = true, .interactive = false};
  p_title = (Panel){.w = 1000, .h = 400, .mpp = 0.0011f, .alpha = 1, .visible = true, .interactive = false};
}

static void recenter(const AppInput* in) {
  v3 f = q_rotate(in->head.rot, V3(0, 0, -1));
  float yaw = atan2f(-f.x, -f.z);
  float floor_y = in->has_floor ? 0.0f : in->head.pos.y - 1.25f;
  sc.content = m4_mul(m4_translate(V3(in->head.pos.x, floor_y, in->head.pos.z)), m4_rot_y(yaw));
  sc.head_h = clampf(in->head.pos.y - floor_y, 0.9f, 2.0f);
  centered = true;
}

static void update_pointers(const AppInput* in) {
  for (int h = 0; h < 2; h++) {
    const Controller* c = &in->ctl[h];
    Pointer* p = &ui_ptr[h];
    bool was_down = p->down;
    memset(p, 0, sizeof(*p));
    sc.laser_len[h] = -1;
    sc.laser_hit[h] = false;
    if (!c->active || !c->aim.valid) continue;
    p->down = was_down ? c->trigger > 0.35f : c->trigger > 0.6f;
    p->pressed = p->down && !prev_trig[h];
    prev_trig[h] = p->down;
    p->scroll = c->stick_y;
    v3 o = c->aim.pos, d = q_rotate(c->aim.rot, V3(0, 0, -1));
    float best = 1e9f;
    for (int i = 0; i < NPANELS; i++) {
      Panel* pn = panels[i];
      if (!pn->visible || !pn->interactive || pn->alpha < 0.5f) continue;
      float x, y;
      float t = ui_ray_panel(pn, o, d, &x, &y);
      if (t > 0 && t < best) {
        best = t;
        p->valid = true;
        p->panel = pn;
        p->x = x;
        p->y = y;
      }
    }
    float bd;
    int page_hit = scene_ray_book(&sc, o, d, &bd);
    if (page_hit && bd < best) {
      p->valid = false;
      best = bd;
      sc.page_highlight[page_hit - 1] = 1;
      if (p->pressed) turn(page_hit == 2 ? 1 : -1);
    }
    p->dist = best;
    sc.laser_len[h] = best < 1e8f ? best : 0.9f;
    sc.laser_hit[h] = best < 1e8f;
    p->hit_world = v3_add(o, v3_scale(d, sc.laser_len[h]));
  }
}

void app_update(const AppInput* in) {
  float dt = clampf(in->dt, 0.0f, 0.1f);
  sc.time = in->time;
  sc.utc = plat_utc_seconds();
  sc.env_light = st.env_light;
  sc.theme = st.theme;
  sc.book_dist = st.book_dist;
  sc.book_height = st.book_height;
  for (int h = 0; h < 2; h++) sc.ctl[h] = in->ctl[h];
  if (!centered || in->recenter) {
    if (in->head.valid) recenter(in);
  }

  // Page flip animation
  if (sc.flip.active) {
    sc.flip.t += dt / 0.62f;
    if (sc.flip.t >= 1) {
      finish_flip();
      if (jump_target) {
        int j = jump_target;
        jump_target = 0;
        start_flip(j);
      } else if (pending_turn) {
        int d = pending_turn > 0 ? 1 : -1;
        pending_turn -= d;
        turn(d);
      }
    }
  }
  // Prefetch neighbours
  request_spread(st.page, 8);
  request_spread(clamp_right(st.page + 2), 4);
  request_spread(clamp_right(st.page - 2), 3);
  pages_pump();

  // Panel layout (content space, relative to eye height)
  float hh = sc.head_h;
  sc.page_highlight[0] = approachf(sc.page_highlight[0], 0, 8, dt);
  sc.page_highlight[1] = approachf(sc.page_highlight[1], 0, 8, dt);
  float bh = hh - 0.36f + st.book_height;
  place_facing(&p_side, V3(-0.66f, hh - 0.08f, -0.98f));
  place_facing(&p_card, V3(0.74f, hh - 0.16f, -1.02f));
  place_facing(&p_title, V3(0, hh + 0.33f, -2.3f));
  place_facing(&p_pill, V3(0, bh - 0.215f, -st.book_dist + 0.175f));
  place_facing(&p_toolbar, V3(0, bh - 0.30f, -st.book_dist + 0.20f));

  ui_fade = approachf(ui_fade, ui_visible ? 1.0f : 0.0f, 10, dt);
  p_side.alpha = ui_fade;
  p_card.alpha = ui_fade * (st.show_earth_card ? 1.0f : 0.0f);
  p_card.visible = p_card.alpha > 0.01f;
  p_title.alpha = 0.35f + 0.65f * ui_fade;
  p_pill.alpha = 1;
  p_toolbar.alpha = 1;

  update_pointers(in);

  // Buttons and thumbsticks
  for (int h = 0; h < 2; h++) {
    const Controller* c = &in->ctl[h];
    stick_cool[h] -= dt;
    bool over_list = ui_ptr[h].valid && ui_ptr[h].panel == &p_side;
    if (c->active && !over_list && stick_cool[h] <= 0 && fabsf(c->stick_x) > 0.7f) {
      turn(c->stick_x < 0 ? 1 : -1);  // push left = forward (right-to-left reading)
      stick_cool[h] = 0.45f;
    }
    if (fabsf(c->stick_x) < 0.3f) stick_cool[h] = fminf(stick_cool[h], 0);
    if (c->a && !prev_btn_a[h]) turn(1);
    if (c->b && !prev_btn_b[h]) turn(-1);
    prev_btn_a[h] = c->a;
    prev_btn_b[h] = c->b;
  }
  if (in->ctl[HAND_LEFT].menu && !prev_menu) ui_visible = !ui_visible;
  prev_menu = in->ctl[HAND_LEFT].menu;

  // Build UI
  ui_begin_frame(dt);
  build_title();
  if (p_card.visible) build_card();
  build_side(dt);
  build_pill();
  build_toolbar();
  ui_end_frame();

  if (save_timer > 0) {
    save_timer -= dt;
    if (save_timer <= 0) state_save(&st);
  }
}

void app_render(const AppView* v) {
  scene_render(v, &sc);
  m4 vp = m4_mul(v->proj, v->view);
  ui_render(&vp, panels, NPANELS);
  scene_render_overlay(v, &sc);
}

void app_shutdown(void) {
  state_save(&st);
  pages_shutdown();
}

// Desktop preview hooks (used by the screenshot renderer only)
void app_debug_set_mode(int m) { mode = m; }
void app_debug_set_query(const char* q) {
  snprintf(query, sizeof(query), "%s", q);
  run_search();
}
void app_debug_set_theme(int t) { st.theme = t; }
void app_debug_go(int page) {
  st.page = clamp_right(page);
  sc.right_page = st.page;
  sc.left_page = st.page + 1;
  sc.flip.active = false;
  scroll_to_current = true;
}
void app_debug_flip(float t, int dir) {
  int target = clamp_right(st.page + 2 * dir);
  start_flip(target);
  sc.flip.t = t;
}
void app_debug_bookmark(int page) {
  if (!state_is_bookmarked(&st, page)) state_toggle_bookmark(&st, page);
}
