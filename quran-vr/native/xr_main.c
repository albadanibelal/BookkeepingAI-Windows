// OpenXR host for Meta Quest (and other Android OpenXR headsets).
// Runs on a Java thread started by MainActivity; talks to the Java UI layer
// (com.quranvr.mushaf.Bridge) for panel bitmaps, decoded Mushaf pages and
// persistent settings.
#include "app.h"
#include <jni.h>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

// ---- libjnigraphics
typedef struct { uint32_t width, height, stride; int32_t format; uint32_t flags; } AndroidBitmapInfo;
int AndroidBitmap_getInfo(JNIEnv* env, jobject bmp, AndroidBitmapInfo* info);
int AndroidBitmap_lockPixels(JNIEnv* env, jobject bmp, void** addr);
int AndroidBitmap_unlockPixels(JNIEnv* env, jobject bmp);

#define CHECK(x) do { XrResult _r = (x); if (XR_FAILED(_r)) { LOGE("%s failed: %d (line %d)", #x, _r, __LINE__); } } while (0)

// ------------------------------------------------------------------ JNI bridge

static JNIEnv* g_env;
static jobject g_bridge;
static jmethodID m_frame, m_pointer, m_scroll, m_onPage, m_panelBitmap, m_pageBitmap, m_loadAsset, m_startPage;
static volatile int g_exitRequested;

static void clear_exc(void) {
    if ((*g_env)->ExceptionCheck(g_env)) {
        (*g_env)->ExceptionDescribe(g_env);
        (*g_env)->ExceptionClear(g_env);
    }
}

// Uploads an android.graphics.Bitmap (ARGB_8888) into a GL texture.
static unsigned upload_bitmap(jobject bmp, unsigned tex, int srgb, int* ow, int* oh) {
    AndroidBitmapInfo info;
    if (!bmp || AndroidBitmap_getInfo(g_env, bmp, &info) != 0 || info.format != 1 /*RGBA_8888*/) return tex;
    void* px = NULL;
    if (AndroidBitmap_lockPixels(g_env, bmp, &px) != 0 || !px) return tex;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(info.stride / 4));
    tex = gl_upload_rgba(tex, (int)info.width, (int)info.height, px, 1, srgb);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    AndroidBitmap_unlockPixels(g_env, bmp);
    if (ow) *ow = (int)info.width;
    if (oh) *oh = (int)info.height;
    return tex;
}

static void cb_pointer(int panel, float u, float v, int ev) {
    (*g_env)->CallVoidMethod(g_env, g_bridge, m_pointer, panel, u, v, ev);
    clear_exc();
}
static void cb_scroll(int panel, float amount) {
    (*g_env)->CallVoidMethod(g_env, g_bridge, m_scroll, panel, amount);
    clear_exc();
}
static void cb_onPage(int page) {
    (*g_env)->CallVoidMethod(g_env, g_bridge, m_onPage, page);
    clear_exc();
}

// ------------------------------------------------------------------ page textures

#define PAGE_CACHE 14
typedef struct { int page; unsigned tex; double used; } PageSlot;
static PageSlot g_cache[PAGE_CACHE];
static int g_uploadBudget;
static double g_clock;

static unsigned page_lookup(int page) {
    for (int i = 0; i < PAGE_CACHE; i++)
        if (g_cache[i].page == page && g_cache[i].tex) { g_cache[i].used = g_clock; return g_cache[i].tex; }
    return 0;
}

static void page_request(int page) {
    if (page < 1 || page > PAGE_COUNT || page_lookup(page) || g_uploadBudget <= 0) return;
    jobject bmp = (*g_env)->CallObjectMethod(g_env, g_bridge, m_pageBitmap, page);
    clear_exc();
    if (!bmp) return;
    int lru = 0;
    for (int i = 1; i < PAGE_CACHE; i++)
        if (g_cache[i].used < g_cache[lru].used) lru = i;
    g_cache[lru].tex = upload_bitmap(bmp, g_cache[lru].tex, 1, NULL, NULL);
    g_cache[lru].page = page;
    g_cache[lru].used = g_clock;
    (*g_env)->DeleteLocalRef(g_env, bmp);
    g_uploadBudget--;
}

static void prefetch_pages(const SceneState* s) {
    g_uploadBudget = 2;
    int r = 2 * s->spread + 1;
    int need[] = {r, r + 1, r + 2, r + 3, r - 1, r - 2, r + 4, r + 5, r - 3, r - 4};
    for (unsigned i = 0; i < sizeof(need) / sizeof(need[0]); i++) page_request(need[i]);
}

