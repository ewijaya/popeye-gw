#include "game.h"
#include "tuning.h"

#include <limits.h>
#include <string.h>

static uint32_t random_next(uint32_t *state) {
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static uint32_t seed_nonzero(uint32_t seed) {
  return seed != 0u ? seed : UINT32_C(0x6d2b79f5);
}

uint8_t game_lane_pose(uint8_t lane) {
  static const uint8_t poses[GAME_LANES] = { 0u, 1u, 3u, 4u };
  return lane < GAME_LANES ? poses[lane] : 2u;
}

uint32_t game_step_interval(const Game *game) {
  /* PP-23 manual: food speed returns to normal every 100 points. These four
   * durations are watch tuning, not measurements of the Nintendo ROM. */
  static const uint16_t steps[] = PGW_FOOD_STEP_MS;
  return steps[(game->score % 100u) / PGW_SPEED_POINTS];
}

uint16_t game_display_score(const Game *game) {
  return (uint16_t)(game->score % 1000u);
}

uint8_t game_cargo_limit(const Game *game) {
  uint32_t cycle_score = game->score % 100u;
  if (cycle_score < PGW_FIRST_CARGO_THRESHOLD) return 1u;
  if (cycle_score < PGW_SECOND_CARGO_THRESHOLD) return 2u;
  return 3u;
}

static uint32_t next_idle(Game *game) {
  uint32_t lo = PGW_IDLE_MIN_MS;
  uint32_t hi = PGW_IDLE_MAX_MS;
  return lo + random_next(&game->attack.rng) % (hi - lo + 1u);
}

void game_init(Game *game, GameMode mode, uint32_t seed) {
  memset(game, 0, sizeof(*game));
  game_start(game, mode, seed);
}

void game_start(Game *game, GameMode mode, uint32_t seed) {
  game_start_timed(game, mode, seed, 0u);
}

uint32_t game_daily_seed(uint32_t date) {
  return date * UINT32_C(2654435761) ^ PGW_DAILY_SEED_SALT;
}

void game_start_timed(Game *game, GameMode mode, uint32_t seed, uint32_t limit_ms) {
  uint32_t highs[2] = { game->high_scores[0], game->high_scores[1] };
  bool swapped = game->controls_swapped;
  memset(game, 0, sizeof(*game));
  game->high_scores[0] = highs[0];
  game->high_scores[1] = highs[1];
  game->controls_swapped = swapped;
  game->mode = mode == GAME_B ? GAME_B : GAME_A;
  game->status = GAME_PLAYING;
  game->resume_status = GAME_PLAYING;
  game->popeye_pose = 2u;
  game->rng = seed_nonzero(seed);
  game->next_cargo_id = 1u;
  game->catch_pose = -1;
  game->splash_lane = -1;
  game->olive_target = (uint8_t)(random_next(&game->rng) % GAME_LANES);
  game->attack.rng = seed_nonzero(seed ^ UINT32_C(0xc8013ea4));
  game->attack.idle_ms_left = next_idle(game);
  game->step_ms_left = game_step_interval(game);
  game->time_limit_ms = game->time_left_ms = limit_ms;
}

bool game_input(Game *game, GameButton button, bool pressed) {
  uint8_t mask;
  int direction;
  uint8_t old_pose;
  if (button != GAME_UP && button != GAME_DOWN) return false;
  mask = (uint8_t)(1u << (unsigned)button);
  if (!pressed) {
    game->held_buttons &= (uint8_t)~mask;
    return false;
  }
  if ((game->held_buttons & mask) != 0u) return false;
  game->held_buttons |= mask;
  if (game->status != GAME_PLAYING && game->status != GAME_OVER) return false;
  direction = button == GAME_UP ? -1 : 1;
  if (game->controls_swapped) direction = -direction;
  old_pose = game->popeye_pose;
  if (direction < 0 && game->popeye_pose > 0u) --game->popeye_pose;
  if (direction > 0 && game->popeye_pose + 1u < GAME_POSES) ++game->popeye_pose;
  if (game->status == GAME_OVER && game->popeye_pose != old_pose) {
    game->miss_cause = GAME_MISS_NONE;
    game->catch_pose = -1;
    game->splash_lane = -1;
  }
  return game->popeye_pose != old_pose;
}

bool game_pause(Game *game) {
  if (game->status != GAME_PLAYING && game->status != GAME_RECOVERING) return false;
  game->resume_status = game->status;
  game->status = GAME_PAUSED;
  return true;
}

bool game_resume(Game *game) {
  if (game->status != GAME_PAUSED) return false;
  game->status = game->resume_status;
  return true;
}

uint32_t game_next_boundary_ms(const Game *game) {
  uint32_t room = game->step_ms_left;
  if (game->status == GAME_RECOVERING) return game->recovery_ms_left;
  if (game->status != GAME_PLAYING) return 0u;
  if (game->time_limit_ms != 0u && game->time_left_ms < room) room = game->time_left_ms;
  return room;
}

uint32_t game_take_events(Game *game) {
  uint32_t events = game->events;
  game->events = 0u;
  return events;
}

static bool attack_strike_time(const Game *game, uint64_t *when) {
  const GameAttack *attack = &game->attack;
  if (attack->phase == GAME_ATTACK_WINDUP) {
    *when = game->step + attack->steps_left;
    return true;
  }
  if (attack->phase == GAME_ATTACK_STRIKE) {
    *when = game->step;
    return true;
  }
  return false;
}

static bool strike_conflicts_cargo(const Game *game, uint64_t strike) {
  unsigned i;
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    const GameCargo *cargo = &game->cargo[i];
    if (cargo->active && cargo->lane == (game->attack.side == GAME_LEFT ? 0u : 3u) &&
        (cargo->landing_step == strike || cargo->landing_step + 1u == strike)) return true;
  }
  return false;
}

