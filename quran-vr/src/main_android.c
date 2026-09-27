// Meta Quest platform layer: NativeActivity + OpenXR + OpenGL ES 3.
//
// The UI thread only relays lifecycle / input-queue callbacks; all XR, GL and
// app work happens on a dedicated render thread.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <jni.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "platform.h"

#define TAG "HolyQuranVR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#ifndef XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT
#define XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT ((XrReferenceSpaceType)1000426000)
#endif
#ifndef XR_EXT_LOCAL_FLOOR_EXTENSION_NAME
#define XR_EXT_LOCAL_FLOOR_EXTENSION_NAME "XR_EXT_local_floor"
#endif
#ifndef XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME
#define XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME "XR_FB_display_refresh_rate"
#endif
typedef XrResult(XRAPI_PTR* PFN_xrEnumerateDisplayRefreshRatesFB_)(XrSession, uint32_t, uint32_t*, float*);
typedef XrResult(XRAPI_PTR* PFN_xrRequestDisplayRefreshRateFB_)(XrSession, float);

// ------------------------------------------------------------ activity state
static ANativeActivity* g_activity;
static pthread_t g_thread;
static volatile int g_destroy;
static pthread_mutex_t g_input_mtx = PTHREAD_MUTEX_INITIALIZER;
static AInputQueue* g_queue;
static ALooper* g_looper;  // render thread looper
static int g_queue_attached;

// ------------------------------------------------------------ platform API
void plat_log(const char* fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  __android_log_write(ANDROID_LOG_INFO, TAG, buf);
}

unsigned char* plat_read_asset(const char* name, int* size) {
  AAsset* a = AAssetManager_open(g_activity->assetManager, name, AASSET_MODE_BUFFER);
  if (!a) return NULL;
  off64_t n = AAsset_getLength64(a);
  unsigned char* d = malloc((size_t)n + 1);
  const void* buf = AAsset_getBuffer(a);
  if (buf) memcpy(d, buf, (size_t)n);
  else if (AAsset_read(a, d, (size_t)n) != (int)n) n = 0;
  AAsset_close(a);
  *size = (int)n;
  return d;
}

static void user_path(char* out, int n, const char* name) { snprintf(out, n, "%s/%s", g_activity->internalDataPath, name); }

unsigned char* plat_read_user(const char* name, int* size) {
  char path[512];
  user_path(path, sizeof(path), name);
  FILE* f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char* d = malloc(n + 1);
  if ((long)fread(d, 1, n, f) != n) n = 0;
  fclose(f);
  *size = (int)n;
  return d;
}

bool plat_write_user(const char* name, const void* data, int size) {
  char path[512], tmp[520];
  user_path(path, sizeof(path), name);
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE* f = fopen(tmp, "wb");
  if (!f) return false;
  bool ok = fwrite(data, 1, size, f) == (size_t)size;
  fclose(f);
  return ok && rename(tmp, path) == 0;
}

double plat_utc_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// ------------------------------------------------------------ XR state
typedef struct {
  XrSwapchain handle;
  int32_t width, height;
  uint32_t count;
  XrSwapchainImageOpenGLESKHR* images;
  GLuint* fbos;
  GLuint depth;
} Swapchain;

static XrInstance xr_instance;
static XrSystemId xr_system;
static XrSession xr_session;
static XrSpace xr_app_space, xr_view_space;
static XrSessionState xr_state = XR_SESSION_STATE_UNKNOWN;
static bool xr_running, xr_quit, has_floor, want_recenter;
static bool ext_local_floor, ext_refresh;
static XrViewConfigurationView xr_config_views[2];
static Swapchain swapchains[2];
static XrActionSet act_set;
static XrAction act_aim, act_grip, act_trigger, act_squeeze, act_stick, act_stick_click, act_primary, act_secondary,
    act_menu, act_haptic;
static XrPath hand_paths[2];
static XrSpace aim_space[2], grip_space[2];

