#ifndef POPEYE_GW_FEEDBACK_SERVICE_H
#define POPEYE_GW_FEEDBACK_SERVICE_H

#include "feedback.h"

void feedback_service_init(void (*redraw)(void));
/* Freeze visuals and cancel the timer while paused, hidden or unfocused. */
void feedback_service_set_running(bool running);
void feedback_service_reset(void);
/* A running game's events: haptics plus one beep when Sound is on. */
void feedback_service_events(uint32_t events, bool record, bool vibration, bool sound);
/* One cue outside the game (alarm ring, setting preview). Stop on pause, focus loss, exit. */
void feedback_service_play(SoundCue cue, bool sound);
void feedback_service_stop_sound(void);
/* The ten-seconds-left cue of a timed round; only while the round is running. */
void feedback_service_warn(bool vibration, bool sound);
void feedback_service_apply(Scene *scene);

#endif
