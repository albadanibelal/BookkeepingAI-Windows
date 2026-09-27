#include "pages.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "platform.h"
#include "quran.h"
#include "stb_image.h"

#define SLOTS 12
#define QUEUE 16

typedef struct {
  int page;
  GLuint tex;
  unsigned last_used;
} Slot;

typedef struct {
  int page;
  int priority;
} Request;

typedef struct {
  int page;
  unsigned char* pixels;
  int w, h;
} Decoded;

static Slot slots[SLOTS];
static unsigned frame_counter;

static pthread_t worker;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static Request queue[QUEUE];
static int nqueue;
static Decoded done[QUEUE];
static int ndone;
static int inflight_page;
static volatile int quit;

static bool is_pending_locked(int page) {
  if (inflight_page == page) return true;
  for (int i = 0; i < nqueue; i++)
    if (queue[i].page == page) return true;
  for (int i = 0; i < ndone; i++)
    if (done[i].page == page) return true;
  return false;
}

static void* worker_main(void* arg) {
  (void)arg;
  for (;;) {
    pthread_mutex_lock(&mtx);
    while (!quit && nqueue == 0) pthread_cond_wait(&cond, &mtx);
    if (quit) {
      pthread_mutex_unlock(&mtx);
      break;
    }
    // Highest priority, most recent request first.
    int best = 0;
    for (int i = 1; i < nqueue; i++)
      if (queue[i].priority >= queue[best].priority) best = i;
    Request r = queue[best];
    queue[best] = queue[--nqueue];
    inflight_page = r.page;
    pthread_mutex_unlock(&mtx);

    char name[64];
    snprintf(name, sizeof(name), "pages/p%03d.png", r.page);
    int size = 0;
    unsigned char* file = plat_read_asset(name, &size);
    Decoded d = {r.page, NULL, 0, 0};
    if (file) {
      int n;
      d.pixels = stbi_load_from_memory(file, size, &d.w, &d.h, &n, 3);
      free(file);
    }
    pthread_mutex_lock(&mtx);
    inflight_page = 0;
    if (ndone < QUEUE) done[ndone++] = d;
    else stbi_image_free(d.pixels);
    pthread_mutex_unlock(&mtx);
  }
  return NULL;
}

void pages_init(void) {
  memset(slots, 0, sizeof(slots));
  quit = 0;
  pthread_create(&worker, NULL, worker_main, NULL);
}

void pages_shutdown(void) {
  pthread_mutex_lock(&mtx);
  quit = 1;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mtx);
  pthread_join(worker, NULL);
  for (int i = 0; i < ndone; i++) stbi_image_free(done[i].pixels);
  ndone = 0;
  for (int i = 0; i < SLOTS; i++)
    if (slots[i].tex) glDeleteTextures(1, &slots[i].tex);
  memset(slots, 0, sizeof(slots));
}

GLuint pages_get(int page) {
  for (int i = 0; i < SLOTS; i++)
    if (slots[i].page == page && slots[i].tex) {
      slots[i].last_used = frame_counter;
      return slots[i].tex;
    }
  return 0;
}

void pages_request(int page, int priority) {
  if (page < 1 || page > QURAN_PAGES) return;
  for (int i = 0; i < SLOTS; i++)
    if (slots[i].page == page && slots[i].tex) {
      slots[i].last_used = frame_counter;
      return;
    }
  pthread_mutex_lock(&mtx);
  if (!is_pending_locked(page)) {
    if (nqueue == QUEUE) {
      // Drop the lowest-priority request.
      int worst = 0;
      for (int i = 1; i < nqueue; i++)
        if (queue[i].priority < queue[worst].priority) worst = i;
      queue[worst] = queue[--nqueue];
    }
    queue[nqueue++] = (Request){page, priority};
    pthread_cond_signal(&cond);
  } else {
    for (int i = 0; i < nqueue; i++)
      if (queue[i].page == page && queue[i].priority < priority) queue[i].priority = priority;
  }
  pthread_mutex_unlock(&mtx);
}

void pages_pump(void) {
  frame_counter++;
  Decoded d = {0};
  pthread_mutex_lock(&mtx);
  if (ndone > 0) {
    d = done[0];
    memmove(&done[0], &done[1], sizeof(Decoded) * (ndone - 1));
    ndone--;
  }
  pthread_mutex_unlock(&mtx);
  if (!d.page) return;
  if (!d.pixels) {
    plat_log("page %d failed to decode", d.page);
    return;
  }
  // LRU slot
  int s = 0;
  for (int i = 0; i < SLOTS; i++) {
    if (!slots[i].tex) {
      s = i;
      break;
    }
    if (slots[i].last_used < slots[s].last_used) s = i;
  }
  if (slots[s].tex) glDeleteTextures(1, &slots[s].tex);
  slots[s].tex = tex_from_pixels(d.pixels, d.w, d.h, 3, false, true);
  slots[s].page = d.page;
  slots[s].last_used = frame_counter;
  stbi_image_free(d.pixels);
}
