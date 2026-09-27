// Interface between the portable app (scene, UI, reader) and a platform layer
// (OpenXR on Meta Quest, or the desktop preview renderer).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "vmath.h"

typedef struct {
  v3 pos;
  quat rot;
  bool valid;
} Pose;

typedef struct {
  bool active;
  Pose aim, grip;
  float trigger, squeeze;
  float stick_x, stick_y;
  bool a, b;      // A/X (primary) and B/Y (secondary)
  bool menu;      // left controller menu button
  bool thumb;     // thumbstick click
} Controller;

enum { HAND_LEFT = 0, HAND_RIGHT = 1 };

typedef struct {
  double time;     // seconds, monotonic
  float dt;        // frame delta
  Pose head;       // in tracking space
  Controller ctl[2];
  bool has_floor;  // tracking space origin is on the floor
  bool recenter;   // platform asks to recenter content (e.g. runtime recenter)
} AppInput;

typedef struct {
  m4 view, proj;
  v3 eye_pos;
  int eye;
  int width, height;
} AppView;

// ---- implemented by the platform ----
unsigned char* plat_read_asset(const char* name, int* size);  // malloc'd, NULL if missing
unsigned char* plat_read_user(const char* name, int* size);   // app private storage
bool plat_write_user(const char* name, const void* data, int size);
void plat_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void plat_haptic(int hand, float amplitude, float seconds);
double plat_utc_seconds(void);  // wall clock, seconds since Unix epoch

// ---- implemented by the app ----
void app_init(void);  // GL context is current
void app_update(const AppInput* in);
void app_render(const AppView* view);
void app_shutdown(void);
