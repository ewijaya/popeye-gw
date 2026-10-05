#ifndef POPEYE_GW_FEEDBACK_H
#define POPEYE_GW_FEEDBACK_H

#include "scene.h"

/* Presentation timing, independent of the provisional PP-23 gameplay tables. */
#define FEEDBACK_PHASE_MS 250u
#define FEEDBACK_DURATION_MS (6u * FEEDBACK_PHASE_MS)

typedef enum { FEEDBACK_SILENT, FEEDBACK_SHORT, FEEDBACK_LONG,
               FEEDBACK_RECORD } FeedbackPulse;
typedef struct { uint32_t events, elapsed_ms; } Feedback;

void feedback_reset(Feedback *feedback);
FeedbackPulse feedback_trigger(Feedback *feedback, uint32_t events, bool record);
void feedback_advance(Feedback *feedback, uint32_t elapsed_ms);
uint32_t feedback_next_delay(const Feedback *feedback);
void feedback_apply(const Feedback *feedback, Scene *scene);

#endif
