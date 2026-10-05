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
  unsigned side;
  game_init(&game, mode, 1u);
  game.step = 10u;
  game.mae_lane = 0u;
  game.mae_target = 0u;
  game.mae_ready = false;
  for (side = 0u; side < GAME_SIDES; ++side) game.attacks[side].idle_ms_left = UINT32_MAX;
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
  game->finn_pose = game_lane_pose(lane);
  assert(game_step(game));
  assert(game->status == GAME_PLAYING);
}

static void drop_next(Game *game) {
  memset(game->cargo, 0, sizeof(game->cargo));
  cargo_at(game, 0u, 0u, 3u);
  game->finn_pose = 2u;
  assert(game_step(game));
}

static void test_inputs(void) {
  Game game = fixture(GAME_A);
  assert(game.finn_pose == 2u);
  assert(game_input(&game, GAME_UP, true));
  assert(game.finn_pose == 1u && game.step == 10u);
  assert(!game_input(&game, GAME_UP, true));
  assert(game_step(&game));
  assert(game.finn_pose == 1u); /* A held button cannot repeat on a tick. */
  (void)game_input(&game, GAME_UP, false);
  press(&game, GAME_UP);
  assert(game.finn_pose == 0u);
  press(&game, GAME_UP);
  assert(game.finn_pose == 0u);
  press(&game, GAME_DOWN);
  press(&game, GAME_DOWN);
  press(&game, GAME_DOWN);
  press(&game, GAME_DOWN);
  press(&game, GAME_DOWN);
  assert(game.finn_pose == 4u);
  game.controls_swapped = true;
  press(&game, GAME_DOWN);
  assert(game.finn_pose == 3u);
  press(&game, GAME_UP);
  assert(game.finn_pose == 4u);
  assert(!game_input(&game, (GameButton)99, true));
  assert(game_pause(&game));
  press(&game, GAME_DOWN);
  assert(game.finn_pose == 4u);
  assert(game_resume(&game));
}

static void test_cargo_and_feedback(void) {
  Game game = fixture(GAME_A);
  unsigned stage;
  cargo_at(&game, 0u, 1u, 0u);
  game.finn_pose = 1u;
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
  /* Mae can immediately reuse the resolved slot. Catch feedback survives it. */
  game = fixture(GAME_A);
  game.mae_ready = true;
  cargo_at(&game, 0u, 0u, 3u);
  game.finn_pose = 0u;
  assert(game_step(&game));
  assert(game.cargo[0].active && game.cargo[0].stage == 0u);
  assert(game.cargo[0].launch_step == game.step);
  assert(game.cargo[0].landing_step == game.step + 4u);
  assert(game.catch_pose == 0 && game.catches == 1u && game.mae_throwing);
  /* Upright centre is not one of the four catch poses. */
  game = fixture(GAME_A);
  drop_next(&game);
  assert(game.score == 0u && game.drops == 1u && game.half_ring);
  assert(game.misses == 0u && game.splash_lane == 0);
}

static void test_drops_misses_and_recovery(void) {
  Game game = fixture(GAME_A);
  unsigned i;
  game.lucky_tide = true;
  drop_next(&game);
  assert(!game.lucky_tide && game.half_ring && game.misses == 0u);
  memset(game.cargo, 0, sizeof(game.cargo));
  cargo_at(&game, 0u, 0u, 3u);
  cargo_at(&game, 1u, 2u, 0u);
  game.finn_pose = 2u;
  assert(game_step(&game));
  assert(!game.half_ring && game.misses == 1u);
  assert(game.status == GAME_RECOVERING && game.miss_cause == GAME_MISS_DROP);
  assert(game.recovery_ms_left == 1500u);
  for (i = 0u; i < GAME_MAX_CARGO; ++i) assert(!game.cargo[i].active);
  assert(!game_input(&game, GAME_UP, true));
  assert(game.finn_pose == 2u);
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
    press(&game, GAME_DOWN);
    assert(memcmp(&game, &ended, sizeof(game)) == 0);
  }
}

