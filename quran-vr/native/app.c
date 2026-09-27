#include "app.h"

#define HIT_NONE (-1)
#define HIT_BOOK 100

typedef struct {
    int trigDown, gripDown;
    int nextDown, prevDown;
    int stickLatch;            // thumbstick must return to centre between flicks
    int hit;                   // panel id, HIT_BOOK or HIT_NONE
    float u, v;                // panel coords of the hit
    float bx, by;              // book-local coords of the hit
} HandState;

static AppBridge B;
static HandState H[2];
static int g_pointerHand = 1;
static int g_hoverPanel = HIT_NONE;
static int g_downPanel = HIT_NONE;

// page turning
static int g_dragging;         // 0 none, 1 laser drag, 2 grip grab
static int g_dragHand;
static float g_dragX0, g_dragT, g_dragTPrev, g_dragVel;
static float g_target;
static int g_queued;           // queued auto-turn direction

static void notify_page(SceneState* s) {
    if (B.onPage) B.onPage(2 * s->spread + 1);
}

void app_init(const AppBridge* bridge, SceneState* s, int startPage) {
    B = *bridge;
    memset(H, 0, sizeof(H));
    for (int i = 0; i < 2; i++) H[i].hit = HIT_NONE;
    s->spread = 0;
    s->turning = 0;
    s->alpha = 0;
    if (startPage >= 1 && startPage <= PAGE_COUNT) s->spread = (startPage - 1) / 2;
    notify_page(s);
}

static int start_turn(SceneState* s, int dir) {
    if (s->turning) return 0;
    if (dir > 0 && s->spread >= SPREAD_COUNT - 1) return 0;
    if (dir < 0 && s->spread <= 0) return 0;
    s->turning = dir;
    s->alpha = dir > 0 ? PI_F : 0.0f;
    return 1;
}

static void auto_turn(SceneState* s, int dir) {
    if (s->turning) {
        if (!g_dragging) g_queued = dir;
        return;
    }
    if (start_turn(s, dir)) g_target = dir > 0 ? 0.0f : PI_F;
}

void app_goto_page(SceneState* s, int page) {
    if (page < 1) page = 1;
    if (page > PAGE_COUNT) page = PAGE_COUNT;
    int target = (page - 1) / 2;
    if (target == s->spread) return;
    if (!s->turning && target == s->spread + 1) { auto_turn(s, 1); return; }
    if (!s->turning && target == s->spread - 1) { auto_turn(s, -1); return; }
    s->turning = 0;
    g_dragging = 0;
    s->spread = target;
    notify_page(s);
}

// ray vs panel quad; returns distance or -1
static float ray_panel(int p, vec3 o, vec3 d, float* u, float* v) {
    mat4 m = scene_panel_matrix(p);
    vec3 c = v3(m.m[12], m.m[13], m.m[14]);
    vec3 ax = v3(m.m[0], m.m[1], m.m[2]), ay = v3(m.m[4], m.m[5], m.m[6]);
    float w = v3len(ax), h = v3len(ay);
    vec3 xn = v3scale(ax, 1.0f / w), yn = v3scale(ay, 1.0f / h);
    vec3 n = v3cross(xn, yn);
    float den = v3dot(d, n);
    if (fabsf(den) < 1e-5f) return -1;
    float t = v3dot(v3sub(c, o), n) / den;
    if (t <= 0) return -1;
    vec3 hp = v3sub(v3mad(o, d, t), c);
    float uu = v3dot(hp, xn) / w + 0.5f, vv = 0.5f - v3dot(hp, yn) / h;
    if (uu < 0 || uu > 1 || vv < 0 || vv > 1) return -1;
    *u = uu;
    *v = vv;
    return t;
}

// ray vs the open book (approximated by a plane just above the pages)
static float ray_book(vec3 o, vec3 d, float* bx, float* by, int bounded) {
    mat4 bm = scene_book_matrix();
    mat4 inv = m4rigid_inverse(bm);
    vec3 lo = m4point(inv, o), ld = m4dir(inv, d);
    float zp = 0.022f;
    if (fabsf(ld.z) < 1e-5f) return -1;
    float t = (zp - lo.z) / ld.z;
    if (t <= 0) return -1;
    vec3 p = v3mad(lo, ld, t);
    if (bounded && (fabsf(p.x) > PAGE_W + 0.02f || fabsf(p.y) > PAGE_H * 0.5f + 0.02f)) return -1;
    *bx = p.x;
    *by = p.y;
    return t;
}

