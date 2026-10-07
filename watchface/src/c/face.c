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

void face_scene_still(Scene *scene, const struct tm *local, bool style_24h) {
  struct tm shown = *local;
  unsigned minute = (unsigned)local->tm_min;
  unsigned lane = minute % GAME_LANES, target = game_lane_pose((uint8_t)lane);
  unsigned beats = FACE_DEMO_LAST - FACE_DEMO_FIRST + 1u;
  unsigned phase = FACE_DEMO_FIRST + minute / GAME_LANES % beats, stage = phase - 1u;
  unsigned threat = target < 2u ? 3u : 0u, first = minute / 20u % 2u;
  unsigned candidates[2], count = 0u, i;
  if (clock_kiss_time(local)) {
    /* One fixed kiss frame on the calm idle scene: Olive's kiss pose and the
     * heart over Popeye, with no food or rival action to crowd it. */
    face_scene(scene, local, style_24h, -1);
    scene_kiss(scene, SCENE_KISS_STEPS - 1u);
    return;
  }
  /* The app's demonstration beat, frozen: from an even second clock_scene takes
   * the lane from the minute (minute % 4) and the beat from the second. The beat
   * comes from the minute too, so 20 minutes show every lane at every stage. */
  shown.tm_sec = (int)phase * 2;
  clock_scene(scene, &shown, style_24h, true, false, false);
  /* Olive's next food, already in flight behind the first in another lane. It
   * keeps the game's launch spacing (stage gap at least Popeye's pose distance)
   * and avoids the lane Brutus threatens, so Popeye could still catch both. */
  for (i = 0u; i < GAME_LANES; ++i)
    if (i != lane && i != threat) candidates[count++] = i;
  for (i = 0u; i < count; ++i) {
    unsigned other = candidates[(first + i) % count];
    unsigned pose = game_lane_pose((uint8_t)other);
    unsigned distance = pose > target ? pose - target : target - pose;
    if (distance > stage) continue;
    scene_light(scene, SEG_CARGO + other * GAME_CARGO_STEPS + stage - distance);
    if (stage == distance) { /* that food has just left Olive's hands */
      scene->bits[SEG_OLIVE_READY / 32u] &= ~(UINT32_C(1) << (SEG_OLIVE_READY % 32u));
      scene_light(scene, SEG_OLIVE_THROW);
    }
    break;
  }
}

/* The Arcade pattern, found by search and checked by the tests against the game's
 * own launch, strike and spacing rules. Throws land four beats after they leave
 * Olive; Brutus strikes on beats 0, 8, 16 and 24, left then right. Each lane holds at
 * most one food at a time. */
#define CROWD_PERIOD 32u
#define CROWD_THROWS 12u
#define CROWD_STRIKE_BEAT 0u
static const uint8_t crowd_launch[CROWD_THROWS] = { 0, 3, 5, 7, 10, 12, 14, 18, 20, 23, 27, 30 };
static const uint8_t crowd_lane[CROWD_THROWS] = { 0, 1, 2, 3, 2, 1, 0, 3, 2, 0, 3, 1 };
/* Olive's quick kiss fills the two free beats after these throws. */
static const uint8_t crowd_kiss[2] = { 15, 24 };

/* Popeye leaves each catch at once, one pose per beat, to be in place for the next. */
static unsigned crowd_popeye(unsigned beat) {
  unsigned i, since = CROWD_PERIOD, from = 0u, here, next, steps;
  for (i = 0u; i < CROWD_THROWS; ++i) {
    unsigned age = (beat + CROWD_PERIOD - (crowd_launch[i] + 4u) % CROWD_PERIOD) % CROWD_PERIOD;
    if (age < since) { since = age; from = i; }
  }
  here = game_lane_pose(crowd_lane[from]);
  next = game_lane_pose(crowd_lane[(from + 1u) % CROWD_THROWS]);
  steps = next > here ? next - here : here - next;
  if (since < steps) steps = since;
  return next > here ? here + steps : here - steps;
}

void face_scene_crowd(Scene *scene, const struct tm *local, bool style_24h,
                      uint32_t frame, unsigned activity) {
  unsigned beat = (unsigned)(frame % CROWD_PERIOD), i, popeye = 2u, caught = 0u;
  unsigned olive = SEG_OLIVE_READY, side = GAME_LEFT, attack = GAME_ATTACK_IDLE;
  unsigned cycle;
  face_scene(scene, local, style_24h, -1);
  scene->bits[(SEG_POPEYE + 2u) / 32u] &= ~(UINT32_C(1) << ((SEG_POPEYE + 2u) % 32u));
  scene->bits[SEG_BRUTUS / 32u] &= ~(UINT32_C(1) << (SEG_BRUTUS % 32u));
  scene->bits[SEG_OLIVE_READY / 32u] &= ~(UINT32_C(1) << (SEG_OLIVE_READY % 32u));
  if ((activity & 1u) != 0u) {
    for (i = 0u; i < CROWD_THROWS; ++i) {
      unsigned stage = (beat + CROWD_PERIOD - crowd_launch[i]) % CROWD_PERIOD;
      if (stage >= GAME_CARGO_STEPS) continue;
      scene_light(scene, SEG_CARGO + crowd_lane[i] * GAME_CARGO_STEPS + stage);
      if (stage == 0u) olive = SEG_OLIVE_THROW;
      if (stage == GAME_CARGO_STEPS - 1u) caught = 1u;
    }
    for (i = 0u; i < 2u; ++i)
      if (beat == crowd_kiss[i] || beat == crowd_kiss[i] + 1u) olive = SEG_OLIVE_KISS;
  }
  if ((activity & 2u) != 0u) {
    popeye = crowd_popeye(beat);
    if (caught != 0u) scene_light(scene, SEG_POPEYE_CATCH);
  }
  if ((activity & 4u) != 0u) {
    /* Eight-beat attacks (five idle, two wind-up, one strike), alternating sides. */
    cycle = (beat + CROWD_PERIOD - CROWD_STRIKE_BEAT + 7u) % CROWD_PERIOD;
    side = cycle / 8u % 2u;
    attack = cycle % 8u < 5u ? GAME_ATTACK_IDLE : cycle % 8u < 7u ? GAME_ATTACK_WINDUP : GAME_ATTACK_STRIKE;
  }
  scene_light(scene, olive);
  scene_light(scene, SEG_POPEYE + popeye);
  scene_light(scene, SEG_BRUTUS + side * 3u + attack);
  if ((activity & 1u) != 0u && clock_kiss_time(local))
    scene_kiss(scene, frame % SCENE_KISS_STEPS);
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
