#include "game.h"
#include "scene.h"
#include "segments.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned count_lit(const Scene *scene, unsigned first, unsigned count) {
  unsigned i;
  unsigned lit = 0u;
  for (i = 0u; i < count; ++i) lit += scene_lit(scene, first + i) ? 1u : 0u;
  return lit;
}

static uint8_t digit_bits(const Scene *scene, unsigned digit) {
  unsigned bar;
  uint8_t bits = 0u;
  for (bar = 0u; bar < 7u; ++bar) {
    if (scene_lit(scene, SEG_DIGIT + digit * 7u + bar)) bits |= (uint8_t)(1u << bar);
  }
  return bits;
}

static Game quiet_game(GameMode mode) {
  Game game;
  game_init(&game, mode, 7u);
  game.olive_lane = 1u;
  return game;
}

static void test_inventory_and_bits(void) {
  Scene scene;
  unsigned seg;
  /* PRD 9.2: 10 + 20 + 8 + 4 + 6 + 4 + 31 + 5 segments. */
  assert(SEG_COUNT == 88);
  assert(SEGMENT_ART_COUNT == SEG_COUNT);
  for (seg = 0u; seg < SEG_COUNT; ++seg) {
    const SegmentArt *art = &segment_art[seg];
    unsigned other;
    assert(art->w > 0 && art->h > 0);
    assert(art->x >= 0 && art->y >= 0);
    assert(art->x + art->w <= 200 && art->y + art->h <= 228);
    assert(art->sheet_x >= 0 && art->sheet_y >= 0);
    assert(art->sheet_x + art->w <= SEGMENT_SHEET_WIDTH);
    assert(art->sheet_y + art->h <= SEGMENT_SHEET_HEIGHT);
    /* Overlapping atlas entries would render fragments of a different pose. */
    for (other = 0u; other < seg; ++other) {
      const SegmentArt *b = &segment_art[other];
      assert(art->sheet_x + art->w <= b->sheet_x || b->sheet_x + b->w <= art->sheet_x ||
             art->sheet_y + art->h <= b->sheet_y || b->sheet_y + b->h <= art->sheet_y);
    }
  }
  scene_clear(&scene);
  for (seg = 0u; seg < SEG_COUNT; ++seg) assert(!scene_lit(&scene, seg));
  scene_light(&scene, SEG_HI);
  scene_light(&scene, SEG_COUNT);
  assert(scene_lit(&scene, SEG_HI));
  assert(!scene_lit(&scene, SEG_COUNT));
  assert(count_lit(&scene, 0u, SEG_COUNT) == 1u);
}

static void test_numbers(void) {
  Scene scene;
  scene_clear(&scene);
  scene_number(&scene, 0u);
  assert(digit_bits(&scene, 3u) == 0x3Fu);
  assert(count_lit(&scene, SEG_DIGIT, 21u) == 0u);
  scene_clear(&scene);
  scene_number(&scene, 7u);
  assert(digit_bits(&scene, 3u) == 0x07u);
  assert(digit_bits(&scene, 2u) == 0u);
  scene_clear(&scene);
  scene_number(&scene, 105u);
  assert(digit_bits(&scene, 0u) == 0u);
  assert(digit_bits(&scene, 1u) == 0x06u);
  assert(digit_bits(&scene, 2u) == 0x3Fu);
  assert(digit_bits(&scene, 3u) == 0x6Du);
  scene_clear(&scene);
  scene_number(&scene, 1248u);
  assert(digit_bits(&scene, 0u) == 0x06u);
  assert(digit_bits(&scene, 1u) == 0x5Bu);
  assert(digit_bits(&scene, 2u) == 0x66u);
  assert(digit_bits(&scene, 3u) == 0x7Fu);
}

static void test_static_scenes(void) {
  Scene scene;
  Game game = quiet_game(GAME_A);
  scene_idle(&scene);
  assert(count_lit(&scene, 0u, SEG_COUNT) == 1u);
  assert(scene_lit(&scene, SEG_POPEYE + 2u));

  scene_game(&scene, &game);
  assert(scene_lit(&scene, SEG_GAME_A) && !scene_lit(&scene, SEG_GAME_B));
  assert(scene_lit(&scene, SEG_POPEYE + 2u));
  assert(count_lit(&scene, SEG_POPEYE, 8u) == 1u);
  assert(scene_lit(&scene, SEG_OLIVE_READY + 1u));
  assert(count_lit(&scene, SEG_OLIVE_READY, 10u) == 1u);
  assert(scene_lit(&scene, SEG_BRUTUS + GAME_ATTACK_IDLE));
  assert(count_lit(&scene, SEG_BRUTUS, 6u) == 1u);
  assert(digit_bits(&scene, 3u) == 0x3Fu);
  assert(count_lit(&scene, SEG_RING, 4u) == 0u);
  assert(!scene_lit(&scene, SEG_GULL) && !scene_lit(&scene, SEG_HI));

  game = quiet_game(GAME_B);
  game.attacks[1].phase = GAME_ATTACK_WINDUP;
  game.olive_throwing = true;
  scene_game(&scene, &game);
  assert(scene_lit(&scene, SEG_GAME_B) && !scene_lit(&scene, SEG_GAME_A));
  assert(scene_lit(&scene, SEG_BRUTUS + GAME_ATTACK_IDLE));
  assert(scene_lit(&scene, SEG_BRUTUS + 3u + GAME_ATTACK_WINDUP));
  assert(count_lit(&scene, SEG_BRUTUS, 6u) == 2u);
  assert(scene_lit(&scene, SEG_OLIVE_THROW + 1u) && !scene_lit(&scene, SEG_OLIVE_READY + 1u));
}