static void finish_turn(SceneState* s) {
    int completed = (s->turning > 0) ? (s->alpha < PI_F * 0.5f) : (s->alpha > PI_F * 0.5f);
    if (completed) {
        s->spread += s->turning;
        notify_page(s);
        if (B.haptic) B.haptic(g_dragging ? g_dragHand : g_pointerHand, 0.35f, 0.03f);
    }
    s->turning = 0;
    s->alpha = 0;
}

static void update_turn(float dt, SceneState* s) {
    if (!s->turning) {
        if (g_queued) {
            int q = g_queued;
            g_queued = 0;
            auto_turn(s, q);
        }
        return;
    }
    if (g_dragging) {
        s->alpha += (g_target - s->alpha) * (1.0f - expf(-dt * 20.0f));
    } else {
        float diff = g_target - s->alpha;
        float step = diff * (1.0f - expf(-dt * 7.5f));
        float minStep = 1.6f * dt;
        if (fabsf(step) < minStep) step = diff > 0 ? minStep : -minStep;
        if (fabsf(step) >= fabsf(diff)) {
            s->alpha = g_target;
            finish_turn(s);
            return;
        }
        s->alpha += step;
    }
}

static void begin_drag(SceneState* s, int hand, int mode, float x) {
    int dir = x < 0 ? 1 : -1;   // lift the left page to go forward (Arabic Mushaf)
    if (!start_turn(s, dir)) return;
    g_dragging = mode;
    g_dragHand = hand;
    g_dragX0 = x;
    g_dragT = g_dragTPrev = 0;
    g_dragVel = 0;
    g_target = s->alpha;
    if (B.haptic) B.haptic(hand, 0.15f, 0.015f);
}

static void update_drag(float dt, SceneState* s, float x) {
    float t = (x - g_dragX0) / (PAGE_W * 0.9f);
    if (s->turning < 0) t = -t;
    t = clampf(t, 0.0f, 1.0f);
    g_dragVel = lerpf(g_dragVel, (t - g_dragTPrev) / (dt > 1e-4f ? dt : 1e-4f), 0.3f);
    g_dragTPrev = t;
    g_dragT = t;
    g_target = s->turning > 0 ? PI_F * (1.0f - t) : PI_F * t;
}

static void end_drag(SceneState* s) {
    int complete = g_dragT > 0.38f || g_dragVel > 1.8f;
    if (s->turning > 0) g_target = complete ? 0.0f : PI_F;
    else g_target = complete ? PI_F : 0.0f;
    g_dragging = 0;
}