static void test_attacks(void) {
  Game game = fixture(GAME_A);
  unsigned pose;
  game.attacks[0].idle_ms_left = 561u;
  assert(game_step(&game));
  assert(game.attacks[0].phase == GAME_ATTACK_IDLE);
  assert(game.attacks[0].idle_ms_left == 1u); /* milliseconds, not step counts */
  assert(!game_advance(&game, 1u));
  assert(game.attacks[0].phase == GAME_ATTACK_IDLE);
  assert(game_step(&game));
  assert(game.attacks[0].phase == GAME_ATTACK_WINDUP && game.attacks[0].steps_left == 2u);
  assert(game_step(&game));
  assert(game.attacks[0].phase == GAME_ATTACK_WINDUP && game.attacks[0].steps_left == 1u);
  assert(game_step(&game));
  assert(game.attacks[0].phase == GAME_ATTACK_STRIKE && game.hits == 0u);
  assert(game_step(&game));
  assert(game.attacks[0].phase == GAME_ATTACK_IDLE);
  assert(game.attacks[0].idle_ms_left >= 4000u && game.attacks[0].idle_ms_left <= 9000u);
  assert(game.attacks[1].phase == GAME_ATTACK_IDLE); /* disabled A pier */
  game = fixture(GAME_A);
  game.attacks[1].idle_ms_left = 0u;
  assert(game_step(&game));
  assert(game.attacks[1].phase == GAME_ATTACK_IDLE && game.attacks[1].idle_ms_left == 0u);
  for (pose = 0u; pose < GAME_POSES; ++pose) {
    game = fixture(GAME_B);
    game.mae_target = 2u;
    game.finn_pose = (uint8_t)pose;
    game.attacks[pose == 4u ? 1u : 0u].phase = GAME_ATTACK_WINDUP;
    game.attacks[pose == 4u ? 1u : 0u].steps_left = 1u;
    game.half_ring = true;
    game.lucky_tide = true;
    assert(game_step(&game));
    if (pose == 0u || pose == 4u) {
      assert(game.status == GAME_RECOVERING && game.misses == 1u && game.hits == 1u);
      assert(game.half_ring && !game.lucky_tide && game.miss_cause == GAME_MISS_HIT);
    } else assert(game.status == GAME_PLAYING && game.hits == 0u);
  }
  /* Hit wins over an unrelated cargo drop on the same tick. */
  game = fixture(GAME_B);
  game.finn_pose = 0u;
  cargo_at(&game, 0u, 3u, 3u);
  game.attacks[0].phase = GAME_ATTACK_WINDUP;
  game.attacks[0].steps_left = 1u;
  assert(game_step(&game));
  assert(game.hits == 1u && game.drops == 0u && game.total_misses == 1u);
  /* Two independently expired timers are staggered instead of sharing strikes. */
  game = fixture(GAME_B);
  game.attacks[0].idle_ms_left = 0u;
  game.attacks[1].idle_ms_left = 0u;
  assert(game_step(&game));
  assert(game.attacks[0].phase == GAME_ATTACK_WINDUP);
  assert(game.attacks[1].phase == GAME_ATTACK_IDLE);
  assert(game_step(&game));
  assert(game.attacks[1].phase == GAME_ATTACK_WINDUP);
  assert(game_step(&game));
  assert(game.attacks[0].phase == GAME_ATTACK_STRIKE && game.attacks[1].phase == GAME_ATTACK_WINDUP);
  assert(game_step(&game));
  assert(game.attacks[0].phase == GAME_ATTACK_IDLE && game.attacks[1].phase == GAME_ATTACK_STRIKE);
  assert(game_step(&game));
  assert(game.attacks[1].idle_ms_left >= 3000u && game.attacks[1].idle_ms_left <= 7000u);
}

