#include "online.h"
#include "store.h"
#include <stdio.h>
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

void online_defaults(OnlineState *state) { memset(state, 0, sizeof(*state)); }

void online_encode(const OnlineState *s, uint8_t out[ONLINE_RECORD_SIZE]) {
  out[0] = 1u;
  out[1] = s->phase;
  out[2] = s->attempts;
  out[3] = 0u;
  write32(out + 4, s->date);
  write32(out + 8, s->score);
  write32(out + 12, s->rank);
  write32(out + 16, s->total);
  write32(out + 20, checksum(out, 20));
}

bool online_decode(OnlineState *s, const uint8_t *data, size_t size) {
  OnlineState d;
  online_defaults(s);
  if (size != ONLINE_RECORD_SIZE || data == NULL || data[0] != 1u || data[1] > ONLINE_SENT || data[3] != 0u ||
      read32(data + 20) != checksum(data, 20)) return false;
  d.phase = data[1];
  d.attempts = data[2];
  d.date = read32(data + 4);
  d.score = read32(data + 8);
  d.rank = read32(data + 12);
  d.total = read32(data + 16);
  if (d.phase == ONLINE_NONE ? (d.attempts != 0u || d.date != 0u || d.score != 0u || d.rank != 0u || d.total != 0u) :
      !store_valid_date(d.date) || d.score == 0u) return false;
  if (d.phase == ONLINE_PENDING && (d.rank != 0u || d.total != 0u)) return false;
  if (d.phase == ONLINE_SENT && (d.rank == 0u || d.total < d.rank)) return false;
  *s = d;
  return true;
}

void online_queue(OnlineState *s, uint32_t date, uint32_t score) {
  online_defaults(s);
  s->phase = ONLINE_PENDING;
  s->date = date;
  s->score = score;
}

void online_succeeded(OnlineState *s, uint32_t rank, uint32_t total) {
  if (s->phase == ONLINE_NONE || rank == 0u || total < rank) return;
  s->phase = ONLINE_SENT;
  s->attempts = 0u;
  s->rank = rank;
  s->total = total;
}

void online_failed(OnlineState *s, bool permanent) {
  if (s->phase != ONLINE_PENDING) return;
  if (permanent) s->attempts = ONLINE_MAX_ATTEMPTS;
  else if (s->attempts < ONLINE_MAX_ATTEMPTS) ++s->attempts;
}

void online_clear(OnlineState *s) { online_defaults(s); }

bool online_should_send(const OnlineState *s) {
  return s->phase == ONLINE_PENDING && s->attempts < ONLINE_MAX_ATTEMPTS;
}

void online_text(char *out, size_t size, bool enabled, bool sending, const OnlineState *s, uint32_t today) {
  if (!enabled) snprintf(out, size, "Online: off");
  else if (s->phase == ONLINE_NONE || s->date != today) snprintf(out, size, "Online: on");
  else if (s->phase == ONLINE_SENT) snprintf(out, size, "Online #%lu/%lu", (unsigned long)s->rank, (unsigned long)s->total);
  else snprintf(out, size, sending ? "Online: sending" : "Online: not sent");
}