void app_update(float dt, const HandInput in[2], SceneState* s) {
    for (int h = 0; h < 2; h++) {
        const HandInput* hi = &in[h];
        HandState* hs = &H[h];
        HandVis* hv = &s->hands[h];
        hv->active = hi->active;
        hv->aim = hi->aim;
        hv->grip = hi->grip;
        hv->trigger = hi->trigger;
        hv->squeeze = hi->squeeze;
        if (!hi->active) {
            hs->hit = HIT_NONE;
            continue;
        }
        vec3 o = hi->aim.pos, d = qrot(hi->aim.ori, v3(0, 0, -1));

        // nearest hit
        float best = 1e9f;
        hs->hit = HIT_NONE;
        for (int p = 0; p < PANEL_COUNT; p++) {
            if (!(s->panelVisible & (1u << p)) || p == PANEL_TITLE) continue;
            float u, v;
            float t = ray_panel(p, o, d, &u, &v);
            if (t > 0 && t < best) { best = t; hs->hit = p; hs->u = u; hs->v = v; }
        }
        float bx, by;
        float tb = ray_book(o, d, &bx, &by, 1);
        if (tb > 0 && tb < best) { best = tb; hs->hit = HIT_BOOK; hs->bx = bx; hs->by = by; }
        hv->rayHit = hs->hit != HIT_NONE;
        hv->rayLen = hv->rayHit ? best : 1.2f;

        // trigger with hysteresis
        int trigPressed = 0, trigReleased = 0;
        if (!hs->trigDown && hi->trigger > 0.6f) { hs->trigDown = 1; trigPressed = 1; }
        else if (hs->trigDown && hi->trigger < 0.35f) { hs->trigDown = 0; trigReleased = 1; }
        int gripPressed = 0, gripReleased = 0;
        if (!hs->gripDown && hi->squeeze > 0.6f) { hs->gripDown = 1; gripPressed = 1; }
        else if (hs->gripDown && hi->squeeze < 0.35f) { hs->gripDown = 0; gripReleased = 1; }

        if (trigPressed) g_pointerHand = h;

        // --- page turning by hand
        if (trigPressed && hs->hit == HIT_BOOK && !g_dragging && !s->turning) begin_drag(s, h, 1, hs->bx);
        if (gripPressed && !g_dragging && !s->turning) {
            mat4 inv = m4rigid_inverse(scene_book_matrix());
            vec3 lp = m4point(inv, hi->grip.pos);
            if (fabsf(lp.x) < PAGE_W + 0.06f && fabsf(lp.y) < PAGE_H * 0.5f + 0.06f && lp.z > -0.04f && lp.z < 0.16f)
                begin_drag(s, h, 2, lp.x);
        }
        if (g_dragging && g_dragHand == h) {
            if (g_dragging == 1) {
                float x, y;
                if (ray_book(o, d, &x, &y, 0) > 0) update_drag(dt, s, x);
                if (trigReleased) end_drag(s);
            } else {
                mat4 inv = m4rigid_inverse(scene_book_matrix());
                vec3 lp = m4point(inv, hi->grip.pos);
                update_drag(dt, s, lp.x);
                if (gripReleased) end_drag(s);
            }
        }

        // --- thumbstick flick / buttons: left = next page (Arabic reading direction)
        if (!hs->stickLatch && fabsf(hi->stick.x) > 0.75f) {
            hs->stickLatch = 1;
            auto_turn(s, hi->stick.x < 0 ? 1 : -1);
        } else if (hs->stickLatch && fabsf(hi->stick.x) < 0.3f) {
            hs->stickLatch = 0;
        }
        if (hi->btnNext && !hs->nextDown) auto_turn(s, 1);
        if (hi->btnPrev && !hs->prevDown) auto_turn(s, -1);
        hs->nextDown = hi->btnNext;
        hs->prevDown = hi->btnPrev;

        // thumbstick y scrolls the list under the laser
        if (hs->hit >= 0 && hs->hit < PANEL_COUNT && fabsf(hi->stick.y) > 0.2f && B.uiScroll)
            B.uiScroll(hs->hit, -hi->stick.y * dt * 5.0f);

        // --- UI pointer events (pointer hand only)
        if (h == g_pointerHand) {
            int panel = (hs->hit >= 0 && hs->hit < PANEL_COUNT) ? hs->hit : HIT_NONE;
            if (panel != g_hoverPanel && g_hoverPanel != HIT_NONE && B.uiPointer)
                B.uiPointer(g_hoverPanel, 0, 0, UI_LEAVE);
            g_hoverPanel = panel;
            if (panel != HIT_NONE && B.uiPointer) {
                B.uiPointer(panel, hs->u, hs->v, UI_HOVER);
                if (trigPressed) {
                    g_downPanel = panel;
                    B.uiPointer(panel, hs->u, hs->v, UI_DOWN);
                    if (B.haptic) B.haptic(h, 0.12f, 0.012f);
                }
            }
            if (trigReleased && g_downPanel != HIT_NONE && B.uiPointer) {
                if (panel == g_downPanel) B.uiPointer(panel, hs->u, hs->v, UI_UP);
                else B.uiPointer(g_downPanel, -1, -1, UI_UP);
                g_downPanel = HIT_NONE;
            }
        }
    }
    for (int h = 0; h < 2; h++) s->hands[h].pointer = (h == g_pointerHand);
    update_turn(dt, s);
}
