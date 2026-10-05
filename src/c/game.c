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
  uint32_t start = game->mode == GAME_A ? HC_A_START_STEP_MS : HC_B_START_STEP_MS;
  uint32_t floor = game->mode == GAME_A ? HC_A_MIN_STEP_MS : HC_B_MIN_STEP_MS;
  uint32_t levels = game->score / HC_SPEED_POINTS;
  if (levels >= (start - floor + HC_SPEED_DECREMENT_MS - 1u) / HC_SPEED_DECREMENT_MS) {
    return floor;
  }
  return start - levels * HC_SPEED_DECREMENT_MS;
}

uint16_t game_display_score(const Game *game) {
  return (uint16_t)(game->score % 1000u);
}

uint8_t game_cargo_limit(const Game *game) {
  if (game->mode == GAME_A) {
    if (game->score < HC_A_FIRST_CARGO_THRESHOLD) return HC_A_INITIAL_CARGO_LIMIT;
    if (game->score < HC_A_SECOND_CARGO_THRESHOLD) return HC_A_MIDDLE_CARGO_LIMIT;
    return HC_A_FINAL_CARGO_LIMIT;
  }
  return game->score < HC_B_CARGO_THRESHOLD ? HC_B_INITIAL_CARGO_LIMIT : HC_B_FINAL_CARGO_LIMIT;
}

static uint32_t next_idle(Game *game, unsigned side) {
  uint32_t lo = game->mode == GAME_A ? HC_A_IDLE_MIN_MS : HC_B_IDLE_MIN_MS;
  uint32_t hi = game->mode == GAME_A ? HC_A_IDLE_MAX_MS : HC_B_IDLE_MAX_MS;
  return lo + random_next(&game->attacks[side].rng) % (hi - lo + 1u);
}

void game_init(Game *game, GameMode mode, uint32_t seed) {
  memset(game, 0, sizeof(*game));
  game_start(game, mode, seed);
}

