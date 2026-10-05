#ifndef HARBOR_CATCH_SCENE_H
#define HARBOR_CATCH_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"

/* The PRD 9.2 segment inventory. IDs only: positions belong to the view. */
enum {
  SEG_MAE_READY = 0,                       /* + lane, 4 */
  SEG_MAE_THROW = SEG_MAE_READY + 4,       /* + lane, 4 */
  SEG_MAE_BELL = SEG_MAE_THROW + 4,        /* + pose, 2 */
  SEG_CARGO = SEG_MAE_BELL + 2,            /* + lane * 5 + stage, 20 */
  SEG_FINN = SEG_CARGO + 20,               /* + pose, 5 */
  SEG_FINN_DIZZY_LEFT = SEG_FINN + 5,
  SEG_FINN_DIZZY_RIGHT,
  SEG_FINN_CATCH,
  SEG_SPLASH,                              /* + lane, 4 */
  SEG_GRIZZLE = SEG_SPLASH + 4,            /* + side * 3 + phase, 6 */
  SEG_RING = SEG_GRIZZLE + 6,              /* + ring, 3 */
  SEG_RING_HALF = SEG_RING + 3,
  SEG_DIGIT,                               /* + digit * 7 + a..g, 28 */
  SEG_COLON = SEG_DIGIT + 28,
  SEG_AM,
  SEG_PM,
  SEG_GAME_A,
  SEG_GAME_B,
  SEG_BELL,
  SEG_GULL,
  SEG_HI,
  SEG_COUNT
};

#define SCENE_DIGITS 4u
#define SCENE_WORDS ((SEG_COUNT + 31) / 32)

typedef struct {
  uint32_t bits[SCENE_WORDS];
} Scene;

void scene_clear(Scene *scene);
void scene_light(Scene *scene, unsigned segment);
bool scene_lit(const Scene *scene, unsigned segment);
/* Right-aligned in the four digits without leading zeros; 0 shows one zero. */
void scene_number(Scene *scene, uint16_t value);
/* Idle shell until the M4 clock: Finn upright in the boat, nothing else. */
void scene_idle(Scene *scene);
void scene_game(Scene *scene, const Game *game);

#endif
