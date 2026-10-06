#include "game.h"
#include "tuning.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void press(Game *game, GameButton button) {
  (void)game_input(game, button, true);
  (void)game_input(game, button, false);
}

static Game fixture(GameMode mode) {
  Game game;
  game_init(&game, mode, 1u);
  game.step = 10u;
  game.olive_target = 0u;
  game.olive_ready = false;
  game.attack.idle_ms_left = UINT32_MAX;
  return game;
}

static void cargo_at(Game *game, unsigned slot, uint8_t lane, uint8_t stage) {
  GameCargo *cargo = &game->cargo[slot];
  cargo->active = true;
  cargo->lane = lane;
  cargo->stage = stage;
  cargo->id = game->next_cargo_id++;
  cargo->launch_step = game->step - stage;
  cargo->landing_step = game->step + GAME_CARGO_STEPS - 1u - stage;
}

static void catch_next(Game *game, uint8_t lane) {
  memset(game->cargo, 0, sizeof(game->cargo));
  cargo_at(game, 0u, lane, 3u);
  game->popeye_pose = game_lane_pose(lane);
  assert(game_step(game));
  assert(game->status == GAME_PLAYING);
}

static void drop_next(Game *game) {
  memset(game->cargo, 0, sizeof(game->cargo));
  cargo_at(game, 0u, 0u, 3u);
  game->popeye_pose = 2u;
  assert(game_step(game));
}

static void test_inputs(void) {
  Game game = fixture(GAME_A);
  assert(game.popeye_pose == 2u);
  assert(game_input(&game, GAME_UP, true));
  assert(game.popeye_pose == 1u && game.step == 10u);
  assert(!game_input(&game, GAME_UP, true));
  assert(game_step(&game));
  assert(game.popeye_pose == 1u); /* A held button cannot repeat on a tick. */
  (void)game_input(&game, GAME_UP, false);
  press(&game, GAME_UP);
  assert(game.popeye_pose == 0u);
  press(&game, GAME_UP);
  assert(game.popeye_pose == 0u);
  press(&game, GAME_DOWN);
  press(&game, GAME_DOWN);
  press(&game, GAME_DOWN);
  press(&game, GAME_DOWN);
  press(&game, GAME_DOWN);
  assert(game.popeye_pose == 4u);
  game.controls_swapped = true;
  press(&game, GAME_DOWN);
  assert(game.popeye_pose == 3u);
  press(&game, GAME_UP);
  assert(game.popeye_pose == 4u);
  assert(!game_input(&game, (GameButton)99, true));
  assert(game_pause(&game));
  press(&game, GAME_DOWN);
  assert(game.popeye_pose == 4u);
  assert(game_resume(&game));
}

static void test_cargo_and_feedback(void) {
  Game game = fixture(GAME_A);
  unsigned stage;
  cargo_at(&game, 0u, 1u, 0u);
  game.popeye_pose = 1u;
  for (stage = 1u; stage < 4u; ++stage) {
    assert(game_step(&game));
    assert(game.cargo[0].stage == stage && game.cargo[0].active);
    assert(game.score == 0u);
  }
  assert(game_step(&game));
  assert(game.score == 1u && game.catches == 1u);
  assert(game.catch_pose == 1);
  assert((game_take_events(&game) & GAME_EVENT_CATCH) != 0u);
  assert(game_take_events(&game) == 0u);
  /* Olive can immediately reuse the resolved slot. Catch feedback survives it. */
  game = fixture(GAME_A);
  game.olive_ready = true;
  cargo_at(&game, 0u, 0u, 3u);
  game.popeye_pose = 0u;
  assert(game_step(&game));
  assert(game.cargo[0].active && game.cargo[0].stage == 0u);
  assert(game.cargo[0].launch_step == game.step);
  assert(game.cargo[0].landing_step == game.step + 4u);
  assert(game.catch_pose == 0 && game.catches == 1u && game.olive_throwing);
  /* Upright centre is not one of the four catch poses. */
  game = fixture(GAME_A);
  drop_next(&game);
  assert(game.score == 0u && game.drops == 1u && game.half_ring);
  assert(game.misses == 0u && game.splash_lane == 0);
}

