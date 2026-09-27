#include "scene.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gfx.h"
#include "pages.h"
#include "shaders.h"

// ------------------------------------------------------------ programs
typedef struct {
  GLuint p;
  GLint model, viewproj, albedo, emissive, spec, gloss, mode, param, cam, ldir, lcol, sky, ground, time;
} LitProg;

static LitProg lit;
static GLuint prog_page, prog_sky, prog_stargen, prog_earth, prog_atmo, prog_bill;

// ------------------------------------------------------------ resources
static Mesh m_floor, m_ribs, m_rings, m_strips, m_parapet;
static Mesh m_block, m_cover, m_lectern_top, m_lectern_col, m_lectern_base, m_lectern_glow;
static Mesh m_page_r, m_page_l, m_sphere, m_skybox;
static Mesh m_ctl_body, m_ctl_face, m_ctl_stick, m_ctl_btn, m_ctl_ring;
static GLuint leaf_vao, leaf_vbo, leaf_ibo;
static int leaf_count;
static GLuint tex_frame, tex_frame_open, tex_day, tex_night, tex_water, tex_clouds, cube_stars;
static GLuint bill_vao, bill_vbo;

#define LEAF_NX 28
#define LEAF_NY 6

// Earth placement (content space)
static const float EARTH_AZ = 33.0f, EARTH_EL = 13.0f, EARTH_DIST = 60.0f, EARTH_R = 14.5f;
static const float MAKKAH_LAT = 21.4225f, MAKKAH_LON = 39.8262f;

static void lit_init(void) {
  lit.p = gl_program("lit", VS_MESH, FS_LIT);
#define L(f, n) lit.f = glGetUniformLocation(lit.p, n)
  L(model, "uModel"); L(viewproj, "uViewProj"); L(albedo, "uAlbedo"); L(emissive, "uEmissive");
  L(spec, "uSpec"); L(gloss, "uGloss"); L(mode, "uMode"); L(param, "uParam"); L(cam, "uCamPos");
  L(ldir, "uLightDir"); L(lcol, "uLightCol"); L(sky, "uSkyCol"); L(ground, "uGroundCol"); L(time, "uTime");
#undef L
}

static void uni3(GLuint p, const char* n, v3 v) { glUniform3f(glGetUniformLocation(p, n), v.x, v.y, v.z); }
static void uni1f(GLuint p, const char* n, float v) { glUniform1f(glGetUniformLocation(p, n), v); }
static void uni1i(GLuint p, const char* n, int v) { glUniform1i(glGetUniformLocation(p, n), v); }
static void unim(GLuint p, const char* n, m4 m) { glUniformMatrix4fv(glGetUniformLocation(p, n), 1, GL_FALSE, m.m); }

// ------------------------------------------------------------ geometry builders
static float page_curve(float s) {
  // s: 0 at spine .. 1 at outer edge. Gentle lift typical of an open book.
  return 0.020f * sinf(PI_F * powf(clampf(s, 0, 1), 0.72f)) * (1.0f - 0.3f * s) + 0.0015f;
}

typedef struct { int side; } PageUD;
static v3 page_fn(float u, float v, void* ud) {
  int side = ((PageUD*)ud)->side;  // +1 right page, -1 left page
  float s = side > 0 ? u : 1 - u;  // distance from spine
  float x = side > 0 ? GUTTER + u * PAGE_W : -GUTTER - PAGE_W + u * PAGE_W;
  return V3(x, (v - 0.5f) * PAGE_H, page_curve(s));
}

