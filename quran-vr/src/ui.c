#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "platform.h"
#include "shaders.h"

#define MAX_QUADS 24000
#define MAX_IMAGES 256
#define ATLAS_PX 56.0f

typedef struct {
  float x, y;
  float u, v;
  uint8_t r, g, b, a;
  float s0, s1, s2, s3;
  float c0, c1, c2, c3;
  float mode;
} UIVert;

typedef struct {
  float x, y, w, h;      // atlas px
  float xoff, yoff, adv;  // relative to pen / baseline
} Glyph;

typedef struct {
  char name[24];
  float x, y, w, h;
} Image;

Pointer ui_ptr[2];

static GLuint prog, vao, vbo, ibo, atlas;
static GLint loc_mvp, loc_alpha, loc_atlas;
static int atlas_w = 2048, atlas_h = 2048;
static Glyph glyphs[2][128];
static struct { uint32_t cp; Glyph g[2]; } extra[32];
static int nextra;
static float font_ascent[2];
static Image images[MAX_IMAGES];
static int nimages;

static UIVert* verts;
static int nverts;
static Panel* cur;
static Rect clip_stack[8];
static int nclip;
static float frame_dt;

// Hover animation store
typedef struct {
  uint32_t id;
  float v;
  int touched;
} Anim;
static Anim anims[1024];

static void parse_atlas(char* d) {
  char* line = d;
  while (line && *line) {
    char* nl = strchr(line, '\n');
    if (nl) *nl = 0;
    char name[32];
    int f = 0;
    if (line[0] == 'F') {
      int px, asc, desc;
      if (sscanf(line + 2, "%31s %d %d %d", name, &px, &asc, &desc) == 4) {
        f = strcmp(name, "semibold") == 0;
        font_ascent[f] = (float)asc;
      }
    } else if (line[0] == 'G') {
      unsigned cp;
      float x, y, w, h, xo, yo, adv;
      if (sscanf(line + 2, "%31s %u %f %f %f %f %f %f %f", name, &cp, &x, &y, &w, &h, &xo, &yo, &adv) == 9) {
        f = strcmp(name, "semibold") == 0;
        Glyph g = {x, y, w, h, xo, yo, adv};
        if (cp < 128) glyphs[f][cp] = g;
        else {
          int i = 0;
          while (i < nextra && extra[i].cp != cp) i++;
          if (i == nextra && nextra < 32) extra[nextra++].cp = cp;
          if (i < 32) extra[i].g[f] = g;
        }
      }
    } else if (line[0] == 'I' && nimages < MAX_IMAGES) {
      Image* im = &images[nimages];
      if (sscanf(line + 2, "%23s %f %f %f %f", im->name, &im->x, &im->y, &im->w, &im->h) == 5) nimages++;
    }
    line = nl ? nl + 1 : NULL;
  }
}

void ui_init(void) {
  prog = gl_program("ui", VS_UI, FS_UI);
  loc_mvp = glGetUniformLocation(prog, "uMVP");
  loc_alpha = glGetUniformLocation(prog, "uAlpha");
  loc_atlas = glGetUniformLocation(prog, "uAtlas");
  verts = malloc(sizeof(UIVert) * MAX_QUADS * 4);

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(UIVert) * MAX_QUADS * 4, NULL, GL_DYNAMIC_DRAW);
  uint32_t* idx = malloc(sizeof(uint32_t) * MAX_QUADS * 6);
  for (uint32_t q = 0; q < MAX_QUADS; q++) {
    uint32_t b = q * 4;
    uint32_t* o = idx + q * 6;
    o[0] = b; o[1] = b + 1; o[2] = b + 2; o[3] = b; o[4] = b + 2; o[5] = b + 3;
  }
  glGenBuffers(1, &ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * MAX_QUADS * 6, idx, GL_STATIC_DRAW);
  free(idx);
  const int st = sizeof(UIVert);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void*)8);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void*)16);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, st, (void*)20);
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, st, (void*)36);
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, st, (void*)52);
  glBindVertexArray(0);

  atlas = tex_from_asset("ui_atlas.png", false, true, &atlas_w, &atlas_h);
  int size = 0;
  char* d = (char*)plat_read_asset("ui_atlas.txt", &size);
  if (d) {
    d = realloc(d, size + 1);
    d[size] = 0;
    parse_atlas(d);
    free(d);
  }
}

int ui_img(const char* name) {
  for (int i = 0; i < nimages; i++)
    if (!strcmp(images[i].name, name)) return i;
  plat_log("ui image '%s' not found", name);
  return -1;
}

