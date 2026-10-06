#include "replay.h"

#include <string.h>

static uint32_t checksum(const uint8_t *p, size_t size) {
  uint32_t hash = UINT32_C(2166136261);
  size_t i;
  for (i = 0; i < size; ++i) hash = (hash ^ p[i]) * UINT32_C(16777619);
  return hash;
}

static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void write32(uint8_t *p, uint32_t value) {
  unsigned i;
  for (i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

void replay_begin(Replay *r, const Game *game, uint32_t seed, uint32_t date) {
  memset(r, 0, sizeof(*r));
  r->bytes[0] = REPLAY_VERSION;
  r->bytes[1] = (uint8_t)game->mode;
  r->bytes[2] = game->controls_swapped ? 1u : 0u;
  write32(r->bytes + 4, game->time_limit_ms);
  write32(r->bytes + 8, seed);
  write32(r->bytes + 12, date);
  r->used = REPLAY_HEADER_BYTES;
  r->active = true;
}

static void note(Replay *r, uint32_t kind) {
  uint32_t value;
  size_t length = 1u;
  if (r == NULL || !r->active || r->overflow) return;
  value = (r->clock_ms - r->last_ms) << 2 | kind;
  while ((value >> (7u * length)) != 0u) ++length;
  if (r->events >= REPLAY_MAX_EVENTS || r->used + length + REPLAY_TRAILER_BYTES > REPLAY_MAX_BYTES) {
    r->overflow = true;
    return;
  }
  for (; length > 1u; --length) { r->bytes[r->used++] = (uint8_t)(value | 0x80u); value >>= 7u; }
  r->bytes[r->used++] = (uint8_t)value;
  ++r->events;
  r->last_ms = r->clock_ms;
}

static void tick_clock(Replay *r, const Game *game, uint32_t ms) {
  if (r == NULL || !r->active || r->overflow) return;
  if (game->status != GAME_PLAYING && game->status != GAME_RECOVERING) return;
  if (ms > REPLAY_MAX_MS - r->clock_ms) { r->overflow = true; return; }
  r->clock_ms += ms;
}

bool replay_input(Replay *r, Game *game, GameButton button, bool pressed) {
  uint8_t before = game->held_buttons;
  bool over = game->status == GAME_OVER, moved = game_input(game, button, pressed);
  if (r != NULL && !over && game->held_buttons != before) note(r, (uint32_t)button);
  return moved;
}

bool replay_pause(Replay *r, Game *game) {
  bool changed = game_pause(game);
  if (changed) note(r, 2u);
  return changed;
}

bool replay_resume(Replay *r, Game *game) {
  bool changed = game_resume(game);
  if (changed) note(r, 3u);
  return changed;
}

bool replay_advance(Replay *r, Game *game, uint32_t elapsed_ms) {
  tick_clock(r, game, elapsed_ms);
  return game_advance(game, elapsed_ms);
}

bool replay_step(Replay *r, Game *game) {
  tick_clock(r, game, game_next_boundary_ms(game));
  return game_step(game);
}

size_t replay_finish(Replay *r, const Game *game) {
  size_t length;
  if (!r->active) return 0u;
  r->active = false;
  r->bytes[2] = (uint8_t)((r->bytes[2] & 1u) | (r->overflow ? 2u : 0u));
  write32(r->bytes + 16, game->score);
  write32(r->bytes + 20, r->clock_ms);
  r->bytes[24] = (uint8_t)r->events;
  r->bytes[25] = (uint8_t)(r->events >> 8);
  length = r->used;
  write32(r->bytes + length, checksum(r->bytes, length));
  return length + REPLAY_TRAILER_BYTES;
}

static bool read_varint(const uint8_t *data, size_t end, size_t *pos, uint32_t *value) {
  uint32_t result = 0u;
  unsigned shift;
  for (shift = 0u; shift < 35u; shift += 7u) {
    uint8_t byte;
    if (*pos >= end) return false;
    byte = data[(*pos)++];
    if (shift == 28u && (byte & 0xf0u) != 0u) return false; /* More than 32 bits. */
    result |= (uint32_t)(byte & 0x7fu) << shift;
    if ((byte & 0x80u) == 0u) {
      if (byte == 0u && shift != 0u) return false; /* Not the shortest form. */
      *value = result;
      return true;
    }
  }
  return false;
}

ReplayStatus replay_simulate(const uint8_t *data, size_t length, ReplayResult *out) {
  ReplayResult result;
  Game game;
  size_t end, pos = REPLAY_HEADER_BYTES;
  uint32_t count, i, now = 0u;
  memset(&result, 0, sizeof(result));
  result.status = REPLAY_BAD_FORMAT;
  *out = result;
  if (data == NULL || length < REPLAY_HEADER_BYTES + REPLAY_TRAILER_BYTES || length > REPLAY_MAX_BYTES) return out->status;
  end = length - REPLAY_TRAILER_BYTES;
  if (read32(data + end) != checksum(data, end)) return out->status = REPLAY_BAD_CHECKSUM;
  if (data[0] != REPLAY_VERSION || data[1] > 1u || (data[2] & ~3u) != 0u || data[3] != 0u) return out->status;
  result.mode = data[1];
  result.controls_swapped = (data[2] & 1u) != 0u;
  result.time_limit_ms = read32(data + 4);
  result.seed = read32(data + 8);
  result.date = read32(data + 12);
  result.claimed_score = read32(data + 16);
  result.end_ms = read32(data + 20);
  count = (uint32_t)data[24] | (uint32_t)data[25] << 8;
  result.status = REPLAY_BAD_FORMAT;
  if (result.time_limit_ms > REPLAY_MAX_MS || result.end_ms > REPLAY_MAX_MS || count > REPLAY_MAX_EVENTS ||
      result.date > 99991231u) { *out = result; return result.status; }
  if ((data[2] & 2u) != 0u) { *out = result; return out->status = REPLAY_UNVERIFIABLE; }
  game_init(&game, (GameMode)result.mode, result.seed);
  game_start_timed(&game, (GameMode)result.mode, result.seed, result.time_limit_ms);
  game.controls_swapped = result.controls_swapped;
  for (i = 0u; i < count; ++i) {
    uint32_t value, delta, kind;
    if (!read_varint(data, end, &pos, &value)) { *out = result; return result.status; }
    delta = value >> 2;
    kind = value & 3u;
    if (delta > result.end_ms - now) { *out = result; return result.status; }
    now += delta;
    game_advance(&game, delta);
    if (game.status == GAME_OVER) { *out = result; return result.status; } /* Nothing happens after the end. */
    if (kind < 2u) {
      GameButton button = kind == 0u ? GAME_UP : GAME_DOWN;
      game_input(&game, button, (game.held_buttons & (1u << kind)) == 0u);
    } else if (!(kind == 2u ? game_pause(&game) : game_resume(&game))) {
      *out = result;
      return result.status;
    }
  }
  if (pos != end) { *out = result; return result.status; }
  game_advance(&game, result.end_ms - now);
  result.score = game.score;
  result.catches = game.catches;
  result.drops = game.drops;
  result.hits = game.hits;
  result.total_misses = game.total_misses;
  result.time_left_ms = game.time_left_ms;
  result.game_status = game.status;
  result.time_up = game.time_up;
  result.status = result.score == result.claimed_score ? REPLAY_OK : REPLAY_SCORE_MISMATCH;
  *out = result;
  return result.status;
}
