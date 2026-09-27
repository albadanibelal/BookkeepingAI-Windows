// Asynchronous Mushaf page texture cache.
// Pages are decoded on a worker thread and uploaded on the GL thread.
#pragma once
#include <GLES3/gl3.h>

void pages_init(void);
void pages_shutdown(void);
void pages_request(int page, int priority);  // higher priority loads first
GLuint pages_get(int page);                   // 0 if not resident yet
void pages_pump(void);                        // call once per frame on GL thread