static void test_drops_misses_and_recovery(void) {
  Game game = fixture(GAME_A);
  unsigned i;
  drop_next(&game);
  assert(game.half_ring && game.misses == 0u);
  memset(game.cargo, 0, sizeof(game.cargo));
  cargo_at(&game, 0u, 0u, 3u);
  cargo_at(&game, 1u, 2u, 0u);
  game.popeye_pose = 2u;
  assert(game_step(&game));
  assert(!game.half_ring && game.misses == 1u);
  assert(game.status == GAME_RECOVERING && game.miss_cause == GAME_MISS_DROP);
  assert(game.recovery_ms_left == 1500u);
  for (i = 0u; i < GAME_MAX_CARGO; ++i) assert(!game.cargo[i].active);
  assert(!game_input(&game, GAME_UP, true));
  assert(game.popeye_pose == 2u);
  (void)game_input(&game, GAME_UP, false);
  assert(!game_advance(&game, 1499u));
  assert(game.status == GAME_RECOVERING && game.recovery_ms_left == 1u);
  assert(game_advance(&game, 1u));
  assert(game.status == GAME_PLAYING && game.step_ms_left == 560u);
  for (i = 0u; i < 2u; ++i) {
    drop_next(&game);
    assert(game.half_ring);
    drop_next(&game);
    if (i == 0u) {
      assert(game.misses == 2u && game.status == GAME_RECOVERING);
      assert(game_step(&game));
    }
  }
  assert(game.status == GAME_OVER && game.misses == 3u && game.total_misses == 3u);
  assert(game.drops == 6u && !game.half_ring);
  assert((game.events & GAME_EVENT_OVER) != 0u);
  {
    Game ended = game;
    assert(!game_advance(&game, UINT32_MAX));
    assert(!game_pause(&game) && !game_resume(&game));
    assert(memcmp(&game, &ended, sizeof(game)) == 0);
  }
}

static void test_attacks(void) {
  Game game = fixture(GAME_A);
  unsigned pose, mode, side;
  game.attack.idle_ms_left = 561u;
  assert(game_step(&game));
  assert(game.attack.phase == GAME_ATTACK_IDLE && game.attack.idle_ms_left == 1u);
  assert(!game_advance(&game, 1u));
  assert(game.attack.phase == GAME_ATTACK_IDLE);
  assert(game_step(&game));
  assert(game.attack.phase == GAME_ATTACK_WINDUP && game.attack.steps_left == 2u);
  assert(game_step(&game));
  assert(game.attack.phase == GAME_ATTACK_WINDUP && game.attack.steps_left == 1u);
  assert(game_step(&game));
  assert(game.attack.phase == GAME_ATTACK_STRIKE && game.hits == 0u);
  assert(game_step(&game));
  assert(game.attack.phase == GAME_ATTACK_IDLE && game.attack.side == GAME_LEFT);
  assert(game.attack.idle_ms_left >= 4000u && game.attack.idle_ms_left <= 9000u);
  for (mode = 0; mode < 2; ++mode) {
    for (side = 0; side < (mode == GAME_A ? 1u : 2u); ++side) {
      for (pose = 0; pose < GAME_POSES; ++pose) {
        game = fixture((GameMode)mode);
        game.popeye_pose = (uint8_t)pose;
        game.attack.side = (GameSide)side;
        game.attack.phase = GAME_ATTACK_WINDUP;
        game.attack.steps_left = 1u;
        game.half_ring = true;
        assert(game_step(&game));
        if (pose == (side == GAME_LEFT ? 0u : 4u)) {
          assert(game.status == GAME_RECOVERING && game.misses == 1u && game.hits == 1u);
          assert(game.half_ring && game.miss_cause == GAME_MISS_HIT);
        } else assert(game.status == GAME_PLAYING && game.hits == 0u);
      }
    }
  }
  /* Hit resolves before an unrelated drop, giving exactly one miss. */
  game = fixture(GAME_B);
  game.popeye_pose = 0u;
  cargo_at(&game, 0u, 3u, 3u);
  game.attack.phase = GAME_ATTACK_WINDUP;
  game.attack.steps_left = 1u;
  assert(game_step(&game));
  assert(game.hits == 1u && game.drops == 0u && game.total_misses == 1u);
  /* One Brutus switches sides only after a completed strike in B. */
  game = fixture(GAME_B);
  game.attack.idle_ms_left = 0u;
  assert(game_step(&game) && game.attack.side == GAME_LEFT);
  assert(game_step(&game) && game.attack.side == GAME_LEFT);
  assert(game_step(&game) && game.attack.phase == GAME_ATTACK_STRIKE);
  assert(game_step(&game) && game.attack.side == GAME_RIGHT);
  assert(game.attack.phase == GAME_ATTACK_IDLE);
  memset(game.cargo, 0, sizeof(game.cargo));
  game.attack.phase = GAME_ATTACK_STRIKE;
  game.attack.steps_left = 1u;
  assert(game_step(&game) && game.attack.side == GAME_LEFT);
}

