// Immediate-mode UI drawn on 3D panels (glass cards, text, icons) with
// controller-ray interaction.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "vmath.h"

typedef struct {
  float x, y, w, h;
} Rect;

static inline Rect R(float x, float y, float w, float h) { return (Rect){x, y, w, h}; }
static inline bool rect_has(Rect r, float x, float y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

typedef struct Panel {
  m4 model;       // world transform of the panel center (rigid)
  float w, h;     // size in panel pixels
  float mpp;      // meters per pixel
  float alpha;    // global opacity
  bool visible;
  bool interactive;
  int first_vert, nverts;
} Panel;

enum { FONT_REGULAR = 0, FONT_SEMIBOLD = 1 };
enum { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };

typedef struct {
  bool valid;         // ray hits an interactive panel
  Panel* panel;
  float x, y;         // panel pixels
  float dist;         // meters along the ray
  bool down, pressed; // trigger state / edge this frame
  float scroll;       // thumbstick Y
  v3 hit_world;
  bool consumed;
} Pointer;

extern Pointer ui_ptr[2];

void ui_init(void);
void ui_begin_frame(float dt);
void ui_end_frame(void);  // uploads vertex data

// Ray test against a panel (world-space ray). Returns hit distance or -1.
float ui_ray_panel(const Panel* p, v3 origin, v3 dir, float* px, float* py);

void ui_begin_panel(Panel* p);
void ui_end_panel(void);

void ui_rect(Rect r, float radius, uint32_t top, uint32_t bottom);
void ui_border(Rect r, float radius, float thickness, uint32_t color);
void ui_glow(Rect r, float radius, float softness, uint32_t color);
void ui_image(int img, Rect r, uint32_t color);
void ui_image2(int img, Rect r, uint32_t top, uint32_t bottom);  // vertical gradient tint
void ui_image_fit(int img, float cx, float cy, float height, uint32_t color);  // keeps aspect
float ui_image_aspect(int img);
float ui_text(float x, float baseline, const char* s, float size, int font, uint32_t color, int align);
float ui_text_width(const char* s, float size, int font);
void ui_push_clip(Rect r);
void ui_pop_clip(void);
int ui_img(const char* name);

// Interaction helpers for the current panel.
bool ui_hovered(Rect r);
bool ui_clicked(Rect r);
float ui_anim(uint32_t id, float target, float rate);
uint32_t ui_id(const char* s, int n);

void ui_render(const m4* viewproj, Panel** panels, int n);