float ui_image_aspect(int img) { return img < 0 ? 1 : images[img].w / images[img].h; }

void ui_begin_frame(float dt) {
  nverts = 0;
  frame_dt = dt;
  for (int i = 0; i < 1024; i++)
    if (anims[i].id && !anims[i].touched) anims[i].v = approachf(anims[i].v, 0, 10, dt);
  for (int i = 0; i < 1024; i++) anims[i].touched = 0;
}

void ui_end_frame(void) {
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(UIVert) * MAX_QUADS * 4, NULL, GL_DYNAMIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(UIVert) * nverts, verts);
}

static m4 panel_px_to_world(const Panel* p) {
  return m4_mul(p->model, m4_mul(m4_scale(V3(p->mpp, -p->mpp, 1)), m4_translate(V3(-p->w * 0.5f, -p->h * 0.5f, 0))));
}

float ui_ray_panel(const Panel* p, v3 o, v3 d, float* px, float* py) {
  m4 inv = m4_rigid_inverse(p->model);
  v3 lo = m4_point(inv, o), ld = m4_dir(inv, d);
  if (ld.z >= -1e-5f) return -1;  // parallel or from behind
  float t = -lo.z / ld.z;
  if (t < 0) return -1;
  v3 hit = v3_add(lo, v3_scale(ld, t));
  float x = hit.x / p->mpp + p->w * 0.5f;
  float y = -hit.y / p->mpp + p->h * 0.5f;
  if (x < 0 || y < 0 || x > p->w || y > p->h) return -1;
  *px = x;
  *py = y;
  return t;
}

void ui_begin_panel(Panel* p) {
  cur = p;
  p->first_vert = nverts;
  nclip = 1;
  clip_stack[0] = R(-1e5f, -1e5f, 2e5f, 2e5f);
}

void ui_end_panel(void) {
  cur->nverts = nverts - cur->first_vert;
  cur = NULL;
}

void ui_push_clip(Rect r) {
  if (nclip < 8) clip_stack[nclip++] = r;
}
void ui_pop_clip(void) {
  if (nclip > 1) nclip--;
}

static void push_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
                      uint32_t ctop, uint32_t cbot, float s0, float s1, float s2, float s3, float mode) {
  if (nverts + 4 > MAX_QUADS * 4) return;
  Rect c = clip_stack[nclip - 1];
  float cx0 = c.x, cy0 = c.y, cx1 = c.x + c.w, cy1 = c.y + c.h;
  if (x1 < cx0 || y1 < cy0 || x0 > cx1 || y0 > cy1) return;
  const float xs[4] = {x0, x1, x1, x0}, ys[4] = {y0, y0, y1, y1};
  const float us[4] = {u0, u1, u1, u0}, vs[4] = {v0, v0, v1, v1};
  for (int i = 0; i < 4; i++) {
    uint32_t col = i < 2 ? ctop : cbot;
    verts[nverts++] = (UIVert){xs[i], ys[i], us[i], vs[i],
                               (uint8_t)(col >> 24), (uint8_t)(col >> 16), (uint8_t)(col >> 8), (uint8_t)col,
                               s0, s1, s2, s3, cx0, cy0, cx1, cy1, mode};
  }
}

void ui_rect(Rect r, float radius, uint32_t top, uint32_t bottom) {
  float hw = r.w * 0.5f, hh = r.h * 0.5f, m = 2;
  push_quad(r.x - m, r.y - m, r.x + r.w + m, r.y + r.h + m, -hw - m, -hh - m, hw + m, hh + m, top, bottom, hw, hh, radius, 0, 0);
}

void ui_border(Rect r, float radius, float thickness, uint32_t color) {
  float hw = r.w * 0.5f, hh = r.h * 0.5f, m = 2;
  push_quad(r.x - m, r.y - m, r.x + r.w + m, r.y + r.h + m, -hw - m, -hh - m, hw + m, hh + m, color, color, hw, hh, radius, thickness, 1);
}

void ui_glow(Rect r, float radius, float soft, uint32_t color) {
  float hw = r.w * 0.5f, hh = r.h * 0.5f, m = soft * 1.5f;
  push_quad(r.x - m, r.y - m, r.x + r.w + m, r.y + r.h + m, -hw - m, -hh - m, hw + m, hh + m, color, color, hw, hh, radius, soft, 3);
}