static void test_cargo_and_feedback(void) {
  Scene scene;
  Game game = quiet_game(GAME_A);
  game.cargo[0].active = true;
  game.cargo[0].lane = 2u;
  game.cargo[0].stage = 3u;
  game.cargo[1].lane = 0u; /* inactive slots stay dark */
  game.catch_pose = 4;
  game.splash_lane = 1;
  scene_game(&scene, &game);
  assert(scene_lit(&scene, SEG_CARGO + 2u * GAME_CARGO_STEPS + 3u));
  assert(scene_lit(&scene, SEG_CARGO + 3u * GAME_CARGO_STEPS + 4u));
  assert(count_lit(&scene, SEG_CARGO, 20u) == 2u);
  assert(scene_lit(&scene, SEG_POPEYE_CATCH));
  assert(scene_lit(&scene, SEG_SPLASH + 1u));
  assert(count_lit(&scene, SEG_SPLASH, 4u) == 1u);

  game = quiet_game(GAME_A);
  game.popeye_pose = 0u;
  game.miss_cause = GAME_MISS_HIT;
  game.attacks[0].phase = GAME_ATTACK_STRIKE;
  game.misses = 2u;
  game.half_ring = true;
  scene_game(&scene, &game);
  assert(scene_lit(&scene, SEG_POPEYE_DIZZY_LEFT));
  assert(count_lit(&scene, SEG_POPEYE, 8u) == 1u);
  assert(scene_lit(&scene, SEG_BRUTUS + GAME_ATTACK_STRIKE));
  assert(scene_lit(&scene, SEG_RING) && scene_lit(&scene, SEG_RING + 1u));
  assert(!scene_lit(&scene, SEG_RING + 2u) && scene_lit(&scene, SEG_RING_HALF));

  game.popeye_pose = 4u;
  scene_game(&scene, &game);
  assert(scene_lit(&scene, SEG_POPEYE_DIZZY_RIGHT) && !scene_lit(&scene, SEG_POPEYE_DIZZY_LEFT));

  game = quiet_game(GAME_A);
  game.lucky_tide = true;
  game.new_high_score = true;
  game.score = 1234u;
  scene_game(&scene, &game);
  assert(scene_lit(&scene, SEG_GULL) && !scene_lit(&scene, SEG_HI));
  assert(digit_bits(&scene, 0u) == 0u && digit_bits(&scene, 1u) == 0x5Bu);
  game.status = GAME_OVER;
  scene_game(&scene, &game);
  assert(scene_lit(&scene, SEG_HI));
}

/* A player who never moves loses every round; each state on the way must map
 * to one Popeye, one Olive, at most four cargo segments and the engine's rings. */
static void test_rounds_to_game_over(void) {
  uint32_t seed;
  for (seed = 1u; seed <= 200u; ++seed) {
    GameMode mode = seed % 2u == 0u ? GAME_A : GAME_B;
    Game game;
    Scene scene;
    unsigned steps = 0u;
    game_init(&game, mode, seed);
    while (game.status != GAME_OVER) {
      assert(game_step(&game));
      assert(++steps < 100000u);
      scene_game(&scene, &game);
      assert(count_lit(&scene, SEG_POPEYE, 8u) == 1u);
      assert(count_lit(&scene, SEG_OLIVE_READY, 8u) == 1u);
      assert(count_lit(&scene, SEG_CARGO, 20u) <= GAME_MAX_CARGO + 1u);
      assert(count_lit(&scene, SEG_RING, 3u) == game.misses);
      assert(scene_lit(&scene, SEG_RING_HALF) == game.half_ring);
      assert(count_lit(&scene, SEG_BRUTUS, 6u) == (mode == GAME_A ? 1u : 2u));
    }
    assert(count_lit(&scene, SEG_RING, 3u) == 3u);
    assert(scene_lit(&scene, SEG_HI) == game.new_high_score);
  }
}

int main(void) {
  test_inventory_and_bits();
  test_numbers();
  test_static_scenes();
  test_cargo_and_feedback();
  test_rounds_to_game_over();
  puts("Scene tests passed: 88 segments, digits, feedback and 200 rounds to game over");
  return 0;
}
