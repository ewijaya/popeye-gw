#include "face.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct tm at(int hour, int minute, int second) {
  struct tm local;
  memset(&local, 0, sizeof(local));
  local.tm_year = 126;
  local.tm_mon = 9;
  local.tm_mday = 6;
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = second;
  return local;
}

static unsigned count_lit(const Scene *scene, unsigned first, unsigned count) {
  unsigned i, lit = 0u;
  for (i = 0u; i < count; ++i) lit += scene_lit(scene, first + i) ? 1u : 0u;
  return lit;
}

static bool same(const Scene *a, const Scene *b) { return memcmp(a, b, sizeof(*a)) == 0; }

static void test_static_clock(void) {
  Scene scene, other;
  struct tm local = at(15, 7, 33);
  face_scene(&scene, &local, false, -1);
  assert(scene_lit(&scene, SEG_PM) && !scene_lit(&scene, SEG_AM));
  assert(scene_lit(&scene, SEG_COLON));             /* steady, never blinking */
  assert(scene_lit(&scene, SEG_OLIVE_READY));
  assert(!scene_lit(&scene, SEG_OLIVE_THROW));
  assert(count_lit(&scene, SEG_CARGO, 20u) == 0u);  /* no food while idle */
  assert(!scene_lit(&scene, SEG_BELL));             /* the face has no alarm */
  assert(scene_lit(&scene, SEG_POPEYE + 2u));
  assert(!scene_lit(&scene, SEG_POPEYE_CATCH));
  /* Seconds never change the idle picture. */
  local = at(15, 7, 59);
  face_scene(&other, &local, false, -1);
  assert(same(&scene, &other));
  /* Out-of-range frames are the idle picture too. */
  face_scene(&other, &local, false, FACE_DEMO_LAST + 1);
  assert(same(&scene, &other));
}

static void test_clock_style(void) {
  Scene scene;
  struct tm local = at(0, 5, 0);
  face_scene(&scene, &local, true, -1);
  assert(!scene_lit(&scene, SEG_AM) && !scene_lit(&scene, SEG_PM));
  face_scene(&scene, &local, false, -1);
  assert(scene_lit(&scene, SEG_AM));
  local = at(12, 0, 0);
  face_scene(&scene, &local, false, -1);
  assert(scene_lit(&scene, SEG_PM));
}

static void test_demo_beats(void) {
  Scene idle, scene;
  struct tm local = at(9, 41, 0);
  int frame;
  face_scene(&idle, &local, true, -1);
  for (frame = FACE_DEMO_FIRST; frame <= FACE_DEMO_LAST; ++frame) {
    unsigned digit;
    face_scene(&scene, &local, true, frame);
    assert(scene_lit(&scene, SEG_COLON));
    /* Same time digits as the idle face: only the food scene changes. */
    for (digit = 0u; digit < 28u; ++digit)
      assert(scene_lit(&scene, SEG_DIGIT + digit) == scene_lit(&idle, SEG_DIGIT + digit));
    assert(!scene_lit(&scene, SEG_BELL));
    assert(count_lit(&scene, SEG_CARGO, 20u) == 1u);
    assert(scene_lit(&scene, SEG_OLIVE_THROW) == (frame == 1));
    assert(scene_lit(&scene, SEG_POPEYE_CATCH) == (frame == FACE_DEMO_LAST));
  }
}

static void test_demo_matches_app_clock(void) {
  /* The face must show exactly what the app's attract clock shows at those seconds. */
  unsigned minute;
  int frame;
  for (minute = 0u; minute < 60u; ++minute) {
    for (frame = FACE_DEMO_FIRST; frame <= FACE_DEMO_LAST; ++frame) {
      Scene face, app;
      struct tm local = at(7, (int)minute, 17);
      struct tm app_time = at(7, (int)minute, frame * 2);
      face_scene(&face, &local, false, frame);
      clock_scene(&app, &app_time, false, true, false, false);
      assert(same(&face, &app));
    }
  }
}

static void test_all_lanes_appear(void) {
  unsigned seen = 0u, minute;
  for (minute = 0u; minute < 4u; ++minute) {
    Scene scene;
    struct tm local = at(10, (int)minute, 0);
    unsigned lane;
    face_scene(&scene, &local, true, 2);
    for (lane = 0u; lane < 4u; ++lane)
      if (count_lit(&scene, SEG_CARGO + lane * 5u, 5u) == 1u) seen |= 1u << lane;
  }
  assert(seen == 0xFu);
}

int main(void) {
  test_static_clock();
  test_clock_style();
  test_demo_beats();
  test_demo_matches_app_clock();
  test_all_lanes_appear();
  puts("face tests passed");
  return 0;
}