void ui_image2(int img, Rect r, uint32_t top, uint32_t bottom) {
  if (img < 0) return;
  Image* im = &images[img];
  push_quad(r.x, r.y, r.x + r.w, r.y + r.h, im->x / atlas_w, im->y / atlas_h, (im->x + im->w) / atlas_w,
            (im->y + im->h) / atlas_h, top, bottom, 0, 0, 0, 0, 2);
}

void ui_image(int img, Rect r, uint32_t color) { ui_image2(img, r, color, color); }

void ui_image_fit(int img, float cx, float cy, float height, uint32_t color) {
  if (img < 0) return;
  float w = height * ui_image_aspect(img);
  ui_image(img, R(cx - w * 0.5f, cy - height * 0.5f, w, height), color);
}

static uint32_t utf8_next(const char** s) {
  const unsigned char* p = (const unsigned char*)*s;
  uint32_t c = *p++;
  if (c >= 0xF0) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
  else if (c >= 0xE0) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
  else if (c >= 0xC0) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
  *s = (const char*)p;
  return c;
}

static const Glyph* glyph(int font, uint32_t cp) {
  if (cp < 128) return &glyphs[font][cp];
  for (int i = 0; i < nextra; i++)
    if (extra[i].cp == cp) return &extra[i].g[font];
  return &glyphs[font]['?'];
}

float ui_text_width(const char* s, float size, int font) {
  float scale = size / ATLAS_PX, w = 0;
  while (*s) w += glyph(font, utf8_next(&s))->adv * scale;
  return w;
}

float ui_text(float x, float baseline, const char* s, float size, int font, uint32_t color, int align) {
  float w = ui_text_width(s, size, font);
  if (align == ALIGN_CENTER) x -= w * 0.5f;
  else if (align == ALIGN_RIGHT) x -= w;
  float scale = size / ATLAS_PX;
  while (*s) {
    const Glyph* g = glyph(font, utf8_next(&s));
    if (g->w > 0) {
      float x0 = x + g->xoff * scale, y0 = baseline + g->yoff * scale;
      push_quad(x0, y0, x0 + g->w * scale, y0 + g->h * scale, g->x / atlas_w, g->y / atlas_h, (g->x + g->w) / atlas_w,
                (g->y + g->h) / atlas_h, color, color, 0, 0, 0, 0, 2);
    }
    x += g->adv * scale;
  }
  return w;
}

bool ui_hovered(Rect r) {
  Rect c = clip_stack[nclip - 1];
  for (int i = 0; i < 2; i++) {
    Pointer* p = &ui_ptr[i];
    if (p->valid && p->panel == cur && rect_has(r, p->x, p->y) && rect_has(c, p->x, p->y)) return true;
  }
  return false;
}

bool ui_clicked(Rect r) {
  Rect c = clip_stack[nclip - 1];
  for (int i = 0; i < 2; i++) {
    Pointer* p = &ui_ptr[i];
    if (p->valid && p->pressed && !p->consumed && p->panel == cur && rect_has(r, p->x, p->y) && rect_has(c, p->x, p->y)) {
      p->consumed = true;
      plat_haptic(i, 0.35f, 0.03f);
      return true;
    }
  }
  return false;
}

uint32_t ui_id(const char* s, int n) {
  uint32_t h = 2166136261u;
  while (*s) h = (h ^ (uint8_t)*s++) * 16777619u;
  h = (h ^ (uint32_t)n) * 16777619u;
  return h ? h : 1;
}

float ui_anim(uint32_t id, float target, float rate) {
  uint32_t slot = id & 1023;
  for (int probe = 0; probe < 1024; probe++, slot = (slot + 1) & 1023) {
    Anim* a = &anims[slot];
    if (a->id == id || a->id == 0) {
      if (a->id == 0) {
        a->id = id;
        a->v = 0;
      }
      a->v = approachf(a->v, target, rate, frame_dt);
      a->touched = 1;
      return a->v;
    }
  }
  return target;
}

void ui_render(const m4* viewproj, Panel** panels, int n) {
  glUseProgram(prog);
  glBindVertexArray(vao);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, atlas);
  glUniform1i(loc_atlas, 0);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  for (int i = 0; i < n; i++) {
    Panel* p = panels[i];
    if (!p->visible || p->nverts == 0 || p->alpha <= 0.001f) continue;
    m4 mvp = m4_mul(*viewproj, panel_px_to_world(p));
    glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, mvp.m);
    glUniform1f(loc_alpha, p->alpha);
    glDrawElements(GL_TRIANGLES, p->nverts / 4 * 6, GL_UNSIGNED_INT, (void*)(sizeof(uint32_t) * (p->first_vert / 4 * 6)));
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
}
