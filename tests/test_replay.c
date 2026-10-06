#include "game.h"
#include "replay.h"
#include "tuning.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t rnd(uint32_t *state) {
  uint32_t x = *state;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  return *state = x;
}

static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Recomputes the trailer so tests can forge otherwise well-formed records. */
static void reseal(uint8_t *bytes, size_t length) {
  uint32_t hash = UINT32_C(2166136261);
  size_t i;
  for (i = 0; i + 4 < length; ++i) hash = (hash ^ bytes[i]) * UINT32_C(16777619);
  for (i = 0; i < 4; ++i) bytes[length - 4 + i] = (uint8_t)(hash >> (8u * i));
}

/* Imperfect player: moves toward the soonest food, sometimes elsewhere, via recorded presses. */
static void play_input(Replay *replay, Game *game, uint32_t *rng) {
  unsigned target = 2u, i;
  uint64_t soonest = UINT64_MAX;
  for (i = 0u; i < GAME_MAX_CARGO; ++i)
    if (game->cargo[i].active && game->cargo[i].landing_step < soonest) {
      soonest = game->cargo[i].landing_step;
      target = game_lane_pose(game->cargo[i].lane);
    }
  if (rnd(rng) % 12u == 0u) target = rnd(rng) % GAME_POSES;
  while (game->popeye_pose != target && game->status == GAME_PLAYING) {
    GameButton button = (game->popeye_pose > target) != (game->controls_swapped != 0) ? GAME_UP : GAME_DOWN;
    uint8_t before = game->popeye_pose;
    (void)replay_input(replay, game, button, true);
    (void)replay_input(replay, game, button, false);
    if (game->popeye_pose == before) break;
  }
}

typedef struct { unsigned timeups, early, stopped, paused_end, events; size_t longest; } Tally;

static void check_round(uint32_t seed, GameMode mode, uint32_t limit, bool swapped, Tally *tally) {
  Replay replay;
  Game game;
  ReplayResult result;
  uint32_t rng = seed * 2246822519u + 3266489917u, date = 20260101u + seed % 28u;
  size_t length;
  unsigned iterations;
  game_init(&game, mode, seed);
  game_start_timed(&game, mode, seed, limit);
  game.controls_swapped = swapped;
  replay_begin(&replay, &game, seed, date);
  for (iterations = 0; iterations < 4000u && game.status != GAME_OVER; ++iterations) {
    uint32_t action = rnd(&rng) % 100u;
    if (rnd(&rng) % 500u == 0u) { tally->stopped++; break; } /* Quit part-way. */
    if (action < 35u) play_input(&replay, &game, &rng);
    else if (action < 40u) {
      /* Redundant calls and stray releases are no-ops and are not recorded. */
      (void)replay_input(&replay, &game, GAME_UP, false);
      (void)replay_input(&replay, &game, GAME_DOWN, true);
      (void)replay_input(&replay, &game, GAME_DOWN, true);
      (void)replay_input(&replay, &game, GAME_DOWN, false);
    } else if (action < 47u) {
      if (replay_pause(&replay, &game)) {
        (void)replay_advance(&replay, &game, 1u + rnd(&rng) % 30000u); /* No active time passes. */
        (void)replay_step(&replay, &game);
        (void)replay_input(&replay, &game, GAME_UP, true);
        (void)replay_input(&replay, &game, GAME_UP, false);
        assert(!replay_pause(&replay, &game));
        if (rnd(&rng) % 60u == 0u) { tally->paused_end++; break; } /* Stop while paused. */
        assert(replay_resume(&replay, &game));
      }
    } else if (action < 80u) (void)replay_advance(&replay, &game, 1u + rnd(&rng) % 900u);
    else (void)replay_step(&replay, &game);
  }
  length = replay_finish(&replay, &game);
  assert(length == replay.used + 4u && length <= REPLAY_MAX_BYTES && !replay.overflow);
  assert(replay_simulate(replay.bytes, length, &result) == REPLAY_OK && result.status == REPLAY_OK);
  assert(result.mode == (uint8_t)mode && result.seed == seed && result.date == date);
  assert(result.time_limit_ms == limit && result.controls_swapped == swapped);
  assert(result.score == game.score && result.claimed_score == game.score);
  assert(result.catches == game.catches && result.drops == game.drops && result.hits == game.hits);
  assert(result.total_misses == game.total_misses && result.time_left_ms == game.time_left_ms);
  assert(result.game_status == game.status && result.time_up == game.time_up);
  tally->events += replay.events;
  if (length > tally->longest) tally->longest = length;
  if (game.time_up) tally->timeups++;
  else if (game.status == GAME_OVER) tally->early++;
}

