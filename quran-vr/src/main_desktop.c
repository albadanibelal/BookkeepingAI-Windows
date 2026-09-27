// Desktop preview renderer: runs the exact same app + renderer headlessly
// (EGL + OpenGL ES 3 on Mesa) and writes screenshots of scripted scenes.
// Usage: qvr_preview <assets_dir> <out_dir>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "platform.h"
#include "stb_image_write.h"

static const char* assets_dir = "../assets";
static double fake_utc;

unsigned char* plat_read_asset(const char* name, int* size) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", assets_dir, name);
  FILE* f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char* d = malloc(n + 1);
  if (fread(d, 1, n, f) != (size_t)n) n = 0;
  fclose(f);
  *size = (int)n;
  return d;
}

unsigned char* plat_read_user(const char* name, int* size) {
  (void)name;
  (void)size;
  return NULL;  // previews always start from defaults
}
bool plat_write_user(const char* name, const void* data, int size) {
  (void)name; (void)data; (void)size;
  return true;
}
void plat_log(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}
void plat_haptic(int hand, float a, float s) { (void)hand; (void)a; (void)s; }
double plat_utc_seconds(void) { return fake_utc; }

// debug hooks from app.c
void app_debug_set_mode(int m);
void app_debug_set_query(const char* q);
void app_debug_set_theme(int t);
void app_debug_go(int page);
void app_debug_flip(float t, int dir);
void app_debug_bookmark(int page);

static quat look_rot(v3 from, v3 to) {
  // rotation whose -Z axis points from -> to, Y roughly up
  v3 f = v3_norm(v3_sub(to, from));
  v3 r = v3_norm(v3_cross(f, V3(0, 1, 0)));
  v3 u = v3_cross(r, f);
  // matrix columns r, u, -f -> quaternion
  float m00 = r.x, m11 = u.y, m22 = -f.z;
  float tr = m00 + m11 + m22;
  quat q;
  if (tr > 0) {
    float s = sqrtf(tr + 1.0f) * 2;
    q.w = 0.25f * s;
    q.x = (u.z - (-f.y)) / s;
    q.y = ((-f.x) - r.z) / s;
    q.z = (r.y - u.x) / s;
  } else if (m00 > m11 && m00 > m22) {
    float s = sqrtf(1.0f + m00 - m11 - m22) * 2;
    q.w = (u.z - (-f.y)) / s;
    q.x = 0.25f * s;
    q.y = (u.x + r.y) / s;
    q.z = ((-f.x) + r.z) / s;
  } else if (m11 > m22) {
    float s = sqrtf(1.0f + m11 - m00 - m22) * 2;
    q.w = ((-f.x) - r.z) / s;
    q.x = (u.x + r.y) / s;
    q.y = 0.25f * s;
    q.z = ((-f.y) + u.z) / s;
  } else {
    float s = sqrtf(1.0f + m22 - m00 - m11) * 2;
    q.w = (r.y - u.x) / s;
    q.x = ((-f.x) + r.z) / s;
    q.y = ((-f.y) + u.z) / s;
    q.z = 0.25f * s;
  }
  return q;
}

typedef struct {
  const char* name;
  int mode;          // side panel mode
  int page;
  int theme;
  const char* query;
  float flip_t;      // > 0: capture mid page turn
  v3 look_at;        // camera target
  v3 right_target;   // right controller aim target (0 = none)
  v3 left_target;
  float fov_h;       // horizontal half-FOV degrees
} Shot;

