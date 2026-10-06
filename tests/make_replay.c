/* Test helper: plays one recorded Daily round with an imperfect bot and writes the replay
 * bytes to stdout. Usage: make_replay DATE BOT_SEED NOISE [swapped] [seed=N] [mode=A] [limit=MS]
 * NOISE is the 1-in-N chance per move of heading for a random lane (0 = never). The options
 * after it record a round that is not a real Daily (for verifier tests). */
#include "game.h"
#include "replay.h"
#include "tuning.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rnd(uint32_t *state) {
  uint32_t x = *state;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  return *state = x;
}

static void play_input(Replay *replay, Game *game, uint32_t *rng, uint32_t noise) {
  unsigned target = 2u, i;
  uint64_t soonest = UINT64_MAX;
  for (i = 0u; i < GAME_MAX_CARGO; ++i)
    if (game->cargo[i].active && game->cargo[i].landing_step < soonest) {
      soonest = game->cargo[i].landing_step;
      target = game_lane_pose(game->cargo[i].lane);
    }
  if (noise != 0u && rnd(rng) % noise == 0u) target = rnd(rng) % GAME_POSES;
  while (game->popeye_pose != target && game->status == GAME_PLAYING) {
    GameButton button = (game->popeye_pose > target) != (game->controls_swapped != 0) ? GAME_UP : GAME_DOWN;
    uint8_t before = game->popeye_pose;
    (void)replay_input(replay, game, button, true);
    (void)replay_input(replay, game, button, false);
    if (game->popeye_pose == before) break;
  }
}

int main(int argc, char **argv) {
  static Replay replay;
  Game game;
  uint32_t date, rng, noise, seed, limit = PGW_SPRINT_MS;
  GameMode mode = GAME_B;
  bool swapped = false;
  size_t length;
  unsigned iterations;
  int i;
  if (argc < 4) return 2;
  date = (uint32_t)strtoul(argv[1], NULL, 10);
  rng = (uint32_t)strtoul(argv[2], NULL, 10) * 2246822519u + 3266489917u;
  noise = (uint32_t)strtoul(argv[3], NULL, 10);
  seed = game_daily_seed(date);
  for (i = 4; i < argc; ++i) {
    if (strcmp(argv[i], "swapped") == 0) swapped = true;
    else if (strncmp(argv[i], "seed=", 5) == 0) seed = (uint32_t)strtoul(argv[i] + 5, NULL, 10);
    else if (strcmp(argv[i], "mode=A") == 0) mode = GAME_A;
    else if (strncmp(argv[i], "limit=", 6) == 0) limit = (uint32_t)strtoul(argv[i] + 6, NULL, 10);
    else return 2;
  }
  game_init(&game, mode, seed);
  game_start_timed(&game, mode, seed, limit);
  game.controls_swapped = swapped;
  replay_begin(&replay, &game, seed, date);
  for (iterations = 0u; iterations < 4000u && game.status != GAME_OVER; ++iterations) {
    play_input(&replay, &game, &rng, noise);
    (void)replay_advance(&replay, &game, 100u + rnd(&rng) % 500u);
  }
  length = replay_finish(&replay, &game);
  if (length == 0u || replay.overflow || fwrite(replay.bytes, 1u, length, stdout) != length) return 1;
  return 0;
}