static void build_book(void) {
  MeshBuilder mb;
  PageUD ud = {1};
  mb_init(&mb);
  mb_grid(&mb, 32, 4, page_fn, &ud);
  m_page_r = mesh_upload(&mb);
  mb_free(&mb);
  ud.side = -1;
  mb_init(&mb);
  mb_grid(&mb, 32, 4, page_fn, &ud);
  m_page_l = mesh_upload(&mb);
  mb_free(&mb);

  float hw = GUTTER + PAGE_W;
  mb_init(&mb);
  mb_box(&mb, V3(0, 0, -0.0145f), V3(hw - 0.001f, PAGE_H * 0.5f - 0.001f, 0.0145f));
  m_block = mesh_upload(&mb);
  mb_free(&mb);

  mb_init(&mb);
  mb_box(&mb, V3(0, 0, -0.0325f), V3(hw + 0.012f, PAGE_H * 0.5f + 0.013f, 0.0035f));
  // spine roll
  int first = mb.nv;
  mb_cylinder(&mb, 0.016f, -PAGE_H * 0.5f - 0.013f, PAGE_H * 0.5f + 0.013f, 20, true);
  mb_transform(&mb, first, m4_translate(V3(0, 0, -0.034f)));
  m_cover = mesh_upload(&mb);
  mb_free(&mb);

  // Lectern: tilted plate + ledge (book space), column + base built in content space at draw.
  mb_init(&mb);
  mb_box(&mb, V3(0, -0.010f, -0.045f), V3(0.286f, 0.228f, 0.008f));
  mb_box(&mb, V3(0, -PAGE_H * 0.5f - 0.028f, -0.022f), V3(0.29f, 0.009f, 0.028f));
  m_lectern_top = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  mb_cylinder(&mb, 1.0f, 0.0f, 1.0f, 32, false);
  m_lectern_col = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  mb_cylinder(&mb, 0.26f, 0.0f, 0.035f, 64, true);
  first = mb.nv;
  mb_cylinder(&mb, 0.20f, 0.035f, 0.05f, 64, true);
  m_lectern_base = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  mb_disc(&mb, 0.262f, 0.272f, 0.036f, 96);
  m_lectern_glow = mesh_upload(&mb);
  mb_free(&mb);

  // Dynamic leaf for page turning
  glGenVertexArrays(1, &leaf_vao);
  glBindVertexArray(leaf_vao);
  glGenBuffers(1, &leaf_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, leaf_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vtx) * (LEAF_NX + 1) * (LEAF_NY + 1), NULL, GL_DYNAMIC_DRAW);
  uint32_t idx[LEAF_NX * LEAF_NY * 6];
  int k = 0;
  for (int j = 0; j < LEAF_NY; j++)
    for (int i = 0; i < LEAF_NX; i++) {
      uint32_t a = j * (LEAF_NX + 1) + i;
      idx[k++] = a; idx[k++] = a + 1; idx[k++] = a + LEAF_NX + 2;
      idx[k++] = a; idx[k++] = a + LEAF_NX + 2; idx[k++] = a + LEAF_NX + 1;
    }
  leaf_count = k;
  glGenBuffers(1, &leaf_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, leaf_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)12);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)24);
  glBindVertexArray(0);
}

static void build_room(void) {
  MeshBuilder mb;
  mb_init(&mb);
  mb_disc(&mb, 0.0f, 16.0f, 0.0f, 128);
  m_floor = mesh_upload(&mb);
  mb_free(&mb);

  // Dome ribs: meridian arcs of a 7.6 m dome, leaving the front open as a window.
  const float R = 7.6f;
  mb_init(&mb);
  const float az[] = {-162, -132, -102, -72, -44, 64, 94, 124, 154};
  for (unsigned i = 0; i < sizeof(az) / sizeof(az[0]); i++) {
    int first = mb.nv;
    mb_arc(&mb, R, DEG2RAD(2), DEG2RAD(79), 0.30f, 0.16f, 48);
    // arc lies in XY plane (x radial); rotate so radial points to azimuth
    float a = DEG2RAD(az[i]);
    mb_transform(&mb, first, m4_rot_y(PI_F * 0.5f - a));
  }
  m_ribs = mesh_upload(&mb);
  mb_free(&mb);

  // Horizontal rings: crown at the top + window sill ring
  mb_init(&mb);
  int first = mb.nv;
  mb_arc(&mb, R * cosf(DEG2RAD(79)), 0, 2 * PI_F, 0.34f, 0.26f, 64);
  mb_transform(&mb, first, m4_mul(m4_translate(V3(0, R * sinf(DEG2RAD(79)), 0)), m4_rot_x(PI_F * 0.5f)));
  m_rings = mesh_upload(&mb);
  mb_free(&mb);

  // Parapet: a low curved wall around the deck with a light strip on top
  mb_init(&mb);
  first = mb.nv;
  mb_arc(&mb, 6.6f, 0, 2 * PI_F, 0.5f, 0.16f, 128);
  mb_transform(&mb, first, m4_mul(m4_translate(V3(0, 0.08f, 0)), m4_rot_x(PI_F * 0.5f)));
  first = mb.nv;
  mb_arc(&mb, 6.45f, 0, 2 * PI_F, 0.05f, 0.05f, 192);  // hand rail
  mb_transform(&mb, first, m4_mul(m4_translate(V3(0, 0.98f, 0)), m4_rot_x(PI_F * 0.5f)));
  m_parapet = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  mb_disc(&mb, 6.34f, 6.37f, 0.165f, 256);
  m_strips = mesh_upload(&mb);
  mb_free(&mb);

  mb_init(&mb);
  mb_sphere(&mb, 1.0f, 64, 128);
  m_sphere = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  mb_box(&mb, V3(0, 0, 0), V3(1, 1, 1));
  m_skybox = mesh_upload(&mb);
  mb_free(&mb);
}