static void test_round_trips(void) {
  Tally tally = { 0, 0, 0, 0, 0, 0 };
  uint32_t seed;
  for (seed = 1u; seed <= 3000u; ++seed) {
    static const uint32_t limits[] = { 0u, 20000u, PGW_SPRINT_MS };
    check_round(seed, seed % 2u ? GAME_B : GAME_A, limits[seed / 2u % 3u], seed % 5u == 0u, &tally);
  }
  assert(tally.timeups > 100u && tally.early > 50u && tally.stopped > 20u && tally.paused_end > 5u);
  printf("Replay: 3,000 recorded rounds (%u time-ups, %u early overs, %u quits, %u events, longest %u bytes) re-simulated exactly\n",
         tally.timeups, tally.early, tally.stopped, tally.events, (unsigned)tally.longest);
}

static size_t sample(uint8_t *out, uint32_t seed) {
  Replay replay;
  Game game;
  uint32_t rng = seed;
  unsigned i;
  game_init(&game, GAME_B, seed);
  game_start_timed(&game, GAME_B, seed, PGW_SPRINT_MS);
  replay_begin(&replay, &game, seed, 20261006u);
  for (i = 0; i < 40u && game.status != GAME_OVER; ++i) {
    play_input(&replay, &game, &rng);
    (void)replay_advance(&replay, &game, 100u + rnd(&rng) % 600u);
  }
  assert(replay.events >= 10u);
  i = (unsigned)replay_finish(&replay, &game);
  memcpy(out, replay.bytes, i);
  return i;
}

