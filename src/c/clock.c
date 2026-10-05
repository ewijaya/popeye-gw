#include "clock.h"

uint32_t clock_date(const struct tm *local) {
  return (uint32_t)(local->tm_year + 1900) * 10000u +
         (uint32_t)(local->tm_mon + 1) * 100u + (uint32_t)local->tm_mday;
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
  unsigned phase = attract ? (unsigned)local->tm_sec / 2u : 0u;
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
  scene_light(scene, SEG_POPEYE + (attract ? phase % GAME_POSES : 2u));
  scene_light(scene, SEG_BRUTUS + (attract && phase % 2u ? GAME_ATTACK_WINDUP : GAME_ATTACK_IDLE));
  if (ringing) {
    scene_light(scene, SEG_OLIVE_BELL + (unsigned)local->tm_sec % 2u);
    if (local->tm_sec % 2 == 0) scene_light(scene, SEG_BELL);
  } else {
    scene_light(scene, SEG_OLIVE_READY + phase % GAME_LANES);
    if (alarm_on) scene_light(scene, SEG_BELL);
  }
}
