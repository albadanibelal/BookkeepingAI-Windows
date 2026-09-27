#include "gfx.h"

#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "stb_image.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

void mb_init(MeshBuilder* mb) { memset(mb, 0, sizeof(*mb)); }
void mb_free(MeshBuilder* mb) {
  free(mb->v);
  free(mb->idx);
  memset(mb, 0, sizeof(*mb));
}

uint32_t mb_vert(MeshBuilder* mb, v3 p, v3 n, v2 uv) {
  if (mb->nv == mb->cv) {
    mb->cv = mb->cv ? mb->cv * 2 : 256;
    mb->v = realloc(mb->v, sizeof(Vtx) * mb->cv);
  }
  mb->v[mb->nv] = (Vtx){p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y};
  return (uint32_t)mb->nv++;
}

void mb_tri(MeshBuilder* mb, uint32_t a, uint32_t b, uint32_t c) {
  if (mb->ni + 3 > mb->ci) {
    mb->ci = mb->ci ? mb->ci * 2 : 768;
    mb->idx = realloc(mb->idx, sizeof(uint32_t) * mb->ci);
  }
  mb->idx[mb->ni++] = a;
  mb->idx[mb->ni++] = b;
  mb->idx[mb->ni++] = c;
}

void mb_quad(MeshBuilder* mb, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
  mb_tri(mb, a, b, c);
  mb_tri(mb, a, c, d);
}

void mb_transform(MeshBuilder* mb, int first, m4 m) {
  for (int i = first; i < mb->nv; i++) {
    Vtx* v = &mb->v[i];
    v3 p = m4_point(m, V3(v->px, v->py, v->pz));
    v3 n = v3_norm(m4_dir(m, V3(v->nx, v->ny, v->nz)));
    v->px = p.x; v->py = p.y; v->pz = p.z;
    v->nx = n.x; v->ny = n.y; v->nz = n.z;
  }
}