static void test_corruption(void) {
  uint8_t bytes[REPLAY_MAX_BYTES], work[REPLAY_MAX_BYTES];
  ReplayResult result;
  size_t length = sample(bytes, 9u), i;
  unsigned bit;
  assert(replay_simulate(bytes, length, &result) == REPLAY_OK);
  for (i = 0; i < length; ++i) {
    for (bit = 0; bit < 8; ++bit) {
      memcpy(work, bytes, length);
      work[i] ^= (uint8_t)(1u << bit);
      assert(replay_simulate(work, length, &result) == REPLAY_BAD_CHECKSUM); /* Header, events or trailer. */
    }
  }
  for (i = 0; i < length; ++i) assert(replay_simulate(bytes, i, &result) != REPLAY_OK); /* Truncated. */
  memcpy(work, bytes, length);
  work[length] = 0u;
  assert(replay_simulate(work, length + 1u, &result) != REPLAY_OK); /* Extended. */
  assert(replay_simulate(NULL, length, &result) == REPLAY_BAD_FORMAT);
  assert(replay_simulate(work, REPLAY_MAX_BYTES + 1u, &result) == REPLAY_BAD_FORMAT);

  /* Well-formed checksums over bad content. */
#define FORGE(edit, expected) do { memcpy(work, bytes, length); edit; reseal(work, length); \
    assert(replay_simulate(work, length, &result) == (expected)); } while (0)
  FORGE((void)0, REPLAY_OK);
  FORGE(work[0] = 2, REPLAY_BAD_FORMAT);
  FORGE(work[0] = 0, REPLAY_BAD_FORMAT);
  FORGE(work[1] = 2, REPLAY_BAD_FORMAT);
  FORGE(work[2] |= 4, REPLAY_BAD_FORMAT);
  FORGE(work[3] = 1, REPLAY_BAD_FORMAT);
  FORGE(work[7] = 0xff, REPLAY_BAD_FORMAT); /* Limit far beyond the sanity bound. */
  FORGE(work[23] = 0xff, REPLAY_BAD_FORMAT); /* end_ms beyond the bound. */
  FORGE(work[15] = 0xff, REPLAY_BAD_FORMAT); /* Date out of range. */
  FORGE(work[24]++, REPLAY_BAD_FORMAT);      /* Event count too high... */
  FORGE(work[24]--, REPLAY_BAD_FORMAT);      /* ...or too low leaves bytes unread. */
  FORGE(work[25] = 0x10, REPLAY_BAD_FORMAT); /* Count above the maximum. */
  FORGE(work[16]++, REPLAY_SCORE_MISMATCH);  /* A claim the engine does not reproduce. */
  FORGE(work[2] |= 2, REPLAY_UNVERIFIABLE);
  FORGE(memset(work + 20, 0, 4), REPLAY_BAD_FORMAT); /* end_ms before the last event. */
  memcpy(work, bytes, length);
  work[2] ^= 1; reseal(work, length); /* Swapped controls play differently: never crashes, rarely matches. */
  assert(replay_simulate(work, length, &result) != REPLAY_BAD_FORMAT);
  /* Malformed varints and impossible events. */
  {
    uint8_t forged[64];
    size_t n = REPLAY_HEADER_BYTES;
    memcpy(forged, bytes, REPLAY_HEADER_BYTES);
    forged[16] = forged[17] = forged[18] = forged[19] = 0; /* Claim zero. */
    forged[20] = 100; forged[21] = forged[22] = forged[23] = 0;
    forged[24] = 1; forged[25] = 0;
    forged[n++] = 0x80; forged[n++] = 0x00; /* Non-canonical zero. */
    n += 4;
    reseal(forged, n);
    assert(replay_simulate(forged, n, &result) == REPLAY_BAD_FORMAT);
    n = REPLAY_HEADER_BYTES; forged[n++] = 0x80; n += 4; reseal(forged, n); /* Truncated varint. */
    assert(replay_simulate(forged, n, &result) == REPLAY_BAD_FORMAT);
    n = REPLAY_HEADER_BYTES;
    forged[n++] = 0xff; forged[n++] = 0xff; forged[n++] = 0xff; forged[n++] = 0xff; forged[n++] = 0x1f; /* 33 bits. */
    n += 4; reseal(forged, n);
    assert(replay_simulate(forged, n, &result) == REPLAY_BAD_FORMAT);
    n = REPLAY_HEADER_BYTES; forged[n++] = 3; n += 4; reseal(forged, n); /* Resume while playing. */
    assert(replay_simulate(forged, n, &result) == REPLAY_BAD_FORMAT);
    n = REPLAY_HEADER_BYTES; forged[n++] = 2; n += 4; reseal(forged, n); /* Pause: fine. */
    assert(replay_simulate(forged, n, &result) == REPLAY_OK && result.score == 0u);
    forged[24] = 0; n = REPLAY_HEADER_BYTES + 4; reseal(forged, n); /* No events, 100 ms of play. */
    assert(replay_simulate(forged, n, &result) == REPLAY_OK && result.end_ms == 100u && result.time_left_ms == PGW_SPRINT_MS - 100u);
  }
}

