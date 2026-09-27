// Platform layer. The Android build is compiled freestanding (no NDK is
// required), so the handful of libc / libm / Android functions we use are
// declared here and resolved at load time against the device's libraries.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __ANDROID__

void* malloc(size_t n);
void* calloc(size_t n, size_t s);
void* realloc(void* p, size_t n);
void free(void* p);
void* memcpy(void* d, const void* s, size_t n);
void* memset(void* d, int c, size_t n);
void* memmove(void* d, const void* s, size_t n);
size_t strlen(const char* s);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
int snprintf(char* s, size_t n, const char* fmt, ...);
int vsnprintf(char* s, size_t n, const char* fmt, va_list ap);

struct timespec { long tv_sec; long tv_nsec; };
#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME 0
int clock_gettime(int clk, struct timespec* ts);
int nanosleep(const struct timespec* req, struct timespec* rem);

float sinf(float); float cosf(float); float tanf(float); float sqrtf(float);
float atan2f(float, float); float acosf(float); float asinf(float); float atanf(float);
float fabsf(float); float floorf(float); float powf(float, float); float expf(float);
float fmodf(float, float);
double sin(double); double cos(double); double floor(double); double fmod(double, double);

#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6
int __android_log_print(int prio, const char* tag, const char* fmt, ...);
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "QuranVR", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "QuranVR", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "QuranVR", __VA_ARGS__)

#else  // desktop preview build

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#define LOGI(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define LOGW(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define LOGE(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))

#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

static inline double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static inline double unix_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