// ------------------------------------------------------------------ OpenXR state

typedef struct {
    XrSwapchain handle;
    int32_t width, height;
    uint32_t count;
    XrSwapchainImageOpenGLESKHR images[8];
    GLuint fbo[8];
    GLuint depth;
} Swapchain;

static XrInstance g_inst;
static XrSystemId g_sys;
static XrSession g_session;
static XrSpace g_local;
static XrSessionState g_state = XR_SESSION_STATE_UNKNOWN;
static int g_running;
static Swapchain g_sc[2];
static XrViewConfigurationView g_viewCfg[2];

static XrActionSet g_actionSet;
static XrAction a_aim, a_grip, a_trigger, a_squeeze, a_stick, a_next, a_prev, a_haptic;
static XrPath g_hand[2];
static XrSpace g_aimSpace[2], g_gripSpace[2];
static int g_hapticsOn = 1;

// GL_EXT_multisampled_render_to_texture (4x MSAA resolved on-tile)
typedef void (GL_APIENTRYP FbTex2DMSFn)(GLenum, GLenum, GLenum, GLuint, GLint, GLsizei);
typedef void (GL_APIENTRYP RbStorageMSFn)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
static FbTex2DMSFn p_fbTex2DMS;
static RbStorageMSFn p_rbStorageMS;

static XrPath path(const char* s) {
    XrPath p = XR_NULL_PATH;
    xrStringToPath(g_inst, s, &p);
    return p;
}

static XrAction make_action(XrActionType type, const char* name, const char* loc) {
    XrActionCreateInfo ci = {XR_TYPE_ACTION_CREATE_INFO};
    ci.actionType = type;
    size_t n = strlen(name), m = strlen(loc);
    memcpy(ci.actionName, name, n + 1);
    memcpy(ci.localizedActionName, loc, m + 1);
    ci.countSubactionPaths = 2;
    ci.subactionPaths = g_hand;
    XrAction a = XR_NULL_HANDLE;
    CHECK(xrCreateAction(g_actionSet, &ci, &a));
    return a;
}

static void suggest(const char* profile, const XrActionSuggestedBinding* b, uint32_t n) {
    XrInteractionProfileSuggestedBinding s = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    s.interactionProfile = path(profile);
    s.suggestedBindings = b;
    s.countSuggestedBindings = n;
    XrResult r = xrSuggestInteractionProfileBindings(g_inst, &s);
    if (XR_FAILED(r)) LOGW("bindings for %s rejected (%d)", profile, r);
}

