#ifndef POPEYE_GW_SCENE_H
#define POPEYE_GW_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "game.h"

/* The PRD 9.2 segment inventory plus Olive's kiss (1.4). IDs only: positions belong to the view. */
enum {
  SEG_OLIVE_READY = 0,                       /* fixed left ledge */
  SEG_OLIVE_THROW = SEG_OLIVE_READY + 1,       /* fixed left ledge */
  SEG_OLIVE_BELL = SEG_OLIVE_THROW + 1,        /* + pose, 2 */
  SEG_CARGO = SEG_OLIVE_BELL + 2,            /* + lane * 5 + stage, 20 */
  SEG_POPEYE = SEG_CARGO + 20,               /* + pose, 5 */
  SEG_POPEYE_DIZZY_LEFT = SEG_POPEYE + 5,
  SEG_POPEYE_DIZZY_RIGHT,
  SEG_POPEYE_CATCH,
  SEG_SPLASH,                              /* + lane, 4 */
  SEG_BRUTUS = SEG_SPLASH + 4,            /* + side * 3 + phase, 6 */
  SEG_MISS = SEG_BRUTUS + 6,              /* + mark, 3 */
  SEG_MISS_HALF = SEG_MISS + 3,
  SEG_MISS_LABEL,
  SEG_DIGIT,                               /* + digit * 7 + a..g, 28 */
  SEG_COLON = SEG_DIGIT + 28,
  SEG_AM,
  SEG_PM,
  SEG_GAME_A,
  SEG_GAME_B,
  SEG_BELL,
  SEG_HI,
  SEG_OLIVE_KISS,                           /* Olive throws Popeye a kiss */
  SEG_HEART,                               /* + stage, 5: heart flying to Popeye */
  SEG_POPEYE_HEART = SEG_HEART + 5,        /* above Popeye's centre pose */
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
void scene_digit(Scene *scene, unsigned digit, unsigned value);
/* Static player pose, also useful for scene tests. */
void scene_idle(Scene *scene);
void scene_game(Scene *scene, const Game *game);
/* Olive's kiss, decoration only. Step 0 is her kiss pose, 1-5 fly the heart and
 * 6-7 centre Popeye under a heart. Food and the catch flash are hidden. */
#define SCENE_KISS_STEPS 8u
void scene_kiss(Scene *scene, unsigned step);
/* Brief bonus nod: the kiss pose replaces Olive's ready pose, never her throw. */
void scene_kiss_pose(Scene *scene);
/* Clock-page preview of a saved best: mode lamp, HI and the score modulo 1000. */
void scene_best(Scene *scene, GameMode mode, uint32_t best);

#endif
