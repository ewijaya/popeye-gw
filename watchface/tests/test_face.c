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

static void test_large_clock_layout(void) {
  int style, hour, minute;
  for (style = 0; style < 2; ++style) {
    for (hour = 0; hour < 24; ++hour) {
      for (minute = 0; minute < 60; ++minute) {
        Scene scene;
        FaceClockLayout layout, blink;
        struct tm local = at(hour, minute, 0);
        int digit, shown_hour = style ? hour : (hour % 12 ? hour % 12 : 12);
        int digits[4] = {shown_hour / 10, shown_hour % 10, minute / 10, minute % 10};
        face_scene(&scene, &local, style != 0, -1);
        face_clock_layout(&scene, &layout);
        /* Even the widest time + label keeps eight pixels beside the bezel. */
        assert(layout.starts[0] >= 8 && layout.right <= 192);
        assert(layout.starts[0] + layout.right >= 199);
        assert(layout.starts[0] + layout.right <= 200);
        for (digit = 0; digit < 4; ++digit) {
          assert(layout.widths[digit] == (digits[digit] == 1 ? 4 : 32));
          if (digit > 0)
            assert(layout.starts[digit] >= layout.starts[digit - 1] + layout.widths[digit - 1] + 8);
        }
        assert(layout.colon_x >= layout.starts[1] + layout.widths[1] + 8);
        assert(layout.colon_x + 4 + 8 <= layout.starts[2]);
        if (style) {
          assert(layout.meridiem_x == -1);
          assert(layout.right == layout.starts[3] + layout.widths[3]);
        } else {
          assert(layout.meridiem_x == layout.starts[3] + layout.widths[3] + 8);
          assert(layout.right == layout.meridiem_x + 11);
        }
        /* A blinking colon must not make the clock jump. */
        scene.bits[SEG_COLON / 32u] &= ~(UINT32_C(1) << (SEG_COLON % 32u));
        face_clock_layout(&scene, &blink);
        assert(blink.colon_x == layout.colon_x && blink.right == layout.right);
        for (digit = 0; digit < 4; ++digit) assert(blink.starts[digit] == layout.starts[digit]);
      }
    }
  }
  /* The reported 08:13 AM layout keeps its label next to the minute digits. */
  Scene scene;
  FaceClockLayout layout;
  struct tm local = at(8, 13, 0);
  face_scene(&scene, &local, false, -1);
  face_clock_layout(&scene, &layout);
  assert(layout.starts[0] == 22 && layout.right == 177);
  assert(layout.meridiem_x == 166);
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
  /* The face must show exactly what the app's attract clock shows at those seconds.
   * Minute 0 is Olive's kiss, compressed differently; see test_kiss. */
  unsigned minute;
  int frame;
  for (minute = 1u; minute < 60u; ++minute) {
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
  for (minute = 1u; minute <= 4u; ++minute) { /* minute 0 is Olive's kiss */
    Scene scene;
    struct tm local = at(10, (int)minute, 0);
    unsigned lane;
    face_scene(&scene, &local, true, 2);
    for (lane = 0u; lane < 4u; ++lane)
      if (count_lit(&scene, SEG_CARGO + lane * 5u, 5u) == 1u) seen |= 1u << lane;
  }
  assert(seen == 0xFu);
}

static void test_continuous_cycle(void) {
  struct tm local = at(16, 23, 59);
  Scene idle, scene;
  unsigned popeye_seen = 0u, brutus_seen = 0u, food_seen = 0u, olive_seen = 0u;
  uint32_t frame;
  face_scene(&idle, &local, false, -1);
  for (frame = 0u; frame < 224u; ++frame) {
    unsigned segment;
    face_scene_active(&scene, &local, false, frame, 7u, false, 1u);
    assert(count_lit(&scene, SEG_OLIVE_READY, 4u) == 1u);
    assert(count_lit(&scene, SEG_POPEYE, 7u) == 1u);
    assert(count_lit(&scene, SEG_BRUTUS, 6u) == 1u);
    assert(count_lit(&scene, SEG_CARGO, 20u) <= 1u);
    assert(!scene_lit(&scene, SEG_BELL));
    assert(count_lit(&scene, SEG_MISS, 5u) == 0u);
    for (segment = SEG_DIGIT; segment <= SEG_PM; ++segment)
      assert(scene_lit(&scene, segment) == scene_lit(&idle, segment));
    for (segment = 0u; segment < 5u; ++segment)
      if (scene_lit(&scene, SEG_POPEYE + segment)) popeye_seen |= 1u << segment;
    for (segment = 0u; segment < 6u; ++segment)
      if (scene_lit(&scene, SEG_BRUTUS + segment)) brutus_seen |= 1u << segment;
    for (segment = 0u; segment < 20u; ++segment)
      if (scene_lit(&scene, SEG_CARGO + segment)) food_seen |= 1u << segment;
    for (segment = 0u; segment < 4u; ++segment)
      if (scene_lit(&scene, SEG_OLIVE_READY + segment)) olive_seen |= 1u << segment;
    if (frame % 7u == 6u) assert(count_lit(&scene, SEG_CARGO, 20u) == 0u);
  }
  assert(popeye_seen == 0x1Fu);
  assert(brutus_seen == 0x3Fu);
  assert(food_seen == 0xFFFFFu);
  assert(olive_seen == 3u);
}

static void test_activity_controls(void) {
  struct tm local = at(8, 1, 0); /* not the hour: Olive's kiss is test_kiss */
  unsigned activity, pause;
  for (activity = 0u; activity < 8u; ++activity) {
    for (pause = 0u; pause <= 10u; ++pause) {
      uint32_t frame;
      for (frame = 0u; frame < 64u; ++frame) {
        Scene scene;
        face_scene_active(&scene, &local, true, frame, activity, false, pause);
        assert(count_lit(&scene, SEG_OLIVE_READY, 4u) == 1u);
        assert(count_lit(&scene, SEG_POPEYE, 7u) == 1u);
        assert(count_lit(&scene, SEG_BRUTUS, 6u) == 1u);
        if (!(activity & 1u)) {
          assert(scene_lit(&scene, SEG_OLIVE_READY));
          assert(count_lit(&scene, SEG_CARGO, 20u) == 0u);
        }
        if (!(activity & 2u)) {
          assert(scene_lit(&scene, SEG_POPEYE + 2u));
          assert(!scene_lit(&scene, SEG_POPEYE_CATCH));
        }
        if (!(activity & 4u)) assert(scene_lit(&scene, SEG_BRUTUS));
      }
    }
  }
}

static void test_celebration_and_frame_wrap(void) {
  struct tm local = at(12, 0, 0);
  uint32_t frame;
  for (frame = UINT32_MAX - 63u; frame != 0u; ++frame) {
    Scene scene, bounded;
    face_scene_active(&scene, &local, false, frame, 7u, true, 100u);
    face_scene_active(&bounded, &local, false, frame, 7u, true, 10u);
    assert(same(&scene, &bounded));
    assert(count_lit(&scene, SEG_OLIVE_READY, 4u) == 1u);
    assert(scene_lit(&scene, SEG_OLIVE_BELL + frame % 2u));
    assert(count_lit(&scene, SEG_POPEYE, 7u) == 1u);
    assert(count_lit(&scene, SEG_BRUTUS, 6u) == 1u);
    assert(scene_lit(&scene, SEG_COLON));
  }
}

static void test_minute_does_not_move_cast(void) {
  /* An ordinary minute rollover never moves the cast mid-cycle. The start of an
   * hour is the one deliberate change: Olive's kiss replaces the food (test_kiss). */
  struct tm before = at(10, 41, 59), after = at(10, 42, 0);
  struct tm hour_before = at(23, 59, 59), hour_after = at(0, 0, 0);
  uint32_t frame;
  for (frame = 0u; frame < 112u; ++frame) {
    Scene a, b;
    unsigned segment;
    face_scene_active(&a, &before, true, frame, 7u, false, 1u);
    face_scene_active(&b, &after, true, frame, 7u, false, 1u);
    for (segment = 0u; segment < SEG_COUNT; ++segment)
      if (segment < SEG_DIGIT || segment > SEG_HI)
        assert(scene_lit(&a, segment) == scene_lit(&b, segment));
    face_scene_active(&a, &hour_before, true, frame, 7u, false, 1u);
    face_scene_active(&b, &hour_after, true, frame, 7u, false, 1u);
    assert(!scene_lit(&a, SEG_OLIVE_KISS) && scene_lit(&b, SEG_OLIVE_KISS));
    for (segment = SEG_BRUTUS; segment < SEG_BRUTUS + 6u; ++segment)
      assert(scene_lit(&a, segment) == scene_lit(&b, segment)); /* Brutus keeps his cycle */
  }
}

/* On the hour and on 14 February the demo is Olive's kiss: it starts with her kiss
 * pose and ends with the heart over Popeye, in both animation modes. */
static void test_kiss(void) {
  Scene scene;
  struct tm local = at(9, 0, 3), valentine = at(15, 27, 3);
  int frame;
  uint32_t step;
  valentine.tm_mon = 1; valentine.tm_mday = 14;
  for (frame = FACE_DEMO_FIRST; frame <= FACE_DEMO_LAST; ++frame) {
    face_scene(&scene, &local, false, frame);
    assert(scene_lit(&scene, SEG_OLIVE_KISS) && count_lit(&scene, SEG_CARGO, 20u) == 0u);
    assert(count_lit(&scene, SEG_HEART, 6u) == (frame == FACE_DEMO_FIRST ? 0u : 1u));
    assert(scene_lit(&scene, SEG_POPEYE_HEART) == (frame == FACE_DEMO_LAST));
    face_scene(&scene, &valentine, false, frame);
    assert(scene_lit(&scene, SEG_OLIVE_KISS));
  }
  face_scene(&scene, &local, false, -1); /* the static clock never kisses */
  assert(!scene_lit(&scene, SEG_OLIVE_KISS) && scene_lit(&scene, SEG_OLIVE_READY));
  for (step = 0u; step < 2u * SCENE_KISS_STEPS; ++step) {
    face_scene_active(&scene, &local, false, step, 7u, false, 0u);
    assert(scene_lit(&scene, SEG_OLIVE_KISS));
    assert(scene_lit(&scene, SEG_POPEYE_HEART) == (step % SCENE_KISS_STEPS >= 6u));
    face_scene_active(&scene, &local, false, step, 6u, false, 0u); /* Olive still */
    assert(!scene_lit(&scene, SEG_OLIVE_KISS));
  }
  local = at(9, 1, 3);
  face_scene_active(&scene, &local, false, 3u, 7u, false, 0u);
  assert(!scene_lit(&scene, SEG_OLIVE_KISS));
}

int main(void) {
  test_static_clock();
  test_clock_style();
  test_large_clock_layout();
  test_demo_beats();
  test_demo_matches_app_clock();
  test_all_lanes_appear();
  test_continuous_cycle();
  test_activity_controls();
  test_celebration_and_frame_wrap();
  test_minute_does_not_move_cast();
  test_kiss();
  puts("face tests passed");
  return 0;
}
