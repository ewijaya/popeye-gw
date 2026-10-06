#ifndef POPEYE_GW_TUNING_H
#define POPEYE_GW_TUNING_H

/* Provisional watch timing, NOT recovered PP-23 timing. See docs/pp23-fidelity.md.
 * The manual establishes increasing food speed/quantity and a reset every 100
 * points. It does not supply these durations or thresholds. Same food curve in
 * A/B; B's extra challenge is Brutus changing sides. */
#define PGW_FOOD_STEP_MS { 560u, 440u, 340u, 240u }
#define PGW_SPEED_POINTS 25u
#define PGW_FIRST_CARGO_THRESHOLD 10u
#define PGW_SECOND_CARGO_THRESHOLD 60u
#define PGW_IDLE_MIN_MS 4000u
#define PGW_IDLE_MAX_MS 9000u
#define PGW_WINDUP_STEPS 2u
#define PGW_STRIKE_STEPS 1u
#define PGW_MISS_RECOVERY_MS 1500u
/* Sprint and Daily: Game B rules for this much active play (not paused, not recovering). */
#define PGW_SPRINT_MS 60000u
#define PGW_SPRINT_WARNING_MS 10000u
/* Mixed into the date so Daily is not simply the date as a seed. */
#define PGW_DAILY_SEED_SALT UINT32_C(0x5bd1e995)
/* App behaviour, not game pacing: return to the clock after game over. */
#define PGW_GAME_OVER_IDLE_MS 300000u

#endif
