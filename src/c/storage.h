#ifndef POPEYE_GW_STORAGE_H
#define POPEYE_GW_STORAGE_H
#include "store.h"
/* Loads each record independently; missing/corrupt/unknown records use defaults. */
void storage_load(SaveData *data);
bool storage_save_settings(const Settings *settings);
bool storage_save_scores(const HighScores *scores);
bool storage_save_stats(const Stats *stats);
bool storage_save_modes(const ModeScores *modes);
bool storage_save_online(const OnlineState *online);
/* The best Daily replay of the day (see replay.h), split over persist keys 16-23 in
 * 256-byte chunks; its own checksum detects an interrupted write. Load returns the byte
 * count, or zero if there is none. Persist key 6 holds the leaderboard state; 7-15 are free. */
bool storage_save_replay(const uint8_t *bytes, size_t length);
size_t storage_load_replay(uint8_t *out, size_t capacity);
void storage_clear_replay(void);
/* The stored replay's length and one 256-byte chunk of it (the number of bytes copied, zero
 * if missing or too small a buffer), for sending without a 2 KB buffer. */
size_t storage_replay_length(void);
size_t storage_replay_chunk(unsigned index, uint8_t *out, size_t capacity);
#endif
