#ifndef POPEYE_GW_FEEDBACK_H
#define POPEYE_GW_FEEDBACK_H

#include <stddef.h>

#include "scene.h"

/* Presentation timing, independent of the provisional PP-23 gameplay tables. */
#define FEEDBACK_PHASE_MS 250u
#define FEEDBACK_DURATION_MS (6u * FEEDBACK_PHASE_MS)

typedef enum { FEEDBACK_SILENT, FEEDBACK_SHORT, FEEDBACK_LONG,
               FEEDBACK_RECORD } FeedbackPulse;
typedef struct { uint32_t events, elapsed_ms; } Feedback;

/* LCD-style beeps. One cue per game tick; the table is plain C so it is host-tested
 * and the Speaker adapter only turns it into square-wave notes. */
/* Speaker volume (0-100) for Settings Sound Low/Medium/High; Medium is the default.
 * 1.1-1.2 played every cue at 60, which owners found too loud. */
#define FEEDBACK_SOUND_LEVELS 3u
#define FEEDBACK_SOUND_LOW 10u
#define FEEDBACK_SOUND_MEDIUM 20u
#define FEEDBACK_SOUND_HIGH 35u
#define FEEDBACK_SOUND_MAX_NOTES 8u
typedef enum { CUE_NONE, CUE_CATCH, CUE_MISS, CUE_BONUS, CUE_OVER, CUE_RECORD,
               CUE_ALARM, CUE_WARNING, CUE_KISS } SoundCue; /* WARNING: ten seconds left; KISS: Olive's kiss lands */
typedef struct { uint8_t midi; uint16_t ms; } SoundNote; /* midi 0 is a rest */

/* Priority: game over (ascending when it set a record) > bonus > drop/miss > catch.
 * The third miss and game over therefore yield only the game-over cue. */
SoundCue feedback_sound_cue(uint32_t events, bool record);
const SoundNote *feedback_sound_notes(SoundCue cue, unsigned *count);
/* Volume for a Sound setting: 0 when off; unknown levels play at Medium. */
uint8_t feedback_sound_volume(bool on, unsigned level);

void feedback_reset(Feedback *feedback);
FeedbackPulse feedback_trigger(Feedback *feedback, uint32_t events, bool record);
void feedback_advance(Feedback *feedback, uint32_t elapsed_ms);
uint32_t feedback_next_delay(const Feedback *feedback);
void feedback_apply(const Feedback *feedback, Scene *scene);

#endif