static void init_actions(void) {
    XrActionSetCreateInfo ai = {XR_TYPE_ACTION_SET_CREATE_INFO};
    memcpy(ai.actionSetName, "main", 5);
    memcpy(ai.localizedActionSetName, "Main", 5);
    CHECK(xrCreateActionSet(g_inst, &ai, &g_actionSet));
    g_hand[0] = path("/user/hand/left");
    g_hand[1] = path("/user/hand/right");
    a_aim = make_action(XR_ACTION_TYPE_POSE_INPUT, "aim", "Aim");
    a_grip = make_action(XR_ACTION_TYPE_POSE_INPUT, "grip", "Grip");
    a_trigger = make_action(XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Select / turn page");
    a_squeeze = make_action(XR_ACTION_TYPE_FLOAT_INPUT, "squeeze", "Grab page");
    a_stick = make_action(XR_ACTION_TYPE_VECTOR2F_INPUT, "stick", "Flip / scroll");
    a_next = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "next", "Next page");
    a_prev = make_action(XR_ACTION_TYPE_BOOLEAN_INPUT, "prev", "Previous page");
    a_haptic = make_action(XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Haptics");

    XrActionSuggestedBinding touch[] = {
        {a_aim, path("/user/hand/left/input/aim/pose")},
        {a_aim, path("/user/hand/right/input/aim/pose")},
        {a_grip, path("/user/hand/left/input/grip/pose")},
        {a_grip, path("/user/hand/right/input/grip/pose")},
        {a_trigger, path("/user/hand/left/input/trigger/value")},
        {a_trigger, path("/user/hand/right/input/trigger/value")},
        {a_squeeze, path("/user/hand/left/input/squeeze/value")},
        {a_squeeze, path("/user/hand/right/input/squeeze/value")},
        {a_stick, path("/user/hand/left/input/thumbstick")},
        {a_stick, path("/user/hand/right/input/thumbstick")},
        {a_next, path("/user/hand/left/input/x/click")},
        {a_next, path("/user/hand/right/input/a/click")},
        {a_prev, path("/user/hand/left/input/y/click")},
        {a_prev, path("/user/hand/right/input/b/click")},
        {a_haptic, path("/user/hand/left/output/haptic")},
        {a_haptic, path("/user/hand/right/output/haptic")},
    };
    suggest("/interaction_profiles/oculus/touch_controller", touch, sizeof(touch) / sizeof(touch[0]));

    XrActionSuggestedBinding simple[] = {
        {a_aim, path("/user/hand/left/input/aim/pose")},
        {a_aim, path("/user/hand/right/input/aim/pose")},
        {a_grip, path("/user/hand/left/input/grip/pose")},
        {a_grip, path("/user/hand/right/input/grip/pose")},
        {a_trigger, path("/user/hand/left/input/select/click")},
        {a_trigger, path("/user/hand/right/input/select/click")},
        {a_haptic, path("/user/hand/left/output/haptic")},
        {a_haptic, path("/user/hand/right/output/haptic")},
    };
    suggest("/interaction_profiles/khr/simple_controller", simple, sizeof(simple) / sizeof(simple[0]));

    for (int h = 0; h < 2; h++) {
        XrActionSpaceCreateInfo si = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
        si.subactionPath = g_hand[h];
        si.poseInActionSpace.orientation.w = 1;
        si.action = a_aim;
        CHECK(xrCreateActionSpace(g_session, &si, &g_aimSpace[h]));
        si.action = a_grip;
        CHECK(xrCreateActionSpace(g_session, &si, &g_gripSpace[h]));
    }
    XrSessionActionSetsAttachInfo at = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    at.countActionSets = 1;
    at.actionSets = &g_actionSet;
    CHECK(xrAttachSessionActionSets(g_session, &at));
}

static void cb_haptic(int hand, float amp, float seconds) {
    if (!g_hapticsOn || !g_running) return;
    XrHapticVibration v = {XR_TYPE_HAPTIC_VIBRATION};
    v.amplitude = amp;
    v.duration = (XrDuration)(seconds * 1e9);
    v.frequency = XR_FREQUENCY_UNSPECIFIED;
    XrHapticActionInfo hi = {XR_TYPE_HAPTIC_ACTION_INFO};
    hi.action = a_haptic;
    hi.subactionPath = g_hand[hand & 1];
    xrApplyHapticFeedback(g_session, &hi, (XrHapticBaseHeader*)&v);
}

static float get_float(XrAction a, int h) {
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = a;
    gi.subactionPath = g_hand[h];
    XrActionStateFloat st = {XR_TYPE_ACTION_STATE_FLOAT};
    if (XR_FAILED(xrGetActionStateFloat(g_session, &gi, &st)) || !st.isActive) return 0;
    return st.currentState;
}
static int get_bool(XrAction a, int h) {
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = a;
    gi.subactionPath = g_hand[h];
    XrActionStateBoolean st = {XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_FAILED(xrGetActionStateBoolean(g_session, &gi, &st)) || !st.isActive) return 0;
    return st.currentState;
}
static vec2 get_vec2(XrAction a, int h) {
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = a;
    gi.subactionPath = g_hand[h];
    XrActionStateVector2f st = {XR_TYPE_ACTION_STATE_VECTOR2F};
    vec2 r = {0, 0};
    if (XR_FAILED(xrGetActionStateVector2f(g_session, &gi, &st)) || !st.isActive) return r;
    r.x = st.currentState.x;
    r.y = st.currentState.y;
    return r;
}

static pose to_pose(XrPosef p) {
    pose r;
    r.ori.x = p.orientation.x; r.ori.y = p.orientation.y; r.ori.z = p.orientation.z; r.ori.w = p.orientation.w;
    r.pos = v3(p.position.x, p.position.y, p.position.z);
    return r;
}

static int locate(XrSpace sp, XrTime t, pose* out) {
    XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(sp, g_local, t, &loc))) return 0;
    const XrSpaceLocationFlags need = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if ((loc.locationFlags & need) != need) return 0;
    *out = to_pose(loc.pose);
    return 1;
}

// ------------------------------------------------------------------ EGL

