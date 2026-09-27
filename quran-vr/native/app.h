// Interaction logic: controller rays, UI pointing, and turning the Mushaf's
// pages by hand (trigger-drag on a page, grip-grab near the page, thumbstick
// flick or face buttons).
#pragma once
#include "scene.h"

enum { UI_HOVER = 0, UI_DOWN = 1, UI_UP = 2, UI_LEAVE = 3 };

typedef struct {
    void (*uiPointer)(int panel, float u, float v, int ev);
    void (*uiScroll)(int panel, float amount);
    void (*onPage)(int rightPage);
    void (*haptic)(int hand, float amplitude, float seconds);
} AppBridge;

typedef struct {
    int active;
    pose aim, grip;
    float trigger, squeeze;
    vec2 stick;
    int btnNext, btnPrev;    // A/X = next page, B/Y = previous page
} HandInput;

void app_init(const AppBridge* bridge, SceneState* s, int startPage);
void app_update(float dt, const HandInput in[2], SceneState* s);
void app_goto_page(SceneState* s, int page);
