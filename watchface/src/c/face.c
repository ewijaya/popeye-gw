#include "face.h"

void face_clock_layout(const Scene *scene, FaceClockLayout *layout) {
  unsigned digit, bar;
  bool meridiem = scene_lit(scene, SEG_AM) || scene_lit(scene, SEG_PM);
  int width = 8 + 20 + 8 + (meridiem ? 8 + 11 : 0);
  for (digit = 0u; digit < 4u; ++digit) {
    unsigned pattern = 0u;
    for (bar = 0u; bar < 7u; ++bar)
      if (scene_lit(scene, SEG_DIGIT + digit * 7u + bar)) pattern |= 1u << bar;
    layout->widths[digit] = pattern == 0x06u ? 4 : 32;
    width += layout->widths[digit];
  }
  layout->starts[0] = (200 - width) / 2;
  layout->starts[1] = layout->starts[0] + layout->widths[0] + 8;
  layout->colon_x = layout->starts[1] + layout->widths[1] + 8;
  layout->starts[2] = layout->colon_x + 4 + 8;
  layout->starts[3] = layout->starts[2] + layout->widths[2] + 8;
  layout->right = layout->starts[3] + layout->widths[3];
  layout->meridiem_x = meridiem ? layout->right + 8 : -1;
  if (meridiem) layout->right = layout->meridiem_x + 11;
}

void face_scene(Scene *scene, const struct tm *local, bool style_24h, int frame) {
  struct tm shown = *local;
  if (frame < FACE_DEMO_FIRST || frame > FACE_DEMO_LAST) {
    clock_scene(scene, &shown, style_24h, false, false, false);
    return;
  }
  /* clock_scene derives the demo beat from the seconds. An even second keeps the
   * colon lit and selects the requested beat; hour and minute stay real, so the
   * food lane still follows the minute exactly as in the app. */
  shown.tm_sec = frame * 2;
  clock_scene(scene, &shown, style_24h, true, false, false);
  /* On the hour and on 14 February the minute demo is Olive's kiss instead,
   * from her kiss pose to the heart over Popeye across the five beats. */
  if (clock_kiss_time(local))
    scene_kiss(scene, (unsigned)(frame - FACE_DEMO_FIRST) * (SCENE_KISS_STEPS - 1u) /
                      (FACE_DEMO_LAST - FACE_DEMO_FIRST));
}

void face_scene_active(Scene *scene, const struct tm *local, bool style_24h,
                       uint32_t frame, unsigned activity, bool celebration,
                       unsigned pause) {
  static const unsigned brutus_poses[8] = { 0u, 0u, 1u, 1u, 2u, 2u, 1u, 0u };
  unsigned length, phase, lane, target, pose = 2u;
  unsigned side = GAME_LEFT, attack = GAME_ATTACK_IDLE;
  unsigned olive = SEG_OLIVE_READY;
  if (pause > 10u) pause = 10u;
  length = 6u + pause;
  phase = frame % length;
  /* Lane belongs to the continuous cycle, so a minute rollover cannot move
   * food or Popeye midway through a throw. */
  lane = frame / length % GAME_LANES;
  target = game_lane_pose((uint8_t)lane);

  /* Clock digits remain identical across every animation frame. */
  face_scene(scene, local, style_24h, -1);
  /* Remove the three default cast poses without affecting other segments. */
  scene->bits[(SEG_POPEYE + 2u) / 32u] &=
      ~(UINT32_C(1) << ((SEG_POPEYE + 2u) % 32u));
  scene->bits[SEG_BRUTUS / 32u] &= ~(UINT32_C(1) << (SEG_BRUTUS % 32u));
  scene->bits[SEG_OLIVE_READY / 32u] &=
      ~(UINT32_C(1) << (SEG_OLIVE_READY % 32u));

  if ((activity & 1u) != 0u && phase > 0u && phase < 6u) {
    scene_light(scene, SEG_CARGO + lane * GAME_CARGO_STEPS + phase - 1u);
    if (phase == 1u) olive = SEG_OLIVE_THROW;
  }
  if ((activity & 2u) != 0u) {
    if (phase == 1u || phase == 2u) pose = target < 2u ? 1u : 3u;
    else if (phase > 2u && phase < 6u) pose = target;
    if ((activity & 1u) != 0u && phase == 5u) scene_light(scene, SEG_POPEYE_CATCH);
  }
  if ((activity & 4u) != 0u) {
    side = frame / 8u % 2u;
    attack = brutus_poses[frame % 8u];
  }
  if (celebration) {
    if ((activity & 1u) != 0u) olive = SEG_OLIVE_BELL + frame % 2u;
    if ((activity & 2u) != 0u) {
      pose = 1u + frame % 3u;
      if (frame % 2u == 0u) scene_light(scene, SEG_POPEYE_CATCH);
    }
  }
  scene_light(scene, olive);
  scene_light(scene, SEG_POPEYE + pose);
  scene_light(scene, SEG_BRUTUS + side * 3u + attack);
  if ((activity & 1u) != 0u && !celebration && clock_kiss_time(local))
    scene_kiss(scene, frame % SCENE_KISS_STEPS);
}
