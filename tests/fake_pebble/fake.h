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
extern bool fake_quiet, fake_timer_fail;
extern unsigned fake_short_pulses, fake_long_pulses, fake_patterns, fake_vibe_cancels;
extern uint32_t fake_pattern[5], fake_timer_delay;
extern bool fake_timer_pending;
extern bool fake_muted;
extern unsigned fake_speaker_plays, fake_speaker_stops, fake_speaker_count, fake_speaker_volume;
extern uint8_t fake_speaker_midi[8], fake_speaker_waveform[8];
extern uint16_t fake_speaker_ms[8];
void fake_feedback_reset(void);
void fake_elapse(uint32_t ms);
void fake_timer_fire(void);
#endif
