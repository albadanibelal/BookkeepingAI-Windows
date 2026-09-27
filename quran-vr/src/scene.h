// 3D scene: space observation deck, lectern + Mushaf, Earth, sky, controllers.
#pragma once
#include <GLES3/gl3.h>

#include "platform.h"
#include "vmath.h"

#define PAGE_W 0.265f
#define PAGE_H 0.408f
#define GUTTER 0.003f

typedef struct {
  // Page-turn animation (leaf rotating around the spine)
  bool active;
  int dir;           // +1: forward (left leaf goes to the right), -1: backward
  float t;           // 0..1
  int front_page;    // page shown on the leaf's front face
  int back_page;     // page shown on the back face
} Flip;

typedef struct {
  m4 content;          // content space -> tracking space
  float head_h;        // eye height above floor (content space)
  double time;
  double utc;
  float env_light;     // 0..1
  int theme;           // 0 classic, 1 sepia, 2 night
  float book_dist, book_height;
  int right_page, left_page;  // pages lying flat on each side
  Flip flip;
  Controller ctl[2];
  float laser_len[2];  // < 0: no laser
  bool laser_hit[2];
  float page_highlight[2];  // hover highlight on right/left page
} SceneState;

void scene_init(void);
void scene_render(const AppView* view, const SceneState* s);
void scene_render_overlay(const AppView* view, const SceneState* s);  // lasers, cursors
// Book transform (book local -> tracking space) and hit test on the pages.
m4 scene_book_xform(const SceneState* s);
// Returns 0 = miss, 1 = right page, 2 = left page.
int scene_ray_book(const SceneState* s, v3 origin, v3 dir, float* dist);
// Earth info for the UI
void scene_earth_info(const SceneState* s, bool* makkah_daylight);
