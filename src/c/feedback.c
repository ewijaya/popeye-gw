#include "feedback.h"

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