static void ellipsoid(MeshBuilder* mb, v3 c, v3 r, float tilt_deg) {
  int first = mb->nv;
  mb_sphere(mb, 1.0f, 18, 32);
  mb_transform(mb, first, m4_mul(m4_mul(m4_translate(c), m4_rot_x(DEG2RAD(tilt_deg))), m4_scale(r)));
}

static void build_controller(void) {
  // Stylised Touch-style controller in grip space (-Z forward, +Y up).
  MeshBuilder mb;
  mb_init(&mb);
  ellipsoid(&mb, V3(0, -0.022f, 0.030f), V3(0.0185f, 0.0215f, 0.050f), -38);  // handle
  ellipsoid(&mb, V3(0, 0.006f, -0.018f), V3(0.031f, 0.017f, 0.033f), -18);   // head
  m_ctl_body = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  ellipsoid(&mb, V3(0, 0.018f, -0.020f), V3(0.0265f, 0.0065f, 0.028f), -18);  // face plate
  m_ctl_face = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  int first = mb.nv;
  mb_cylinder(&mb, 0.0085f, 0.0f, 0.009f, 24, true);
  mb_transform(&mb, first, m4_mul(m4_translate(V3(-0.007f, 0.021f, -0.010f)), m4_rot_x(DEG2RAD(-18))));
  first = mb.nv;
  mb_sphere(&mb, 1.0f, 8, 16);
  mb_transform(&mb, first, m4_mul(m4_translate(V3(-0.007f, 0.031f, -0.013f)), m4_scale(V3(0.0095f, 0.003f, 0.0095f))));
  m_ctl_stick = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  const float bx[2] = {0.011f, 0.004f}, bz[2] = {-0.026f, -0.037f};
  for (int i = 0; i < 2; i++) {
    first = mb.nv;
    mb_cylinder(&mb, 0.0052f, 0.0f, 0.004f, 16, true);
    mb_transform(&mb, first, m4_mul(m4_translate(V3(bx[i], 0.020f, bz[i])), m4_rot_x(DEG2RAD(-18))));
  }
  m_ctl_btn = mesh_upload(&mb);
  mb_free(&mb);
  mb_init(&mb);
  ellipsoid(&mb, V3(0, -0.014f, -0.030f), V3(0.009f, 0.012f, 0.010f), -30);  // trigger
  m_ctl_ring = mesh_upload(&mb);
  mb_free(&mb);
}

static void gen_stars(void) {
  const int S = 1024;
  glGenTextures(1, &cube_stars);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cube_stars);
  for (int f = 0; f < 6; f++)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGBA8, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  GLuint fbo, vao;
  GLint prev_fbo, vp[4];
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
  glGetIntegerv(GL_VIEWPORT, vp);
  glGenFramebuffers(1, &fbo);
  glGenVertexArrays(1, &vao);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glUseProgram(prog_stargen);
  glBindVertexArray(vao);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glViewport(0, 0, S, S);
  for (int f = 0; f < 6; f++) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, cube_stars, 0);
    uni1i(prog_stargen, "uFace", f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
  glDeleteFramebuffers(1, &fbo);
  glDeleteVertexArrays(1, &vao);
  glViewport(vp[0], vp[1], vp[2], vp[3]);
  glEnable(GL_DEPTH_TEST);
}

void scene_init(void) {
  lit_init();
  prog_page = gl_program("page", VS_MESH, FS_PAGE);
  prog_sky = gl_program("sky", VS_SKY, FS_SKY);
  prog_stargen = gl_program("stargen", VS_FULLSCREEN, FS_STARGEN);
  prog_earth = gl_program("earth", VS_MESH, FS_EARTH);
  prog_atmo = gl_program("atmo", VS_MESH, FS_ATMO);
  prog_bill = gl_program("bill", VS_BILL, FS_BILL);
  build_book();
  build_room();
  build_controller();
  tex_frame = tex_from_asset("frame.png", true, true, NULL, NULL);
  tex_frame_open = tex_from_asset("frame_open.png", true, true, NULL, NULL);
  tex_day = tex_from_asset("earth_day.jpg", true, true, NULL, NULL);
  tex_night = tex_from_asset("earth_night.jpg", true, true, NULL, NULL);
  tex_water = tex_from_asset("earth_water.png", false, true, NULL, NULL);
  tex_clouds = tex_from_asset("earth_clouds.png", false, true, NULL, NULL);
  glBindTexture(GL_TEXTURE_2D, tex_clouds);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gen_stars();

  glGenVertexArrays(1, &bill_vao);
  glBindVertexArray(bill_vao);
  glGenBuffers(1, &bill_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, bill_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9 * 6 * 8, NULL, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 36, (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 36, (void*)12);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 36, (void*)20);
  glBindVertexArray(0);
}

