// Desktop preview: renders the VR scene headlessly (Mesa EGL surfaceless) from
// a fixed head pose to a PPM image, using the same scene.c as the headset.
//   preview <texdir> <out.ppm> <spread> [turnDir alpha] [yawDeg pitchDeg] [width height]
#include "../native/scene.h"
#include "../native/app.h"

static unsigned load_raw(const char* dir, const char* name, int srgb, int* ow, int* oh) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.rgba", dir, name);
    FILE* f = fopen(path, "rb");
    if (!f) { LOGE("missing %s", path); return 0; }
    unsigned char hdr[8];
    fread(hdr, 1, 8, f);
    int w = (hdr[0] << 24) | (hdr[1] << 16) | (hdr[2] << 8) | hdr[3];
    int h = (hdr[4] << 24) | (hdr[5] << 16) | (hdr[6] << 8) | hdr[7];
    unsigned char* px = malloc((size_t)w * h * 4);
    fread(px, 1, (size_t)w * h * 4, f);
    fclose(f);
    unsigned t = gl_upload_rgba(0, w, h, px, 1, srgb);
    free(px);
    if (ow) *ow = w;
    if (oh) *oh = h;
    return t;
}

static const char* g_dir;
static unsigned g_pages[PAGE_COUNT + 1];
static unsigned page_tex(int page) {
    if (page < 1 || page > PAGE_COUNT) return 0;
    if (!g_pages[page]) {
        char n[32];
        snprintf(n, sizeof(n), "page%03d", page);
        g_pages[page] = load_raw(g_dir, n, 1, NULL, NULL);
    }
    return g_pages[page];
}

int main(int argc, char** argv) {
    g_dir = argv[1];
    const char* out = argv[2];
    int spread = atoi(argv[3]);
    int turn = argc > 5 ? atoi(argv[4]) : 0;
    float alpha = argc > 5 ? (float)atof(argv[5]) : 0;
    float yaw = argc > 7 ? (float)atof(argv[6]) : 0, pitch = argc > 7 ? (float)atof(argv[7]) : -12;
    int W = argc > 9 ? atoi(argv[8]) : 1600, H = argc > 9 ? atoi(argv[9]) : 1100;

    EGLDisplay d = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    eglInitialize(d, NULL, NULL);
    EGLint ca[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
    EGLConfig cfg; EGLint n;
    eglChooseConfig(d, ca, &cfg, 1, &n);
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint cx[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext ctx = eglCreateContext(d, cfg, EGL_NO_CONTEXT, cx);
    eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);

    GLuint fbo, color, depth, rfbo, rcolor;
    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_SRGB8_ALPHA8, W, H);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, W, H);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glGenRenderbuffers(1, &rcolor);
    glBindRenderbuffer(GL_RENDERBUFFER, rcolor);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, W, H);
    glGenFramebuffers(1, &rfbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rfbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rcolor);

    scene_init();
    scene_set_earth_textures(load_raw(g_dir, "day", 1, 0, 0), load_raw(g_dir, "night", 1, 0, 0),
                             load_raw(g_dir, "clouds", 0, 0, 0), load_raw(g_dir, "ocean", 0, 0, 0));
    for (int p = 0; p < PANEL_COUNT; p++) {
        char nm[16]; int w, h;
        snprintf(nm, sizeof(nm), "panel%d", p);
        unsigned t = load_raw(g_dir, nm, 0, &w, &h);
        scene_set_panel_texture(p, t, w, h);
    }

    SceneState s;
    memset(&s, 0, sizeof(s));
    s.time = 1.0;
    s.utc = getenv("UTC") ? atof(getenv("UTC")) : 1790000000.0;
    s.showStation = 1;
    s.markerLat = 24.7136f; s.markerLon = 46.6753f;
    s.spread = spread; s.turning = turn; s.alpha = alpha;
    s.panelVisible = getenv("SHEET") ? 0x3B : 0x1F;
    for (int i = 0; i < PANEL_COUNT; i++) s.panelFade[i] = 1;
    // controllers held in front, lasers pointing at the book
    for (int h = 0; h < 2; h++) {
        HandVis* hv = &s.hands[h];
        hv->active = 1;
        float side = h ? 0.20f : -0.20f;
        hv->grip.pos = v3(side, -0.52f, -0.30f);
        hv->grip.ori = qmul(qaxis(v3(0, 1, 0), h ? 0.25f : -0.25f), qaxis(v3(1, 0, 0), -0.55f));
        hv->aim = hv->grip;
        hv->rayLen = h ? 0.45f : 0.35f;
        hv->rayHit = 1;
        hv->pointer = h;
    }

    View v;
    mat4 head = m4mul(m4rotY(deg2rad(yaw)), m4rotX(deg2rad(pitch)));
    v.view = m4rigid_inverse(head);
    v.eye = v3(0, 0, 0);
    float hf = deg2rad(52), vf = atanf(tanf(hf) * H / W);
    v.proj = m4fov(-hf, hf, vf, -vf, 0.05f, 100.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, W, H);
    // pre-load page textures
    for (int k = -1; k <= 1; k++) { page_tex(2 * (spread + k) + 1); page_tex(2 * (spread + k) + 2); }
    scene_render(&v, &s, page_tex);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rfbo);
    glBlitFramebuffer(0, 0, W, H, 0, 0, W, H, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, rfbo);
    unsigned char* px = malloc((size_t)W * H * 4);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
    GLenum err = glGetError();
    if (err) LOGE("GL error %x", err);
    FILE* f = fopen(out, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = H - 1; y >= 0; y--)
        for (int x = 0; x < W; x++) fwrite(px + ((size_t)y * W + x) * 4, 1, 3, f);
    fclose(f);
    return 0;
}