static EGLDisplay egl_display;
static EGLConfig egl_config;
static EGLContext egl_context;
static EGLSurface egl_surface;
static PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC p_glFramebufferTexture2DMultisampleEXT;
static PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC p_glRenderbufferStorageMultisampleEXT;
static int msaa_samples;

#define XR_CHECK(x)                                              \
  do {                                                           \
    XrResult r_ = (x);                                           \
    if (XR_FAILED(r_)) {                                         \
      LOGE("%s failed: %d (%s:%d)", #x, r_, __FILE__, __LINE__); \
      return false;                                              \
    }                                                            \
  } while (0)

static XrPath path_of(const char* s) {
  XrPath p = XR_NULL_PATH;
  xrStringToPath(xr_instance, s, &p);
  return p;
}

void plat_haptic(int hand, float amplitude, float seconds) {
  if (!xr_running || xr_state != XR_SESSION_STATE_FOCUSED) return;
  XrHapticVibration v = {XR_TYPE_HAPTIC_VIBRATION};
  v.amplitude = amplitude;
  v.duration = (XrDuration)(seconds * 1e9);
  v.frequency = XR_FREQUENCY_UNSPECIFIED;
  XrHapticActionInfo info = {XR_TYPE_HAPTIC_ACTION_INFO};
  info.action = act_haptic;
  info.subactionPath = hand_paths[hand];
  xrApplyHapticFeedback(xr_session, &info, (XrHapticBaseHeader*)&v);
}

static bool has_ext(XrExtensionProperties* exts, uint32_t n, const char* name) {
  for (uint32_t i = 0; i < n; i++)
    if (!strcmp(exts[i].extensionName, name)) return true;
  return false;
}

static bool xr_create_instance(void) {
  PFN_xrInitializeLoaderKHR init_loader = NULL;
  if (XR_SUCCEEDED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&init_loader)) && init_loader) {
    XrLoaderInitInfoAndroidKHR li = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    li.applicationVM = g_activity->vm;
    li.applicationContext = g_activity->clazz;
    init_loader((const XrLoaderInitInfoBaseHeaderKHR*)&li);
  }
  uint32_t n = 0;
  XR_CHECK(xrEnumerateInstanceExtensionProperties(NULL, 0, &n, NULL));
  XrExtensionProperties* exts = calloc(n, sizeof(XrExtensionProperties));
  for (uint32_t i = 0; i < n; i++) exts[i].type = XR_TYPE_EXTENSION_PROPERTIES;
  xrEnumerateInstanceExtensionProperties(NULL, n, &n, exts);
  const char* enabled[8];
  uint32_t ne = 0;
  enabled[ne++] = XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
  enabled[ne++] = XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME;
  if ((ext_local_floor = has_ext(exts, n, XR_EXT_LOCAL_FLOOR_EXTENSION_NAME))) enabled[ne++] = XR_EXT_LOCAL_FLOOR_EXTENSION_NAME;
  if ((ext_refresh = has_ext(exts, n, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME))) enabled[ne++] = XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;
  bool gles = has_ext(exts, n, XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
  free(exts);
  if (!gles) {
    LOGE("runtime lacks XR_KHR_opengl_es_enable");
    return false;
  }
  XrInstanceCreateInfoAndroidKHR ai = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  ai.applicationVM = g_activity->vm;
  ai.applicationActivity = g_activity->clazz;
  XrInstanceCreateInfo ci = {XR_TYPE_INSTANCE_CREATE_INFO};
  ci.next = &ai;
  strcpy(ci.applicationInfo.applicationName, "Holy Quran VR");
  ci.applicationInfo.applicationVersion = 1;
  strcpy(ci.applicationInfo.engineName, "QuranVR");
  ci.applicationInfo.engineVersion = 1;
  ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 34);
  ci.enabledExtensionCount = ne;
  ci.enabledExtensionNames = enabled;
  XR_CHECK(xrCreateInstance(&ci, &xr_instance));
  XrSystemGetInfo si = {XR_TYPE_SYSTEM_GET_INFO};
  si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XR_CHECK(xrGetSystem(xr_instance, &si, &xr_system));
  return true;
}