// ------------------------------------------------------------ transforms
m4 scene_book_xform(const SceneState* s) {
  const float tilt = DEG2RAD(34);  // page normal tilted 34 deg up from horizontal-forward
  v3 pos = V3(0, s->head_h - 0.36f + s->book_height, -s->book_dist);
  return m4_mul(s->content, m4_mul(m4_translate(pos), m4_rot_x(-tilt)));
}

int scene_ray_book(const SceneState* s, v3 o, v3 d, float* dist) {
  m4 bx = scene_book_xform(s);
  m4 inv = m4_rigid_inverse(bx);
  v3 lo = m4_point(inv, o), ld = m4_dir(inv, d);
  if (ld.z >= -1e-5f) return 0;
  float t = (0.012f - lo.z) / ld.z;
  if (t < 0) return 0;
  v3 h = v3_add(lo, v3_scale(ld, t));
  if (fabsf(h.y) > PAGE_H * 0.5f) return 0;
  *dist = t;
  if (h.x > GUTTER && h.x < GUTTER + PAGE_W) return 1;
  if (h.x < -GUTTER && h.x > -GUTTER - PAGE_W) return 2;
  return 0;
}

static v3 latlon(float lat_deg, float lon_deg) {
  float la = DEG2RAD(lat_deg), lo = DEG2RAD(lon_deg);
  return V3(cosf(la) * sinf(lo), sinf(la), cosf(la) * cosf(lo));
}

// Earth rotation (local -> content) so Makkah faces the viewer, north up (tilted).
static void earth_frame(const SceneState* s, v3* center, m4* rot) {
  float az = DEG2RAD(EARTH_AZ), el = DEG2RAD(EARTH_EL);
  v3 dir = V3(sinf(az) * cosf(el), sinf(el), -cosf(az) * cosf(el));
  *center = v3_add(V3(0, s->head_h, 0), v3_scale(dir, EARTH_DIST));
  v3 F = v3_scale(dir, -1);  // toward viewer
  v3 U = v3_norm(v3_sub(V3(0, 1, 0), v3_scale(F, F.y)));
  U = q_rotate(q_axis_angle(F, DEG2RAD(-18)), U);  // axial tilt look
  v3 Rt = v3_cross(U, F);
  float la = DEG2RAD(MAKKAH_LAT), lo = DEG2RAD(MAKKAH_LON);
  v3 m = latlon(MAKKAH_LAT, MAKKAH_LON);
  v3 n = V3(-sinf(la) * sinf(lo), cosf(la), -sinf(la) * cosf(lo));
  v3 e = v3_cross(n, m);
  // R = [Rt U F] * [e n m]^T
  m4 A = m4_identity(), B = m4_identity();
  A.m[0] = Rt.x; A.m[1] = Rt.y; A.m[2] = Rt.z;
  A.m[4] = U.x; A.m[5] = U.y; A.m[6] = U.z;
  A.m[8] = F.x; A.m[9] = F.y; A.m[10] = F.z;
  B.m[0] = e.x; B.m[4] = e.y; B.m[8] = e.z;
  B.m[1] = n.x; B.m[5] = n.y; B.m[9] = n.z;
  B.m[2] = m.x; B.m[6] = m.y; B.m[10] = m.z;
  *rot = m4_mul(A, B);
}

static v3 subsolar(double utc) {
  time_t t = (time_t)utc;
  struct tm tmv;
  gmtime_r(&t, &tmv);
  float hours = tmv.tm_hour + tmv.tm_min / 60.0f + tmv.tm_sec / 3600.0f;
  float decl = -23.44f * cosf(2 * PI_F / 365.0f * (tmv.tm_yday + 10));
  float lon = (12.0f - hours) * 15.0f;
  return latlon(decl, lon);
}

void scene_earth_info(const SceneState* s, bool* day) {
  v3 sun = subsolar(s->utc);
  *day = v3_dot(sun, latlon(MAKKAH_LAT, MAKKAH_LON)) > 0;
}

// ------------------------------------------------------------ drawing helpers
typedef struct {
  const AppView* v;
  const SceneState* s;
  m4 vp;
  v3 ldir, lcol, sky, ground;
} Ctx;

