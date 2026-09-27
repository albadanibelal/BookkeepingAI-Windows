// Small vector / quaternion / matrix library. Matrices are column-major (GL).
#pragma once
#include <math.h>
#include <stdbool.h>

#define PI_F 3.14159265358979f
#define DEG2RAD(d) ((d) * (PI_F / 180.0f))

typedef struct { float x, y; } v2;
typedef struct { float x, y, z; } v3;
typedef struct { float x, y, z, w; } v4;
typedef struct { float x, y, z, w; } quat;
typedef struct { float m[16]; } m4;

static inline v2 V2(float x, float y) { return (v2){x, y}; }
static inline v3 V3(float x, float y, float z) { return (v3){x, y, z}; }
static inline v4 V4(float x, float y, float z, float w) { return (v4){x, y, z, w}; }
static inline v3 v3_add(v3 a, v3 b) { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline v3 v3_sub(v3 a, v3 b) { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline v3 v3_scale(v3 a, float s) { return V3(a.x * s, a.y * s, a.z * s); }
static inline v3 v3_mul(v3 a, v3 b) { return V3(a.x * b.x, a.y * b.y, a.z * b.z); }
static inline float v3_dot(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline v3 v3_cross(v3 a, v3 b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float v3_len(v3 a) { return sqrtf(v3_dot(a, a)); }
static inline v3 v3_norm(v3 a) { float l = v3_len(a); return l > 1e-8f ? v3_scale(a, 1.0f / l) : V3(0, 0, 0); }
static inline v3 v3_lerp(v3 a, v3 b, float t) { return v3_add(a, v3_scale(v3_sub(b, a), t)); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothf(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static inline float approachf(float cur, float target, float rate, float dt) {
  return target + (cur - target) * expf(-rate * dt);
}

static inline quat q_identity(void) { return (quat){0, 0, 0, 1}; }
static inline quat q_axis_angle(v3 axis, float a) {
  axis = v3_norm(axis);
  float s = sinf(a * 0.5f);
  return (quat){axis.x * s, axis.y * s, axis.z * s, cosf(a * 0.5f)};
}
static inline quat q_mul(quat a, quat b) {
  return (quat){
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
static inline quat q_conj(quat q) { return (quat){-q.x, -q.y, -q.z, q.w}; }
static inline v3 q_rotate(quat q, v3 v) {
  v3 u = V3(q.x, q.y, q.z);
  v3 t = v3_scale(v3_cross(u, v), 2.0f);
  return v3_add(v3_add(v, v3_scale(t, q.w)), v3_cross(u, t));
}

static inline m4 m4_identity(void) {
  m4 r = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  return r;
}
static inline m4 m4_mul(m4 a, m4 b) {
  m4 r;
  for (int c = 0; c < 4; c++)
    for (int rr = 0; rr < 4; rr++) {
      float s = 0;
      for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
      r.m[c * 4 + rr] = s;
    }
  return r;
}
static inline m4 m4_translate(v3 t) {
  m4 r = m4_identity();
  r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
  return r;
}
static inline m4 m4_scale(v3 s) {
  m4 r = m4_identity();
  r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
  return r;
}
static inline m4 m4_from_quat(quat q) {
  float x = q.x, y = q.y, z = q.z, w = q.w;
  m4 r = m4_identity();
  r.m[0] = 1 - 2 * (y * y + z * z); r.m[1] = 2 * (x * y + z * w); r.m[2] = 2 * (x * z - y * w);
  r.m[4] = 2 * (x * y - z * w); r.m[5] = 1 - 2 * (x * x + z * z); r.m[6] = 2 * (y * z + x * w);
  r.m[8] = 2 * (x * z + y * w); r.m[9] = 2 * (y * z - x * w); r.m[10] = 1 - 2 * (x * x + y * y);
  return r;
}
static inline m4 m4_trs(v3 t, quat q, v3 s) {
  return m4_mul(m4_translate(t), m4_mul(m4_from_quat(q), m4_scale(s)));
}
static inline m4 m4_rot_x(float a) { return m4_from_quat(q_axis_angle(V3(1, 0, 0), a)); }
static inline m4 m4_rot_y(float a) { return m4_from_quat(q_axis_angle(V3(0, 1, 0), a)); }
static inline m4 m4_rot_z(float a) { return m4_from_quat(q_axis_angle(V3(0, 0, 1), a)); }
// Inverse of a rigid transform (rotation + translation).
static inline m4 m4_rigid_inverse(m4 a) {
  m4 r = m4_identity();
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) r.m[i * 4 + j] = a.m[j * 4 + i];
  v3 t = V3(a.m[12], a.m[13], a.m[14]);
  r.m[12] = -(r.m[0] * t.x + r.m[4] * t.y + r.m[8] * t.z);
  r.m[13] = -(r.m[1] * t.x + r.m[5] * t.y + r.m[9] * t.z);
  r.m[14] = -(r.m[2] * t.x + r.m[6] * t.y + r.m[10] * t.z);
  return r;
}
static inline v3 m4_point(m4 a, v3 p) {
  return V3(a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z + a.m[12],
            a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z + a.m[13],
            a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14]);
}
static inline v3 m4_dir(m4 a, v3 p) {
  return V3(a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z,
            a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z,
            a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z);
}
// Asymmetric projection from OpenXR-style tangent half-angles.
static inline m4 m4_proj_fov(float left, float right, float up, float down, float n, float f) {
  float tl = tanf(left), tr = tanf(right), tu = tanf(up), td = tanf(down);
  float w = tr - tl, h = tu - td;
  m4 r = {{0}};
  r.m[0] = 2 / w;
  r.m[5] = 2 / h;
  r.m[8] = (tr + tl) / w;
  r.m[9] = (tu + td) / h;
  r.m[10] = -(f + n) / (f - n);
  r.m[11] = -1;
  r.m[14] = -(2 * f * n) / (f - n);
  return r;
}
static inline m4 m4_look_at(v3 eye, v3 target, v3 up) {
  v3 f = v3_norm(v3_sub(target, eye));
  v3 s = v3_norm(v3_cross(f, up));
  v3 u = v3_cross(s, f);
  m4 r = m4_identity();
  r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
  r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
  r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
  r.m[12] = -v3_dot(s, eye); r.m[13] = -v3_dot(u, eye); r.m[14] = v3_dot(f, eye);
  return r;
}
// sRGB color (0..255) to linear float.
static inline float srgb_to_lin(float c) {
  c /= 255.0f;
  return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}
static inline v3 SRGB(int r, int g, int b) { return V3(srgb_to_lin(r), srgb_to_lin(g), srgb_to_lin(b)); }
