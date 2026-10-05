#ifndef TEST_FAKE_H
#define TEST_FAKE_H
#include "pebble.h"
extern time_t fake_now, fake_scheduled;
extern int fake_schedule_calls, fake_fail_schedules, fake_schedule_error;
extern bool fake_write_fail, fake_launch;
extern WakeupId fake_id;
extern int32_t fake_cookie;
void fake_reset(void);
void fake_fire(void);
#endif
