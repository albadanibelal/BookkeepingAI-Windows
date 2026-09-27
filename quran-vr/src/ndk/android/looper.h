// Minimal NDK <android/looper.h> declarations.
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct ALooper; typedef struct ALooper ALooper;
enum { ALOOPER_PREPARE_ALLOW_NON_CALLBACKS = 1 << 0 };
enum { ALOOPER_POLL_WAKE = -1, ALOOPER_POLL_CALLBACK = -2, ALOOPER_POLL_TIMEOUT = -3, ALOOPER_POLL_ERROR = -4 };
typedef int (*ALooper_callbackFunc)(int fd, int events, void* data);
ALooper* ALooper_prepare(int opts);
int ALooper_pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData);
#ifdef __cplusplus
}
#endif
