#ifndef POPEYE_GW_GAME_H
#define POPEYE_GW_GAME_H

#include <stdbool.h>
#include <stdint.h>

#define GAME_LANES 4u
#define GAME_CARGO_STEPS 5u
#define GAME_MAX_CARGO 3u
#define GAME_MAX_MISSES 3u
#define GAME_POSES 5u

typedef enum { GAME_A = 0, GAME_B = 1 } GameMode;
typedef enum {
  GAME_PLAYING, GAME_RECOVERING, GAME_PAUSED, GAME_OVER
} GameStatus;
typedef enum { GAME_UP = 0, GAME_DOWN = 1 } GameButton;
typedef enum { GAME_LEFT, GAME_RIGHT } GameSide;
typedef enum { GAME_ATTACK_IDLE, GAME_ATTACK_WINDUP, GAME_ATTACK_STRIKE } GameAttackPhase;
typedef enum { GAME_MISS_NONE, GAME_MISS_DROP, GAME_MISS_HIT } GameMissCause;

enum {
  GAME_EVENT_CATCH = 1u << 0,
  GAME_EVENT_DROP = 1u << 1,
  GAME_EVENT_MISS = 1u << 2,
  GAME_EVENT_BONUS = 1u << 3,
  GAME_EVENT_HIGH_SCORE = 1u << 4,
  GAME_EVENT_OVER = 1u << 5,
  GAME_EVENT_LAUNCH = 1u << 6,
  GAME_EVENT_TIME_UP = 1u << 7 /* timed rounds only; always accompanied by GAME_EVENT_OVER */
};

typedef struct {
  bool active;
  uint8_t lane;
  uint8_t stage; /* 0..4 correspond to the five fixed cargo segments. */
  uint32_t id;
  uint64_t launch_step;
  uint64_t landing_step;
} GameCargo;

typedef struct {
  GameSide side;
  GameAttackPhase phase;
  uint8_t steps_left;
  uint32_t idle_ms_left;
  uint32_t rng;
} GameAttack;

/* Public, fixed-size render state. The engine owns its fields; normal callers
 * change state through the functions below. No pixels, clock, allocation or
 * platform dependencies are present. Counters are true totals, never digits.
 *
 * Timing interpretation: a launch lights segment 1 without moving the new
 * cargo that tick. Four subsequent ticks light segments 2..5; segment 5 is
 * resolved on that catch/drop tick. active identifies airborne segments; a
 * resolved slot may be reused for a new launch immediately. Render segment 5
 * and catch/drop feedback from catch_pose/splash_lane on the resolution tick.
 * Miss feedback remains through recovery.
 * A wind-up begins on a boundary and is visible for two complete intervals;
 * the following boundary is the strike. Idle countdowns measure playing
 * milliseconds, not a random number of steps. A blocked wind-up remains idle
 * until its full warning and strike can coexist with all launched cargo.
 *
 * A hit adds one miss without consuming a pending dropped food item. A
 * second drop consumes that pending item. Each catch scores one point. At
 * 200/500 displayed points full miss marks clear; pending-drop clearing is
 * provisional (the PP-23 manual does not specify that edge case).
 * Score is a true total for saved records, displayed modulo 1000. Food tempo
 * and capacity reset each 100 points; exact tables remain unmeasured.
 * Scores saturate at UINT32_MAX rather than wrap the saved high score.
 *
 * Recovery freezes cargo/attack/step time; pause freezes recovery too. A third
 * miss ends immediately. Resume preserves the exact remaining interval. Each
 * tick resolves attack hits first (a hit ends the tick and clears all cargo),
 * then cargo catches/drops, then Olive. One tick cannot award two full misses.
 */
typedef struct {
  GameMode mode;
  GameStatus status;
  GameStatus resume_status;
  uint8_t popeye_pose; /* left far=0, left near=1, centre=2, right near=3, far=4 */
  uint8_t misses;
  bool half_ring;
  bool controls_swapped;
  uint8_t held_buttons;
  uint32_t score;
  uint32_t high_scores[2];
  bool new_high_score;
  uint32_t rng;
  uint64_t step;
  uint32_t step_ms_left;
  uint32_t recovery_ms_left;
  uint32_t next_cargo_id;
  GameCargo cargo[GAME_MAX_CARGO];
  GameAttack attack; /* One Brutus: left in A, changes sides in B. */
  uint8_t olive_target; /* Next cargo arc; Olive stays beside her car. */
  bool olive_ready;
  bool olive_throwing;
  int8_t catch_pose;
  int8_t splash_lane;
  GameMissCause miss_cause;
  uint32_t events; /* accumulated until game_take_events */
  uint32_t catches;
  uint32_t drops;
  uint32_t hits;
  uint32_t total_misses;
  /* Optional active-time limit (Sprint, Daily); zero limit means untimed. Only time
   * consumed while PLAYING counts: MISS recovery, pause and game over do not. */
  uint32_t time_limit_ms;
  uint32_t time_left_ms;
  bool time_up; /* the round ended by reaching the limit, not by three misses */
} Game;

/* init clears all records; start keeps mode-specific highs and the swap setting.
 * Seed zero is valid and maps to a fixed nonzero xorshift state. */
void game_init(Game *game, GameMode mode, uint32_t seed);
void game_start(Game *game, GameMode mode, uint32_t seed);
/* Same rules with an active-play limit. When time_left_ms reaches zero the round
 * ends like game over, with GAME_EVENT_TIME_UP | GAME_EVENT_OVER. The tick that lands
 * exactly on the limit is still resolved first; three misses still end it early. */
void game_start_timed(Game *game, GameMode mode, uint32_t seed, uint32_t limit_ms);
/* Seed shared by everyone on a local calendar date YYYYMMDD (Daily). */
uint32_t game_daily_seed(uint32_t date);
/* A repeated pressed=true for a held button does nothing. Release it before
 * another press. Input moves and clamps immediately; paused/recovery input
 * records button edges but cannot move Popeye. Movement remains active after
 * game over, as documented in MAME; it cannot resume scoring or timers.
 * The app maps Select/Back to separate pause/resume functions. */
bool game_input(Game *game, GameButton button, bool pressed);
bool game_pause(Game *game);
bool game_resume(Game *game);
/* Advance only active time. Calls may span ticks and recovery. No time is
 * consumed while paused/over. Returns whether render state may have changed. */
bool game_advance(Game *game, uint32_t elapsed_ms);
/* Advance to the next tick, or the end of recovery; respects partial timers. */
bool game_step(Game *game);
/* Milliseconds until game_step's next boundary: the end of recovery, the next tick or the
 * time limit, whichever is first. Zero when paused or over. */
uint32_t game_next_boundary_ms(const Game *game);
uint32_t game_take_events(Game *game);
uint32_t game_step_interval(const Game *game);
uint16_t game_display_score(const Game *game);
uint8_t game_cargo_limit(const Game *game);
uint8_t game_lane_pose(uint8_t lane);

#endif