static void test_reservations_and_olive(void) {
  Game game;
  unsigned stage;
  /* Existing cargo vetoes a new wind-up for BOTH forbidden landing ticks. */
  for (stage = 1u; stage <= 2u; ++stage) {
    game = fixture(GAME_A);
    cargo_at(&game, 0u, 0u, (uint8_t)stage);
    game.attack.idle_ms_left = 0u;
    assert(game_step(&game));
    assert(game.attack.phase == GAME_ATTACK_IDLE);
  }
  game = fixture(GAME_B);
  cargo_at(&game, 0u, 3u, 1u);
  game.attack.side = GAME_RIGHT;
  game.attack.idle_ms_left = 0u;
  assert(game_step(&game));
  assert(game.attack.phase == GAME_ATTACK_IDLE);
  /* Left near -> right near crosses centre, so requires TWO ticks. */
  game = fixture(GAME_B);
  game.score = 60u;
  game.olive_target = 2u;
  game.olive_ready = true;
  cargo_at(&game, 0u, 1u, 0u); /* after next tick, landing separation is one */
  assert(game_step(&game));
  assert(!game.olive_throwing);
  assert(game_step(&game));
  assert(game.olive_throwing); /* separation two now permits it */
  /* Every reservation is inspected, even nonconsecutive injected entries. This
   * adversarial fixture is deliberately inconsistent before the new proposal. */
  game = fixture(GAME_B);
  game.score = 60u;
  game.olive_target = 3u;
  game.olive_ready = true;
  cargo_at(&game, 0u, 0u, 0u); /* future separation one: far-left forbids far-right */
  cargo_at(&game, 1u, 2u, 0u); /* same proposal is reachable from near-right */
  assert(game_step(&game));
  assert(!game.olive_throwing);
  /* A changed food arc gets one ready frame at Olive's fixed ledge. */
  game = fixture(GAME_A);
  game.olive_target = 3u;
  assert(game_step(&game) && game.olive_ready && !game.olive_throwing);
  assert(game_step(&game) && game.olive_throwing);
  assert(game.cargo[0].lane == 3u);
  assert(game.cargo[0].stage == 0u && game.cargo[0].launch_step == game.step);
  /* The third-cargo ramp is attainable: left, near-left, near-left gives
   * launch gaps 2 then 1, and all pairwise pose distances remain feasible. */
  game = fixture(GAME_A);
  game.score = 60u;
  game.olive_ready = true;
  assert(game_step(&game) && game.olive_throwing);
  assert(game_step(&game) && !game.olive_throwing);
  assert(game_step(&game) && game.olive_throwing);
  assert(game_step(&game) && game.olive_throwing);
  assert(game.cargo[0].active && game.cargo[1].active && game.cargo[2].active);
  assert(game.cargo[0].lane == 0u && game.cargo[1].lane == 1u && game.cargo[2].lane == 1u);
  /* Initial far-right Popeye can reach a fresh far-left launch in four presses. */
  game = fixture(GAME_A);
  game.popeye_pose = 4u;
  game.olive_ready = true;
  assert(game_step(&game) && game.olive_throwing);
  for (stage = 0u; stage < 4u; ++stage) {
    press(&game, GAME_UP);
    assert(game_step(&game));
  }
  assert(game.catches == 1u && game.drops == 0u);
}

static void test_scoring(void) {
  Game game;
  uint32_t boundaries[] = { 199u, 499u, 1199u, 1499u, 2199u, 2499u };
  unsigned i, misses;
  for (i = 0u; i < sizeof(boundaries) / sizeof(boundaries[0]); ++i) {
    for (misses = 0u; misses < 3u; ++misses) {
      game = fixture(GAME_A);
      game.score = boundaries[i];
      game.misses = (uint8_t)misses;
      game.half_ring = true;
      catch_next(&game, 1u);
      assert(game.score == boundaries[i] + 1u && game.misses == 0u && !game.half_ring);
      assert((game_take_events(&game) & GAME_EVENT_BONUS) != 0u);
      catch_next(&game, 1u);
      assert(game.score == boundaries[i] + 2u); /* No invented double-point bonus. */
    }
  }
  game = fixture(GAME_A);
  game.half_ring = true;
  catch_next(&game, 1u);
  assert(game.half_ring); /* Pending-drop handling remains provisional. */
  drop_next(&game);
  assert(game.misses == 1u && !game.half_ring);
  game = fixture(GAME_A);
  game.score = 999u;
  catch_next(&game, 1u);
  assert(game.score == 1000u && game_display_score(&game) == 0u);
  assert(game.high_scores[GAME_A] == 1000u && game_step_interval(&game) == 560u);
  game.controls_swapped = true;
  game_start(&game, GAME_B, 3u);
  assert(game.score == 0u && game.controls_swapped);
  assert(game.high_scores[GAME_A] == 1000u && game.high_scores[GAME_B] == 0u);
  game.step = 10u;
  game.score = UINT32_MAX - 1u;
  game.attack.idle_ms_left = UINT32_MAX;
  catch_next(&game, 1u);
  assert(game.score == UINT32_MAX && game.high_scores[GAME_B] == UINT32_MAX);
  catch_next(&game, 1u);
  assert(game.score == UINT32_MAX);
}