void mb_box(MeshBuilder* mb, v3 c, v3 h) {
  static const float N[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  for (int f = 0; f < 6; f++) {
    v3 n = V3(N[f][0], N[f][1], N[f][2]);
    v3 u = fabsf(n.y) > 0.5f ? V3(1, 0, 0) : V3(0, 1, 0);
    v3 v = v3_cross(n, u);
    v3 base = v3_add(c, v3_mul(n, h));
    v3 du = v3_mul(u, h), dv = v3_mul(v, h);
    uint32_t a = mb_vert(mb, v3_sub(v3_sub(base, du), dv), n, V2(0, 0));
    uint32_t b = mb_vert(mb, v3_sub(v3_add(base, du), dv), n, V2(1, 0));
    uint32_t cc = mb_vert(mb, v3_add(v3_add(base, du), dv), n, V2(1, 1));
    uint32_t d = mb_vert(mb, v3_add(v3_sub(base, du), dv), n, V2(0, 1));
    mb_quad(mb, a, d, cc, b);
  }
}

void mb_sphere(MeshBuilder* mb, float r, int rings, int segs) {
  uint32_t base = (uint32_t)mb->nv;
  for (int i = 0; i <= rings; i++) {
    float v = (float)i / rings;
    float lat = PI_F * 0.5f - v * PI_F;
    for (int j = 0; j <= segs; j++) {
      float u = (float)j / segs;
      float lon = u * 2 * PI_F - PI_F;
      v3 n = V3(cosf(lat) * sinf(lon), sinf(lat), cosf(lat) * cosf(lon));
      mb_vert(mb, v3_scale(n, r), n, V2(u, v));
    }
  }
  for (int i = 0; i < rings; i++)
    for (int j = 0; j < segs; j++) {
      uint32_t a = base + i * (segs + 1) + j, b = a + segs + 1;
      mb_quad(mb, a, b, b + 1, a + 1);
    }
}

void mb_cylinder(MeshBuilder* mb, float r, float y0, float y1, int segs, bool caps) {
  uint32_t base = (uint32_t)mb->nv;
  for (int j = 0; j <= segs; j++) {
    float a = (float)j / segs * 2 * PI_F;
    v3 n = V3(cosf(a), 0, -sinf(a));
    mb_vert(mb, V3(n.x * r, y0, n.z * r), n, V2((float)j / segs, 0));
    mb_vert(mb, V3(n.x * r, y1, n.z * r), n, V2((float)j / segs, 1));
  }
  for (int j = 0; j < segs; j++) {
    uint32_t a = base + j * 2;
    mb_quad(mb, a, a + 2, a + 3, a + 1);
  }
  if (caps) {
    mb_disc(mb, 0, r, y1, segs);
    int first = mb->nv;
    mb_disc(mb, 0, r, y0, segs);
    // flip bottom cap
    for (int i = first; i < mb->nv; i++) mb->v[i].ny = -1;
    for (int i = mb->ni - segs * 6; i < mb->ni; i += 3) {
      uint32_t t = mb->idx[i + 1];
      mb->idx[i + 1] = mb->idx[i + 2];
      mb->idx[i + 2] = t;
    }
  }
}

void mb_disc(MeshBuilder* mb, float r0, float r1, float y, int segs) {
  uint32_t base = (uint32_t)mb->nv;
  for (int j = 0; j <= segs; j++) {
    float a = (float)j / segs * 2 * PI_F;
    float c = cosf(a), s = -sinf(a);
    mb_vert(mb, V3(c * r0, y, s * r0), V3(0, 1, 0), V2(0.5f + 0.5f * c * r0 / r1, 0.5f + 0.5f * s * r0 / r1));
    mb_vert(mb, V3(c * r1, y, s * r1), V3(0, 1, 0), V2(0.5f + 0.5f * c, 0.5f + 0.5f * s));
  }
  for (int j = 0; j < segs; j++) {
    uint32_t a = base + j * 2;
    mb_quad(mb, a, a + 1, a + 3, a + 2);
  }
}

void mb_arc(MeshBuilder* mb, float radius, float a0, float a1, float width, float depth, int segs) {
  // Four faces: outer, inner, front (+Z), back (-Z)
  for (int face = 0; face < 4; face++) {
    uint32_t base = (uint32_t)mb->nv;
    for (int j = 0; j <= segs; j++) {
      float a = a0 + (a1 - a0) * j / segs;
      v3 radial = V3(cosf(a), sinf(a), 0);
      float ro = radius + width * 0.5f, ri = radius - width * 0.5f, hz = depth * 0.5f;
      v3 p0, p1, n;
      switch (face) {
        case 0: p0 = v3_add(v3_scale(radial, ro), V3(0, 0, -hz)); p1 = v3_add(v3_scale(radial, ro), V3(0, 0, hz)); n = radial; break;
        case 1: p0 = v3_add(v3_scale(radial, ri), V3(0, 0, hz)); p1 = v3_add(v3_scale(radial, ri), V3(0, 0, -hz)); n = v3_scale(radial, -1); break;
        case 2: p0 = v3_add(v3_scale(radial, ro), V3(0, 0, hz)); p1 = v3_add(v3_scale(radial, ri), V3(0, 0, hz)); n = V3(0, 0, 1); break;
        default: p0 = v3_add(v3_scale(radial, ri), V3(0, 0, -hz)); p1 = v3_add(v3_scale(radial, ro), V3(0, 0, -hz)); n = V3(0, 0, -1); break;
      }
      float u = (float)j / segs;
      mb_vert(mb, p0, n, V2(u, 0));
      mb_vert(mb, p1, n, V2(u, 1));
    }
    for (int j = 0; j < segs; j++) {
      uint32_t a = base + j * 2;
      mb_quad(mb, a, a + 2, a + 3, a + 1);
    }
  }
}

void mb_grid(MeshBuilder* mb, int nx, int ny, v3 (*fn)(float u, float v, void* ud), void* ud) {
  uint32_t base = (uint32_t)mb->nv;
  for (int j = 0; j <= ny; j++)
    for (int i = 0; i <= nx; i++) {
      float u = (float)i / nx, v = (float)j / ny;
      mb_vert(mb, fn(u, v, ud), V3(0, 0, 1), V2(u, v));
    }
  for (int j = 0; j < ny; j++)
    for (int i = 0; i < nx; i++) {
      uint32_t a = base + j * (nx + 1) + i;
      mb_quad(mb, a, a + 1, a + nx + 2, a + nx + 1);
    }
  // Normals from finite differences
  for (int j = 0; j <= ny; j++)
    for (int i = 0; i <= nx; i++) {
      Vtx* v = &mb->v[base + j * (nx + 1) + i];
      int i0 = i > 0 ? i - 1 : i, i1 = i < nx ? i + 1 : i;
      int j0 = j > 0 ? j - 1 : j, j1 = j < ny ? j + 1 : j;
      Vtx* a = &mb->v[base + j * (nx + 1) + i0];
      Vtx* b = &mb->v[base + j * (nx + 1) + i1];
      Vtx* c = &mb->v[base + j0 * (nx + 1) + i];
      Vtx* d = &mb->v[base + j1 * (nx + 1) + i];
      v3 du = V3(b->px - a->px, b->py - a->py, b->pz - a->pz);
      v3 dv = V3(d->px - c->px, d->py - c->py, d->pz - c->pz);
      v3 n = v3_norm(v3_cross(du, dv));
      v->nx = n.x; v->ny = n.y; v->nz = n.z;
    }
}

Mesh mesh_upload(MeshBuilder* mb) {
  Mesh m = {0};
  glGenVertexArrays(1, &m.vao);
  glBindVertexArray(m.vao);
  glGenBuffers(1, &m.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vtx) * mb->nv, mb->v, GL_STATIC_DRAW);
  glGenBuffers(1, &m.ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * mb->ni, mb->idx, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)12);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)24);
  glBindVertexArray(0);
  m.count = mb->ni;
  return m;
}