static void lit_begin(Ctx* c) {
  glUseProgram(lit.p);
  glUniformMatrix4fv(lit.viewproj, 1, GL_FALSE, c->vp.m);
  glUniform3f(lit.cam, c->v->eye_pos.x, c->v->eye_pos.y, c->v->eye_pos.z);
  glUniform3f(lit.ldir, c->ldir.x, c->ldir.y, c->ldir.z);
  glUniform3f(lit.lcol, c->lcol.x, c->lcol.y, c->lcol.z);
  glUniform3f(lit.sky, c->sky.x, c->sky.y, c->sky.z);
  glUniform3f(lit.ground, c->ground.x, c->ground.y, c->ground.z);
  glUniform1f(lit.time, (float)c->s->time);
}

static void lit_draw(const Mesh* m, m4 model, v3 albedo, v3 emissive, float spec, float gloss, int mode, v4 param) {
  glUniformMatrix4fv(lit.model, 1, GL_FALSE, model.m);
  glUniform3f(lit.albedo, albedo.x, albedo.y, albedo.z);
  glUniform3f(lit.emissive, emissive.x, emissive.y, emissive.z);
  glUniform1f(lit.spec, spec);
  glUniform1f(lit.gloss, gloss);
  glUniform1i(lit.mode, mode);
  glUniform4f(lit.param, param.x, param.y, param.z, param.w);
  mesh_draw(m);
}

typedef struct { float x, y, z, u, v, r, g, b, a; } BVert;

static void bill_quad(BVert* q, v3 p0, v3 p1, v3 p2, v3 p3, v4 col) {
  v3 ps[4] = {p0, p1, p2, p3};
  const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  const int order[6] = {0, 1, 2, 0, 2, 3};
  for (int i = 0; i < 6; i++) {
    int k = order[i];
    q[i] = (BVert){ps[k].x, ps[k].y, ps[k].z, uv[k][0], uv[k][1], col.x, col.y, col.z, col.w};
  }
}

static void bill_draw(Ctx* c, BVert* q, int nquads, int kind) {
  glUseProgram(prog_bill);
  unim(prog_bill, "uViewProj", c->vp);
  uni1i(prog_bill, "uKind", kind);
  glBindVertexArray(bill_vao);
  glBindBuffer(GL_ARRAY_BUFFER, bill_vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(BVert) * 6 * nquads, q);
  glDrawArrays(GL_TRIANGLES, 0, 6 * nquads);
}

static void billboard(Ctx* c, v3 center, float size, v4 col, int kind) {
  v3 to_eye = v3_norm(v3_sub(c->v->eye_pos, center));
  v3 right = v3_norm(v3_cross(V3(0, 1, 0), to_eye));
  if (v3_len(right) < 0.01f) right = V3(1, 0, 0);
  v3 up = v3_cross(to_eye, right);
  right = v3_scale(right, size);
  up = v3_scale(up, size);
  BVert q[6];
  bill_quad(q, v3_sub(v3_sub(center, right), up), v3_sub(v3_add(center, right), up), v3_add(v3_add(center, right), up),
            v3_add(v3_sub(center, right), up), col);
  bill_draw(c, q, 1, kind);
}

// ------------------------------------------------------------ page drawing
static void page_uniforms(const SceneState* s) {
  static const int ink[3][3] = {{22, 18, 14}, {52, 34, 20}, {226, 214, 186}};
  static const int gold[3][3] = {{168, 128, 52}, {160, 112, 44}, {205, 170, 92}};
  static const int mark[3][3] = {{24, 92, 66}, {110, 64, 30}, {120, 190, 150}};
  static const float tint[3][3] = {{1, 1, 1}, {1.0f, 0.90f, 0.74f}, {1, 1, 1}};
  int t = s->theme;
  v3 a = SRGB(ink[t][0], ink[t][1], ink[t][2]);
  v3 g = SRGB(gold[t][0], gold[t][1], gold[t][2]);
  v3 m = SRGB(mark[t][0], mark[t][1], mark[t][2]);
  uni3(prog_page, "uInkCol", a);
  uni3(prog_page, "uGoldCol", g);
  uni3(prog_page, "uMarkCol", m);
  uni3(prog_page, "uPaperTint", V3(tint[t][0], tint[t][1], tint[t][2]));
  uni1f(prog_page, "uNight", t == 2 ? 1.0f : 0.0f);
}

static void bind_page(int page, float spine, float highlight) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, page <= 2 ? tex_frame_open : tex_frame);
  GLuint t = pages_get(page);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, t ? t : tex_frame);
  uni1f(prog_page, "uLoaded", t ? 1.0f : 0.0f);
  uni1f(prog_page, "uSpine", spine);
  uni1f(prog_page, "uHighlight", highlight);
}