static EGLDisplay g_dpy;
static EGLConfig g_cfg;
static EGLContext g_ctx;
static EGLSurface g_pbuf;

static int init_egl(void) {
    g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(g_dpy, NULL, NULL);
    EGLint attrs[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                      EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLES, 0,
                      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
                      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
    EGLint n = 0;
    if (!eglChooseConfig(g_dpy, attrs, &g_cfg, 1, &n) || n < 1) { LOGE("eglChooseConfig failed"); return 0; }
    EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    g_ctx = eglCreateContext(g_dpy, g_cfg, EGL_NO_CONTEXT, ctxAttrs);
    if (g_ctx == EGL_NO_CONTEXT) { LOGE("eglCreateContext failed"); return 0; }
    EGLint pb[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    g_pbuf = eglCreatePbufferSurface(g_dpy, g_cfg, pb);
    if (!eglMakeCurrent(g_dpy, g_pbuf, g_pbuf, g_ctx)) { LOGE("eglMakeCurrent failed"); return 0; }
    const char* ext = (const char*)glGetString(GL_EXTENSIONS);
    int hasMS = 0;
    for (const char* p = ext; p && *p; p++)
        if (!strncmp(p, "GL_EXT_multisampled_render_to_texture", 37)) { hasMS = 1; break; }
    if (hasMS) {
        p_fbTex2DMS = (FbTex2DMSFn)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");
        p_rbStorageMS = (RbStorageMSFn)eglGetProcAddress("glRenderbufferStorageMultisampleEXT");
    }
    LOGI("GL: %s / MSAA-rtt: %d", glGetString(GL_RENDERER), p_fbTex2DMS != NULL);
    return 1;
}

// ------------------------------------------------------------------ swapchains

static void create_swapchains(void) {
    uint32_t n = 0;
    CHECK(xrEnumerateViewConfigurationViews(g_inst, g_sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, NULL));
    if (n > 2) n = 2;
    for (uint32_t i = 0; i < n; i++) g_viewCfg[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    CHECK(xrEnumerateViewConfigurationViews(g_inst, g_sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n, g_viewCfg));

    int64_t formats[32];
    uint32_t nf = 0;
    CHECK(xrEnumerateSwapchainFormats(g_session, 32, &nf, formats));
    int64_t fmt = nf ? formats[0] : GL_RGBA8;
    for (uint32_t i = 0; i < nf; i++)
        if (formats[i] == GL_SRGB8_ALPHA8) fmt = GL_SRGB8_ALPHA8;

    for (int e = 0; e < 2; e++) {
        Swapchain* sc = &g_sc[e];
        // a little supersampling keeps the Mushaf glyphs crisp
        sc->width = (int32_t)(g_viewCfg[e].recommendedImageRectWidth * 1.2f);
        sc->height = (int32_t)(g_viewCfg[e].recommendedImageRectHeight * 1.2f);
        if (sc->width > (int32_t)g_viewCfg[e].maxImageRectWidth) sc->width = g_viewCfg[e].maxImageRectWidth;
        if (sc->height > (int32_t)g_viewCfg[e].maxImageRectHeight) sc->height = g_viewCfg[e].maxImageRectHeight;
        XrSwapchainCreateInfo ci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        ci.format = fmt;
        ci.sampleCount = 1;
        ci.width = sc->width;
        ci.height = sc->height;
        ci.faceCount = 1;
        ci.arraySize = 1;
        ci.mipCount = 1;
        CHECK(xrCreateSwapchain(g_session, &ci, &sc->handle));
        CHECK(xrEnumerateSwapchainImages(sc->handle, 0, &sc->count, NULL));
        if (sc->count > 8) sc->count = 8;
        for (uint32_t i = 0; i < sc->count; i++) sc->images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
        CHECK(xrEnumerateSwapchainImages(sc->handle, sc->count, &sc->count, (XrSwapchainImageBaseHeader*)sc->images));

        glGenRenderbuffers(1, &sc->depth);
        glBindRenderbuffer(GL_RENDERBUFFER, sc->depth);
        if (p_rbStorageMS) p_rbStorageMS(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, sc->width, sc->height);
        else glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, sc->width, sc->height);
        glGenFramebuffers((GLsizei)sc->count, sc->fbo);
        for (uint32_t i = 0; i < sc->count; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, sc->fbo[i]);
            if (p_fbTex2DMS) p_fbTex2DMS(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sc->images[i].image, 0, 4);
            else glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sc->images[i].image, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sc->depth);
            GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (st != GL_FRAMEBUFFER_COMPLETE) LOGE("framebuffer incomplete: 0x%x", st);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    LOGI("swapchains %dx%d fmt 0x%x", g_sc[0].width, g_sc[0].height, (int)fmt);
}

// ------------------------------------------------------------------ main loop

static unsigned load_asset(const char* name, int srgb) {
    jstring js = (*g_env)->NewStringUTF(g_env, name);
    jobject bmp = (*g_env)->CallObjectMethod(g_env, g_bridge, m_loadAsset, js);
    clear_exc();
    unsigned t = upload_bitmap(bmp, 0, srgb, NULL, NULL);
    if (bmp) (*g_env)->DeleteLocalRef(g_env, bmp);
    (*g_env)->DeleteLocalRef(g_env, js);
    return t;
}

static void handle_events(void) {
    XrEventDataBuffer ev = {XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(g_inst, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged* sc = (XrEventDataSessionStateChanged*)&ev;
            g_state = sc->state;
            LOGI("session state %d", g_state);
            if (g_state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                CHECK(xrBeginSession(g_session, &bi));
                g_running = 1;
            } else if (g_state == XR_SESSION_STATE_STOPPING) {
                CHECK(xrEndSession(g_session));
                g_running = 0;
            } else if (g_state == XR_SESSION_STATE_EXITING || g_state == XR_SESSION_STATE_LOSS_PENDING) {
                g_exitRequested = 1;
            }
        } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            g_exitRequested = 1;
        }
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
        ev.next = NULL;
    }
}

JNIEXPORT void JNICALL Java_com_quranvr_mushaf_MainActivity_nativeRequestExit(JNIEnv* env, jclass cls) {
    (void)env; (void)cls;
    g_exitRequested = 1;
}

JNIEXPORT void JNICALL Java_com_quranvr_mushaf_MainActivity_nativeRun(JNIEnv* env, jobject activity, jobject bridge) {
    g_env = env;
    g_bridge = (*env)->NewGlobalRef(env, bridge);
    g_exitRequested = 0;
    jclass bc = (*env)->GetObjectClass(env, bridge);
    m_frame = (*env)->GetMethodID(env, bc, "frame", "(D)[I");
    m_pointer = (*env)->GetMethodID(env, bc, "pointer", "(IFFI)V");
    m_scroll = (*env)->GetMethodID(env, bc, "scroll", "(IF)V");
    m_onPage = (*env)->GetMethodID(env, bc, "onPage", "(I)V");
    m_panelBitmap = (*env)->GetMethodID(env, bc, "panelBitmap", "(I)Landroid/graphics/Bitmap;");
    m_pageBitmap = (*env)->GetMethodID(env, bc, "pageBitmap", "(I)Landroid/graphics/Bitmap;");
    m_loadAsset = (*env)->GetMethodID(env, bc, "loadAsset", "(Ljava/lang/String;)Landroid/graphics/Bitmap;");
    m_startPage = (*env)->GetMethodID(env, bc, "startPage", "()I");
    clear_exc();

    JavaVM* vm = NULL;
    (*env)->GetJavaVM(env, &vm);

    // ---- loader + instance
    PFN_xrInitializeLoaderKHR initLoader = NULL;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&initLoader);
    if (initLoader) {
        XrLoaderInitInfoAndroidKHR li = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        li.applicationVM = vm;
        li.applicationContext = activity;
        CHECK(initLoader((XrLoaderInitInfoBaseHeaderKHR*)&li));
    }
    const char* exts[] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME, XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfoAndroidKHR ai = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    ai.applicationVM = vm;
    ai.applicationActivity = activity;
    XrInstanceCreateInfo ci = {XR_TYPE_INSTANCE_CREATE_INFO};
    ci.next = &ai;
    memcpy(ci.applicationInfo.applicationName, "Quran VR", 9);
    memcpy(ci.applicationInfo.engineName, "MushafEngine", 13);
    ci.applicationInfo.applicationVersion = 1;
    ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
    ci.enabledExtensionCount = 2;
    ci.enabledExtensionNames = exts;
    XrResult r = xrCreateInstance(&ci, &g_inst);
    if (XR_FAILED(r)) { LOGE("xrCreateInstance failed %d", r); return; }

    XrSystemGetInfo sgi = {XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    CHECK(xrGetSystem(g_inst, &sgi, &g_sys));

    PFN_xrGetOpenGLESGraphicsRequirementsKHR getReq = NULL;
    xrGetInstanceProcAddr(g_inst, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&getReq);
    XrGraphicsRequirementsOpenGLESKHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
    if (getReq) CHECK(getReq(g_inst, g_sys, &req));

    if (!init_egl()) return;

    XrGraphicsBindingOpenGLESAndroidKHR gb = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    gb.display = g_dpy;
    gb.config = g_cfg;
    gb.context = g_ctx;
    XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &gb;
    sci.systemId = g_sys;
    r = xrCreateSession(g_inst, &sci, &g_session);
    if (XR_FAILED(r)) { LOGE("xrCreateSession failed %d", r); return; }

    XrReferenceSpaceCreateInfo rs = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rs.poseInReferenceSpace.orientation.w = 1;
    CHECK(xrCreateReferenceSpace(g_session, &rs, &g_local));

    create_swapchains();
    init_actions();

    // ---- scene + assets
    scene_init();
    scene_set_earth_textures(load_asset("earth/day.jpg", 1), load_asset("earth/night.jpg", 1),
                             load_asset("earth/clouds.jpg", 0), load_asset("earth/ocean.jpg", 0));
    unsigned panelTex[PANEL_COUNT] = {0};

    SceneState s;
    memset(&s, 0, sizeof(s));
    AppBridge ab = {cb_pointer, cb_scroll, cb_onPage, cb_haptic};
    int startPage = (*env)->CallIntMethod(env, g_bridge, m_startPage);
    clear_exc();
    app_init(&ab, &s, startPage);

    double t0 = now_seconds(), last = t0;
    double frozenUtc = 0;
    int liveEarth = 1;

    while (!g_exitRequested) {
        handle_events();
        if (!g_running) {
            struct timespec ts = {0, 20 * 1000000};
            nanosleep(&ts, NULL);
            continue;
        }
        XrFrameWaitInfo fwi = {XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState fs = {XR_TYPE_FRAME_STATE};
        if (XR_FAILED(xrWaitFrame(g_session, &fwi, &fs))) continue;
        XrFrameBeginInfo fbi = {XR_TYPE_FRAME_BEGIN_INFO};
        CHECK(xrBeginFrame(g_session, &fbi));

        double tnow = now_seconds();
        float dt = (float)(tnow - last);
        if (dt > 0.1f) dt = 0.1f;
        last = tnow;
        g_clock = tnow;

        // ---- UI frame (Java)
        jintArray arr = (jintArray)(*env)->CallObjectMethod(env, g_bridge, m_frame, tnow - t0);
        clear_exc();
        int ui[6] = {0, 0x1F, -1, 13, 247136, 466753};
        if (arr) {
            (*env)->GetIntArrayRegion(env, arr, 0, 6, ui);
            (*env)->DeleteLocalRef(env, arr);
        }
        for (int p = 0; p < PANEL_COUNT; p++) {
            if (!(ui[0] & (1 << p)) && panelTex[p]) continue;
            jobject bmp = (*env)->CallObjectMethod(env, g_bridge, m_panelBitmap, p);
            clear_exc();
            if (bmp) {
                int w = 0, h = 0;
                panelTex[p] = upload_bitmap(bmp, panelTex[p], 0, &w, &h);
                scene_set_panel_texture(p, panelTex[p], w, h);
                (*env)->DeleteLocalRef(env, bmp);
            }
        }
        if (ui[2] > 0) app_goto_page(&s, ui[2]);
        s.showStation = ui[3] & 1;
        s.nightMode = (ui[3] >> 1) & 1;
        int live = (ui[3] >> 2) & 1;
        g_hapticsOn = (ui[3] >> 3) & 1;
        s.markerLat = ui[4] / 1e4f;
        s.markerLon = ui[5] / 1e4f;
        s.time = tnow - t0;
        s.utc = unix_time();
        if (live != liveEarth) {
            liveEarth = live;
            frozenUtc = s.utc + s.earthUtcOffset;
        }
        s.earthUtcOffset = liveEarth ? 0.0 : frozenUtc - s.utc;
        for (int p = 0; p < PANEL_COUNT; p++) {
            float target = (ui[1] & (1 << p)) ? 1.0f : 0.0f;
            s.panelFade[p] += (target - s.panelFade[p]) * (1.0f - expf(-dt * 10.0f));
            if (s.panelFade[p] < 0.01f && target == 0) s.panelFade[p] = 0;
        }
        s.panelVisible = 0;
        for (int p = 0; p < PANEL_COUNT; p++)
            if (s.panelFade[p] > 0.02f || (ui[1] & (1 << p))) s.panelVisible |= 1u << p;

        // ---- input
        XrActiveActionSet aas = {g_actionSet, XR_NULL_PATH};
        XrActionsSyncInfo sync = {XR_TYPE_ACTIONS_SYNC_INFO};
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &aas;
        xrSyncActions(g_session, &sync);
        HandInput in[2];
        memset(in, 0, sizeof(in));
        for (int h = 0; h < 2; h++) {
            int ok = locate(g_aimSpace[h], fs.predictedDisplayTime, &in[h].aim);
            ok &= locate(g_gripSpace[h], fs.predictedDisplayTime, &in[h].grip);
            in[h].active = ok;
            in[h].trigger = get_float(a_trigger, h);
            in[h].squeeze = get_float(a_squeeze, h);
            in[h].stick = get_vec2(a_stick, h);
            in[h].btnNext = get_bool(a_next, h);
            in[h].btnPrev = get_bool(a_prev, h);
        }
        app_update(dt, in, &s);
        prefetch_pages(&s);

        // ---- render
        XrCompositionLayerProjectionView pv[2];
        XrCompositionLayerProjection layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        const XrCompositionLayerBaseHeader* layers[1];
        uint32_t layerCount = 0;
        if (fs.shouldRender) {
            XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
            XrViewState vs = {XR_TYPE_VIEW_STATE};
            XrViewLocateInfo vli = {XR_TYPE_VIEW_LOCATE_INFO};
            vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            vli.displayTime = fs.predictedDisplayTime;
            vli.space = g_local;
            uint32_t nv = 0;
            CHECK(xrLocateViews(g_session, &vli, &vs, 2, &nv, views));
            if ((vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
                (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
                for (int e = 0; e < 2; e++) {
                    Swapchain* sc = &g_sc[e];
                    uint32_t idx = 0;
                    XrSwapchainImageAcquireInfo aqi = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                    CHECK(xrAcquireSwapchainImage(sc->handle, &aqi, &idx));
                    XrSwapchainImageWaitInfo wi = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                    wi.timeout = XR_INFINITE_DURATION;
                    CHECK(xrWaitSwapchainImage(sc->handle, &wi));

                    glBindFramebuffer(GL_FRAMEBUFFER, sc->fbo[idx]);
                    glViewport(0, 0, sc->width, sc->height);
                    View v;
                    pose eye = to_pose(views[e].pose);
                    v.view = m4rigid_inverse(m4pose(eye));
                    v.eye = eye.pos;
                    v.proj = m4fov(views[e].fov.angleLeft, views[e].fov.angleRight, views[e].fov.angleUp,
                                   views[e].fov.angleDown, 0.05f, 100.0f);
                    scene_render(&v, &s, page_lookup);
                    GLenum discard = GL_DEPTH_ATTACHMENT;
                    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &discard);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);

                    XrSwapchainImageReleaseInfo ri = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    CHECK(xrReleaseSwapchainImage(sc->handle, &ri));

                    memset(&pv[e], 0, sizeof(pv[e]));
                    pv[e].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                    pv[e].pose = views[e].pose;
                    pv[e].fov = views[e].fov;
                    pv[e].subImage.swapchain = sc->handle;
                    pv[e].subImage.imageRect.extent.width = sc->width;
                    pv[e].subImage.imageRect.extent.height = sc->height;
                }
                layer.space = g_local;
                layer.viewCount = 2;
                layer.views = pv;
                layers[0] = (const XrCompositionLayerBaseHeader*)&layer;
                layerCount = 1;
            }
        }
        XrFrameEndInfo fei = {XR_TYPE_FRAME_END_INFO};
        fei.displayTime = fs.predictedDisplayTime;
        fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        fei.layerCount = layerCount;
        fei.layers = layers;
        CHECK(xrEndFrame(g_session, &fei));
    }

    LOGI("exiting");
    if (g_running) xrEndSession(g_session);
    g_running = 0;
    xrDestroySession(g_session);
    xrDestroyInstance(g_inst);
    eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(g_dpy, g_pbuf);
    eglDestroyContext(g_dpy, g_ctx);
    (*env)->DeleteGlobalRef(env, g_bridge);
}