static bool egl_init(void) {
  PFN_xrGetOpenGLESGraphicsRequirementsKHR get_req = NULL;
  XR_CHECK(xrGetInstanceProcAddr(xr_instance, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&get_req));
  XrGraphicsRequirementsOpenGLESKHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
  XR_CHECK(get_req(xr_instance, xr_system, &req));

  egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint maj, min;
  if (!eglInitialize(egl_display, &maj, &min)) return false;
  EGLint attr[] = {EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0,
                   EGL_STENCIL_SIZE, 0, EGL_SAMPLES, 0, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
                   EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_NONE};
  EGLint nc = 0;
  if (!eglChooseConfig(egl_display, attr, &egl_config, 1, &nc) || nc < 1) {
    LOGE("eglChooseConfig failed");
    return false;
  }
  EGLint ctx_attr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, ctx_attr);
  EGLint pb[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
  egl_surface = eglCreatePbufferSurface(egl_display, egl_config, pb);
  if (!eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context)) {
    LOGE("eglMakeCurrent failed");
    return false;
  }
  const char* exts = (const char*)glGetString(GL_EXTENSIONS);
  if (exts && strstr(exts, "GL_EXT_multisampled_render_to_texture")) {
    p_glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");
    p_glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress("glRenderbufferStorageMultisampleEXT");
    if (p_glFramebufferTexture2DMultisampleEXT && p_glRenderbufferStorageMultisampleEXT) msaa_samples = 4;
  }
  LOGI("GL %s, %s, MSAA %d", glGetString(GL_RENDERER), glGetString(GL_VERSION), msaa_samples);
  return true;
}