static void update_leaf(const Flip* f) {
  // Leaf starts on the left side (forward) or right side (backward) and
  // rotates around the spine (book local Y axis) with a soft curl.
  float e = f->t < 0.5f ? 2 * f->t * f->t : 1 - powf(-2 * f->t + 2, 2) * 0.5f;
  float phi = e * PI_F;
  Vtx v[(LEAF_NX + 1) * (LEAF_NY + 1)];
  for (int j = 0; j <= LEAF_NY; j++)
    for (int i = 0; i <= LEAF_NX; i++) {
      float s = (float)i / LEAF_NX;  // distance from spine
      float y = ((float)j / LEAF_NY - 0.5f) * PAGE_H;
      float lag = 0.55f * sinf(phi) * powf(s, 1.3f);
      float a = phi - lag + 0.9f * sinf(phi) * (float)(j - LEAF_NY / 2) / LEAF_NY * s * 0.35f;
      float r = GUTTER + s * PAGE_W;
      float base = page_curve(s) * (1 - sinf(phi));
      // angle measured from the resting side
      float x, z;
      if (f->dir > 0) { x = -r * cosf(a); z = r * sinf(a) + base + 0.002f; }
      else { x = r * cosf(a); z = r * sinf(a) + base + 0.002f; }
      float u = f->dir > 0 ? 1 - s : s;  // front face texture u
      v[j * (LEAF_NX + 1) + i] = (Vtx){x, y, z, 0, 0, 1, u, (float)j / LEAF_NY};
    }
  // normals
  for (int j = 0; j <= LEAF_NY; j++)
    for (int i = 0; i <= LEAF_NX; i++) {
      Vtx* c = &v[j * (LEAF_NX + 1) + i];
      Vtx* a = &v[j * (LEAF_NX + 1) + (i > 0 ? i - 1 : i)];
      Vtx* b = &v[j * (LEAF_NX + 1) + (i < LEAF_NX ? i + 1 : i)];
      Vtx* d = &v[(j > 0 ? j - 1 : j) * (LEAF_NX + 1) + i];
      Vtx* e2 = &v[(j < LEAF_NY ? j + 1 : j) * (LEAF_NX + 1) + i];
      v3 du = V3(b->px - a->px, b->py - a->py, b->pz - a->pz);
      v3 dv = V3(e2->px - d->px, e2->py - d->py, e2->pz - d->pz);
      v3 n = v3_norm(v3_cross(du, dv));
      if (f->dir < 0) n = v3_scale(n, -1);
      c->nx = n.x; c->ny = n.y; c->nz = n.z;
    }
  glBindBuffer(GL_ARRAY_BUFFER, leaf_vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
}

static void draw_book(Ctx* c) {
  const SceneState* s = c->s;
  m4 bx = scene_book_xform(s);

  // Lectern and covers
  lit_begin(c);
  v3 graphite = SRGB(38, 40, 46);
  lit_draw(&m_lectern_top, bx, SRGB(58, 38, 26), V3(0, 0, 0), 0.55f, 50, 0, V4(0, 0, 0, 0));
  // Column rises from the floor to the underside of the tilted plate.
  m4 inv_content = m4_rigid_inverse(s->content);
  v3 pc = m4_point(inv_content, m4_point(bx, V3(0, -0.02f, -0.056f)));
  float col_h = pc.y - 0.03f;
  m4 colx = m4_mul(s->content, m4_mul(m4_translate(V3(pc.x, 0.03f, pc.z)), m4_scale(V3(0.026f, col_h, 0.026f))));
  lit_draw(&m_lectern_col, colx, SRGB(180, 184, 192), V3(0, 0, 0), 0.9f, 80, 0, V4(0, 0, 0, 0));
  m4 basex = m4_mul(s->content, m4_translate(V3(pc.x, 0.0f, pc.z)));
  lit_draw(&m_lectern_base, basex, graphite, V3(0, 0, 0), 0.6f, 50, 0, V4(0, 0, 0, 0));
  lit_draw(&m_lectern_glow, basex, V3(0, 0, 0), v3_scale(SRGB(110, 200, 255), 1.4f), 0, 1, 4, V4(0, 0, 0, 0));

  lit_draw(&m_cover, bx, SRGB(24, 58, 44), V3(0, 0, 0), 0.35f, 30, 3, V4(2 * (GUTTER + PAGE_W + 0.012f), PAGE_H + 0.026f, 0, 0));
  lit_draw(&m_block, bx, SRGB(236, 226, 200), V3(0, 0, 0), 0.05f, 8, 2, V4(0, 0, 0, 0));

  // Pages
  glUseProgram(prog_page);
  unim(prog_page, "uModel", bx);
  unim(prog_page, "uViewProj", c->vp);
  uni3(prog_page, "uCamPos", c->v->eye_pos);
  uni3(prog_page, "uLightDir", c->ldir);
  uni1i(prog_page, "uFrame", 0);
  uni1i(prog_page, "uInk", 1);
  page_uniforms(s);
  uni1f(prog_page, "uBack", 0.0f);
  bind_page(s->right_page, 0.0f, s->page_highlight[0]);
  mesh_draw(&m_page_r);
  bind_page(s->left_page, 1.0f, s->page_highlight[1]);
  mesh_draw(&m_page_l);

  if (s->flip.active) {
    update_leaf(&s->flip);
    glBindVertexArray(leaf_vao);
    // Front face: page lifted from the resting side
    glEnable(GL_CULL_FACE);
    // Leaf triangles wind clockwise (seen from the page front) for a forward
    // turn and counter-clockwise for a backward turn.
    glCullFace(s->flip.dir > 0 ? GL_FRONT : GL_BACK);
    bind_page(s->flip.front_page, s->flip.dir > 0 ? 1.0f : 0.0f, 0);
    glDrawElements(GL_TRIANGLES, leaf_count, GL_UNSIGNED_INT, 0);
    // Back face: the page that lands on the other side (texture u mirrored)
    glCullFace(s->flip.dir > 0 ? GL_BACK : GL_FRONT);
    uni1f(prog_page, "uBack", 1.0f);
    bind_page(s->flip.back_page, s->flip.dir > 0 ? 0.0f : 1.0f, 0);
    glDrawElements(GL_TRIANGLES, leaf_count, GL_UNSIGNED_INT, 0);
    uni1f(prog_page, "uBack", 0.0f);
    glCullFace(GL_BACK);
  }
}

static void draw_controller(Ctx* c, const Controller* ctl, int hand) {
  if (!ctl->active || !ctl->grip.valid) return;
  m4 g = m4_trs(ctl->grip.pos, ctl->grip.rot, V3(hand == HAND_LEFT ? -1 : 1, 1, 1));
  lit_begin(c);
  glDisable(GL_CULL_FACE);
  lit_draw(&m_ctl_body, g, SRGB(232, 234, 238), V3(0, 0, 0), 0.35f, 30, 0, V4(0, 0, 0, 0));
  lit_draw(&m_ctl_face, g, SRGB(28, 29, 33), V3(0, 0, 0), 0.8f, 60, 0, V4(0, 0, 0, 0));
  lit_draw(&m_ctl_ring, g, SRGB(40, 42, 48), V3(0, 0, 0), 0.5f, 40, 0, V4(0, 0, 0, 0));
  lit_draw(&m_ctl_stick, g, SRGB(20, 20, 22), V3(0, 0, 0), 0.4f, 30, 0, V4(0, 0, 0, 0));
  lit_draw(&m_ctl_btn, g, SRGB(50, 52, 58), v3_scale(SRGB(90, 170, 255), ctl->trigger * 0.4f), 0.6f, 40, 0, V4(0, 0, 0, 0));
  glEnable(GL_CULL_FACE);
}

static void draw_lasers(Ctx* c) {
  const SceneState* s = c->s;
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  for (int h = 0; h < 2; h++) {
    const Controller* ctl = &s->ctl[h];
    if (!ctl->active || !ctl->aim.valid || s->laser_len[h] <= 0) continue;
    v3 o = ctl->aim.pos;
    v3 d = q_rotate(ctl->aim.rot, V3(0, 0, -1));
    float len = s->laser_len[h];
    v3 e = v3_add(o, v3_scale(d, len));
    v3 mid = v3_scale(v3_add(o, e), 0.5f);
    v3 side = v3_norm(v3_cross(d, v3_sub(c->v->eye_pos, mid)));
    float w = 0.0035f;
    v3 sw = v3_scale(side, w);
    BVert q[6];
    float bright = s->laser_hit[h] ? 1.0f : 0.55f;
    v4 col = V4(0.55f, 0.85f, 1.0f, bright * (0.6f + 0.4f * ctl->trigger));
    bill_quad(q, v3_sub(o, sw), v3_sub(e, sw), v3_add(e, sw), v3_add(o, sw), col);
    bill_draw(c, q, 1, 1);
    if (s->laser_hit[h]) billboard(c, e, 0.012f + 0.006f * ctl->trigger, V4(0.7f, 0.92f, 1.0f, 1.2f), 0);
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
}

void scene_render(const AppView* v, const SceneState* s) {
  Ctx c;
  c.v = v;
  c.s = s;
  c.vp = m4_mul(v->proj, v->view);
  float env = s->env_light;
  c.ldir = v3_norm(m4_dir(s->content, V3(0.35f, 0.88f, 0.32f)));
  c.lcol = v3_scale(V3(1.0f, 0.97f, 0.92f), 1.05f * env);
  c.sky = v3_scale(V3(0.56f, 0.64f, 0.78f), 0.95f * env);
  c.ground = v3_scale(V3(0.34f, 0.33f, 0.33f), 0.9f * env);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  // Earth frame and sun
  v3 ec;
  m4 er;
  earth_frame(s, &ec, &er);
  v3 sun_local = subsolar(s->utc);
  v3 sun_content = m4_dir(er, sun_local);
  v3 sun_world = v3_norm(m4_dir(s->content, sun_content));
  v3 earth_dir_content = v3_norm(v3_sub(ec, V3(0, s->head_h, 0)));
  float sun_near = v3_dot(sun_content, earth_dir_content);
  float sun_vis = smoothf((sun_near - 0.80f) / 0.12f);

  // Sky
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glUseProgram(prog_sky);
  m4 rot_view = v->view;
  rot_view.m[12] = rot_view.m[13] = rot_view.m[14] = 0;
  unim(prog_sky, "uViewRotProj", m4_mul(v->proj, rot_view));
  uni3(prog_sky, "uSunDir", sun_world);
  uni1f(prog_sky, "uSunVis", sun_vis);
  uni1i(prog_sky, "uStars", 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cube_stars);
  mesh_draw(&m_skybox);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);

  // Earth
  m4 em = m4_mul(s->content, m4_mul(m4_translate(ec), m4_mul(er, m4_scale(V3(EARTH_R, EARTH_R, EARTH_R)))));
  glUseProgram(prog_earth);
  unim(prog_earth, "uModel", em);
  unim(prog_earth, "uViewProj", c.vp);
  uni3(prog_earth, "uSunDir", sun_world);
  uni3(prog_earth, "uCamPos", v->eye_pos);
  uni3(prog_earth, "uMarker", v3_norm(m4_dir(s->content, m4_dir(er, latlon(MAKKAH_LAT, MAKKAH_LON)))));
  uni1f(prog_earth, "uTime", (float)fmod(s->time, 10000.0));
  uni1i(prog_earth, "uDay", 0);
  uni1i(prog_earth, "uNightTex", 1);
  uni1i(prog_earth, "uWater", 2);
  uni1i(prog_earth, "uClouds", 3);
  GLuint et[4] = {tex_day, tex_night, tex_water, tex_clouds};
  for (int i = 0; i < 4; i++) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, et[i]);
  }
  mesh_draw(&m_sphere);
  // Atmosphere halo
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glDepthMask(GL_FALSE);
  glUseProgram(prog_atmo);
  unim(prog_atmo, "uModel", m4_mul(em, m4_scale(V3(1.035f, 1.035f, 1.035f))));
  unim(prog_atmo, "uViewProj", c.vp);
  uni3(prog_atmo, "uSunDir", sun_world);
  uni3(prog_atmo, "uCamPos", v->eye_pos);
  mesh_draw(&m_sphere);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);

  // Room
  lit_begin(&c);
  m4 C = s->content;
  lit_draw(&m_floor, C, SRGB(196, 202, 212), V3(0, 0, 0), 0.25f, 24, 1, V4(env, 0, 0, 0));
  lit_draw(&m_parapet, C, SRGB(228, 231, 236), V3(0, 0, 0), 0.3f, 30, 0, V4(0, 0, 0, 0));
  lit_draw(&m_ribs, C, SRGB(236, 238, 242), V3(0, 0, 0), 0.35f, 36, 6, V4(0, 0, 0, 0));
  lit_draw(&m_rings, C, SRGB(236, 238, 242), v3_scale(SRGB(140, 210, 255), 0.35f), 0.3f, 30, 0, V4(0, 0, 0, 0));
  glDisable(GL_CULL_FACE);
  lit_draw(&m_strips, C, V3(0, 0, 0), v3_scale(SRGB(120, 205, 255), 1.6f * (0.4f + 0.6f * env)), 0, 1, 4, V4(0, 0, 0, 0));
  glEnable(GL_CULL_FACE);

  draw_book(&c);
  for (int h = 0; h < 2; h++) draw_controller(&c, &s->ctl[h], h);

  // Sun glare over everything distant
  if (sun_vis > 0.001f) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    v3 sp = v3_add(v->eye_pos, v3_scale(sun_world, 150.0f));
    billboard(&c, sp, 26.0f, V4(1.0f, 0.88f, 0.7f, 1.4f * sun_vis), 2);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
}

void scene_render_overlay(const AppView* v, const SceneState* s) {
  Ctx c;
  c.v = v;
  c.s = s;
  c.vp = m4_mul(v->proj, v->view);
  draw_lasers(&c);
}
