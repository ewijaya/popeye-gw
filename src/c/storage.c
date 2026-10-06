#include <pebble.h>
#include "storage.h"

enum { KEY_SETTINGS = 1, KEY_SCORES = 2, KEY_STATS = 4 }; /* 3 is the wakeup id. */

void storage_load(SaveData *data) {
  uint8_t buffer[STATS_RECORD_SIZE];
  settings_defaults(&data->settings);
  scores_defaults(&data->scores);
  stats_defaults(&data->stats);
  if (persist_get_size(KEY_SETTINGS) == SETTINGS_RECORD_SIZE &&
      persist_read_data(KEY_SETTINGS, buffer, SETTINGS_RECORD_SIZE) == SETTINGS_RECORD_SIZE)
    settings_decode(&data->settings, buffer, SETTINGS_RECORD_SIZE);
  if (persist_get_size(KEY_SCORES) == SCORES_RECORD_SIZE &&
      persist_read_data(KEY_SCORES, buffer, SCORES_RECORD_SIZE) == SCORES_RECORD_SIZE)
    scores_decode(&data->scores, buffer, SCORES_RECORD_SIZE);
  if (persist_get_size(KEY_STATS) == STATS_RECORD_SIZE &&
      persist_read_data(KEY_STATS, buffer, STATS_RECORD_SIZE) == STATS_RECORD_SIZE)
    stats_decode(&data->stats, buffer, STATS_RECORD_SIZE);
}

bool storage_save_settings(const Settings *settings) {
  uint8_t buffer[SETTINGS_RECORD_SIZE];
  settings_encode(settings, buffer);
  return persist_write_data(KEY_SETTINGS, buffer, sizeof(buffer)) == sizeof(buffer);
}

bool storage_save_scores(const HighScores *scores) {
  uint8_t buffer[SCORES_RECORD_SIZE];
  scores_encode(scores, buffer);
  return persist_write_data(KEY_SCORES, buffer, sizeof(buffer)) == sizeof(buffer);
}

bool storage_save_stats(const Stats *stats) {
  uint8_t buffer[STATS_RECORD_SIZE];
  stats_encode(stats, buffer);
  return persist_write_data(KEY_STATS, buffer, sizeof(buffer)) == sizeof(buffer);
}
