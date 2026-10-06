#include "clock.h"
#include "feedback_service.h"
#include "fake.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned lit_count(const Scene *scene, unsigned first, unsigned count) {
  unsigned n = 0, i;
  for (i = first; i < first + count; ++i) n += scene_lit(scene, i);
  return n;
}

static void test_demo(void) {
  Scene scene, previous;
  struct tm local = {0};
  unsigned sec, seen = 0;
  /* Two full 48-second cycles, including a minute boundary. */
  for (sec = 0; sec < 96; ++sec) {
    unsigned phase = sec / 2u % 6u, lane = sec / 12u % GAME_LANES;
    local.tm_min = (int)(sec / 60u); local.tm_sec = (int)(sec % 60u);
    clock_scene(&scene, &local, true, true, false, false);
    assert(lit_count(&scene, SEG_CARGO, 20) == (phase != 0u));
    assert(lit_count(&scene, SEG_POPEYE, GAME_POSES) == 1);
    assert(lit_count(&scene, SEG_OLIVE_READY, 4) == 1);
    assert(lit_count(&scene, SEG_BRUTUS, 6) == 1);
    assert(scene_lit(&scene, SEG_OLIVE_THROW) == (phase == 1u));
    if (phase != 0u) {
      assert(scene_lit(&scene, SEG_CARGO + lane * 5u + phase - 1u));
      seen |= 1u << (lane * 5u + phase - 1u);
    }
    if (phase == 5u) {
      assert(scene_lit(&scene, SEG_POPEYE + game_lane_pose((uint8_t)lane)));
      assert(scene_lit(&scene, SEG_POPEYE_CATCH));
    }
    assert(lit_count(&scene, SEG_GAME_A, 2) == 0); /* Clock is not a real round. */
    clock_scene(&scene, &local, true, true, true, true);
    assert(lit_count(&scene, SEG_CARGO, 20) == 0);
    assert(scene_lit(&scene, SEG_OLIVE_BELL + sec % 2u));
    assert(scene_lit(&scene, SEG_POPEYE + 2u));
  }
  assert(seen == (1u << 20) - 1u); /* Every food position appears. */
  local.tm_min = 0; local.tm_sec = 0;
  clock_scene(&previous, &local, true, false, true, false);
  for (sec = 1; sec < 60; ++sec) {
    local.tm_sec = (int)sec;
    clock_scene(&scene, &local, true, false, true, false);
    assert(memcmp(&scene, &previous, sizeof(scene)) == 0);
    assert(lit_count(&scene, SEG_CARGO, 20) == 0);
  }
}

static Scene full_scene(void) {
  Scene scene;
  unsigned i;
  scene_clear(&scene);
  for (i = 0; i < SEG_COUNT; ++i) scene_light(&scene, i);
  return scene;
}

static void test_feedback(void) {
  Feedback feedback;
  Scene scene;
  feedback_reset(&feedback);
  assert(feedback_trigger(&feedback, GAME_EVENT_CATCH | GAME_EVENT_HIGH_SCORE, true) == FEEDBACK_SILENT);
  assert(feedback_trigger(&feedback, GAME_EVENT_DROP, false) == FEEDBACK_SILENT);
  assert(feedback_next_delay(&feedback) == 0);
  assert(feedback_trigger(&feedback, GAME_EVENT_MISS, false) == FEEDBACK_SHORT);
  feedback_advance(&feedback, 250);
  scene = full_scene(); feedback_apply(&feedback, &scene);
  assert(lit_count(&scene, SEG_SPLASH, 4) == 0);
  assert(lit_count(&scene, SEG_POPEYE_DIZZY_LEFT, 2) == 0);
  assert(lit_count(&scene, SEG_MISS, 3) == 3); /* Miss accounting stays legible. */
  assert(lit_count(&scene, SEG_DIGIT, 28) == 28);
  assert(feedback_trigger(&feedback, GAME_EVENT_BONUS, false) == FEEDBACK_SHORT);
  feedback_advance(&feedback, 250);
  assert(feedback_trigger(&feedback, GAME_EVENT_CATCH, false) == FEEDBACK_SILENT);
  scene = full_scene(); feedback_apply(&feedback, &scene);
  assert(!scene_lit(&scene, SEG_MISS_LABEL)); /* Catches do not restart bonus. */
  assert(feedback_trigger(&feedback, GAME_EVENT_OVER | GAME_EVENT_MISS, true) == FEEDBACK_RECORD);
  feedback_advance(&feedback, 250);
  scene = full_scene(); feedback_apply(&feedback, &scene);
  assert(lit_count(&scene, SEG_DIGIT, 28) == 0 && !scene_lit(&scene, SEG_HI));
  assert(lit_count(&scene, SEG_SPLASH, 4) == 0);
  assert(scene_lit(&scene, SEG_GAME_B) && scene_lit(&scene, SEG_MISS_LABEL));
  feedback_advance(&feedback, UINT32_MAX);
  assert(feedback_next_delay(&feedback) == 0);
  scene = full_scene(); feedback_apply(&feedback, &scene);
  assert(scene_lit(&scene, SEG_HI)); /* Finishes readable, without an idle timer. */
  assert(feedback_trigger(&feedback, GAME_EVENT_OVER | GAME_EVENT_MISS, false) == FEEDBACK_LONG);
}

