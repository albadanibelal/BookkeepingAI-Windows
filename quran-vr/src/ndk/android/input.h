// Minimal NDK <android/input.h> declarations (input queue draining only).
#pragma once
#include <stdint.h>
#include <android/looper.h>
#ifdef __cplusplus
extern "C" {
#endif
struct AInputEvent; typedef struct AInputEvent AInputEvent;
struct AInputQueue; typedef struct AInputQueue AInputQueue;
enum { AINPUT_EVENT_TYPE_KEY = 1, AINPUT_EVENT_TYPE_MOTION = 2 };
enum { AKEY_EVENT_ACTION_DOWN = 0, AKEY_EVENT_ACTION_UP = 1 };
enum { AKEYCODE_BACK = 4 };
void AInputQueue_attachLooper(AInputQueue* queue, ALooper* looper, int ident, ALooper_callbackFunc callback, void* data);
void AInputQueue_detachLooper(AInputQueue* queue);
int32_t AInputQueue_getEvent(AInputQueue* queue, AInputEvent** outEvent);
int32_t AInputQueue_preDispatchEvent(AInputQueue* queue, AInputEvent* event);
void AInputQueue_finishEvent(AInputQueue* queue, AInputEvent* event, int handled);
int32_t AInputEvent_getType(const AInputEvent* event);
int32_t AKeyEvent_getKeyCode(const AInputEvent* key_event);
int32_t AKeyEvent_getAction(const AInputEvent* key_event);
#ifdef __cplusplus
}
#endif
