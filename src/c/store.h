#ifndef POPEYE_GW_STORE_H
#define POPEYE_GW_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SETTINGS_RECORD_SIZE 8u
#define SCORES_RECORD_SIZE 21u

typedef struct {
  bool swap_buttons, vibration, ghosts, attract, alarm_on, landscape, buttons_bottom;
  uint8_t alarm_hour, alarm_minute;
} Settings;

typedef struct {
  uint32_t best[2];
  uint32_t date[2]; /* Local calendar date YYYYMMDD; zero means no score. */
} HighScores;

typedef struct {
  Settings settings;
  HighScores scores;
} SaveData;

void settings_defaults(Settings *settings);
void scores_defaults(HighScores *scores);
/* Versioned byte formats with checksums, independent of C padding/endianness.
 * Every failed decode initializes its output to defaults. */
void settings_encode(const Settings *settings, uint8_t out[SETTINGS_RECORD_SIZE]);
bool settings_decode(Settings *settings, const uint8_t *data, size_t size);
void scores_encode(const HighScores *scores, uint8_t out[SCORES_RECORD_SIZE]);
bool scores_decode(HighScores *scores, const uint8_t *data, size_t size);
bool scores_record(HighScores *scores, unsigned mode, uint32_t score, uint32_t date);

#endif