static unsigned redraws;
static void redraw(void) { ++redraws; }

static void test_service(void) {
  Scene scene;
  unsigned i;
  fake_reset(); fake_feedback_reset(); redraws = 0;
  feedback_service_init(redraw);
  feedback_service_set_running(true);
  feedback_service_events(GAME_EVENT_MISS, false, true, false);
  assert(fake_short_pulses == 1 && fake_timer_pending);
  fake_elapse(100);
  feedback_service_set_running(false); /* Pause or focus loss, mid-phase. */
  assert(!fake_timer_pending);
  fake_elapse(30000);
  feedback_service_set_running(true);
  assert(fake_timer_delay == 150);
  fake_timer_fire();
  scene = full_scene(); feedback_service_apply(&scene);
  assert(!scene_lit(&scene, SEG_SPLASH));
  for (i = 0; i < 5; ++i) fake_timer_fire();
  assert(!fake_timer_pending && redraws == 6 && fake_short_pulses == 1);
  scene = full_scene(); feedback_service_apply(&scene);
  assert(scene_lit(&scene, SEG_SPLASH));

  feedback_service_events(GAME_EVENT_OVER | GAME_EVENT_MISS, true, true, false);
  assert(fake_patterns == 1 && fake_long_pulses == 0 && fake_short_pulses == 1);
  assert(fake_pattern[0] == 400 && fake_pattern[2] == 90 && fake_pattern[4] == 90);
  feedback_service_reset(); /* Restart/exit cancels stale end-of-round flashes. */
  assert(!fake_timer_pending);
  feedback_service_events(GAME_EVENT_OVER, false, true, false);
  assert(fake_long_pulses == 1);
  feedback_service_reset();
  fake_quiet = true;
  feedback_service_events(GAME_EVENT_MISS, false, true, false);
  assert(fake_short_pulses == 1 && fake_timer_pending); /* Visuals still work. */
  fake_quiet = false;
  feedback_service_events(GAME_EVENT_BONUS, false, false, false);
  assert(fake_short_pulses == 1);
  feedback_service_set_running(false);
  feedback_service_events(GAME_EVENT_MISS, false, true, false);
  assert(!fake_timer_pending && fake_short_pulses == 1);
  feedback_service_reset();
  feedback_service_set_running(true);
  feedback_service_events(GAME_EVENT_CATCH | GAME_EVENT_HIGH_SCORE | GAME_EVENT_DROP, true, true, false);
  assert(!fake_timer_pending && fake_vibe_cancels == 3);
  fake_timer_fail = true;
  feedback_service_events(GAME_EVENT_BONUS, false, false, false);
  scene = full_scene(); feedback_service_apply(&scene);
  assert(scene_lit(&scene, SEG_MISS_LABEL) && !fake_timer_pending);
  fake_timer_fail = false;
  feedback_service_events(GAME_EVENT_MISS, false, false, false);
  fake_elapse(10000); /* Delayed SDK callback must not play a burst of old frames. */
  fake_timer_fire();
  assert(!fake_timer_pending);
  feedback_service_set_running(false);
  feedback_service_reset();
}

static unsigned total_ms(SoundCue cue) {
  unsigned count, i, ms = 0;
  const SoundNote *notes = feedback_sound_notes(cue, &count);
  for (i = 0; i < count; ++i) ms += notes[i].ms;
  return ms;
}