static void test_difficulty(void) {
  unsigned mode, cycle;
  for (mode = 0; mode < 2; ++mode) {
    for (cycle = 0; cycle <= 1000; cycle += 100) {
      Game game = fixture((GameMode)mode);
      game.score = cycle;
      assert(game_step_interval(&game) == 560u && game_cargo_limit(&game) == 1u);
      game.score = cycle + 9u;
      assert(game_cargo_limit(&game) == 1u);
      game.score = cycle + 10u;
      assert(game_cargo_limit(&game) == 2u);
      game.score = cycle + 25u;
      assert(game_step_interval(&game) == 440u);
      game.score = cycle + 59u;
      assert(game_cargo_limit(&game) == 2u && game_step_interval(&game) == 340u);
      game.score = cycle + 60u;
      assert(game_cargo_limit(&game) == 3u);
      game.score = cycle + 99u;
      assert(game_step_interval(&game) == 240u);
    }
  }
}

static void test_game_over_movement(void) {
  Game game = fixture(GAME_A), before;
  game.status = GAME_OVER;
  game.misses = 3u;
  game.miss_cause = GAME_MISS_HIT;
  before = game;
  press(&game, GAME_UP);
  assert(game.popeye_pose == 1u && game.miss_cause == GAME_MISS_NONE);
  press(&game, GAME_DOWN);
  assert(game.popeye_pose == 2u && game.status == GAME_OVER);
  assert(!game_step(&game) && !game_advance(&game, 10000u));
  assert(game.score == before.score && game.misses == before.misses && game.step == before.step);
  assert(game.attack.idle_ms_left == before.attack.idle_ms_left);
}

static void test_pause_and_elapsed(void) {
  Game game = fixture(GAME_A);
  Game paused;
  uint32_t idle_before;
  assert(!game_advance(&game, 250u));
  assert(game.step_ms_left == 310u);
  assert(game_pause(&game));
  paused = game;
  assert(!game_advance(&game, UINT32_MAX) && !game_step(&game));
  assert(memcmp(&game, &paused, sizeof(game)) == 0);
  assert(!game_pause(&game) && game_resume(&game) && !game_resume(&game));
  assert(!game_advance(&game, 309u) && game.step == 10u);
  assert(game_advance(&game, 1u) && game.step == 11u);
  drop_next(&game);
  drop_next(&game);
  idle_before = game.attack.idle_ms_left;
  assert(!game_advance(&game, 700u) && game.recovery_ms_left == 800u);
  assert(game.attack.idle_ms_left == idle_before);
  assert(game_pause(&game));
  paused = game;
  assert(!game_advance(&game, 2000u));
  assert(memcmp(&game, &paused, sizeof(game)) == 0);
  assert(game_resume(&game) && game.status == GAME_RECOVERING);
  assert(game_advance(&game, 900u));
  assert(game.status == GAME_PLAYING && game.step_ms_left == 460u);
  assert(game.attack.idle_ms_left == idle_before - 100u);
}

static void test_determinism(void) {
  Game a, b;
  unsigned i;
  game_init(&a, GAME_B, 0u);
  game_init(&b, GAME_B, 0u);
  /* Explicit xorshift reference vector, not just comparing two opaque calls. */
  assert(a.rng == UINT32_C(0x40aec71f));
  assert(a.olive_target == 3u);
  for (i = 0u; i < 1000u; ++i) {
    if (i % 3u == 0u) { press(&a, GAME_UP); press(&b, GAME_UP); }
    if (i % 5u == 0u) { press(&a, GAME_DOWN); press(&b, GAME_DOWN); }
    (void)game_advance(&a, 137u);
    (void)game_advance(&b, 137u);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
  }
  game_init(&a, GAME_B, 123u);
  game_init(&b, GAME_B, 123u);
  (void)game_advance(&a, 12345u);
  for (i = 0u; i < 123u; ++i) (void)game_advance(&b, 100u);
  (void)game_advance(&b, 45u);
  assert(memcmp(&a, &b, sizeof(a)) == 0);
  game_init(&b, GAME_B, 124u);
  assert(a.rng != b.rng);
}

