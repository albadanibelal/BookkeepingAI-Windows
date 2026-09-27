// Small vector / matrix helpers. Matrices are column-major (OpenGL layout).
#pragma once
#include "platform.h"

#define PI_F 3.14159265358979f

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } quat;
typedef struct { float m[16]; } mat4;
typedef struct { quat ori; vec3 pos; } pose;

static inline vec3 v3(float x, float y, float z) { vec3 r = {x, y, z}; return r; }
static inline vec3 v3add(vec3 a, vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline vec3 v3sub(vec3 a, vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline vec3 v3scale(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline vec3 v3mad(vec3 a, vec3 b, float s) { return v3(a.x + b.x * s, a.y + b.y * s, a.z + b.z * s); }
static inline float v3dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline vec3 v3cross(vec3 a, vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float v3len(vec3 a) { return sqrtf(v3dot(a, a)); }
static inline vec3 v3norm(vec3 a) { float l = v3len(a); return l > 1e-8f ? v3scale(a, 1.0f / l) : v3(0, 0, 0); }
static inline vec3 v3lerp(vec3 a, vec3 b, float t) { return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothstepf(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
static inline float deg2rad(float d) { return d * PI_F / 180.0f; }

static inline quat qidentity(void) { quat q = {0, 0, 0, 1}; return q; }
static inline quat qaxis(vec3 axis, float angle) {
    axis = v3norm(axis);
    float s = sinf(angle * 0.5f);
    quat q = {axis.x * s, axis.y * s, axis.z * s, cosf(angle * 0.5f)};
    return q;
}
static inline quat qmul(quat a, quat b) {
    quat r = {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
    return r;
}
static inline vec3 qrot(quat q, vec3 v) {
    vec3 u = v3(q.x, q.y, q.z);
    vec3 t = v3scale(v3cross(u, v), 2.0f);
    return v3add(v3add(v, v3scale(t, q.w)), v3cross(u, t));
}

static inline mat4 m4identity(void) {
    mat4 r = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    return r;
}
static inline mat4 m4mul(mat4 a, mat4 b) {
    mat4 r;
    for (int c = 0; c < 4; c++)
        for (int rr = 0; rr < 4; rr++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
static inline mat4 m4translate(vec3 t) {
    mat4 r = m4identity();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}
static inline mat4 m4scale(vec3 s) {
    mat4 r = m4identity();
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}
static inline mat4 m4quat(quat q) {
    float x = q.x, y = q.y, z = q.z, w = q.w;
    mat4 r = {{1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0,
               2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0,
               2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0,
               0, 0, 0, 1}};
    return r;
}
static inline mat4 m4basis(vec3 x, vec3 y, vec3 z, vec3 origin) {
    mat4 r = {{x.x, x.y, x.z, 0, y.x, y.y, y.z, 0, z.x, z.y, z.z, 0, origin.x, origin.y, origin.z, 1}};
    return r;
}
static inline mat4 m4pose(pose p) { return m4mul(m4translate(p.pos), m4quat(p.ori)); }
static inline mat4 m4rotY(float a) { return m4quat(qaxis(v3(0, 1, 0), a)); }
static inline mat4 m4rotX(float a) { return m4quat(qaxis(v3(1, 0, 0), a)); }
static inline mat4 m4rotZ(float a) { return m4quat(qaxis(v3(0, 0, 1), a)); }

// Rigid inverse (rotation + translation only).
static inline mat4 m4rigid_inverse(mat4 a) {
    mat4 r = m4identity();
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r.m[i * 4 + j] = a.m[j * 4 + i];
    vec3 t = v3(a.m[12], a.m[13], a.m[14]);
    r.m[12] = -(r.m[0] * t.x + r.m[4] * t.y + r.m[8] * t.z);
    r.m[13] = -(r.m[1] * t.x + r.m[5] * t.y + r.m[9] * t.z);
    r.m[14] = -(r.m[2] * t.x + r.m[6] * t.y + r.m[10] * t.z);
    return r;
}
static inline vec3 m4point(mat4 a, vec3 p) {
    return v3(a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z + a.m[12],
              a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z + a.m[13],
              a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14]);
}
static inline vec3 m4dir(mat4 a, vec3 p) {
    return v3(a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z,
              a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z,
              a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z);
}

// Asymmetric projection from OpenXR field-of-view angles (radians).
static inline mat4 m4fov(float left, float right, float up, float down, float n, float f) {
    float l = tanf(left), r = tanf(right), u = tanf(up), d = tanf(down);
    float w = r - l, h = u - d;
    mat4 m = {{0}};
    m.m[0] = 2.0f / w;
    m.m[5] = 2.0f / h;
    m.m[8] = (r + l) / w;
    m.m[9] = (u + d) / h;
    m.m[10] = -(f + n) / (f - n);
    m.m[11] = -1.0f;
    m.m[14] = -(2.0f * f * n) / (f - n);
    return m;
}
