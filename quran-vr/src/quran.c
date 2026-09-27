#include "quran.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

QuranMeta Q;

static char* next_field(char** s) {
  char* start = *s;
  if (!start) return "";
  char* bar = strchr(start, '|');
  if (bar) {
    *bar = 0;
    *s = bar + 1;
  } else {
    *s = NULL;
  }
  return start;
}

bool quran_load_meta(void) {
  int size = 0;
  char* data = (char*)plat_read_asset("meta.txt", &size);
  if (!data) return false;
  data = realloc(data, size + 1);
  data[size] = 0;
  char* line = data;
  while (line && *line) {
    char* nl = strchr(line, '\n');
    if (nl) *nl = 0;
    char* p = line;
    char* kind = next_field(&p);
    if (kind[0] == 'S') {
      int n = atoi(next_field(&p));
      if (n >= 1 && n <= QURAN_SURAHS) {
        Surah* s = &Q.surah[n];
        snprintf(s->translit, sizeof(s->translit), "%s", next_field(&p));
        snprintf(s->meaning, sizeof(s->meaning), "%s", next_field(&p));
        s->ayahs = atoi(next_field(&p));
        s->start_page = atoi(next_field(&p));
        s->meccan = next_field(&p)[0] == 'M';
      }
    } else if (kind[0] == 'P') {
      int n = atoi(next_field(&p));
      if (n >= 1 && n <= QURAN_PAGES) {
        PageInfo* pg = &Q.page[n];
        pg->juz = atoi(next_field(&p));
        pg->first_surah = atoi(next_field(&p));
        pg->first_ayah = atoi(next_field(&p));
        pg->last_surah = atoi(next_field(&p));
        pg->last_ayah = atoi(next_field(&p));
      }
    } else if (kind[0] == 'J') {
      int n = atoi(next_field(&p));
      if (n >= 1 && n <= QURAN_JUZ) {
        Q.juz[n].surah = atoi(next_field(&p));
        Q.juz[n].ayah = atoi(next_field(&p));
        Q.juz[n].page = atoi(next_field(&p));
      }
    }
    line = nl ? nl + 1 : NULL;
  }
  free(data);
  return Q.surah[114].start_page > 0 && Q.page[604].juz > 0;
}

static int cmp_ref(int s1, int a1, int s2, int a2) { return s1 != s2 ? s1 - s2 : a1 - a2; }

int quran_page_of_ayah(int surah, int ayah) {
  if (surah < 1 || surah > QURAN_SURAHS || ayah < 1 || ayah > Q.surah[surah].ayahs) return 0;
  for (int p = Q.surah[surah].start_page; p <= QURAN_PAGES; p++) {
    PageInfo* pg = &Q.page[p];
    if (cmp_ref(surah, ayah, pg->first_surah, pg->first_ayah) >= 0 && cmp_ref(surah, ayah, pg->last_surah, pg->last_ayah) <= 0)
      return p;
  }
  return 0;
}

int quran_surah_at_page(int page) {
  if (page < 1) page = 1;
  if (page > QURAN_PAGES) page = QURAN_PAGES;
  return Q.page[page].first_surah;
}

// Normalize a name for fuzzy matching: lowercase letters/digits only.
static void norm(const char* in, char* out, int max) {
  int n = 0;
  for (; *in && n < max - 1; in++) {
    unsigned char c = (unsigned char)*in;
    if (isalnum(c)) out[n++] = (char)tolower(c);
  }
  out[n] = 0;
}

int quran_search(const char* query, SearchResult* out, int max) {
  int n = 0;
  char q[64];
  norm(query, q, sizeof(q));
  if (!q[0]) return 0;

  // "2:255" style ayah reference
  const char* colon = strchr(query, ':');
  if (colon) {
    int s = atoi(query), a = atoi(colon + 1);
    int p = quran_page_of_ayah(s, a);
    if (p && n < max) out[n++] = (SearchResult){RES_AYAH, s, a, p};
    return n;
  }
  bool numeric = true;
  for (const char* c = q; *c; c++)
    if (!isdigit((unsigned char)*c)) numeric = false;
  if (numeric) {
    int v = atoi(q);
    if (v >= 1 && v <= QURAN_SURAHS && n < max) out[n++] = (SearchResult){RES_SURAH, v, 0, Q.surah[v].start_page};
    if (v >= 1 && v <= QURAN_PAGES && n < max) out[n++] = (SearchResult){RES_PAGE, v, 0, v};
    if (v >= 1 && v <= QURAN_JUZ && n < max) out[n++] = (SearchResult){RES_JUZ, v, 0, Q.juz[v].page};
    return n;
  }
  // Name search: prefix matches first (ignoring the "al" article), then substring.
  for (int pass = 0; pass < 2; pass++) {
    for (int s = 1; s <= QURAN_SURAHS && n < max; s++) {
      char name[64], mean[64];
      norm(Q.surah[s].translit, name, sizeof(name));
      norm(Q.surah[s].meaning, mean, sizeof(mean));
      // Skip the transliterated article: "al-", "an-", "ash-", ...
      const char* bare = name;
      if (name[0] == 'a' && name[1] == 's' && name[2] == 'h') bare = name + 3;
      else if (name[0] == 'a' && name[1] && strchr("lnrstdz", name[1])) bare = name + 2;
      bool prefix = strncmp(name, q, strlen(q)) == 0 || strncmp(bare, q, strlen(q)) == 0;
      bool sub = strstr(name, q) || strstr(mean, q);
      if ((pass == 0 && prefix) || (pass == 1 && sub && !prefix))
        out[n++] = (SearchResult){RES_SURAH, s, 0, Q.surah[s].start_page};
    }
  }
  return n;
}

