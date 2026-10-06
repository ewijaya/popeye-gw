#include <pebble.h>
#include "feedback_service.h"

static Feedback s_feedback;
static AppTimer *s_timer;
static uint64_t s_started;
static bool s_running;
static void (*s_redraw)(void);
/* Kept static in case the Speaker keeps reading the notes after the call returns. */
static SpeakerNote s_notes[FEEDBACK_SOUND_MAX_NOTES];

static uint64_t now_ms(void) {
  time_t seconds;
  uint16_t millis = time_ms(&seconds, NULL);
  return (uint64_t)seconds * 1000u + millis;
}

static void cancel(void) {
  if (s_timer != NULL) app_timer_cancel(s_timer);
  s_timer = NULL;
}

static void advance(void) {
  uint64_t now = now_ms(), elapsed = now >= s_started ? now - s_started : 0u;
  if (s_running) feedback_advance(&s_feedback, elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed);
  s_started = now;
}

static void fired(void *data);

static void schedule(void) {
  uint32_t delay = feedback_next_delay(&s_feedback);
  cancel();
  if (s_running && delay != 0u) {
    s_timer = app_timer_register(delay, fired, NULL);
    /* If the system cannot supply a timer, keep a stable, readable scene. */
    if (s_timer == NULL) feedback_reset(&s_feedback);
  }
}

static void fired(void *data) {
  (void)data;
  s_timer = NULL;
  advance();
  schedule();
  if (s_redraw != NULL) s_redraw();
}

void feedback_service_stop_sound(void) { speaker_stop(); }

/* Same rule as vibration plus the system speaker mute, which also covers Quiet Time. */
void feedback_service_play(SoundCue cue, uint8_t volume) {
  unsigned count, i;
  const SoundNote *notes = feedback_sound_notes(cue, &count);
  if (notes == NULL || volume == 0u || speaker_is_muted() || quiet_time_is_active()) return;
  for (i = 0u; i < count; ++i)
    s_notes[i] = (SpeakerNote) { .midi_note = notes[i].midi, .waveform = SpeakerWaveformSquare,
                                 .duration_ms = notes[i].ms };
  speaker_stop(); /* Replace a cue that is still sounding. */
  speaker_play_notes(s_notes, count, volume);
}

void feedback_service_warn(bool vibration, uint8_t volume) {
  if (!s_running) return;
  feedback_service_play(CUE_WARNING, volume);
  if (vibration && !quiet_time_is_active()) vibes_short_pulse();
}

void feedback_service_init(void (*redraw)(void)) {
  feedback_service_reset();
  s_running = false;
  s_redraw = redraw;
}

void feedback_service_reset(void) {
  speaker_stop();
  cancel();
  feedback_reset(&s_feedback);
  s_started = now_ms();
}

void feedback_service_set_running(bool running) {
  if (!running) speaker_stop();
  if (s_running == running) return;
  advance();
  s_running = running;
  schedule();
}

void feedback_service_events(uint32_t events, bool record, bool vibration, uint8_t volume) {
  FeedbackPulse pulse;
  advance();
  pulse = feedback_trigger(&s_feedback, events, record);
  schedule();
  if (!s_running) return;
  feedback_service_play(feedback_sound_cue(events, record), volume);
  if (!vibration || quiet_time_is_active() || pulse == FEEDBACK_SILENT) return;
  /* Pebble ignores a new pulse while another is running. Replace it once,
   * and combine game-over + record into one long-then-double pattern. */
  vibes_cancel();
  if (pulse == FEEDBACK_SHORT) vibes_short_pulse();
  else if (pulse == FEEDBACK_LONG) vibes_long_pulse();
  else {
    static const uint32_t durations[] = { 400u, 200u, 90u, 100u, 90u };
    vibes_enqueue_custom_pattern((VibePattern) { .durations = durations, .num_segments = 5u });
  }
}

void feedback_service_apply(Scene *scene) { feedback_apply(&s_feedback, scene); }