static void test_sound_cues(void) {
  unsigned count, cue, i;
  const SoundNote *notes;
  assert(feedback_sound_cue(0, false) == CUE_NONE);
  assert(feedback_sound_cue(GAME_EVENT_LAUNCH | GAME_EVENT_HIGH_SCORE, true) == CUE_NONE);
  assert(feedback_sound_cue(GAME_EVENT_CATCH | GAME_EVENT_LAUNCH, false) == CUE_CATCH);
  assert(feedback_sound_cue(GAME_EVENT_DROP, false) == CUE_MISS);
  assert(feedback_sound_cue(GAME_EVENT_MISS, false) == CUE_MISS); /* Brutus hit. */
  assert(feedback_sound_cue(GAME_EVENT_DROP | GAME_EVENT_MISS, false) == CUE_MISS);
  assert(feedback_sound_cue(GAME_EVENT_CATCH | GAME_EVENT_BONUS | GAME_EVENT_HIGH_SCORE, true) == CUE_BONUS);
  /* Third miss + game over: only the game-over cue, ascending when it is a record. */
  assert(feedback_sound_cue(GAME_EVENT_OVER | GAME_EVENT_MISS | GAME_EVENT_DROP, false) == CUE_OVER);
  assert(feedback_sound_cue(GAME_EVENT_OVER | GAME_EVENT_MISS | GAME_EVENT_DROP, true) == CUE_RECORD);
  assert(feedback_sound_cue(GAME_EVENT_OVER | GAME_EVENT_BONUS | GAME_EVENT_CATCH, false) == CUE_OVER);
  assert(feedback_sound_notes(CUE_NONE, &count) == NULL && count == 0);
  for (cue = CUE_CATCH; cue <= CUE_WARNING; ++cue) {
    notes = feedback_sound_notes((SoundCue)cue, &count);
    assert(notes != NULL && count >= 1 && count <= FEEDBACK_SOUND_MAX_NOTES);
    for (i = 0; i < count; ++i) assert(notes[i].midi <= 127 && notes[i].ms >= 30 && notes[i].ms <= 300);
    assert(total_ms((SoundCue)cue) <= (cue == CUE_ALARM ? 600u : 450u));
  }
  notes = feedback_sound_notes(CUE_CATCH, &count);
  assert(count == 1 && notes[0].midi >= 84);
  assert(feedback_sound_notes(CUE_MISS, &count)[0].midi < notes[0].midi - 12); /* Clearly lower. */
  assert(feedback_sound_notes(CUE_BONUS, &count) != NULL && count == 2);
  notes = feedback_sound_notes(CUE_OVER, &count);
  for (i = 1; i < count; ++i) assert(notes[i].midi < notes[i - 1].midi);
  notes = feedback_sound_notes(CUE_RECORD, &count);
  assert(count >= 3);
  for (i = 1; i < count; ++i) assert(notes[i].midi > notes[i - 1].midi);
  notes = feedback_sound_notes(CUE_ALARM, &count);
  assert(count >= 3 && notes[0].midi != 0 && notes[1].midi == 0); /* Repeated beeps with rests. */
  notes = feedback_sound_notes(CUE_WARNING, &count);
  assert(count == 3 && notes[0].midi == notes[2].midi && notes[1].midi == 0 && total_ms(CUE_WARNING) <= 200u);
  assert(feedback_sound_cue(GAME_EVENT_TIME_UP | GAME_EVENT_OVER, false) == CUE_OVER); /* Time-up is a game over. */
  assert(feedback_sound_cue(GAME_EVENT_TIME_UP | GAME_EVENT_OVER | GAME_EVENT_CATCH, true) == CUE_RECORD);
}