static void test_reservations_and_mae(void) {
  Game game;
  unsigned stage;
  /* Existing cargo vetoes a new wind-up for BOTH forbidden landing ticks. */
  for (stage = 1u; stage <= 2u; ++stage) {
    game = fixture(GAME_A);
    cargo_at(&game, 0u, 0u, (uint8_t)stage);
    game.attacks[0].idle_ms_left = 0u;
    assert(game_step(&game));
    assert(game.attacks[0].phase == GAME_ATTACK_IDLE);
  }
  game = fixture(GAME_B);
  cargo_at(&game, 0u, 3u, 1u);
  game.attacks[1].idle_ms_left = 0u;
  assert(game_step(&game));
  assert(game.attacks[1].phase == GAME_ATTACK_IDLE);
  /* Left near -> right near crosses centre, so requires TWO ticks. */
  game = fixture(GAME_B);
  game.score = 60u;
  game.mae_lane = 2u;
  game.mae_target = 2u;
  game.mae_ready = true;
  cargo_at(&game, 0u, 1u, 0u); /* after next tick, landing separation is one */
  assert(game_step(&game));
  assert(!game.mae_throwing);
  assert(game_step(&game));
  assert(game.mae_throwing); /* separation two now permits it */
  /* Every reservation is inspected, even nonconsecutive injected entries. This
   * adversarial fixture is deliberately inconsistent before the new proposal. */
  game = fixture(GAME_B);
  game.score = 60u;
  game.mae_lane = 3u;
  game.mae_target = 3u;
  game.mae_ready = true;
  cargo_at(&game, 0u, 0u, 0u); /* future separation one: far-left forbids far-right */
  cargo_at(&game, 1u, 2u, 0u); /* same proposal is reachable from near-right */
  assert(game_step(&game));
  assert(!game.mae_throwing);
  /* Mae walks one spot per tick, shows arrival, and only THEN throws. */
  game = fixture(GAME_A);
  game.mae_target = 3u;
  assert(game_step(&game) && game.mae_lane == 1u && !game.mae_throwing);
  assert(game_step(&game) && game.mae_lane == 2u && !game.mae_throwing);
  assert(game_step(&game) && game.mae_lane == 3u && !game.mae_throwing);
  assert(game_step(&game) && game.mae_lane == 3u && game.mae_throwing);
  assert(game.cargo[0].stage == 0u && game.cargo[0].launch_step == game.step);
  /* The third-cargo ramp is attainable: left, near-left, near-left gives
   * launch gaps 2 then 1, and all pairwise pose distances remain feasible. */
  game = fixture(GAME_A);
  game.score = 60u;
  game.mae_ready = true;
  assert(game_step(&game) && game.mae_throwing);
  assert(game_step(&game) && !game.mae_throwing);
  assert(game_step(&game) && game.mae_throwing);
  assert(game_step(&game) && game.mae_throwing);
  assert(game.cargo[0].active && game.cargo[1].active && game.cargo[2].active);
  assert(game.cargo[0].lane == 0u && game.cargo[1].lane == 1u && game.cargo[2].lane == 1u);
  /* Initial far-right Finn can reach a fresh far-left launch in four presses. */
  game = fixture(GAME_A);
  game.finn_pose = 4u;
  game.mae_ready = true;
  assert(game_step(&game) && game.mae_throwing);
  for (stage = 0u; stage < 4u; ++stage) {
    press(&game, GAME_UP);
    assert(game_step(&game));
  }
  assert(game.catches == 1u && game.drops == 0u);
}

static void test_scoring(void) {
  Game game = fixture(GAME_A);
  uint32_t boundaries[] = { 199u, 499u, 1199u, 1499u, 2199u, 2499u };
  unsigned i;
  game.score = 199u;
  game.misses = 2u;
  game.half_ring = true;
  catch_next(&game, 1u);
  assert(game.score == 200u && game.misses == 0u && !game.half_ring && !game.lucky_tide);
  catch_next(&game, 1u);
  assert(game.score == 201u);
  for (i = 0u; i < sizeof(boundaries) / sizeof(boundaries[0]); ++i) {
    game = fixture(GAME_A);
    game.score = boundaries[i];
    game.lucky_tide = true;
    game.events = 0u;
    catch_next(&game, 1u);
    assert(game.score == boundaries[i] + 2u && game.lucky_tide);
    assert((game.events & GAME_EVENT_LUCKY_TIDE) != 0u);
    game = fixture(GAME_A);
    game.score = boundaries[i];
    game.misses = 1u;
    game.half_ring = true;
    /* A manually injected Lucky+miss state tests +2 crossing while clearing. */
    game.lucky_tide = true;
    catch_next(&game, 1u);
    assert(game.score == boundaries[i] + 2u && game.misses == 0u && !game.half_ring);
  }
  game = fixture(GAME_A);
  game.score = 199u;
  game.half_ring = true;
  catch_next(&game, 1u);
  assert(game.lucky_tide && game.half_ring); /* Only full rings decide milestone. */
  drop_next(&game);
  assert(!game.lucky_tide && !game.half_ring && game.misses == 1u);
  game = fixture(GAME_A);
  game.score = 999u;
  catch_next(&game, 1u);
  assert(game.score == 1000u && game_display_score(&game) == 0u);
  assert(game.high_scores[GAME_A] == 1000u && game_step_interval(&game) == 240u);
  game = fixture(GAME_A);
  game.score = 999u;
  game.lucky_tide = true;
  catch_next(&game, 1u);
  assert(game.score == 1001u && game_display_score(&game) == 1u);
  assert(game.high_scores[GAME_A] == 1001u && game.high_scores[GAME_B] == 0u);
  assert(game_step_interval(&game) == 240u);
  game.controls_swapped = true;
  game_start(&game, GAME_B, 3u);
  assert(game.score == 0u && !game.lucky_tide && game.controls_swapped);
  assert(game.high_scores[GAME_A] == 1001u && game.high_scores[GAME_B] == 0u);
  game.step = 10u;
  game.score = 2000u;
  game.attacks[0].idle_ms_left = UINT32_MAX;
  game.attacks[1].idle_ms_left = UINT32_MAX;
  catch_next(&game, 1u);
  assert(game.high_scores[GAME_B] == 2001u && game.high_scores[GAME_A] == 1001u);
  game.score = UINT32_MAX - 1u;
  game.lucky_tide = true;
  catch_next(&game, 1u);
  assert(game.score == UINT32_MAX && game.high_scores[GAME_B] == UINT32_MAX);
  catch_next(&game, 1u);
  assert(game.score == UINT32_MAX);
}

