#include "store.h"
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
  settings->vibration = settings->ghosts = settings->attract = true;
  settings->alarm_hour = 7u;
}

void scores_defaults(HighScores *scores) { memset(scores, 0, sizeof(*scores)); }

void settings_encode(const Settings *s, uint8_t out[SETTINGS_RECORD_SIZE]) {
  out[0] = 2u;
  out[1] = (uint8_t)(s->swap_buttons | s->vibration << 1 | s->ghosts << 2 |
                     s->attract << 3 | s->alarm_on << 4 | s->landscape << 5);
  out[2] = s->alarm_hour;
  out[3] = s->alarm_minute;
  write32(out + 4, checksum(out, 4));
}

bool settings_decode(Settings *s, const uint8_t *data, size_t size) {
  settings_defaults(s);
  if (size != SETTINGS_RECORD_SIZE || data == NULL || (data[0] != 1u && data[0] != 2u) ||
      (data[1] & (data[0] == 1u ? 0xe0u : 0xc0u)) != 0u || data[2] > 23u || data[3] > 59u ||
      read32(data + 4) != checksum(data, 4)) return false;
  s->swap_buttons = (data[1] & 1u) != 0;
  s->vibration = (data[1] & 2u) != 0;
  s->ghosts = (data[1] & 4u) != 0;
  s->attract = (data[1] & 8u) != 0;
  s->alarm_on = (data[1] & 16u) != 0;
  s->landscape = data[0] >= 2u && (data[1] & 32u) != 0;
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
