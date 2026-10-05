#ifndef POPEYE_GW_CLOCK_H
#define POPEYE_GW_CLOCK_H
#ifdef PBL_PLATFORM_EMERY
#include <pebble.h> /* The SDK time.h only declares time_t. */
#else
#include <time.h>
#endif
#include "scene.h"

void clock_scene(Scene *scene, const struct tm *local, bool style_24h,
                 bool attract, bool alarm_on, bool ringing);
/* Next local calendar occurrence strictly after now, using libc DST rules. */
time_t clock_next_alarm(time_t now, unsigned hour, unsigned minute);
uint32_t clock_date(const struct tm *local);
#endif
