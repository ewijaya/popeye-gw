#include <pebble.h>
#include "storage.h"

enum { KEY_SETTINGS = 1, KEY_SCORES = 2, KEY_STATS = 4, KEY_MODES = 5, /* 3 is the wakeup id. */
       KEY_REPLAY = 16, REPLAY_CHUNK = 256, REPLAY_CHUNKS = 8 };

void storage_load(SaveData *data) {
  uint8_t buffer[STATS_RECORD_SIZE];
  settings_defaults(&data->settings);
  scores_defaults(&data->scores);
  stats_defaults(&data->stats);
  modes_defaults(&data->modes);
  if (persist_get_size(KEY_SETTINGS) == SETTINGS_RECORD_SIZE &&
      persist_read_data(KEY_SETTINGS, buffer, SETTINGS_RECORD_SIZE) == SETTINGS_RECORD_SIZE)
    settings_decode(&data->settings, buffer, SETTINGS_RECORD_SIZE);
  if (persist_get_size(KEY_SCORES) == SCORES_RECORD_SIZE &&
      persist_read_data(KEY_SCORES, buffer, SCORES_RECORD_SIZE) == SCORES_RECORD_SIZE)
    scores_decode(&data->scores, buffer, SCORES_RECORD_SIZE);
  if (persist_get_size(KEY_STATS) == STATS_RECORD_SIZE &&
      persist_read_data(KEY_STATS, buffer, STATS_RECORD_SIZE) == STATS_RECORD_SIZE)
    stats_decode(&data->stats, buffer, STATS_RECORD_SIZE);
  if (persist_get_size(KEY_MODES) == MODES_RECORD_SIZE &&
      persist_read_data(KEY_MODES, buffer, MODES_RECORD_SIZE) == MODES_RECORD_SIZE)
    modes_decode(&data->modes, buffer, MODES_RECORD_SIZE);
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

bool storage_save_modes(const ModeScores *modes) {
  uint8_t buffer[MODES_RECORD_SIZE];
  modes_encode(modes, buffer);
  return persist_write_data(KEY_MODES, buffer, sizeof(buffer)) == sizeof(buffer);
}

void storage_clear_replay(void) {
  unsigned i;
  for (i = 0; i < REPLAY_CHUNKS; ++i) persist_delete(KEY_REPLAY + i);
}

bool storage_save_replay(const uint8_t *bytes, size_t length) {
  unsigned used = (unsigned)((length + REPLAY_CHUNK - 1u) / REPLAY_CHUNK), i;
  if (length == 0u || used > REPLAY_CHUNKS) return false;
  for (i = 0; i < used; ++i) {
    size_t size = length - i * REPLAY_CHUNK < REPLAY_CHUNK ? length - i * REPLAY_CHUNK : REPLAY_CHUNK;
    if (persist_write_data(KEY_REPLAY + i, bytes + i * REPLAY_CHUNK, size) != (int)size) {
      storage_clear_replay(); /* Never leave a half-written replay behind. */
      return false;
    }
  }
  for (; i < REPLAY_CHUNKS; ++i) persist_delete(KEY_REPLAY + i);
  return true;
}

size_t storage_load_replay(uint8_t *out, size_t capacity) {
  size_t total = 0u;
  unsigned i;
  for (i = 0; i < REPLAY_CHUNKS; ++i) {
    int size = persist_get_size(KEY_REPLAY + i);
    if (size <= 0) break;
    if (total + (size_t)size > capacity || persist_read_data(KEY_REPLAY + i, out + total, (size_t)size) != size) return 0u;
    total += (size_t)size;
    if (size < REPLAY_CHUNK) break;
  }
  return total;
}
