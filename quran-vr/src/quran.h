// Quran metadata (Madina Mushaf, 604 pages), search and reader persistence.
#pragma once
#include <stdbool.h>

#define QURAN_PAGES 604
#define QURAN_SURAHS 114
#define QURAN_JUZ 30
#define MAX_BOOKMARKS 64

typedef struct {
  char translit[40];
  char meaning[48];
  int ayahs;
  int start_page;
  bool meccan;
} Surah;

typedef struct {
  int juz;
  int first_surah, first_ayah;
  int last_surah, last_ayah;
} PageInfo;

typedef struct {
  int surah, ayah, page;
} JuzInfo;

typedef struct {
  Surah surah[QURAN_SURAHS + 1];     // 1-based
  PageInfo page[QURAN_PAGES + 1];    // 1-based
  JuzInfo juz[QURAN_JUZ + 1];        // 1-based
} QuranMeta;

extern QuranMeta Q;

bool quran_load_meta(void);
int quran_page_of_ayah(int surah, int ayah);  // 0 if invalid
int quran_surah_at_page(int page);            // surah of the page's first line

// Search: results are "go to page" targets with a label.
typedef enum { RES_SURAH, RES_PAGE, RES_JUZ, RES_AYAH } ResultKind;
typedef struct {
  ResultKind kind;
  int value, value2;  // surah / page / juz, ayah for RES_AYAH
  int page;
} SearchResult;
int quran_search(const char* query, SearchResult* out, int max);

// Persistent reader state
typedef struct {
  int page;                 // current right-hand page of the spread (odd)
  int theme;                // 0 classic, 1 sepia, 2 night
  float env_light;          // environment brightness 0..1
  float book_dist;          // meters
  float book_height;        // offset meters
  bool show_earth_card;
  int nbookmarks;
  int bookmarks[MAX_BOOKMARKS];
} ReaderState;

void state_defaults(ReaderState* s);
void state_load(ReaderState* s);
void state_save(const ReaderState* s);
bool state_is_bookmarked(const ReaderState* s, int page);
void state_toggle_bookmark(ReaderState* s, int page);