/* Independent player: build a tiny time-expanded graph from visible segments
 * and visible wind-ups. Every graph edge is stay or ONE button press; node
 * constraints are catch deadlines and actual strike poses. Backward dynamic
 * programming chooses a viable first edge. It does not call engine scheduler
 * logic, replay engine state, inspect idle timers, or predict unseen throws.
 * Cargo time is derived from its visible segment, not its reservation field.
 */
static unsigned bot_choose_pose(const Game *game) {
  bool viable[6][GAME_POSES] = {{ false }};
  bool blocked[6][GAME_POSES] = {{ false }};
  int deadline[6] = { -1, -1, -1, -1, -1, -1 };
  unsigned horizon = 1u, i, pose;
  int time;
  unsigned preference = 2u;
  unsigned first_due = 6u;
  static const unsigned lane_pose[4] = { 0u, 1u, 3u, 4u };
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    const GameCargo *cargo = &game->cargo[i];
    unsigned due;
    if (!cargo->active) continue;
    due = 4u - cargo->stage;
    assert(due > 0u && due <= 4u);
    assert(deadline[due] == -1 || deadline[due] == (int)lane_pose[cargo->lane]);
    deadline[due] = (int)lane_pose[cargo->lane];
    if (due > horizon) horizon = due;
    if (due < first_due) { first_due = due; preference = lane_pose[cargo->lane]; }
  }
  if (game->attack.phase == GAME_ATTACK_WINDUP) {
    unsigned due = game->attack.steps_left;
    assert(due == 1u || due == 2u);
    blocked[due][game->attack.side == GAME_LEFT ? 0u : 4u] = true;
    if (due > horizon) horizon = due;
  }
  for (time = (int)horizon; time >= 0; --time) {
    for (pose = 0u; pose < GAME_POSES; ++pose) {
      unsigned next;
      if (blocked[time][pose] || (deadline[time] >= 0 && deadline[time] != (int)pose)) continue;
      if ((unsigned)time == horizon) { viable[time][pose] = true; continue; }
      for (next = pose == 0u ? 0u : pose - 1u;
           next < GAME_POSES && next <= pose + 1u; ++next) {
        if (viable[time + 1][next]) { viable[time][pose] = true; break; }
      }
    }
  }
  assert(viable[0][game->popeye_pose]);
  {
    unsigned best = GAME_POSES, best_distance = GAME_POSES;
    for (pose = game->popeye_pose == 0u ? 0u : game->popeye_pose - 1u;
         pose < GAME_POSES && pose <= game->popeye_pose + 1u; ++pose) {
      unsigned distance = pose > preference ? pose - preference : preference - pose;
      if (viable[1][pose] && distance < best_distance) { best = pose; best_distance = distance; }
    }
    assert(best < GAME_POSES);
    return best;
  }
}

