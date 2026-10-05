#ifndef POPEYE_GW_FEEDBACK_SERVICE_H
#define POPEYE_GW_FEEDBACK_SERVICE_H

#include "feedback.h"

void feedback_service_init(void (*redraw)(void));
/* Freeze visuals and cancel the timer while paused, hidden or unfocused. */
void feedback_service_set_running(bool running);
void feedback_service_reset(void);
void feedback_service_events(uint32_t events, bool record, bool vibration);
void feedback_service_apply(Scene *scene);

#endif
