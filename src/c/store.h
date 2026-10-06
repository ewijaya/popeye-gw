#ifndef POPEYE_GW_STORE_H
#define POPEYE_GW_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "online.h"

#define SETTINGS_RECORD_SIZE 8u
#define SCORES_RECORD_SIZE 21u
#define STATS_RECORD_SIZE 37u
#define MODES_RECORD_SIZE 37u

typedef struct {
  /* buttons_bottom is remembered even when landscape is false. */
  bool swap_buttons, vibration, ghosts, attract, alarm_on, landscape, buttons_bottom, sound, online;
  uint8_t alarm_hour, alarm_minute;
  uint8_t theme; /* Theme from theme.h; Classic by default. */
  uint8_t sound_level; /* 0 Low, 1 Medium (default), 2 High; used while sound is on. */
} Settings;

typedef struct {
  uint32_t best[2];
  uint32_t date[2]; /* Local calendar date YYYYMMDD; zero means no score. */
} HighScores;

/* Lifetime totals. Never reset by high-score resets; saturate rather than wrap. */
typedef struct {
  uint32_t games[2];    /* Rounds played per mode: once scored, or ended. */
  uint32_t catches, drops, hits, bonuses; /* hits: Brutus; bonuses: 200/500 clears. */
  uint32_t best_streak; /* Longest run of catches without a MISS. */
  uint32_t play_seconds; /* Active play only: not paused, hidden or on a menu. */
} Stats;

/* Sprint and Daily (both 60 s of Game B rules) keep their own bests and round counts. */
typedef struct {
  uint32_t sprint_best, sprint_date;      /* Dates are local YYYYMMDD; zero means none. */
  uint32_t daily_best, daily_best_date;   /* All-time Daily best and the day it was set. */
  uint32_t daily_today, daily_today_date; /* Best Daily score on daily_today_date. */
  uint32_t sprint_games, daily_games;     /* Rounds played, counted like Stats games. */
} ModeScores;

typedef struct {
  Settings settings;
  HighScores scores;
  Stats stats;
  ModeScores modes;
  OnlineState online;
} SaveData;

/* A real calendar date YYYYMMDD, years 1970 to 9999. */
bool store_valid_date(uint32_t date);
void settings_defaults(Settings *settings);
void scores_defaults(HighScores *scores);
/* Versioned byte formats with checksums, independent of C padding/endianness.
 * Every failed decode initializes its output to defaults. */
void settings_encode(const Settings *settings, uint8_t out[SETTINGS_RECORD_SIZE]);
bool settings_decode(Settings *settings, const uint8_t *data, size_t size);
void scores_encode(const HighScores *scores, uint8_t out[SCORES_RECORD_SIZE]);
bool scores_decode(HighScores *scores, const uint8_t *data, size_t size);
bool scores_record(HighScores *scores, unsigned mode, uint32_t score, uint32_t date);

void modes_defaults(ModeScores *modes);
void modes_encode(const ModeScores *modes, uint8_t out[MODES_RECORD_SIZE]);
bool modes_decode(ModeScores *modes, const uint8_t *data, size_t size);
/* Records a Sprint or Daily score (every Daily attempt counts toward today's best).
 * Returns whether anything changed and needs saving. */
bool modes_record(ModeScores *modes, bool daily, uint32_t score, uint32_t date);
/* Today's Daily best, or zero if Daily has not been scored on that date. */
uint32_t modes_daily_today(const ModeScores *modes, uint32_t date);
void modes_count_game(ModeScores *modes, bool daily);
/* Clears the bests but keeps the round counts. */
void modes_reset_scores(ModeScores *modes);

void stats_defaults(Stats *stats);
void stats_encode(const Stats *stats, uint8_t out[STATS_RECORD_SIZE]);
bool stats_decode(Stats *stats, const uint8_t *data, size_t size);
/* Folds in one game tick's events. streak is the live run and restarts each game;
 * a MISS without a DROP is a Brutus hit. */
void stats_note_events(Stats *stats, uint32_t events, uint32_t *streak);
void stats_count_game(Stats *stats, unsigned mode);
/* Adds active milliseconds, carrying the sub-second remainder in *carry_ms. */
void stats_add_play_ms(Stats *stats, uint32_t *carry_ms, uint32_t ms);

#endif