int main(int argc, char** argv) {
  if (argc > 1) assets_dir = argv[1];
  const char* out_dir = argc > 2 ? argv[2] : ".";
  const int W = argc > 3 ? atoi(argv[3]) : 1890, H = argc > 4 ? atoi(argv[4]) : 1260;
  // Fixed wall clock for reproducible "live" Earth lighting: 2026-03-21 12:20 UTC
  fake_utc = 1774095600.0;

  PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
  EGLDisplay dpy = getPlatformDisplay ? getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL) : EGL_NO_DISPLAY;
  if (dpy == EGL_NO_DISPLAY) dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj, min;
  if (!eglInitialize(dpy, &maj, &min)) {
    fprintf(stderr, "eglInitialize failed\n");
    return 1;
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLint cfg_attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
  EGLConfig cfg;
  EGLint ncfg = 0;
  eglChooseConfig(dpy, cfg_attr, &cfg, 1, &ncfg);
  EGLint ctx_attr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, ncfg ? cfg : NULL, EGL_NO_CONTEXT, ctx_attr);
  if (!ctx || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    fprintf(stderr, "context creation failed (0x%x)\n", eglGetError());
    return 1;
  }
  fprintf(stderr, "GL: %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

  // Offscreen targets: 4x MSAA sRGB color + depth, resolved to single-sample.
  GLuint msfbo, msc, msd, fbo, tex;
  glGenRenderbuffers(1, &msc);
  glBindRenderbuffer(GL_RENDERBUFFER, msc);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_SRGB8_ALPHA8, W, H);
  glGenRenderbuffers(1, &msd);
  glBindRenderbuffer(GL_RENDERBUFFER, msd);
  glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, W, H);
  glGenFramebuffers(1, &msfbo);
  glBindFramebuffer(GL_FRAMEBUFFER, msfbo);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msc);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msd);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, W, H);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, msfbo);

  app_init();

  const float eye_y = 1.25f;
  Shot shots[] = {
      {"01_reading", 0, 1, 0, NULL, 0, V3(0, 0.86f, -1.0f), V3(0, 0, 0), V3(-0.62f, 1.10f, -0.98f), 50},
      {"02_search", 1, 50, 0, "yas", 0, V3(-0.12f, 0.90f, -1.0f), V3(-0.56f, 1.02f, -0.98f), V3(0, 0, 0), 50},
      {"03_bookmarks", 2, 282, 0, NULL, 0, V3(-0.1f, 0.90f, -1.0f), V3(0, 0, 0), V3(-0.60f, 1.24f, -0.98f), 50},
      {"04_settings_night", 3, 502, 2, NULL, 0, V3(-0.1f, 0.90f, -1.0f), V3(-0.58f, 1.30f, -0.98f), V3(0, 0, 0), 50},
      {"05_page_turn", 0, 3, 0, NULL, 0.42f, V3(0, 0.88f, -0.9f), V3(0, 0, 0), V3(0, 0, 0), 38},
      {"06_close_reading", 0, 50, 1, NULL, 0, V3(0, 0.80f, -0.60f), V3(0, 0, 0), V3(0, 0, 0), 34},
  };
  int nshots = sizeof(shots) / sizeof(shots[0]);
  const char* only = getenv("QVR_SHOT");
  for (int si = 0; si < nshots; si++) {
    Shot* s = &shots[si];
    if (only && !strstr(s->name, only)) continue;
    app_debug_set_theme(s->theme);
    app_debug_go(s->page);
    app_debug_set_mode(s->mode);
    if (s->query) app_debug_set_query(s->query);
    if (si == 2) {
      app_debug_bookmark(2);
      app_debug_bookmark(50);
      app_debug_bookmark(282);
      app_debug_bookmark(440);
      app_debug_bookmark(582);
    }
    AppInput in;
    memset(&in, 0, sizeof(in));
    in.has_floor = true;
    in.head.valid = true;
    in.head.pos = V3(0, eye_y, 0);
    in.head.rot = q_identity();
    v3 targets[2] = {s->left_target, s->right_target};
    for (int h = 0; h < 2; h++) {
      Controller* c = &in.ctl[h];
      c->active = true;
      v3 gp = V3(h ? 0.19f : -0.19f, 0.84f, -0.30f);
      c->grip.valid = c->aim.valid = true;
      c->grip.pos = gp;
      v3 tgt = v3_len(targets[h]) > 0 ? targets[h] : v3_add(gp, V3(h ? 0.08f : -0.08f, 0.35f, -1.0f));
      c->grip.rot = look_rot(gp, v3_add(gp, V3(h ? 0.1f : -0.1f, 0.25f, -1)));
      c->aim.pos = v3_add(gp, V3(0, 0.02f, -0.05f));
      c->aim.rot = look_rot(c->aim.pos, tgt);
      if (v3_len(targets[h]) == 0) c->aim.valid = false;  // hide laser
    }
    // Warm up: recenter, stream pages, settle animations.
    double t = 100.0 + si * 10;
    for (int f = 0; f < 90; f++) {
      in.time = t;
      in.dt = 1.0f / 72;
      t += in.dt;
      if (s->flip_t > 0 && f == 80) app_debug_flip(0.0f, 1);
      if (s->flip_t > 0 && f > 80) in.dt = s->flip_t * 0.62f / 9.0f;
      app_update(&in);
      if (f < 60) usleep(15000);
      in.recenter = false;
    }
    // Camera
    v3 eye = V3(0, eye_y, 0);
    m4 view = m4_look_at(eye, s->look_at, V3(0, 1, 0));
    float hf = DEG2RAD(s->fov_h);
    float vf = atanf(tanf(hf) * H / W);
    AppView v = {view, m4_proj_fov(-hf, hf, vf, -vf, 0.05f, 400.0f), eye, 0, W, H};
    glBindFramebuffer(GL_FRAMEBUFFER, msfbo);
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    app_render(&v);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msfbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    unsigned char* px = malloc(W * H * 4);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (int i = 0; i < W * H; i++) px[i * 4 + 3] = 255;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.png", out_dir, s->name);
    stbi_flip_vertically_on_write(1);
    stbi_write_png(path, W, H, 4, px, W * 4);
    free(px);
    fprintf(stderr, "wrote %s (glError 0x%x)\n", path, glGetError());
  }
  app_shutdown();
  return 0;
}
