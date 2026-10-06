#ifndef POPEYE_GW_STORAGE_H
#define POPEYE_GW_STORAGE_H
#include "store.h"
/* Loads each record independently; missing/corrupt/unknown records use defaults. */
void storage_load(SaveData *data);
bool storage_save_settings(const Settings *settings);
bool storage_save_scores(const HighScores *scores);
bool storage_save_stats(const Stats *stats);
#endif