static void test_difficulty(void) {
  Game game = fixture(GAME_A);
  game.score = 0u;
  assert(game_step_interval(&game) == 560u && game_cargo_limit(&game) == 1u);
  game.score = 9u;
  assert(game_cargo_limit(&game) == 1u);
  game.score = 10u;
  assert(game_cargo_limit(&game) == 2u);
  game.score = 24u;
  assert(game_step_interval(&game) == 560u);
  game.score = 25u;
  assert(game_step_interval(&game) == 544u);
  game.score = 59u;
  assert(game_cargo_limit(&game) == 2u);
  game.score = 60u;
  assert(game_cargo_limit(&game) == 3u);
  game.score = 499u;
  assert(game_step_interval(&game) == 256u);
  game.score = 500u;
  assert(game_step_interval(&game) == 240u);
  game.score = 1000u;
  assert(game_step_interval(&game) == 240u);
  game.mode = GAME_B;
  game.score = 0u;
  assert(game_step_interval(&game) == 440u && game_cargo_limit(&game) == 2u);
  game.score = 29u;
  assert(game_cargo_limit(&game) == 2u);
  game.score = 30u;
  assert(game_cargo_limit(&game) == 3u);
  game.score = 374u;
  assert(game_step_interval(&game) == 216u);
  game.score = 375u;
  assert(game_step_interval(&game) == 200u);
  game.score = 1000u;
  assert(game_step_interval(&game) == 200u);
  game.score = UINT32_MAX;
  assert(game_step_interval(&game) == 200u);
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
  idle_before = game.attacks[0].idle_ms_left;
  assert(!game_advance(&game, 700u) && game.recovery_ms_left == 800u);
  assert(game.attacks[0].idle_ms_left == idle_before);
  assert(game_pause(&game));
  paused = game;
  assert(!game_advance(&game, 2000u));
  assert(memcmp(&game, &paused, sizeof(game)) == 0);
  assert(game_resume(&game) && game.status == GAME_RECOVERING);
  assert(game_advance(&game, 900u));
  assert(game.status == GAME_PLAYING && game.step_ms_left == 460u);
  assert(game.attacks[0].idle_ms_left == idle_before - 100u);
}