/* A punch must respect cargo already in flight. Unsafe wind-ups wait until
 * their full two-step warning and strike fit the vulnerable-side reservations. */
static bool can_windup(const Game *game) {
  return !strike_conflicts_cargo(game, game->step + PGW_WINDUP_STEPS);
}

static void update_attack(Game *game) {
  GameAttack *attack = &game->attack;
  if (attack->phase == GAME_ATTACK_WINDUP) {
    if (--attack->steps_left == 0u) {
      attack->phase = GAME_ATTACK_STRIKE;
      attack->steps_left = PGW_STRIKE_STEPS;
    }
  } else if (attack->phase == GAME_ATTACK_STRIKE) {
    if (--attack->steps_left == 0u) {
      attack->phase = GAME_ATTACK_IDLE;
      if (game->mode == GAME_B) {
        /* Alternating after each strike is provisional until a trace supplies
         * the original side-switch schedule. Never show two rivals at once. */
        attack->side = attack->side == GAME_LEFT ? GAME_RIGHT : GAME_LEFT;
      }
      attack->idle_ms_left = next_idle(game);
    }
  }
  if (attack->phase == GAME_ATTACK_IDLE && attack->idle_ms_left == 0u && can_windup(game)) {
    attack->phase = GAME_ATTACK_WINDUP;
    attack->steps_left = PGW_WINDUP_STEPS;
  }
}

static void miss(Game *game, GameMissCause cause) {
  unsigned i;
  ++game->misses;
  ++game->total_misses;
  game->miss_cause = cause;
  game->events |= GAME_EVENT_MISS;
  for (i = 0u; i < GAME_MAX_CARGO; ++i) game->cargo[i].active = false;
  if (game->misses >= GAME_MAX_MISSES) {
    game->status = GAME_OVER;
    game->events |= GAME_EVENT_OVER;
  } else {
    game->status = GAME_RECOVERING;
    game->recovery_ms_left = PGW_MISS_RECOVERY_MS;
  }
}

static void caught(Game *game, uint8_t lane) {
  uint32_t before = game->score;
  uint64_t after = (uint64_t)before + 1u;
  uint64_t block = (uint64_t)before / 1000u * 1000u;
  uint64_t milestones[2] = { block + 200u, block + 500u };
  unsigned i;
  game->score = after > UINT32_MAX ? UINT32_MAX : (uint32_t)after;
  ++game->catches;
  game->catch_pose = (int8_t)game_lane_pose(lane);
  game->events |= GAME_EVENT_CATCH;
  for (i = 0u; i < 2u; ++i) {
    if (before < milestones[i] && game->score >= milestones[i]) {
      game->misses = 0u;
      game->half_ring = false;
      game->events |= GAME_EVENT_BONUS;
    }
  }
  if (game->score > game->high_scores[game->mode]) {
    game->high_scores[game->mode] = game->score;
    game->new_high_score = true;
    game->events |= GAME_EVENT_HIGH_SCORE;
  }
}

static void dropped(Game *game, uint8_t lane) {
  ++game->drops;
  game->splash_lane = (int8_t)lane;
  game->events |= GAME_EVENT_DROP;
  if (game->half_ring) {
    game->half_ring = false;
    miss(game, GAME_MISS_DROP);
  } else {
    game->half_ring = true;
  }
}

/* Fairness certificate: each commitment compares with EVERY live reservation,
 * using distance in Popeye's five poses (not four lanes). Launch+4 grants the
 * initial centre pose enough time to reach any lane. Consecutive reservations
 * therefore form a feasible one-move-per-step route; the all-pairs test also
 * rejects nonconsecutive incompatibilities. Misses clear the route. A strike
 * prevents cargo on its side landing on its strike tick or the tick before the strike,
 * whether the attack or cargo was committed first. Between two far catches on the same side,
 * an intervening strike needs a single move out and back; the excluded previous
 * tick supplies this time. Travel between different lanes never needs a far
 * pose except at its endpoint. Wind-ups always provide two visible intervals.
 * Olive prepares a changed arc for one tick; capacity/spacing can only delay a
 * target until existing cargo resolves (at most four ticks), so no target can
 * stall permanently. Random choices affect variety, never these invariants.
 */