// ------------------------------------------------------------------ state

void state_defaults(ReaderState* s) {
  memset(s, 0, sizeof(*s));
  s->page = 1;
  s->theme = 0;
  s->env_light = 0.8f;
  s->book_dist = 0.62f;
  s->book_height = 0.0f;
  s->show_earth_card = true;
}

void state_load(ReaderState* s) {
  state_defaults(s);
  int size = 0;
  char* d = (char*)plat_read_user("reader_state.txt", &size);
  if (!d) return;
  d = realloc(d, size + 1);
  d[size] = 0;
  char* line = d;
  while (line && *line) {
    char* nl = strchr(line, '\n');
    if (nl) *nl = 0;
    char key[32];
    float v;
    if (sscanf(line, "%31s %f", key, &v) == 2) {
      if (!strcmp(key, "page")) s->page = (int)v;
      else if (!strcmp(key, "theme")) s->theme = (int)v;
      else if (!strcmp(key, "env_light")) s->env_light = v;
      else if (!strcmp(key, "book_dist")) s->book_dist = v;
      else if (!strcmp(key, "book_height")) s->book_height = v;
      else if (!strcmp(key, "earth_card")) s->show_earth_card = v != 0;
      else if (!strcmp(key, "bookmark") && s->nbookmarks < MAX_BOOKMARKS) s->bookmarks[s->nbookmarks++] = (int)v;
    }
    line = nl ? nl + 1 : NULL;
  }
  free(d);
  if (s->page < 1 || s->page > QURAN_PAGES) s->page = 1;
  s->page |= 1;
  s->theme = s->theme < 0 || s->theme > 2 ? 0 : s->theme;
  s->env_light = clampf(s->env_light, 0.2f, 1.0f);
  s->book_dist = clampf(s->book_dist, 0.4f, 0.9f);
  s->book_height = clampf(s->book_height, -0.3f, 0.3f);
}

void state_save(const ReaderState* s) {
  char buf[4096];
  int n = snprintf(buf, sizeof(buf), "page %d\ntheme %d\nenv_light %.3f\nbook_dist %.3f\nbook_height %.3f\nearth_card %d\n",
                   s->page, s->theme, s->env_light, s->book_dist, s->book_height, s->show_earth_card ? 1 : 0);
  for (int i = 0; i < s->nbookmarks && n < (int)sizeof(buf) - 32; i++) n += snprintf(buf + n, sizeof(buf) - n, "bookmark %d\n", s->bookmarks[i]);
  plat_write_user("reader_state.txt", buf, n);
}

bool state_is_bookmarked(const ReaderState* s, int page) {
  for (int i = 0; i < s->nbookmarks; i++)
    if (s->bookmarks[i] == page) return true;
  return false;
}

void state_toggle_bookmark(ReaderState* s, int page) {
  for (int i = 0; i < s->nbookmarks; i++)
    if (s->bookmarks[i] == page) {
      memmove(&s->bookmarks[i], &s->bookmarks[i + 1], sizeof(int) * (s->nbookmarks - i - 1));
      s->nbookmarks--;
      return;
    }
  if (s->nbookmarks < MAX_BOOKMARKS) s->bookmarks[s->nbookmarks++] = page;
  // keep sorted
  for (int i = 1; i < s->nbookmarks; i++)
    for (int j = i; j > 0 && s->bookmarks[j] < s->bookmarks[j - 1]; j--) {
      int t = s->bookmarks[j];
      s->bookmarks[j] = s->bookmarks[j - 1];
      s->bookmarks[j - 1] = t;
    }
}