static bool xr_create_session(void) {
  XrGraphicsBindingOpenGLESAndroidKHR gb = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
  gb.display = egl_display;
  gb.config = egl_config;
  gb.context = egl_context;
  XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
  sci.next = &gb;
  sci.systemId = xr_system;
  XR_CHECK(xrCreateSession(xr_instance, &sci, &xr_session));

  // Reference spaces: prefer a floor-level space so the deck sits on the real floor.
  XrReferenceSpaceCreateInfo rs = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  rs.poseInReferenceSpace.orientation.w = 1;
  has_floor = false;
  if (ext_local_floor) {
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
    has_floor = XR_SUCCEEDED(xrCreateReferenceSpace(xr_session, &rs, &xr_app_space));
  }
  if (!has_floor) {
    uint32_t n = 0;
    XrReferenceSpaceType types[8];
    xrEnumerateReferenceSpaces(xr_session, 8, &n, types);
    for (uint32_t i = 0; i < n; i++)
      if (types[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        has_floor = XR_SUCCEEDED(xrCreateReferenceSpace(xr_session, &rs, &xr_app_space));
      }
  }
  if (!has_floor) {
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    XR_CHECK(xrCreateReferenceSpace(xr_session, &rs, &xr_app_space));
  }
  rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  XR_CHECK(xrCreateReferenceSpace(xr_session, &rs, &xr_view_space));
  return true;
}

static bool xr_create_swapchains(void) {
  uint32_t nv = 0;
  xr_config_views[0].type = xr_config_views[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
  XR_CHECK(xrEnumerateViewConfigurationViews(xr_instance, xr_system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &nv, xr_config_views));
  uint32_t nf = 0;
  XR_CHECK(xrEnumerateSwapchainFormats(xr_session, 0, &nf, NULL));
  int64_t* fmts = calloc(nf, sizeof(int64_t));
  xrEnumerateSwapchainFormats(xr_session, nf, &nf, fmts);
  int64_t fmt = fmts[0];
  for (uint32_t i = 0; i < nf; i++)
    if (fmts[i] == GL_SRGB8_ALPHA8) fmt = GL_SRGB8_ALPHA8;
  if (fmt != GL_SRGB8_ALPHA8)
    for (uint32_t i = 0; i < nf; i++)
      if (fmts[i] == GL_RGBA8) fmt = GL_RGBA8;
  free(fmts);
  for (int e = 0; e < 2; e++) {
    Swapchain* sc = &swapchains[e];
    XrSwapchainCreateInfo ci = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    ci.format = fmt;
    ci.sampleCount = 1;
    ci.width = xr_config_views[e].recommendedImageRectWidth;
    ci.height = xr_config_views[e].recommendedImageRectHeight;
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1;
    XR_CHECK(xrCreateSwapchain(xr_session, &ci, &sc->handle));
    sc->width = ci.width;
    sc->height = ci.height;
    XR_CHECK(xrEnumerateSwapchainImages(sc->handle, 0, &sc->count, NULL));
    sc->images = calloc(sc->count, sizeof(XrSwapchainImageOpenGLESKHR));
    for (uint32_t i = 0; i < sc->count; i++) sc->images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
    XR_CHECK(xrEnumerateSwapchainImages(sc->handle, sc->count, &sc->count, (XrSwapchainImageBaseHeader*)sc->images));
    sc->fbos = calloc(sc->count, sizeof(GLuint));
    glGenRenderbuffers(1, &sc->depth);
    glBindRenderbuffer(GL_RENDERBUFFER, sc->depth);
    if (msaa_samples) p_glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, msaa_samples, GL_DEPTH_COMPONENT24, sc->width, sc->height);
    else glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, sc->width, sc->height);
    for (uint32_t i = 0; i < sc->count; i++) {
      glGenFramebuffers(1, &sc->fbos[i]);
      glBindFramebuffer(GL_FRAMEBUFFER, sc->fbos[i]);
      if (msaa_samples)
        p_glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sc->images[i].image, 0, msaa_samples);
      else
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sc->images[i].image, 0);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sc->depth);
      GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (st != GL_FRAMEBUFFER_COMPLETE) LOGE("framebuffer incomplete 0x%x", st);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  LOGI("swapchain %dx%d format 0x%llx", swapchains[0].width, swapchains[0].height, (long long)fmt);
  return true;
}

static XrAction make_action(const char* name, const char* loc, XrActionType type) {
  XrActionCreateInfo ci = {XR_TYPE_ACTION_CREATE_INFO};
  ci.actionType = type;
  strcpy(ci.actionName, name);
  strcpy(ci.localizedActionName, loc);
  ci.countSubactionPaths = 2;
  ci.subactionPaths = hand_paths;
  XrAction a = XR_NULL_HANDLE;
  xrCreateAction(act_set, &ci, &a);
  return a;
}

static void suggest(const char* profile, const XrActionSuggestedBinding* b, uint32_t n) {
  XrInteractionProfileSuggestedBinding sb = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  sb.interactionProfile = path_of(profile);
  sb.suggestedBindings = b;
  sb.countSuggestedBindings = n;
  XrResult r = xrSuggestInteractionProfileBindings(xr_instance, &sb);
  if (XR_FAILED(r)) LOGE("bindings for %s rejected: %d", profile, r);
}