static void test_overflow(void) {
  Replay replay;
  Game game;
  ReplayResult result;
  unsigned i;
  size_t length;
  /* More than the maximum number of events. */
  game_init(&game, GAME_B, 4u);
  game_start_timed(&game, GAME_B, 4u, PGW_SPRINT_MS);
  replay_begin(&replay, &game, 4u, 20261006u);
  for (i = 0; i < REPLAY_MAX_EVENTS / 2u; ++i) {
    (void)replay_input(&replay, &game, GAME_UP, true);
    (void)replay_input(&replay, &game, GAME_UP, false);
  }
  assert(!replay.overflow && replay.events == REPLAY_MAX_EVENTS);
  (void)replay_input(&replay, &game, GAME_UP, true);
  assert(replay.overflow && replay.events == REPLAY_MAX_EVENTS); /* Nothing is silently dropped or truncated. */
  length = replay_finish(&replay, &game);
  assert(length <= REPLAY_MAX_BYTES && (replay.bytes[2] & 2u));
  assert(replay_simulate(replay.bytes, length, &result) == REPLAY_UNVERIFIABLE);
  /* More than the byte budget: large gaps make 3-byte events. The clock is advanced directly
   * because a real round would end first. */
  game_init(&game, GAME_B, 4u);
  game_start_timed(&game, GAME_B, 4u, 0u);
  replay_begin(&replay, &game, 4u, 20261006u);
  for (i = 0; i < 700u && !replay.overflow; ++i) {
    replay.clock_ms += 10000u;
    (void)replay_input(&replay, &game, GAME_DOWN, i % 2u == 0u);
    assert(replay.used + 4u <= REPLAY_MAX_BYTES);
  }
  assert(replay.overflow && i < 700u && replay.events < REPLAY_MAX_EVENTS);
  assert(replay_finish(&replay, &game) <= REPLAY_MAX_BYTES);
  /* Active clock beyond the sanity bound. */
  game_init(&game, GAME_B, 4u);
  game_start_timed(&game, GAME_B, 4u, 0u);
  replay_begin(&replay, &game, 4u, 20261006u);
  (void)replay_advance(&replay, &game, REPLAY_MAX_MS);
  assert(!replay.overflow);
  game_init(&game, GAME_B, 4u);
  game_start_timed(&game, GAME_B, 4u, 0u);
  replay_begin(&replay, &game, 4u, 20261006u);
  (void)replay_advance(&replay, &game, 5000u);
  assert(!replay.overflow);
  (void)replay_advance(&replay, &game, REPLAY_MAX_MS);
  assert(replay.overflow);
  /* A NULL recorder is simply the engine. */
  game_init(&game, GAME_B, 4u);
  game_start_timed(&game, GAME_B, 4u, 100u);
  assert(replay_input(NULL, &game, GAME_UP, true) && replay_advance(NULL, &game, 100u) && game.time_up);
  assert(replay_finish(&replay, &game) != 0u && replay_finish(&replay, &game) == 0u); /* Sealing twice yields nothing. */
}

static void test_format(void) {
  /* A hand-built round pins the documented byte format. */
  Replay replay;
  Game game;
  ReplayResult result;
  static const uint8_t expected[] = {
    1, 1, 1, 0,                 /* version, Game B rules, controls swapped, reserved */
    0x60, 0xea, 0x00, 0x00,     /* time limit 60000 */
    7, 0, 0, 0,                 /* seed */
    0x8e, 0x28, 0x35, 0x01,     /* date 20261006 = 0x0135288e */
    0, 0, 0, 0,                 /* final score */
    0xf4, 0x01, 0x00, 0x00,     /* end_ms 500 */
    3, 0,                       /* three events */
    0xb0, 0x09,                 /* (300 << 2 | 0) = 1200: UP pressed after 300 ms */
    0xa0, 0x06,                 /* (200 << 2 | 0) = 800: UP released 200 ms later */
    0x02                        /* (0 << 2 | 2): pause at once */
  };
  size_t length;
  game_init(&game, GAME_B, 7u);
  game_start_timed(&game, GAME_B, 7u, PGW_SPRINT_MS);
  game.controls_swapped = true;
  replay_begin(&replay, &game, 7u, 20261006u);
  (void)replay_advance(&replay, &game, 300u);
  (void)replay_input(&replay, &game, GAME_UP, true);
  (void)replay_advance(&replay, &game, 200u);
  (void)replay_input(&replay, &game, GAME_UP, false);
  assert(replay_pause(&replay, &game));
  (void)replay_advance(&replay, &game, 9999u); /* Paused: no active time passes. */
  length = replay_finish(&replay, &game);
  assert(length == sizeof(expected) + 4u && memcmp(replay.bytes, expected, sizeof(expected)) == 0);
  assert(read32(replay.bytes + sizeof(expected)) != 0u);
  assert(replay_simulate(replay.bytes, length, &result) == REPLAY_OK);
  assert(result.game_status == GAME_PAUSED && result.end_ms == 500u && result.time_left_ms == PGW_SPRINT_MS - 500u);
  assert(game_daily_seed(20261006u) == UINT32_C(0x0bbfc1bb) && game_daily_seed(20261007u) == UINT32_C(0xb574484a));
}

int main(void) {
  test_format();
  test_round_trips();
  test_corruption();
  test_overflow();
  puts("Replay tests passed: byte format, recorded rounds re-simulate exactly, corrupt/truncated/overflowing records rejected");
  return 0;
}