static void test_fairness(void) {
  unsigned mode, seed;
  uint64_t total_steps = 0u;
  uint64_t total_catches = 0u;
  uint64_t total_strikes[2] = { 0u, 0u };
  unsigned observed_max[2][3] = {{ 0u }};
  static const unsigned poses[4] = { 0u, 1u, 3u, 4u };
  for (mode = 0u; mode < 2u; ++mode) {
    for (seed = 0u; seed < 10000u; ++seed) {
      Game game;
      uint64_t lane_last_reservation[4] = { 0u, 0u, 0u, 0u };
      uint64_t lane_last_landing[4] = { 0u, 0u, 0u, 0u };
      unsigned lanes_seen = 0u;
      unsigned strikes[2] = { 0u, 0u };
      unsigned warning_ticks = 0u;
      uint64_t last_catch_step = 0u;
      game_init(&game, (GameMode)mode, seed);
      while (game.score < 1000u) {
        Game before = game;
        unsigned destination = bot_choose_pose(&game);
        unsigned i, j;
        assert(game.step < 16000u && game.step - last_catch_step < 32u);
        if (destination < game.popeye_pose) press(&game, GAME_UP);
        else if (destination > game.popeye_pose) press(&game, GAME_DOWN);
        assert(game.popeye_pose == destination);
        assert(game_step(&game));
        assert(game.status == GAME_PLAYING);
        assert(game.drops == 0u && game.hits == 0u && game.total_misses == 0u);
        assert(game.misses == 0u && !game.half_ring);
        if (game.catches != before.catches) last_catch_step = game.step;
        for (i = 0u; i < GAME_MAX_CARGO; ++i) {
          if (before.cargo[i].active && before.cargo[i].stage == 3u) {
            lane_last_landing[before.cargo[i].lane] = game.step;
          }
          if (game.cargo[i].active) {
            const GameCargo *cargo = &game.cargo[i];
            assert(cargo->landing_step == game.step + 4u - cargo->stage);
            if (cargo->launch_step == game.step) {
              uint8_t lane = cargo->lane;
              assert(cargo->stage == 0u && game.olive_throwing);
              assert(before.olive_target == lane && before.olive_ready);
              lanes_seen |= 1u << lane;
              for (j = 0u; j < 4u; ++j) {
                unsigned distance = poses[lane] > poses[j] ? poses[lane] - poses[j] : poses[j] - poses[lane];
                if (lane_last_reservation[j] != 0u) {
                  assert(cargo->landing_step >= lane_last_reservation[j]);
                  assert(cargo->landing_step - lane_last_reservation[j] >= distance);
                }
              }
              lane_last_reservation[lane] = cargo->landing_step;
            } else {
              bool prior_found = false;
              for (j = 0u; j < GAME_MAX_CARGO; ++j) {
                if (before.cargo[j].active && before.cargo[j].id == cargo->id) {
                  assert(cargo->stage == before.cargo[j].stage + 1u);
                  prior_found = true;
                }
              }
              assert(prior_found);
            }
            for (j = i + 1u; j < GAME_MAX_CARGO; ++j) {
              const GameCargo *other = &game.cargo[j];
              uint64_t delta;
              unsigned distance;
              if (!other->active) continue;
              delta = cargo->landing_step > other->landing_step ? cargo->landing_step - other->landing_step : other->landing_step - cargo->landing_step;
              distance = poses[cargo->lane] > poses[other->lane] ? poses[cargo->lane] - poses[other->lane] : poses[other->lane] - poses[cargo->lane];
              assert(delta >= distance);
            }
          }
        }
        {
          unsigned active = 0u;
          for (i = 0u; i < GAME_MAX_CARGO; ++i) if (game.cargo[i].active) ++active;
          unsigned cycle_score = game.score % 100u;
          unsigned band = cycle_score < 10u ? 0u : cycle_score < 60u ? 1u : 2u;
          bool launched = false;
          for (i = 0; i < GAME_MAX_CARGO; ++i)
            if (game.cargo[i].active && game.cargo[i].launch_step == game.step) launched = true;
          /* Already airborne food may finish after the 100-point reset. */
          if (launched) assert(active <= game_cargo_limit(&game));
          if (active <= game_cargo_limit(&game) && active > observed_max[mode][band]) observed_max[mode][band] = active;
        }
        {
          const GameAttack *attack = &game.attack;
          uint8_t far_lane = attack->side == GAME_LEFT ? 0u : 3u;
          if (attack->phase == GAME_ATTACK_WINDUP) {
            uint64_t strike = game.step + attack->steps_left;
            if (before.attack.phase == GAME_ATTACK_IDLE) warning_ticks = 0u;
            ++warning_ticks;
            assert(warning_ticks <= 2u);
            for (j = 0u; j < GAME_MAX_CARGO; ++j) {
              if (game.cargo[j].active && game.cargo[j].lane == far_lane) {
                assert(game.cargo[j].landing_step != strike);
                assert(game.cargo[j].landing_step + 1u != strike);
              }
            }
          } else if (attack->phase == GAME_ATTACK_STRIKE) {
            assert(before.attack.phase == GAME_ATTACK_WINDUP && warning_ticks == 2u);
            assert(lane_last_landing[far_lane] != game.step);
            assert(lane_last_landing[far_lane] + 1u != game.step);
            ++strikes[attack->side];
          }
          if (mode == GAME_A) assert(attack->side == GAME_LEFT);
          if (attack->side != before.attack.side)
            assert(mode == GAME_B && before.attack.phase == GAME_ATTACK_STRIKE && attack->phase == GAME_ATTACK_IDLE);
        }
      }
      assert(lanes_seen == 15u && game.catches >= 500u);
      assert(strikes[0] > 0u);
      if (mode == GAME_B) assert(strikes[1] > 0u);
      else assert(strikes[1] == 0u);
      total_steps += game.step;
      total_catches += game.catches;
      total_strikes[0] += strikes[0];
      total_strikes[1] += strikes[1];
    }
    assert(observed_max[mode][0] == 1u && observed_max[mode][1] == 2u && observed_max[mode][2] == 3u);
    printf("Game %c: 10,000 seeds from zero through >=1,000 true points; zero unavoidable misses; zero drops, hits, misses or stalls\n", mode == GAME_A ? 'A' : 'B');
    fflush(stdout);
  }
  puts("Food ramp coverage: A/B maxima 1/2/3 per 100-point cycle, including airborne food at resets");
  printf("Fairness coverage: %llu steps, %llu catches, %llu left / %llu right strikes; all four lanes every seed\n",
         (unsigned long long)total_steps, (unsigned long long)total_catches,
         (unsigned long long)total_strikes[0], (unsigned long long)total_strikes[1]);
}