static void test_determinism(void) {
  Game a, b;
  unsigned i;
  game_init(&a, GAME_B, 0u);
  game_init(&b, GAME_B, 0u);
  /* Explicit xorshift reference vector, not just comparing two opaque calls. */
  assert(a.rng == UINT32_C(0x40aec71f));
  assert(a.mae_target == 3u);
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
  for (i = 0u; i < GAME_SIDES; ++i) {
    if (game->attacks[i].phase == GAME_ATTACK_WINDUP) {
      unsigned due = game->attacks[i].steps_left;
      assert(due == 1u || due == 2u);
      blocked[due][i == 0u ? 0u : 4u] = true;
      if (due > horizon) horizon = due;
    }
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
  assert(viable[0][game->finn_pose]);
  {
    unsigned best = GAME_POSES, best_distance = GAME_POSES;
    for (pose = game->finn_pose == 0u ? 0u : game->finn_pose - 1u;
         pose < GAME_POSES && pose <= game->finn_pose + 1u; ++pose) {
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
      unsigned warning_ticks[2] = { 0u, 0u };
      uint64_t last_catch_step = 0u;
      game_init(&game, (GameMode)mode, seed);
      while (game.score < 1000u) {
        Game before = game;
        unsigned destination = bot_choose_pose(&game);
        unsigned i, j;
        assert(game.step < 16000u && game.step - last_catch_step < 32u);
        if (destination < game.finn_pose) press(&game, GAME_UP);
        else if (destination > game.finn_pose) press(&game, GAME_DOWN);
        assert(game.finn_pose == destination);
        assert(game_step(&game));
        assert(game.status == GAME_PLAYING);
        assert(game.drops == 0u && game.hits == 0u && game.total_misses == 0u);
        assert(game.misses == 0u && !game.half_ring);
        assert(!(game.attacks[0].phase == GAME_ATTACK_STRIKE && game.attacks[1].phase == GAME_ATTACK_STRIKE));
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
              assert(cargo->stage == 0u && game.mae_throwing);
              assert(before.mae_lane == lane && before.mae_ready);
              assert(game.mae_lane == before.mae_lane); /* Never throw while walking. */
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
          unsigned band = game.mode == GAME_A ? (game.score < 10u ? 0u : game.score < 60u ? 1u : 2u) : (game.score < 30u ? 0u : 1u);
          assert(active <= game_cargo_limit(&game));
          if (active > observed_max[mode][band]) observed_max[mode][band] = active;
        }
        for (i = 0u; i < GAME_SIDES; ++i) {
          const GameAttack *attack = &game.attacks[i];
          uint8_t far_lane = i == 0u ? 0u : 3u;
          if (attack->phase == GAME_ATTACK_WINDUP) {
            uint64_t strike = game.step + attack->steps_left;
            if (before.attacks[i].phase == GAME_ATTACK_IDLE) warning_ticks[i] = 0u;
            ++warning_ticks[i];
            assert(warning_ticks[i] <= 2u);
            for (j = 0u; j < GAME_MAX_CARGO; ++j) {
              if (game.cargo[j].active && game.cargo[j].lane == far_lane) {
                assert(game.cargo[j].landing_step != strike);
                assert(game.cargo[j].landing_step + 1u != strike);
              }
            }
          } else if (attack->phase == GAME_ATTACK_STRIKE) {
            assert(before.attacks[i].phase == GAME_ATTACK_WINDUP && warning_ticks[i] == 2u);
            assert(lane_last_landing[far_lane] != game.step);
            assert(lane_last_landing[far_lane] + 1u != game.step);
            ++strikes[i];
          }
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
    if (mode == GAME_A) {
      assert(observed_max[mode][0] == 1u && observed_max[mode][1] == 2u && observed_max[mode][2] == 3u);
    } else assert(observed_max[mode][0] == 2u && observed_max[mode][1] == 3u);
    printf("Game %c: 10,000 seeds from zero through >=1,000 true points; zero unavoidable misses; zero drops, hits, misses or stalls\n", mode == GAME_A ? 'A' : 'B');
    fflush(stdout);
  }
  puts("Cargo ramp coverage: A maxima 1/2/3 below 10 / below 60 / thereafter; B maxima 2/3 below 30 / thereafter");
  printf("Fairness coverage: %llu steps, %llu catches, %llu left / %llu right strikes; all four lanes every seed\n",
         (unsigned long long)total_steps, (unsigned long long)total_catches,
         (unsigned long long)total_strikes[0], (unsigned long long)total_strikes[1]);
}

int main(void) {
  test_inputs();
  test_cargo_and_feedback();
  test_drops_misses_and_recovery();
  test_attacks();
  test_reservations_and_mae();
  test_scoring();
  test_difficulty();
  test_pause_and_elapsed();
  test_determinism();
  puts("Rule tests passed");
  fflush(stdout);
  test_fairness();
  puts("All Harbor Catch tests passed");
  return 0;
}
