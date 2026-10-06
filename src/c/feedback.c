#include "feedback.h"

/* Original short motifs, all square wave. */
static const SoundNote catch_notes[] = { { 91, 45 } };
static const SoundNote miss_notes[] = { { 72, 150 } };
static const SoundNote bonus_notes[] = { { 88, 70 }, { 95, 110 } };
static const SoundNote over_notes[] = { { 79, 110 }, { 76, 110 }, { 72, 220 } };
static const SoundNote record_notes[] = { { 84, 80 }, { 88, 80 }, { 91, 80 }, { 96, 200 } };
static const SoundNote warning_notes[] = { { 93, 60 }, { 0, 50 }, { 93, 60 } };
static const SoundNote alarm_notes[] = { { 96, 70 }, { 0, 60 }, { 96, 70 }, { 0, 60 }, { 96, 70 }, { 0, 60 }, { 96, 70 } };

SoundCue feedback_sound_cue(uint32_t events, bool record) {
  if (events & GAME_EVENT_OVER) return record ? CUE_RECORD : CUE_OVER;
  if (events & GAME_EVENT_BONUS) return CUE_BONUS;
  if (events & (GAME_EVENT_MISS | GAME_EVENT_DROP)) return CUE_MISS;
  if (events & GAME_EVENT_CATCH) return CUE_CATCH;
  return CUE_NONE;
}

const SoundNote *feedback_sound_notes(SoundCue cue, unsigned *count) {
#define CUE_NOTES(array) (*count = (unsigned)(sizeof(array) / sizeof((array)[0])), (array))
  switch (cue) {
    case CUE_CATCH: return CUE_NOTES(catch_notes);
    case CUE_MISS: return CUE_NOTES(miss_notes);
    case CUE_BONUS: return CUE_NOTES(bonus_notes);
    case CUE_OVER: return CUE_NOTES(over_notes);
    case CUE_RECORD: return CUE_NOTES(record_notes);
    case CUE_ALARM: return CUE_NOTES(alarm_notes);
    case CUE_WARNING: return CUE_NOTES(warning_notes);
    case CUE_NONE: break;
  }
#undef CUE_NOTES
  *count = 0u;
  return NULL;
}

void feedback_reset(Feedback *feedback) {
  feedback->events = 0u;
  feedback->elapsed_ms = 0u;
}

FeedbackPulse feedback_trigger(Feedback *feedback, uint32_t events, bool record) {
  uint32_t visible = events & (GAME_EVENT_MISS | GAME_EVENT_BONUS | GAME_EVENT_OVER);
  if (visible != 0u) {
    feedback->events = visible;
    feedback->elapsed_ms = 0u;
  }
  /* A record is celebrated once, at game over, not on every new best catch. */
  if (events & GAME_EVENT_OVER) return record ? FEEDBACK_RECORD : FEEDBACK_LONG;
  if (events & (GAME_EVENT_MISS | GAME_EVENT_BONUS)) return FEEDBACK_SHORT;
  return FEEDBACK_SILENT;
}

void feedback_advance(Feedback *feedback, uint32_t elapsed_ms) {
  if (feedback->events == 0u) return;
  if (elapsed_ms >= FEEDBACK_DURATION_MS - feedback->elapsed_ms) feedback_reset(feedback);
  else feedback->elapsed_ms += elapsed_ms;
}

uint32_t feedback_next_delay(const Feedback *feedback) {
  return feedback->events == 0u ? 0u : FEEDBACK_PHASE_MS - feedback->elapsed_ms % FEEDBACK_PHASE_MS;
}

static void hide(Scene *scene, unsigned first, unsigned count) {
  unsigned i;
  for (i = first; i < first + count; ++i)
    scene->bits[i / 32u] &= ~(UINT32_C(1) << (i % 32u));
}

void feedback_apply(const Feedback *feedback, Scene *scene) {
  if (feedback->events == 0u || feedback->elapsed_ms / FEEDBACK_PHASE_MS % 2u == 0u) return;
  if (feedback->events & GAME_EVENT_MISS) {
    hide(scene, SEG_SPLASH, GAME_LANES);
    hide(scene, SEG_POPEYE_DIZZY_LEFT, 2u);
  }
  /* Cleared miss cans stay cleared; only the label acknowledges the bonus. */
  if (feedback->events & GAME_EVENT_BONUS) hide(scene, SEG_MISS_LABEL, 1u);
  if (feedback->events & GAME_EVENT_OVER) {
    hide(scene, SEG_DIGIT, SCENE_DIGITS * 7u);
    hide(scene, SEG_HI, 1u);
  }
}

uint8_t feedback_sound_volume(bool on, unsigned level) {
  static const uint8_t volumes[FEEDBACK_SOUND_LEVELS] = {
    FEEDBACK_SOUND_LOW, FEEDBACK_SOUND_MEDIUM, FEEDBACK_SOUND_HIGH
  };
  if (!on) return 0u;
  return volumes[level < FEEDBACK_SOUND_LEVELS ? level : 1u];
}