/* Sprint / Daily: an active-play limit on Game B rules. */
static uint32_t test_rand(uint32_t *state) {
  uint32_t x = *state;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  return *state = x;
}

/* Imperfect player used by the timing tests: follows the soonest food but errs sometimes. */
static void sloppy_input(Game *game, uint32_t *rng, unsigned error_in) {
  unsigned target = 2u, i;
  uint64_t soonest = UINT64_MAX;
  for (i = 0u; i < GAME_MAX_CARGO; ++i)
    if (game->cargo[i].active && game->cargo[i].landing_step < soonest) {
      soonest = game->cargo[i].landing_step;
      target = game_lane_pose(game->cargo[i].lane);
    }
  if (test_rand(rng) % error_in == 0u) target = test_rand(rng) % GAME_POSES;
  while (game->status == GAME_PLAYING && game->popeye_pose != target) press(game, game->popeye_pose > target ? GAME_UP : GAME_DOWN);
}

/* Plays one timed round with inputs every 100 active-ms mark, advancing in `chunk`-sized
 * calls, pausing now and then. Returns playing and recovery milliseconds consumed. */
static void play_timed(Game *game, GameMode mode, uint32_t seed, uint32_t limit, uint32_t chunk,
                       uint32_t *playing_ms, uint32_t *recovery_ms, bool check_clock) {
  uint32_t rng = seed * 2654435761u + 1u, mark;
  game_init(game, mode, seed);
  game_start_timed(game, mode, seed, limit);
  *playing_ms = *recovery_ms = 0u;
  for (mark = 0u; mark < 1000u && game->status != GAME_OVER; ++mark) {
    uint32_t left = 100u;
    sloppy_input(game, &rng, 3u);
    if (test_rand(&rng) % 7u == 0u) {
      Game frozen;
      assert(game_pause(game));
      frozen = *game;
      assert(!game_advance(game, 5000u) && !game_step(game));
      assert(memcmp(&frozen, game, sizeof(frozen)) == 0); /* Pause neither ticks nor spends time. */
      assert(game_resume(game));
    }
    while (left != 0u && game->status != GAME_OVER) {
      uint32_t ms = left < chunk ? left : chunk, before_left = game->time_left_ms;
      GameStatus before = game->status;
      game_advance(game, ms);
      if (check_clock) {
        assert(ms == 1u);
        if (before == GAME_PLAYING) { ++*playing_ms; assert(game->status == GAME_OVER || game->time_left_ms == before_left - 1u); }
        else { ++*recovery_ms; assert(game->time_left_ms == before_left); } /* Recovery freezes the clock. */
      }
      left -= ms;
    }
  }
}

