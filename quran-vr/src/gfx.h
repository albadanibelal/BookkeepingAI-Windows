// OpenGL ES 3.0 helpers: programs, textures, mesh building.
#pragma once
#include <GLES3/gl3.h>
#include <stdint.h>
#include "vmath.h"

typedef struct {
  float px, py, pz;
  float nx, ny, nz;
  float u, v;
} Vtx;

typedef struct {
  Vtx* v;
  int nv, cv;
  uint32_t* idx;
  int ni, ci;
} MeshBuilder;

typedef struct {
  GLuint vao, vbo, ibo;
  int count;
} Mesh;

void mb_init(MeshBuilder* mb);
void mb_free(MeshBuilder* mb);
uint32_t mb_vert(MeshBuilder* mb, v3 p, v3 n, v2 uv);
void mb_tri(MeshBuilder* mb, uint32_t a, uint32_t b, uint32_t c);
void mb_quad(MeshBuilder* mb, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
// Transform all vertices added since `first` by m (normals by its rotation).
void mb_transform(MeshBuilder* mb, int first, m4 m);

// Primitives (unit-ish; transform with mb_transform)
void mb_box(MeshBuilder* mb, v3 center, v3 half);
void mb_sphere(MeshBuilder* mb, float r, int rings, int segs);  // uv: equirect (u=lon, v=lat)
void mb_cylinder(MeshBuilder* mb, float r, float y0, float y1, int segs, bool caps);
void mb_disc(MeshBuilder* mb, float r0, float r1, float y, int segs);  // annulus facing +Y
// Rectangular cross-section swept along a circular arc in the XY plane,
// centered at origin, from angle a0 to a1 (radians, 0 = +X).
void mb_arc(MeshBuilder* mb, float radius, float a0, float a1, float width, float depth, int segs);
// Rounded-edge slab (for lectern plate / cover)
void mb_grid(MeshBuilder* mb, int nx, int ny, v3 (*fn)(float u, float v, void* ud), void* ud);

Mesh mesh_upload(MeshBuilder* mb);
void mesh_draw(const Mesh* m);

GLuint gl_program(const char* name, const char* vs, const char* fs);
GLuint tex_from_png(const unsigned char* data, int size, bool srgb, bool mips, int* w, int* h);
GLuint tex_from_asset(const char* name, bool srgb, bool mips, int* w, int* h);
GLuint tex_from_pixels(const unsigned char* px, int w, int h, int channels, bool srgb, bool mips);
void tex_params(GLenum target, bool mips, bool repeat);