void mesh_draw(const Mesh* m) {
  glBindVertexArray(m->vao);
  glDrawElements(GL_TRIANGLES, m->count, GL_UNSIGNED_INT, 0);
}

static GLuint compile(GLenum type, const char* name, const char* src) {
  static const char* header =
      "#version 300 es\n"
      "precision highp float;\n"
      "precision mediump sampler2D;\n";
  const char* srcs[2] = {header, src};
  GLuint s = glCreateShader(type);
  glShaderSource(s, 2, srcs, NULL);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[4096];
    glGetShaderInfoLog(s, sizeof(log), NULL, log);
    plat_log("shader %s (%s) compile error:\n%s", name, type == GL_VERTEX_SHADER ? "vs" : "fs", log);
  }
  return s;
}

GLuint gl_program(const char* name, const char* vs, const char* fs) {
  GLuint p = glCreateProgram();
  GLuint v = compile(GL_VERTEX_SHADER, name, vs), f = compile(GL_FRAGMENT_SHADER, name, fs);
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[4096];
    glGetProgramInfoLog(p, sizeof(log), NULL, log);
    plat_log("program %s link error:\n%s", name, log);
  }
  glDeleteShader(v);
  glDeleteShader(f);
  return p;
}

void tex_params(GLenum target, bool mips, bool repeat) {
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
  if (mips) glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8.0f);
  glGetError();  // anisotropy may be unsupported
}

GLuint tex_from_pixels(const unsigned char* px, int w, int h, int channels, bool srgb, bool mips) {
  GLuint t;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  GLenum fmt = channels == 1 ? GL_RED : channels == 2 ? GL_RG : channels == 3 ? GL_RGB : GL_RGBA;
  GLenum ifmt;
  if (channels == 1) ifmt = GL_R8;
  else if (channels == 2) ifmt = GL_RG8;
  else if (channels == 3) ifmt = srgb ? GL_SRGB8 : GL_RGB8;
  else ifmt = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
  glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, px);
  if (mips) glGenerateMipmap(GL_TEXTURE_2D);
  tex_params(GL_TEXTURE_2D, mips, false);
  return t;
}

GLuint tex_from_png(const unsigned char* data, int size, bool srgb, bool mips, int* w, int* h) {
  int x, y, n;
  // GLES3 cannot render to (or generate mipmaps for) GL_SRGB8, so expand sRGB
  // images to RGBA.
  unsigned char* px = stbi_load_from_memory(data, size, &x, &y, &n, srgb ? 4 : 0);
  if (!px) return 0;
  if (srgb) n = 4;
  GLuint t = tex_from_pixels(px, x, y, n, srgb, mips);
  stbi_image_free(px);
  if (w) *w = x;
  if (h) *h = y;
  return t;
}

GLuint tex_from_asset(const char* name, bool srgb, bool mips, int* w, int* h) {
  int size = 0;
  unsigned char* d = plat_read_asset(name, &size);
  if (!d) {
    plat_log("missing asset %s", name);
    return 0;
  }
  GLuint t = tex_from_png(d, size, srgb, mips, w, h);
  free(d);
  return t;
}
