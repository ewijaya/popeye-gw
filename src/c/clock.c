#include "clock.h"

uint32_t clock_date(const struct tm *local) {
  return (uint32_t)(local->tm_year + 1900) * 10000u +
         (uint32_t)(local->tm_mon + 1) * 100u + (uint32_t)local->tm_mday;
}

bool clock_valentine(const struct tm *local) {
  return local->tm_mon == 1 && local->tm_mday == 14;
}

bool clock_kiss_time(const struct tm *local) {
  return local->tm_min == 0 || clock_valentine(local);
}

time_t clock_next_alarm(time_t now, unsigned hour, unsigned minute) {
  struct tm *local = localtime(&now), target;
  time_t next;
  if (local == NULL || hour > 23u || minute > 59u) return (time_t)-1;
  target = *local;
  target.tm_hour = (int)hour;
  target.tm_min = (int)minute;
  target.tm_sec = 0;
  target.tm_isdst = -1;
  next = mktime(&target);
  if (next != (time_t)-1 && next <= now) {
    /* Use the original date: mktime may normalize a nonexistent DST hour. */
    target = *localtime(&now);
    ++target.tm_mday;
    target.tm_hour = (int)hour;
    target.tm_min = (int)minute;
    target.tm_sec = 0;
    target.tm_isdst = -1;
    next = mktime(&target);
  }
  return next;
}

void clock_scene(Scene *scene, const struct tm *local, bool style_24h,
                 bool attract, bool alarm_on, bool ringing) {
  unsigned hour = (unsigned)local->tm_hour;
  unsigned beat = ((unsigned)local->tm_hour * 3600u +
                   (unsigned)local->tm_min * 60u + (unsigned)local->tm_sec) / 2u;
  unsigned phase = beat % 6u, lane = beat / 6u % GAME_LANES;
  unsigned pose = 2u, side = GAME_LEFT, attack = GAME_ATTACK_IDLE;
  bool demo = attract && !ringing;
  scene_clear(scene);
  if (!style_24h) {
    scene_light(scene, hour < 12u ? SEG_AM : SEG_PM);
    hour %= 12u;
    if (hour == 0u) hour = 12u;
  }
  scene_digit(scene, 0u, hour / 10u);
  scene_digit(scene, 1u, hour % 10u);
  scene_digit(scene, 2u, (unsigned)local->tm_min / 10u);
  scene_digit(scene, 3u, (unsigned)local->tm_min % 10u);
  if (!attract || local->tm_sec % 2 == 0) scene_light(scene, SEG_COLON);
  if (demo) {
    unsigned target = game_lane_pose((uint8_t)lane);
    /* A deliberately slow, silent demonstration: ready, release, four fixed
     * food positions, then catch. No score, random state or gameplay timer. */
    if (phase >= 2u) pose = target < 2u ? 1u : 3u;
    if (phase >= 3u) pose = target;
    if (phase != 0u) scene_light(scene, SEG_CARGO + lane * GAME_CARGO_STEPS + phase - 1u);
    if (phase == 5u) scene_light(scene, SEG_POPEYE_CATCH);
    side = target < 2u ? GAME_RIGHT : GAME_LEFT;
    if (phase == 2u || phase == 3u) attack = GAME_ATTACK_WINDUP;
    if (phase == 4u) attack = GAME_ATTACK_STRIKE;
  }
  scene_light(scene, SEG_POPEYE + pose);
  scene_light(scene, SEG_BRUTUS + side * 3u + attack);
  if (ringing) {
    scene_light(scene, SEG_OLIVE_BELL + (unsigned)local->tm_sec % 2u);
    if (local->tm_sec % 2 == 0) scene_light(scene, SEG_BELL);
  } else {
    scene_light(scene, demo && phase == 1u ? SEG_OLIVE_THROW : SEG_OLIVE_READY);
    if (alarm_on) scene_light(scene, SEG_BELL);
    /* Once in the first 16 s of each hour, and continuously on 14 February. */
    if (demo && clock_valentine(local)) scene_kiss(scene, beat % SCENE_KISS_STEPS);
    else if (demo && local->tm_min == 0 && local->tm_sec < 16)
      scene_kiss(scene, (unsigned)local->tm_sec / 2u);
  }
}