static bool can_launch(const Game *game, uint8_t lane) {
  uint64_t landing = game->step + GAME_CARGO_STEPS - 1u;
  uint8_t pose = game_lane_pose(lane);
  unsigned i;
  unsigned active = 0u;
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    const GameCargo *cargo = &game->cargo[i];
    uint8_t other_pose;
    unsigned distance;
    uint64_t separation;
    if (!cargo->active) continue;
    ++active;
    other_pose = game_lane_pose(cargo->lane);
    distance = pose > other_pose ? pose - other_pose : other_pose - pose;
    separation = landing > cargo->landing_step ? landing - cargo->landing_step : cargo->landing_step - landing;
    if (separation < distance) return false;
  }
  if (active >= game_cargo_limit(game)) return false;
  {
    uint64_t strike;
    if (lane == (game->attack.side == GAME_LEFT ? 0u : 3u) && attack_strike_time(game, &strike) &&
        (landing == strike || landing + 1u == strike)) return false;
  }
  return true;
}

static void update_olive(Game *game) {
  unsigned i;
  if (!game->olive_ready) {
    game->olive_ready = true;
    return;
  }
  if (!can_launch(game, game->olive_target)) return;
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    GameCargo *cargo = &game->cargo[i];
    if (cargo->active) continue;
    cargo->active = true;
    cargo->lane = game->olive_target;
    cargo->stage = 0u;
    cargo->id = game->next_cargo_id++;
    cargo->launch_step = game->step;
    cargo->landing_step = game->step + GAME_CARGO_STEPS - 1u;
    game->olive_throwing = true;
    game->olive_target = (uint8_t)(random_next(&game->rng) % GAME_LANES);
    /* Repeated arcs may follow immediately; a changed arc gets one ready
     * frame at the same fixed ledge before its next throw. */
    game->olive_ready = game->olive_target == cargo->lane;
    game->events |= GAME_EVENT_LAUNCH;
    return;
  }
}

static void tick(Game *game) {
  unsigned i;
  ++game->step;
  game->catch_pose = -1;
  game->splash_lane = -1;
  game->miss_cause = GAME_MISS_NONE;
  game->olive_throwing = false;
  update_attack(game);
  if (game->attack.phase == GAME_ATTACK_STRIKE &&
      game->popeye_pose == (game->attack.side == GAME_LEFT ? 0u : 4u)) {
    ++game->hits;
    miss(game, GAME_MISS_HIT);
    return;
  }
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    GameCargo *cargo = &game->cargo[i];
    if (!cargo->active) continue;
    ++cargo->stage;
    if (cargo->stage == GAME_CARGO_STEPS - 1u) {
      uint8_t lane = cargo->lane;
      cargo->active = false;
      if (game->popeye_pose == game_lane_pose(lane)) caught(game, lane);
      else dropped(game, lane);
      if (game->status != GAME_PLAYING) return;
    }
  }
  update_olive(game);
}

static void end_time(Game *game) {
  unsigned i;
  for (i = 0u; i < GAME_MAX_CARGO; ++i) game->cargo[i].active = false;
  game->status = GAME_OVER;
  game->time_up = true;
  game->events |= GAME_EVENT_TIME_UP | GAME_EVENT_OVER;
}

static void elapse_idle(Game *game, uint32_t elapsed) {
  GameAttack *attack = &game->attack;
  if (attack->phase == GAME_ATTACK_IDLE) {
    attack->idle_ms_left = elapsed >= attack->idle_ms_left ? 0u : attack->idle_ms_left - elapsed;
  }
}

bool game_advance(Game *game, uint32_t elapsed_ms) {
  bool changed = false;
  while (elapsed_ms != 0u && (game->status == GAME_PLAYING || game->status == GAME_RECOVERING)) {
    uint32_t chunk;
    if (game->status == GAME_RECOVERING) {
      chunk = elapsed_ms < game->recovery_ms_left ? elapsed_ms : game->recovery_ms_left;
      game->recovery_ms_left -= chunk;
      elapsed_ms -= chunk;
      if (game->recovery_ms_left == 0u) {
        game->status = GAME_PLAYING;
        game->step_ms_left = game_step_interval(game);
        game->miss_cause = GAME_MISS_NONE;
        game->splash_lane = -1;
        changed = true;
      }
    } else {
      uint32_t room = game_next_boundary_ms(game);
      chunk = elapsed_ms < room ? elapsed_ms : room;
      elapse_idle(game, chunk);
      game->step_ms_left -= chunk;
      if (game->time_limit_ms != 0u) game->time_left_ms -= chunk;
      elapsed_ms -= chunk;
      if (game->step_ms_left == 0u) {
        tick(game);
        game->step_ms_left = game_step_interval(game);
        changed = true;
      }
      if (game->time_limit_ms != 0u && game->time_left_ms == 0u && game->status != GAME_OVER) {
        end_time(game);
        changed = true;
      }
    }
  }
  return changed;
}

bool game_step(Game *game) {
  if (game->status == GAME_RECOVERING) return game_advance(game, game->recovery_ms_left);
  if (game->status == GAME_PLAYING) return game_advance(game, game_next_boundary_ms(game));
  return false;
}
