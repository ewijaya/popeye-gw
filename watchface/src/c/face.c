#include "face.h"

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
}
