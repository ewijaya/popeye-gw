#include "scene.h"

#include <string.h>

/* Seven-segment patterns, bit 0 = a (top) through bit 6 = g (middle). */
static const uint8_t digit_patterns[10] = {
  0x3Fu, 0x06u, 0x5Bu, 0x4Fu, 0x66u, 0x6Du, 0x7Du, 0x07u, 0x7Fu, 0x6Fu
};

void scene_clear(Scene *scene) {
  memset(scene, 0, sizeof(*scene));
}

void scene_light(Scene *scene, unsigned segment) {
  if (segment < SEG_COUNT) scene->bits[segment / 32u] |= UINT32_C(1) << (segment % 32u);
}

bool scene_lit(const Scene *scene, unsigned segment) {
  return segment < SEG_COUNT && (scene->bits[segment / 32u] >> (segment % 32u) & 1u) != 0u;
}

void scene_digit(Scene *scene, unsigned digit, unsigned value) {
  unsigned bit;
  if (digit >= SCENE_DIGITS || value > 9u) return;
  for (bit = 0u; bit < 7u; ++bit) {
    if ((digit_patterns[value] >> bit & 1u) != 0u)
      scene_light(scene, SEG_DIGIT + digit * 7u + bit);
  }
}

void scene_number(Scene *scene, uint16_t value) {
  unsigned digit = SCENE_DIGITS;
  do {
    scene_digit(scene, --digit, value % 10u);
    value = (uint16_t)(value / 10u);
  } while (value != 0u && digit != 0u);
}

void scene_idle(Scene *scene) {
  scene_clear(scene);
  scene_light(scene, SEG_POPEYE + 2u);
}

static unsigned lane_for_pose(int8_t pose) {
  return pose < 2 ? (unsigned)pose : (unsigned)pose - 1u;
}

void scene_game(Scene *scene, const Game *game) {
  unsigned i;
  scene_clear(scene);
  scene_light(scene, game->mode == GAME_A ? SEG_GAME_A : SEG_GAME_B);
  scene_number(scene, game_display_score(game));
  scene_light(scene, game->olive_throwing ? SEG_OLIVE_THROW : SEG_OLIVE_READY);
  for (i = 0u; i < GAME_MAX_CARGO; ++i) {
    const GameCargo *cargo = &game->cargo[i];
    if (cargo->active) scene_light(scene, SEG_CARGO + cargo->lane * GAME_CARGO_STEPS + cargo->stage);
  }
  /* A resolved catch shows its cargo on segment 5 for the resolution tick. */
  if (game->catch_pose >= 0) {
    scene_light(scene, SEG_CARGO + lane_for_pose(game->catch_pose) * GAME_CARGO_STEPS +
                       GAME_CARGO_STEPS - 1u);
    scene_light(scene, SEG_POPEYE_CATCH);
  }
  if (game->splash_lane >= 0) scene_light(scene, SEG_SPLASH + (unsigned)game->splash_lane);
  if (game->miss_cause == GAME_MISS_HIT) {
    scene_light(scene, game->popeye_pose == 0u ? SEG_POPEYE_DIZZY_LEFT : SEG_POPEYE_DIZZY_RIGHT);
  } else {
    scene_light(scene, SEG_POPEYE + game->popeye_pose);
  }
  scene_light(scene, SEG_BRUTUS + (unsigned)game->attack.side * 3u + (unsigned)game->attack.phase);
  scene_light(scene, SEG_MISS_LABEL);
  for (i = 0u; i < game->misses && i < GAME_MAX_MISSES; ++i) scene_light(scene, SEG_MISS + i);
  if (game->half_ring) scene_light(scene, SEG_MISS_HALF);
  if (game->status == GAME_OVER && game->new_high_score) scene_light(scene, SEG_HI);
}

void scene_best(Scene *scene, GameMode mode, uint32_t best) {
  scene_clear(scene);
  scene_light(scene, mode == GAME_A ? SEG_GAME_A : SEG_GAME_B);
  scene_number(scene, (uint16_t)(best % 1000u));
  scene_light(scene, SEG_HI);
  scene_light(scene, SEG_OLIVE_READY);
  scene_light(scene, SEG_POPEYE + 2u);
  scene_light(scene, SEG_BRUTUS + (mode == GAME_A ? GAME_LEFT : GAME_RIGHT) * 3u);
}

static void unlight(Scene *scene, unsigned first, unsigned count) {
  unsigned i;
  for (i = first; i < first + count && i < SEG_COUNT; ++i)
    scene->bits[i / 32u] &= ~(UINT32_C(1) << (i % 32u));
}

void scene_kiss(Scene *scene, unsigned step) {
  if (step >= SCENE_KISS_STEPS) return;
  unlight(scene, SEG_OLIVE_READY, 4u); /* ready, throw and both bell frames */
  unlight(scene, SEG_CARGO, GAME_LANES * GAME_CARGO_STEPS);
  unlight(scene, SEG_POPEYE_CATCH, 1u);
  unlight(scene, SEG_HEART, 6u); /* a later frame replaces an earlier one */
  scene_light(scene, SEG_OLIVE_KISS);
  if (step >= 1u && step <= 5u) scene_light(scene, SEG_HEART + step - 1u);
  if (step >= 6u) {
    unlight(scene, SEG_POPEYE, GAME_POSES);
    unlight(scene, SEG_POPEYE_DIZZY_LEFT, 2u);
    scene_light(scene, SEG_POPEYE + 2u);
    scene_light(scene, SEG_POPEYE_HEART);
  }
}

void scene_kiss_pose(Scene *scene) {
  if (!scene_lit(scene, SEG_OLIVE_READY)) return;
  unlight(scene, SEG_OLIVE_READY, 1u);
  scene_light(scene, SEG_OLIVE_KISS);
}
