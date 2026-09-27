// Scene description + renderer interface (pure GLES 3.0, platform independent).
#pragma once
#include "vmath.h"

#define PAGE_COUNT 604
#define SPREAD_COUNT 302          // spread k shows page 2k+1 (right) and 2k+2 (left)

// Book dimensions (meters) -- a large-format Mushaf on a lectern.
#define PAGE_W 0.262f
#define PAGE_H 0.377f

// UI panels (textures are drawn by the Java UI layer).
enum {
    PANEL_SURAHS = 0,
    PANEL_TITLE,
    PANEL_EARTH,
    PANEL_PAGER,
    PANEL_DOCK,
    PANEL_SHEET,
    PANEL_COUNT
};

typedef struct {
    float width;          // world width in meters (height follows texture aspect)
    vec3 pos;             // centre
    float yaw, pitch;     // orientation (radians)
    int texW, texH;
} PanelLayout;

typedef struct {
    int active;
    pose grip, aim;
    float trigger, squeeze;
    float rayLen;         // laser length (m)
    int rayHit;           // laser hits something (draw cursor)
    int pointer;          // is the UI pointer hand (brighter laser)
} HandVis;

typedef struct {
    double time;          // seconds since start (animation)
    double utc;           // unix time (Earth lighting)
    double earthUtcOffset;// frozen offset when live Earth is off
    int showStation;
    int nightMode;
    float markerLat, markerLon;

    // book
    int spread;
    int turning;          // 0 none, +1 forward (leaf travels left -> right), -1 backward
    float alpha;          // leaf angle around the spine: 0 = lying right, PI = lying left

    unsigned panelVisible;
    float panelFade[PANEL_COUNT];
    HandVis hands[2];
} SceneState;

typedef struct {
    mat4 view, proj;
    vec3 eye;
} View;

// Host-provided page textures (GL names); 0 = not ready yet.
typedef unsigned (*PageTexFn)(int page);

void scene_init(void);
void scene_set_earth_textures(unsigned day, unsigned night, unsigned clouds, unsigned spec);
void scene_set_panel_texture(int panel, unsigned tex, int w, int h);
void scene_render(const View* v, const SceneState* s, PageTexFn pageTex);

const PanelLayout* scene_panel_layout(int panel);
void scene_set_panel_size(int panel, int texW, int texH);
mat4 scene_panel_matrix(int panel);          // unit quad [-.5,.5]^2 -> world
mat4 scene_book_matrix(void);                // book local -> world
float scene_page_surface_z(float x);         // top-page height above cover at local x

// GL texture helper: uploads RGBA8 (sRGB) pixels, builds mipmaps.
unsigned gl_upload_rgba(unsigned tex, int w, int h, const void* pixels, int mipmaps, int srgb);