void game_start(Game *game, GameMode mode, uint32_t seed) {
  uint32_t highs[2] = { game->high_scores[0], game->high_scores[1] };
  bool swapped = game->controls_swapped;
  unsigned i;
  memset(game, 0, sizeof(*game));
  game->high_scores[0] = highs[0];
  game->high_scores[1] = highs[1];
  game->controls_swapped = swapped;
  game->mode = mode == GAME_B ? GAME_B : GAME_A;
  game->status = GAME_PLAYING;
  game->resume_status = GAME_PLAYING;
  game->finn_pose = 2u;
  game->rng = seed_nonzero(seed);
  game->next_cargo_id = 1u;
  game->catch_pose = -1;
  game->splash_lane = -1;
  game->mae_target = (uint8_t)(random_next(&game->rng) % GAME_LANES);
  for (i = 0u; i < GAME_SIDES; ++i) {
    game->attacks[i].rng = seed_nonzero(seed ^ (i == 0u ? UINT32_C(0xa341316c) : UINT32_C(0xc8013ea4)));
    game->attacks[i].idle_ms_left = next_idle(game, i);
  }
  game->step_ms_left = game_step_interval(game);
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
  if (game->status != GAME_PLAYING) return false;
  direction = button == GAME_UP ? -1 : 1;
  if (game->controls_swapped) direction = -direction;
  old_pose = game->finn_pose;
  if (direction < 0 && game->finn_pose > 0u) --game->finn_pose;
  if (direction > 0 && game->finn_pose + 1u < GAME_POSES) ++game->finn_pose;
  return game->finn_pose != old_pose;
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

uint32_t game_take_events(Game *game) {
  uint32_t events = game->events;
  game->events = 0u;
  return events;
}

static bool attack_strike_time(const Game *game, unsigned side, uint64_t *when) {
  const GameAttack *attack = &game->attacks[side];
  if (game->mode == GAME_A && side == 1u) return false;
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

static bool strike_conflicts_cargo(const Game *game, unsigned side, uint64_t strike) {
  unsigned i;
  uint8_t lane = side == 0u ? 0u : 3u;
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    const GameCargo *cargo = &game->cargo[i];
    if (cargo->active && cargo->lane == lane &&
        (cargo->landing_step == strike || cargo->landing_step + 1u == strike)) return true;
  }
  return false;
}

/* Attack commitment must obey the same reservations as cargo commitment. This
 * check is essential: looking only at attacks while throwing cannot protect
 * cargo already in flight when an idle timer expires. Each pier's elapsed-ms
 * timer and random stream remain independent; only unsafe starts are deferred.
 */
static bool can_windup(const Game *game, unsigned side) {
  uint64_t strike = game->step + HC_WINDUP_STEPS;
  uint64_t other_strike;
  if (strike_conflicts_cargo(game, side, strike)) return false;
  if (attack_strike_time(game, 1u - side, &other_strike) && other_strike == strike) return false;
  return true;
}

static void update_attacks(Game *game) {
  unsigned side;
  unsigned sides = game->mode == GAME_A ? 1u : 2u;
  /* Advance both existing phases before considering new commitments, so side
   * iteration cannot turn one side's old phase into a false collision. */
  for (side = 0u; side < sides; ++side) {
    GameAttack *attack = &game->attacks[side];
    if (attack->phase == GAME_ATTACK_WINDUP) {
      --attack->steps_left;
      if (attack->steps_left == 0u) {
        attack->phase = GAME_ATTACK_STRIKE;
        attack->steps_left = HC_STRIKE_STEPS;
      }
    } else if (attack->phase == GAME_ATTACK_STRIKE) {
      if (--attack->steps_left == 0u) {
        attack->phase = GAME_ATTACK_IDLE;
        attack->idle_ms_left = next_idle(game, side);
      }
    }
  }
  for (side = 0u; side < sides; ++side) {
    GameAttack *attack = &game->attacks[side];
    if (attack->phase == GAME_ATTACK_IDLE && attack->idle_ms_left == 0u && can_windup(game, side)) {
      attack->phase = GAME_ATTACK_WINDUP;
      attack->steps_left = HC_WINDUP_STEPS;
    }
  }
}

static void miss(Game *game, GameMissCause cause) {
  unsigned i;
  ++game->misses;
  ++game->total_misses;
  game->lucky_tide = false;
  game->miss_cause = cause;
  game->events |= GAME_EVENT_MISS;
  for (i = 0u; i < GAME_MAX_CARGO; ++i) game->cargo[i].active = false;
  if (game->misses >= GAME_MAX_MISSES) {
    game->status = GAME_OVER;
    game->events |= GAME_EVENT_OVER;
  } else {
    game->status = GAME_RECOVERING;
    game->recovery_ms_left = HC_MISS_RECOVERY_MS;
  }
}

static void caught(Game *game, uint8_t lane) {
  uint32_t before = game->score;
  uint32_t points = game->lucky_tide ? 2u : 1u;
  uint64_t after = (uint64_t)before + points;
  uint64_t block = (uint64_t)before / 1000u * 1000u;
  uint64_t milestones[2] = { block + 200u, block + 500u };
  unsigned i;
  game->score = after > UINT32_MAX ? UINT32_MAX : (uint32_t)after;
  ++game->catches;
  game->catch_pose = (int8_t)game_lane_pose(lane);
  game->events |= GAME_EVENT_CATCH;
  for (i = 0u; i < 2u; ++i) {
    if (before < milestones[i] && game->score >= milestones[i]) {
      if (game->misses != 0u) {
        game->misses = 0u;
        game->half_ring = false;
      } else {
        game->lucky_tide = true;
        game->events |= GAME_EVENT_LUCKY_TIDE;
      }
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
  game->lucky_tide = false;
  game->events |= GAME_EVENT_DROP;
  if (game->half_ring) {
    game->half_ring = false;
    miss(game, GAME_MISS_DROP);
  } else {
    game->half_ring = true;
  }
}

/* Fairness certificate: each commitment compares with EVERY live reservation,
 * using distance in Finn's five poses (not four lanes). Launch+4 grants the
 * initial centre pose enough time to reach any lane. Consecutive reservations
 * therefore form a feasible one-move-per-step route; the all-pairs test also
 * rejects nonconsecutive incompatibilities. Misses clear the route. A strike
 * prevents far cargo landing on its strike tick or the tick before the strike,
 * whether the attack or cargo was committed first. Between two far catches on the same side,
 * an intervening strike needs a single move out and back; the excluded previous
 * tick supplies this time. Travel between different lanes never needs a far
 * pose except at its endpoint. Wind-ups always provide two visible intervals.
 * Mae takes at most three walking ticks; capacity/spacing can only delay a
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
  for (i = 0u; i < GAME_SIDES; ++i) {
    uint64_t strike;
    uint8_t far_lane = i == 0u ? 0u : 3u;
    if (lane == far_lane && attack_strike_time(game, i, &strike) &&
        (landing == strike || landing + 1u == strike)) return false;
  }
  return true;
}

static void update_mae(Game *game) {
  unsigned i;
  if (game->mae_lane != game->mae_target) {
    if (game->mae_lane < game->mae_target) ++game->mae_lane;
    else --game->mae_lane;
    game->mae_ready = game->mae_lane == game->mae_target;
    return; /* The arrival must be visible before a throw. */
  }
  if (!game->mae_ready) {
    game->mae_ready = true;
    return;
  }
  if (!can_launch(game, game->mae_lane)) return;
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    GameCargo *cargo = &game->cargo[i];
    if (cargo->active) continue;
    cargo->active = true;
    cargo->lane = game->mae_lane;
    cargo->stage = 0u;
    cargo->id = game->next_cargo_id++;
    cargo->launch_step = game->step;
    cargo->landing_step = game->step + GAME_CARGO_STEPS - 1u;
    game->mae_throwing = true;
    game->mae_target = (uint8_t)(random_next(&game->rng) % GAME_LANES);
    /* Already visible at this spot: consecutive throws are permitted here.
     * A different spot still requires a visible walking/arrival tick. */
    game->mae_ready = game->mae_target == game->mae_lane;
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
  game->mae_throwing = false;
  update_attacks(game);
  for (i = 0u; i < GAME_SIDES; ++i) {
    if (game->attacks[i].phase == GAME_ATTACK_STRIKE &&
        game->finn_pose == (i == 0u ? 0u : 4u)) {
      ++game->hits;
      miss(game, GAME_MISS_HIT);
      return;
    }
  }
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    GameCargo *cargo = &game->cargo[i];
    if (!cargo->active) continue;
    ++cargo->stage;
    if (cargo->stage == GAME_CARGO_STEPS - 1u) {
      uint8_t lane = cargo->lane;
      cargo->active = false;
      if (game->finn_pose == game_lane_pose(lane)) caught(game, lane);
      else dropped(game, lane);
      if (game->status != GAME_PLAYING) return;
    }
  }
  update_mae(game);
}

static void elapse_idle(Game *game, uint32_t elapsed) {
  unsigned side;
  unsigned sides = game->mode == GAME_A ? 1u : 2u;
  for (side = 0u; side < sides; ++side) {
    GameAttack *attack = &game->attacks[side];
    if (attack->phase == GAME_ATTACK_IDLE) {
      attack->idle_ms_left = elapsed >= attack->idle_ms_left ? 0u : attack->idle_ms_left - elapsed;
    }
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
      chunk = elapsed_ms < game->step_ms_left ? elapsed_ms : game->step_ms_left;
      elapse_idle(game, chunk);
      game->step_ms_left -= chunk;
      elapsed_ms -= chunk;
      if (game->step_ms_left == 0u) {
        tick(game);
        game->step_ms_left = game_step_interval(game);
        changed = true;
      }
    }
  }
  return changed;
}

bool game_step(Game *game) {
  if (game->status == GAME_RECOVERING) return game_advance(game, game->recovery_ms_left);
  if (game->status == GAME_PLAYING) return game_advance(game, game->step_ms_left);
  return false;
}