static bool xr_create_actions(void) {
  XrActionSetCreateInfo si = {XR_TYPE_ACTION_SET_CREATE_INFO};
  strcpy(si.actionSetName, "reader");
  strcpy(si.localizedActionSetName, "Reader");
  XR_CHECK(xrCreateActionSet(xr_instance, &si, &act_set));
  hand_paths[0] = path_of("/user/hand/left");
  hand_paths[1] = path_of("/user/hand/right");
  act_aim = make_action("aim", "Aim", XR_ACTION_TYPE_POSE_INPUT);
  act_grip = make_action("grip", "Grip", XR_ACTION_TYPE_POSE_INPUT);
  act_trigger = make_action("select", "Select", XR_ACTION_TYPE_FLOAT_INPUT);
  act_squeeze = make_action("squeeze", "Squeeze", XR_ACTION_TYPE_FLOAT_INPUT);
  act_stick = make_action("thumbstick", "Thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT);
  act_stick_click = make_action("thumbstick_click", "Thumbstick click", XR_ACTION_TYPE_BOOLEAN_INPUT);
  act_primary = make_action("next_page", "Next page", XR_ACTION_TYPE_BOOLEAN_INPUT);
  act_secondary = make_action("previous_page", "Previous page", XR_ACTION_TYPE_BOOLEAN_INPUT);
  act_menu = make_action("toggle_ui", "Toggle interface", XR_ACTION_TYPE_BOOLEAN_INPUT);
  act_haptic = make_action("haptic", "Haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT);

  const char* H[2] = {"/user/hand/left", "/user/hand/right"};
  char p[24][96];
  XrActionSuggestedBinding b[24];
  uint32_t n = 0;
#define BIND(act, fmt, ...)                        \
  do {                                             \
    snprintf(p[n], sizeof(p[n]), fmt, __VA_ARGS__); \
    b[n].action = act;                             \
    b[n].binding = path_of(p[n]);                  \
    n++;                                           \
  } while (0)
  for (int h = 0; h < 2; h++) {
    BIND(act_aim, "%s/input/aim/pose", H[h]);
    BIND(act_grip, "%s/input/grip/pose", H[h]);
    BIND(act_trigger, "%s/input/trigger/value", H[h]);
    BIND(act_squeeze, "%s/input/squeeze/value", H[h]);
    BIND(act_stick, "%s/input/thumbstick", H[h]);
    BIND(act_stick_click, "%s/input/thumbstick/click", H[h]);
    BIND(act_haptic, "%s/output/haptic", H[h]);
  }
  BIND(act_primary, "%s/input/x/click", H[0]);
  BIND(act_secondary, "%s/input/y/click", H[0]);
  BIND(act_primary, "%s/input/a/click", H[1]);
  BIND(act_secondary, "%s/input/b/click", H[1]);
  BIND(act_menu, "%s/input/menu/click", H[0]);
  suggest("/interaction_profiles/oculus/touch_controller", b, n);

  // Generic fallback
  n = 0;
  for (int h = 0; h < 2; h++) {
    BIND(act_aim, "%s/input/aim/pose", H[h]);
    BIND(act_grip, "%s/input/grip/pose", H[h]);
    BIND(act_trigger, "%s/input/select/click", H[h]);
    BIND(act_haptic, "%s/output/haptic", H[h]);
  }
  BIND(act_menu, "%s/input/menu/click", H[0]);
  suggest("/interaction_profiles/khr/simple_controller", b, n);
#undef BIND

  for (int h = 0; h < 2; h++) {
    XrActionSpaceCreateInfo ci = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
    ci.subactionPath = hand_paths[h];
    ci.poseInActionSpace.orientation.w = 1;
    ci.action = act_aim;
    XR_CHECK(xrCreateActionSpace(xr_session, &ci, &aim_space[h]));
    ci.action = act_grip;
    XR_CHECK(xrCreateActionSpace(xr_session, &ci, &grip_space[h]));
  }
  XrSessionActionSetsAttachInfo ai = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  ai.countActionSets = 1;
  ai.actionSets = &act_set;
  XR_CHECK(xrAttachSessionActionSets(xr_session, &ai));
  return true;
}

static void request_refresh_rate(void) {
  if (!ext_refresh) return;
  PFN_xrEnumerateDisplayRefreshRatesFB_ enum_rates = NULL;
  PFN_xrRequestDisplayRefreshRateFB_ request = NULL;
  xrGetInstanceProcAddr(xr_instance, "xrEnumerateDisplayRefreshRatesFB", (PFN_xrVoidFunction*)&enum_rates);
  xrGetInstanceProcAddr(xr_instance, "xrRequestDisplayRefreshRateFB", (PFN_xrVoidFunction*)&request);
  if (!enum_rates || !request) return;
  float rates[16];
  uint32_t n = 0;
  if (XR_FAILED(enum_rates(xr_session, 16, &n, rates))) return;
  float best = 0;
  for (uint32_t i = 0; i < n; i++)
    if (rates[i] <= 90.5f && rates[i] > best) best = rates[i];
  if (best > 0) request(xr_session, best);
}

static void handle_events(void) {
  XrEventDataBuffer ev = {XR_TYPE_EVENT_DATA_BUFFER};
  while (xrPollEvent(xr_instance, &ev) == XR_SUCCESS) {
    switch (ev.type) {
      case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        xr_quit = true;
        break;
      case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
        want_recenter = true;
        break;
      case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
        XrEventDataSessionStateChanged* e = (XrEventDataSessionStateChanged*)&ev;
        xr_state = e->state;
        LOGI("session state %d", xr_state);
        if (xr_state == XR_SESSION_STATE_READY) {
          XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
          bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
          if (XR_SUCCEEDED(xrBeginSession(xr_session, &bi))) {
            xr_running = true;
            request_refresh_rate();
          }
        } else if (xr_state == XR_SESSION_STATE_STOPPING) {
          xrEndSession(xr_session);
          xr_running = false;
        } else if (xr_state == XR_SESSION_STATE_EXITING || xr_state == XR_SESSION_STATE_LOSS_PENDING) {
          xr_quit = true;
        }
        break;
      }
      default:
        break;
    }
    ev.type = XR_TYPE_EVENT_DATA_BUFFER;
  }
}

static Pose to_pose(XrPosef p, bool valid) {
  Pose r;
  r.pos = V3(p.position.x, p.position.y, p.position.z);
  r.rot = (quat){p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w};
  r.valid = valid;
  return r;
}

static Pose locate(XrSpace space, XrTime t) {
  XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
  if (XR_FAILED(xrLocateSpace(space, xr_app_space, t, &loc))) return (Pose){0};
  bool ok = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) && (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
  return to_pose(loc.pose, ok);
}

static float get_float(XrAction a, int h) {
  XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = a;
  gi.subactionPath = hand_paths[h];
  XrActionStateFloat s = {XR_TYPE_ACTION_STATE_FLOAT};
  if (XR_FAILED(xrGetActionStateFloat(xr_session, &gi, &s)) || !s.isActive) return 0;
  return s.currentState;
}

static bool get_bool(XrAction a, int h) {
  XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
  gi.action = a;
  gi.subactionPath = hand_paths[h];
  XrActionStateBoolean s = {XR_TYPE_ACTION_STATE_BOOLEAN};
  if (XR_FAILED(xrGetActionStateBoolean(xr_session, &gi, &s)) || !s.isActive) return false;
  return s.currentState;
}

static void read_input(AppInput* in, XrTime t) {
  bool focused = xr_state == XR_SESSION_STATE_FOCUSED;
  if (focused) {
    XrActiveActionSet as = {act_set, XR_NULL_PATH};
    XrActionsSyncInfo sync = {XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &as;
    xrSyncActions(xr_session, &sync);
  }
  for (int h = 0; h < 2; h++) {
    Controller* c = &in->ctl[h];
    memset(c, 0, sizeof(*c));
    if (!focused) continue;  // hide controllers while the system UI has focus
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = act_aim;
    gi.subactionPath = hand_paths[h];
    XrActionStatePose ps = {XR_TYPE_ACTION_STATE_POSE};
    xrGetActionStatePose(xr_session, &gi, &ps);
    c->active = ps.isActive;
    if (!c->active) continue;
    c->aim = locate(aim_space[h], t);
    c->grip = locate(grip_space[h], t);
    c->trigger = get_float(act_trigger, h);
    c->squeeze = get_float(act_squeeze, h);
    XrActionStateVector2f v = {XR_TYPE_ACTION_STATE_VECTOR2F};
    gi.action = act_stick;
    if (XR_SUCCEEDED(xrGetActionStateVector2f(xr_session, &gi, &v)) && v.isActive) {
      c->stick_x = v.currentState.x;
      c->stick_y = v.currentState.y;
    }
    c->a = get_bool(act_primary, h);
    c->b = get_bool(act_secondary, h);
    c->thumb = get_bool(act_stick_click, h);
    c->menu = h == 0 && get_bool(act_menu, h);
  }
}

static void drain_input_queue(void) {
  pthread_mutex_lock(&g_input_mtx);
  if (g_queue && !g_queue_attached) {
    AInputQueue_attachLooper(g_queue, g_looper, 1, NULL, NULL);
    g_queue_attached = 1;
  }
  int events;
  void* data;
  while (ALooper_pollOnce(0, NULL, &events, &data) >= 0) {
    if (g_queue) {
      AInputEvent* e = NULL;
      while (AInputQueue_getEvent(g_queue, &e) >= 0) {
        if (AInputQueue_preDispatchEvent(g_queue, e)) continue;
        AInputQueue_finishEvent(g_queue, e, 1);
      }
    }
  }
  pthread_mutex_unlock(&g_input_mtx);
}

static void render_frame(double* last_time) {
  XrFrameWaitInfo wi = {XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState fs = {XR_TYPE_FRAME_STATE};
  if (XR_FAILED(xrWaitFrame(xr_session, &wi, &fs))) return;
  XrFrameBeginInfo bi = {XR_TYPE_FRAME_BEGIN_INFO};
  if (XR_FAILED(xrBeginFrame(xr_session, &bi))) return;

  XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
  XrViewLocateInfo vli = {XR_TYPE_VIEW_LOCATE_INFO};
  vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  vli.displayTime = fs.predictedDisplayTime;
  vli.space = xr_app_space;
  XrViewState vs = {XR_TYPE_VIEW_STATE};
  uint32_t nviews = 0;
  xrLocateViews(xr_session, &vli, &vs, 2, &nviews, views);

  AppInput in;
  memset(&in, 0, sizeof(in));
  in.time = fs.predictedDisplayTime * 1e-9;
  in.dt = *last_time > 0 ? (float)(in.time - *last_time) : 1.0f / 72;
  *last_time = in.time;
  in.head = locate(xr_view_space, fs.predictedDisplayTime);
  in.has_floor = has_floor;
  in.recenter = want_recenter;
  want_recenter = false;
  read_input(&in, fs.predictedDisplayTime);
  app_update(&in);

  XrCompositionLayerProjectionView pviews[2];
  XrCompositionLayerProjection layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  const XrCompositionLayerBaseHeader* layers[1];
  uint32_t nlayers = 0;
  bool pose_ok = (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) && (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
  if (fs.shouldRender && pose_ok && nviews == 2) {
    for (int e = 0; e < 2; e++) {
      Swapchain* sc = &swapchains[e];
      uint32_t idx = 0;
      XrSwapchainImageAcquireInfo ai = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
      xrAcquireSwapchainImage(sc->handle, &ai, &idx);
      XrSwapchainImageWaitInfo swi = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
      swi.timeout = XR_INFINITE_DURATION;
      xrWaitSwapchainImage(sc->handle, &swi);

      glBindFramebuffer(GL_FRAMEBUFFER, sc->fbos[idx]);
      glViewport(0, 0, sc->width, sc->height);
      glClearColor(0, 0, 0, 1);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      Pose eye = to_pose(views[e].pose, true);
      m4 eye_world = m4_trs(eye.pos, eye.rot, V3(1, 1, 1));
      AppView av;
      av.view = m4_rigid_inverse(eye_world);
      av.proj = m4_proj_fov(views[e].fov.angleLeft, views[e].fov.angleRight, views[e].fov.angleUp, views[e].fov.angleDown, 0.05f, 400.0f);
      av.eye_pos = eye.pos;
      av.eye = e;
      av.width = sc->width;
      av.height = sc->height;
      app_render(&av);
      const GLenum discard[1] = {GL_DEPTH_ATTACHMENT};
      glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, discard);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      XrSwapchainImageReleaseInfo ri = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
      xrReleaseSwapchainImage(sc->handle, &ri);

      memset(&pviews[e], 0, sizeof(pviews[e]));
      pviews[e].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
      pviews[e].pose = views[e].pose;
      pviews[e].fov = views[e].fov;
      pviews[e].subImage.swapchain = sc->handle;
      pviews[e].subImage.imageRect.extent.width = sc->width;
      pviews[e].subImage.imageRect.extent.height = sc->height;
    }
    layer.space = xr_app_space;
    layer.viewCount = 2;
    layer.views = pviews;
    layers[nlayers++] = (XrCompositionLayerBaseHeader*)&layer;
  }
  XrFrameEndInfo ei = {XR_TYPE_FRAME_END_INFO};
  ei.displayTime = fs.predictedDisplayTime;
  ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  ei.layerCount = nlayers;
  ei.layers = layers;
  xrEndFrame(xr_session, &ei);
}

static bool xr_init_all(void) {
  if (!xr_create_instance()) return false;
  if (!egl_init()) return false;
  if (!xr_create_session()) return false;
  if (!xr_create_swapchains()) return false;
  if (!xr_create_actions()) return false;
  return true;
}

static void* render_thread(void* arg) {
  (void)arg;
  JNIEnv* env = NULL;
  (*g_activity->vm)->AttachCurrentThread(g_activity->vm, (void**)&env, NULL);
  g_looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

  if (!xr_init_all()) {
    LOGE("OpenXR initialisation failed");
    ANativeActivity_finish(g_activity);
    (*g_activity->vm)->DetachCurrentThread(g_activity->vm);
    return NULL;
  }
  app_init();
  double last = 0;
  while (!g_destroy && !xr_quit) {
    drain_input_queue();
    handle_events();
    if (!xr_running) {
      usleep(20000);
      last = 0;
      continue;
    }
    render_frame(&last);
  }
  app_shutdown();
  for (int e = 0; e < 2; e++)
    if (swapchains[e].handle) xrDestroySwapchain(swapchains[e].handle);
  if (xr_session) xrDestroySession(xr_session);
  if (xr_instance) xrDestroyInstance(xr_instance);
  eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(egl_display, egl_surface);
  eglDestroyContext(egl_display, egl_context);
  eglTerminate(egl_display);
  if (!g_destroy) ANativeActivity_finish(g_activity);
  (*g_activity->vm)->DetachCurrentThread(g_activity->vm);
  return NULL;
}

// ------------------------------------------------------------ activity callbacks
static void on_destroy(ANativeActivity* a) {
  (void)a;
  g_destroy = 1;
  pthread_join(g_thread, NULL);
}

static void on_input_queue_created(ANativeActivity* a, AInputQueue* q) {
  (void)a;
  pthread_mutex_lock(&g_input_mtx);
  g_queue = q;
  g_queue_attached = 0;
  pthread_mutex_unlock(&g_input_mtx);
}

static void on_input_queue_destroyed(ANativeActivity* a, AInputQueue* q) {
  (void)a;
  pthread_mutex_lock(&g_input_mtx);
  if (g_queue_attached) AInputQueue_detachLooper(q);
  g_queue = NULL;
  g_queue_attached = 0;
  pthread_mutex_unlock(&g_input_mtx);
}

__attribute__((visibility("default"))) void ANativeActivity_onCreate(ANativeActivity* activity, void* saved, size_t saved_size) {
  (void)saved;
  (void)saved_size;
  g_activity = activity;
  g_destroy = 0;
  activity->callbacks->onDestroy = on_destroy;
  activity->callbacks->onInputQueueCreated = on_input_queue_created;
  activity->callbacks->onInputQueueDestroyed = on_input_queue_destroyed;
  pthread_create(&g_thread, NULL, render_thread, NULL);
}