static void test_time_limit(void) {
  Game a, b, c;
  unsigned seed, timeups = 0u, early = 0u, recoveries = 0u;
  uint32_t consumed = 0u;
  /* Untimed rounds are untouched: no limit fields, no time-up, identical to game_start. */
  game_init(&a, GAME_B, 5u);
  game_init(&b, GAME_B, 5u);
  game_start_timed(&b, GAME_B, 5u, 0u);
  assert(memcmp(&a, &b, sizeof(a)) == 0 && a.time_limit_ms == 0u && !a.time_up);
  (void)game_advance(&a, 10u * 60u * 1000u);
  assert(a.status == GAME_OVER && !a.time_up && !(game_take_events(&a) & GAME_EVENT_TIME_UP));
  assert(game_next_boundary_ms(&a) == 0u);
  /* A perfect player is stopped exactly at the limit, mid-step. */
  game_init(&a, GAME_B, 11u);
  game_start_timed(&a, GAME_B, 11u, 5000u);
  assert(a.time_left_ms == 5000u && game_next_boundary_ms(&a) == game_step_interval(&a));
  while (a.status != GAME_OVER) {
    unsigned destination = bot_choose_pose(&a);
    uint32_t ms;
    if (destination < a.popeye_pose) press(&a, GAME_UP);
    else if (destination > a.popeye_pose) press(&a, GAME_DOWN);
    ms = game_next_boundary_ms(&a);
    assert(ms != 0u && ms <= a.step_ms_left && ms <= a.time_left_ms);
    assert(game_step(&a));
    consumed += ms;
    assert(a.total_misses == 0u);
  }
  assert(consumed == 5000u && a.time_up && a.time_left_ms == 0u && a.misses == 0u);
  assert((game_take_events(&a) & (GAME_EVENT_TIME_UP | GAME_EVENT_OVER)) == (GAME_EVENT_TIME_UP | GAME_EVENT_OVER));
  b = a;
  assert(!game_advance(&a, 60000u) && !game_step(&a) && !game_pause(&a) && memcmp(&a, &b, sizeof(a)) == 0);
  /* The tick landing exactly on the limit resolves before time is called. */
  game_init(&a, GAME_B, 3u);
  game_start_timed(&a, GAME_B, 3u, 2u * game_step_interval(&b));
  assert(game_advance(&a, 2u * game_step_interval(&b) - 1u) && a.status == GAME_PLAYING && a.step == 1u);
  assert(a.time_left_ms == 1u && a.step_ms_left == 1u);
  assert(game_advance(&a, 1u) && a.step == 2u && a.time_up && (game_take_events(&a) & GAME_EVENT_TIME_UP));
  /* Time-up, three-miss end and the event pair, over many sloppy rounds. */
  for (seed = 1u; seed <= 300u; ++seed) {
    uint32_t playing, recovery, p2, r2;
    play_timed(&a, GAME_B, seed, 20000u, 1u, &playing, &recovery, true);
    assert(a.status == GAME_OVER);
    if (a.time_up) {
      ++timeups;
      assert(playing == 20000u && a.time_left_ms == 0u && a.misses < GAME_MAX_MISSES);
      assert((game_take_events(&a) & (GAME_EVENT_TIME_UP | GAME_EVENT_OVER)) == (GAME_EVENT_TIME_UP | GAME_EVENT_OVER));
    } else {
      ++early;
      assert(a.misses >= GAME_MAX_MISSES && playing < 20000u && a.time_left_ms == 20000u - playing);
      assert(!(game_take_events(&a) & GAME_EVENT_TIME_UP));
    }
    if (recovery != 0u) ++recoveries;
    /* Chunked advances land in exactly the same state as one-millisecond ones. */
    play_timed(&b, GAME_B, seed, 20000u, 7u, &p2, &r2, false);
    play_timed(&c, GAME_B, seed, 20000u, 100u, &p2, &r2, false);
    (void)game_take_events(&b); (void)game_take_events(&c);
    assert(memcmp(&a, &b, sizeof(a)) == 0 && memcmp(&a, &c, sizeof(a)) == 0);
  }
  assert(timeups > 5u && early > 5u && recoveries > 50u);
}

static void test_timed_fairness(void) {
  unsigned seed;
  uint64_t steps = 0u, catches = 0u;
  for (seed = 0u; seed < 10000u; ++seed) {
    Game game;
    uint32_t consumed = 0u;
    game_init(&game, GAME_B, seed);
    game_start_timed(&game, GAME_B, seed, PGW_SPRINT_MS);
    while (game.status != GAME_OVER) {
      unsigned destination = bot_choose_pose(&game);
      uint32_t ms;
      if (destination < game.popeye_pose) press(&game, GAME_UP);
      else if (destination > game.popeye_pose) press(&game, GAME_DOWN);
      assert(game.popeye_pose == destination);
      ms = game_next_boundary_ms(&game);
      assert(game_step(&game));
      consumed += ms;
      ++steps;
      assert(game.drops == 0u && game.hits == 0u && game.total_misses == 0u);
      assert(game.misses == 0u && !game.half_ring && consumed <= PGW_SPRINT_MS);
    }
    assert(consumed == PGW_SPRINT_MS && game.time_up && game.time_left_ms == 0u);
    assert(game.catches >= 20u);
    catches += game.catches;
  }
  printf("Timed 60 s: 10,000 seeds through time-up; zero unavoidable misses (%llu steps, %llu catches)\n",
         (unsigned long long)steps, (unsigned long long)catches);
}

int main(void) {
  test_inputs();
  test_cargo_and_feedback();
  test_drops_misses_and_recovery();
  test_attacks();
  test_reservations_and_olive();
  test_scoring();
  test_difficulty();
  test_game_over_movement();
  test_pause_and_elapsed();
  test_determinism();
  test_time_limit();
  puts("Rule tests passed");
  fflush(stdout);
  test_fairness();
  test_timed_fairness();
  puts("All Popeye G&W tests passed");
  return 0;
}
