#include "store.h"
#include "game.h"
#include <string.h>

static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void write32(uint8_t *p, uint32_t value) {
  unsigned i;
  for (i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

static uint32_t checksum(const uint8_t *p, size_t size) {
  uint32_t hash = UINT32_C(2166136261);
  size_t i;
  for (i = 0; i < size; ++i) hash = (hash ^ p[i]) * UINT32_C(16777619);
  return hash;
}

void settings_defaults(Settings *settings) {
  memset(settings, 0, sizeof(*settings));
  settings->vibration = settings->ghosts = settings->attract = settings->sound = true;
  settings->buttons_bottom = true;
  settings->alarm_hour = 7u;
}

void scores_defaults(HighScores *scores) { memset(scores, 0, sizeof(*scores)); }

void settings_encode(const Settings *s, uint8_t out[SETTINGS_RECORD_SIZE]) {
  out[0] = 5u;
  out[1] = (uint8_t)(s->swap_buttons | s->vibration << 1 | s->ghosts << 2 |
                     s->attract << 3 | s->alarm_on << 4 | s->landscape << 5 |
                     s->buttons_bottom << 6 | s->sound << 7);
  out[2] = s->alarm_hour;
  out[3] = s->alarm_minute;
  write32(out + 4, checksum(out, 4));
}

bool settings_decode(Settings *s, const uint8_t *data, size_t size) {
  settings_defaults(s);
  if (size != SETTINGS_RECORD_SIZE || data == NULL || data[0] < 1u || data[0] > 5u ||
      (data[1] & (data[0] == 1u ? 0xe0u : data[0] == 2u ? 0xc0u : data[0] < 5u ? 0x80u : 0u)) != 0u || data[2] > 23u || data[3] > 59u ||
      read32(data + 4) != checksum(data, 4)) return false;
  s->swap_buttons = (data[1] & 1u) != 0;
  s->vibration = (data[1] & 2u) != 0;
  s->ghosts = (data[1] & 4u) != 0;
  s->attract = (data[1] & 8u) != 0;
  s->alarm_on = (data[1] & 16u) != 0;
  s->landscape = data[0] >= 2u && (data[1] & 32u) != 0;
  /* v5 adds Sound in bit 7; earlier records default it On. */
  s->sound = data[0] < 5u || (data[1] & 128u) != 0;
  /* v4 retains button position while Vertical. Earlier Vertical records had
   * no position preference; default their first Horizontal selection to Bottom.
   * Existing horizontal Top/Bottom selections keep their exact orientation. */
  s->buttons_bottom = data[0] >= 4u ? (data[1] & 64u) != 0 :
                      !s->landscape || (data[0] == 3u && (data[1] & 64u) != 0);
  s->alarm_hour = data[2];
  s->alarm_minute = data[3];
  return true;
}

static bool valid_date(uint32_t date) {
  static const unsigned days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
  unsigned year = date / 10000u, month = date / 100u % 100u, day = date % 100u;
  unsigned maximum;
  if (year < 1970u || year > 9999u || month < 1u || month > 12u) return false;
  maximum = days[month - 1u];
  if (month == 2u && year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u)) ++maximum;
  return day >= 1u && day <= maximum;
}

void scores_encode(const HighScores *s, uint8_t out[SCORES_RECORD_SIZE]) {
  unsigned i;
  out[0] = 1u;
  for (i = 0; i < 2; ++i) {
    write32(out + 1 + i * 8, s->best[i]);
    write32(out + 5 + i * 8, s->date[i]);
  }
  write32(out + 17, checksum(out, 17));
}

bool scores_decode(HighScores *s, const uint8_t *data, size_t size) {
  HighScores decoded;
  unsigned i;
  scores_defaults(s);
  if (size != SCORES_RECORD_SIZE || data == NULL || data[0] != 1u ||
      read32(data + 17) != checksum(data, 17)) return false;
  for (i = 0; i < 2; ++i) {
    decoded.best[i] = read32(data + 1 + i * 8);
    decoded.date[i] = read32(data + 5 + i * 8);
    if (decoded.best[i] == 0u ? decoded.date[i] != 0u : !valid_date(decoded.date[i])) return false;
  }
  *s = decoded;
  return true;
}

bool scores_record(HighScores *s, unsigned mode, uint32_t score, uint32_t date) {
  if (mode > 1u || score <= s->best[mode] || !valid_date(date)) return false;
  s->best[mode] = score;
  s->date[mode] = date;
  return true;
}

void stats_defaults(Stats *stats) { memset(stats, 0, sizeof(*stats)); }

static uint32_t *stats_fields(Stats *s, unsigned i) {
  uint32_t *fields[] = { &s->games[0], &s->games[1], &s->catches, &s->drops, &s->hits,
                         &s->bonuses, &s->best_streak, &s->play_seconds };
  return fields[i];
}

void stats_encode(const Stats *stats, uint8_t out[STATS_RECORD_SIZE]) {
  Stats copy = *stats;
  unsigned i;
  out[0] = 1u;
  for (i = 0; i < 8; ++i) write32(out + 1 + i * 4, *stats_fields(&copy, i));
  write32(out + 33, checksum(out, 33));
}

bool stats_decode(Stats *s, const uint8_t *data, size_t size) {
  Stats decoded;
  unsigned i;
  stats_defaults(s);
  stats_defaults(&decoded);
  if (size != STATS_RECORD_SIZE || data == NULL || data[0] != 1u ||
      read32(data + 33) != checksum(data, 33)) return false;
  for (i = 0; i < 8; ++i) *stats_fields(&decoded, i) = read32(data + 1 + i * 4);
  if (decoded.best_streak > decoded.catches) return false;
  *s = decoded;
  return true;
}

static void add_saturating(uint32_t *total, uint32_t amount) {
  *total = amount > UINT32_MAX - *total ? UINT32_MAX : *total + amount;
}

void stats_note_events(Stats *s, uint32_t events, uint32_t *streak) {
  if (events & GAME_EVENT_CATCH) {
    add_saturating(&s->catches, 1u);
    add_saturating(streak, 1u);
    if (*streak > s->best_streak) s->best_streak = *streak;
  }
  if (events & GAME_EVENT_DROP) add_saturating(&s->drops, 1u);
  if (events & GAME_EVENT_MISS) {
    if (!(events & GAME_EVENT_DROP)) add_saturating(&s->hits, 1u);
    *streak = 0u;
  }
  if (events & GAME_EVENT_BONUS) add_saturating(&s->bonuses, 1u);
}

void stats_count_game(Stats *s, unsigned mode) {
  if (mode < 2u) add_saturating(&s->games[mode], 1u);
}

void stats_add_play_ms(Stats *s, uint32_t *carry_ms, uint32_t ms) {
  uint32_t total = *carry_ms % 1000u;
  add_saturating(&s->play_seconds, ms / 1000u);
  total += ms % 1000u;
  add_saturating(&s->play_seconds, total / 1000u);
  *carry_ms = total % 1000u;
}