static void test_sound_service(void) {
  unsigned plays;
  fake_reset(); fake_feedback_reset();
  feedback_service_init(redraw);
  feedback_service_set_running(true);
  feedback_service_events(GAME_EVENT_CATCH | GAME_EVENT_LAUNCH, false, false, true);
  assert(fake_speaker_plays == 1 && fake_speaker_count == 1 && fake_speaker_midi[0] == 91);
  assert(fake_speaker_waveform[0] == SpeakerWaveformSquare && fake_speaker_ms[0] == 45);
  assert(fake_speaker_volume == FEEDBACK_SOUND_VOLUME && fake_speaker_stops >= 1);
  assert(fake_short_pulses == 0); /* Catches stay silent to the wrist. */
  feedback_service_events(GAME_EVENT_LAUNCH | GAME_EVENT_HIGH_SCORE, true, true, true);
  assert(fake_speaker_plays == 1); /* No cue for launches or new-best catches. */
  /* Sound is independent of vibration: a miss both beeps and pulses; either can be off. */
  feedback_service_events(GAME_EVENT_MISS, false, true, true);
  assert(fake_speaker_plays == 2 && fake_short_pulses == 1 && fake_speaker_midi[0] == 72);
  feedback_service_events(GAME_EVENT_DROP, false, false, true);
  assert(fake_speaker_plays == 3 && fake_short_pulses == 1);
  feedback_service_events(GAME_EVENT_BONUS | GAME_EVENT_CATCH, false, true, false);
  assert(fake_speaker_plays == 3 && fake_short_pulses == 2); /* Sound setting Off. */
  feedback_service_events(GAME_EVENT_BONUS | GAME_EVENT_CATCH, false, false, true);
  assert(fake_speaker_plays == 4 && fake_speaker_count == 2);
  /* One cue per tick: third miss + game over is one descending cue, never miss then over. */
  plays = fake_speaker_plays;
  feedback_service_events(GAME_EVENT_OVER | GAME_EVENT_MISS | GAME_EVENT_DROP, false, true, true);
  assert(fake_speaker_plays == plays + 1 && fake_speaker_count == 3);
  assert(fake_speaker_midi[0] > fake_speaker_midi[1] && fake_speaker_midi[1] > fake_speaker_midi[2]);
  feedback_service_events(GAME_EVENT_OVER | GAME_EVENT_MISS, true, true, true);
  assert(fake_speaker_plays == plays + 2 && fake_speaker_count == 4);
  assert(fake_speaker_midi[0] < fake_speaker_midi[3]); /* Ascending for a new record. */
  /* System mute and Quiet Time silence sound only; visuals continue. */
  fake_muted = true;
  feedback_service_events(GAME_EVENT_CATCH, false, false, true);
  feedback_service_play(CUE_ALARM, true);
  assert(fake_speaker_plays == plays + 2);
  fake_muted = false; fake_quiet = true;
  feedback_service_events(GAME_EVENT_MISS, false, true, true);
  assert(fake_speaker_plays == plays + 2 && fake_timer_pending);
  fake_quiet = false;
  /* The alarm ring uses the same gates, without needing a running game. */
  feedback_service_set_running(false);
  assert(fake_timer_pending == false);
  feedback_service_events(GAME_EVENT_CATCH, false, true, true);
  assert(fake_speaker_plays == plays + 2); /* Paused/unfocused: nothing plays. */
  plays = fake_speaker_plays;
  feedback_service_play(CUE_ALARM, true);
  assert(fake_speaker_plays == plays + 1 && fake_speaker_count == 7);
  assert(fake_speaker_midi[0] != 0 && fake_speaker_midi[1] == 0 && fake_speaker_waveform[6] == SpeakerWaveformSquare);
  feedback_service_play(CUE_ALARM, false);
  feedback_service_play(CUE_NONE, true);
  assert(fake_speaker_plays == plays + 1);
  /* Ten seconds left in a timed round: the double blip and a pulse, gated like the other cues. */
  feedback_service_set_running(true);
  plays = fake_speaker_plays;
  feedback_service_warn(true, true);
  assert(fake_speaker_plays == plays + 1 && fake_speaker_count == 3 && fake_short_pulses > 0);
  plays = fake_speaker_plays; fake_short_pulses = 0;
  feedback_service_warn(false, false);
  assert(fake_speaker_plays == plays && fake_short_pulses == 0);
  fake_quiet = true; feedback_service_warn(true, true);
  assert(fake_speaker_plays == plays && fake_short_pulses == 0);
  fake_quiet = false; fake_muted = true; feedback_service_warn(false, true);
  assert(fake_speaker_plays == plays);
  fake_muted = false;
  feedback_service_set_running(false);
  feedback_service_warn(true, true);
  assert(fake_speaker_plays == plays && fake_short_pulses == 0); /* Paused: silent. */
  /* Pause, focus loss, restart and exit stop whatever is sounding. */
  plays = fake_speaker_stops;
  feedback_service_set_running(false);
  assert(fake_speaker_stops == plays + 1); /* Already stopped, still silenced. */
  feedback_service_set_running(true);
  assert(fake_speaker_stops == plays + 1);
  feedback_service_set_running(false);
  feedback_service_reset();
  feedback_service_stop_sound();
  assert(fake_speaker_stops == plays + 4);
}

int main(void) {
  test_demo(); test_feedback(); test_service(); test_sound_cues(); test_sound_service();
  puts("M5: complete attract arcs, finite feedback, pause/resume, timer failure, haptic priority/settings/Quiet Time, LCD beep cues/priority/mute passed");
  return 0;
}
