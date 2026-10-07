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

/* Every Still scene is a frame the game could show: one pose per character, food
 * within the launch rules, and a catch flash only under food on its last stage. */
static void assert_still_valid(const Scene *scene, const struct tm *local, bool style_24h) {
  Scene idle;
  unsigned segment, lane, pose = 5u, side = 2u, attack = 0u, food = 0u;
  int stages[GAME_LANES];
  face_scene(&idle, local, style_24h, -1);
  for (segment = SEG_DIGIT; segment <= SEG_PM; ++segment) /* time and colon intact */
    assert(scene_lit(scene, segment) == scene_lit(&idle, segment));
  assert(count_lit(scene, SEG_OLIVE_READY, 4u) + scene_lit(scene, SEG_OLIVE_KISS) == 1u);
  assert(count_lit(scene, SEG_POPEYE, 7u) == 1u);
  assert(count_lit(scene, SEG_BRUTUS, 6u) == 1u);
  assert(count_lit(scene, SEG_SPLASH, 4u) == 0u && count_lit(scene, SEG_MISS, 5u) == 0u);
  assert(!scene_lit(scene, SEG_BELL) && !scene_lit(scene, SEG_GAME_A) &&
         !scene_lit(scene, SEG_GAME_B) && !scene_lit(scene, SEG_HI));
  assert(count_lit(scene, SEG_HEART, 5u) == 0u); /* no heart left in mid-flight */
  for (segment = 0u; segment < 5u; ++segment)
    if (scene_lit(scene, SEG_POPEYE + segment)) pose = segment;
  for (segment = 0u; segment < 6u; ++segment)
    if (scene_lit(scene, SEG_BRUTUS + segment)) { side = segment / 3u; attack = segment % 3u; }
  assert(pose < 5u && side < 2u);
  for (lane = 0u; lane < GAME_LANES; ++lane) {
    unsigned stage;
    assert(count_lit(scene, SEG_CARGO + lane * 5u, 5u) <= 1u);
    stages[lane] = -1;
    for (stage = 0u; stage < 5u; ++stage)
      if (scene_lit(scene, SEG_CARGO + lane * 5u + stage)) { stages[lane] = (int)stage; ++food; }
  }
  if (clock_kiss_time(local)) {
    assert(scene_lit(scene, SEG_OLIVE_KISS) && scene_lit(scene, SEG_POPEYE_HEART));
    assert(pose == 2u && food == 0u && !scene_lit(scene, SEG_POPEYE_CATCH));
    assert(attack == GAME_ATTACK_IDLE);
    return;
  }
  assert(!scene_lit(scene, SEG_OLIVE_KISS) && !scene_lit(scene, SEG_POPEYE_HEART));
  assert(food == 1u || food == 2u);
  for (lane = 0u; lane < GAME_LANES; ++lane) {
    unsigned other;
    if (stages[lane] < 0) continue;
    /* A rival's wind-up or strike never meets food in the lane he threatens. */
    if (attack != GAME_ATTACK_IDLE) assert(lane != (side == GAME_LEFT ? 0u : 3u));
    for (other = lane + 1u; other < GAME_LANES; ++other) {
      int a = game_lane_pose((uint8_t)lane), b = game_lane_pose((uint8_t)other);
      int gap = stages[lane] - stages[other];
      if (stages[other] < 0) continue;
      /* can_launch: one launch per step, landings at least the pose distance apart. */
      assert(gap != 0 && (gap < 0 ? -gap : gap) >= (a < b ? b - a : a - b));
    }
  }
  assert(scene_lit(scene, SEG_OLIVE_THROW) ==
         (stages[0] == 0 || stages[1] == 0 || stages[2] == 0 || stages[3] == 0));
  if (scene_lit(scene, SEG_POPEYE_CATCH)) {
    for (lane = 0u; lane < GAME_LANES; ++lane)
      if (game_lane_pose((uint8_t)lane) == pose) assert(stages[lane] == 4);
  }
}

static void test_still_scene(void) {
  int style, hour, minute, second;
  unsigned combos = 0u, pairs = 0u, catches = 0u;
  for (style = 0; style < 2; ++style) {
    for (hour = 0; hour < 24; ++hour) {
      for (minute = 0; minute < 60; ++minute) {
        Scene scene, other;
        struct tm local = at(hour, minute, 0);
        unsigned lane;
        face_scene_still(&scene, &local, style != 0);
        assert_still_valid(&scene, &local, style != 0);
        for (second = 1; second < 60; ++second) { /* fixed for the whole minute */
          local.tm_sec = second;
          face_scene_still(&other, &local, style != 0);
          assert(same(&scene, &other));
        }
        if (minute == 0) continue;
        /* The paused demo food is clock_scene's: lane by minute, stage by beat. */
        lane = (unsigned)minute % 4u;
        assert(count_lit(&scene, SEG_CARGO + lane * 5u, 5u) == 1u);
        for (lane = 0u; lane < 20u; ++lane)
          if (scene_lit(&scene, SEG_CARGO + lane)) combos |= 1u << lane;
        if (count_lit(&scene, SEG_CARGO, 20u) == 2u) ++pairs;
        if (scene_lit(&scene, SEG_POPEYE_CATCH)) ++catches;
        /* Neighbouring minutes always differ, so the face visibly drifts. */
        local = at(hour, minute - 1, 0);
        face_scene_still(&other, &local, style != 0);
        assert(!same(&scene, &other));
      }
    }
  }
  assert(combos == 0xFFFFFu && pairs > 0u && catches > 0u);
}

static void test_still_kiss(void) {
  Scene scene, again;
  struct tm local = at(9, 0, 0), next = at(9, 1, 0), valentine = at(15, 27, 41);
  valentine.tm_mon = 1; valentine.tm_mday = 14;
  face_scene_still(&scene, &local, false);
  assert(scene_lit(&scene, SEG_OLIVE_KISS) && scene_lit(&scene, SEG_POPEYE_HEART));
  local.tm_sec = 59;
  face_scene_still(&again, &local, false);
  assert(same(&scene, &again)); /* one fixed kiss frame, not an animation */
  face_scene_still(&scene, &next, false);
  assert(!scene_lit(&scene, SEG_OLIVE_KISS) && !scene_lit(&scene, SEG_POPEYE_HEART));
  face_scene_still(&scene, &valentine, true);
  assert(scene_lit(&scene, SEG_OLIVE_KISS) && scene_lit(&scene, SEG_POPEYE_HEART));
  assert(count_lit(&scene, SEG_CARGO, 20u) == 0u);
  valentine.tm_mday = 15;
  face_scene_still(&scene, &valentine, true);
  assert(!scene_lit(&scene, SEG_OLIVE_KISS));
}

/* The game's static clock and every other mode's static face are unchanged by
 * the Still scene: a checksum of their full-day output taken before it existed. */
static void test_static_paths_unchanged(void) {
  uint32_t hash = UINT32_C(2166136261);
  int style, month, hour, minute, second;
  unsigned pause;
  for (style = 0; style < 2; ++style)
    for (month = 0; month < 2; ++month)
      for (hour = 0; hour < 24; ++hour)
        for (minute = 0; minute < 60; ++minute)
          for (second = 0; second < 60; second += 29) {
            Scene scene[5];
            struct tm local = at(hour, minute, second);
            size_t i;
            local.tm_mon = month ? 1 : 9; local.tm_mday = 14;
            clock_scene(&scene[0], &local, style != 0, false, false, false);
            face_scene(&scene[1], &local, style != 0, -1);
            for (pause = 0u; pause < 3u; ++pause)
              face_scene_active(&scene[2u + pause], &local, style != 0, 0u, 0u, false, pause * 5u);
            for (i = 0u; i < sizeof(scene); ++i) {
              hash ^= ((const unsigned char *)scene)[i];
              hash *= UINT32_C(16777619);
            }
          }
  assert(hash == UINT32_C(0xE3414225));
}

/* The Arcade pattern is checked from what is lit, against the game's own rules. */
typedef struct { int pose, side, attack, stage[GAME_LANES], foods, throwing, kiss, catching; } CrowdFrame;

static CrowdFrame read_crowd(const Scene *scene) {
  CrowdFrame f;
  unsigned i, lane, stage;
  memset(&f, 0, sizeof(f));
  f.pose = f.side = f.attack = -1;
  assert(count_lit(scene, SEG_POPEYE, 7u) == 1u && count_lit(scene, SEG_BRUTUS, 6u) == 1u);
  assert(count_lit(scene, SEG_OLIVE_READY, 4u) + scene_lit(scene, SEG_OLIVE_KISS) == 1u);
  for (i = 0u; i < 5u; ++i) if (scene_lit(scene, SEG_POPEYE + i)) f.pose = (int)i;
  assert(f.pose >= 0); /* never the dizzy poses */
  for (i = 0u; i < 6u; ++i)
    if (scene_lit(scene, SEG_BRUTUS + i)) { f.side = (int)i / 3; f.attack = (int)i % 3; }
  for (lane = 0u; lane < GAME_LANES; ++lane) {
    f.stage[lane] = -1;
    assert(count_lit(scene, SEG_CARGO + lane * 5u, 5u) <= 1u);
    for (stage = 0u; stage < 5u; ++stage)
      if (scene_lit(scene, SEG_CARGO + lane * 5u + stage)) { f.stage[lane] = (int)stage; ++f.foods; }
  }
  f.throwing = scene_lit(scene, SEG_OLIVE_THROW);
  f.kiss = scene_lit(scene, SEG_OLIVE_KISS);
  f.catching = scene_lit(scene, SEG_POPEYE_CATCH);
  assert(count_lit(scene, SEG_SPLASH, 4u) == 0u && count_lit(scene, SEG_MISS, 5u) == 0u);
  assert(!scene_lit(scene, SEG_BELL) && !scene_lit(scene, SEG_HEART + 0u));
  return f;
}

static void test_crowded_arcade(void) {
  struct tm local = at(10, 41, 7); /* not the hour or 14 February */
  Scene idle, scene;
  CrowdFrame frames[96];
  unsigned beat, lane, two_or_more = 0u, kiss_beats = 0u, throws = 0u, sides_struck = 0u, lanes_caught = 0u;
  face_scene(&idle, &local, false, -1);
  for (beat = 0u; beat < 96u; ++beat) { /* three full periods */
    unsigned segment;
    face_scene_crowd(&scene, &local, false, beat, 7u);
    frames[beat] = read_crowd(&scene);
    for (segment = SEG_DIGIT; segment <= SEG_PM; ++segment) /* the time never changes */
      assert(scene_lit(&scene, segment) == scene_lit(&idle, segment));
  }
  for (beat = 0u; beat < 96u; ++beat) {
    const CrowdFrame *f = &frames[beat], *before = &frames[(beat + 95u) % 96u];
    const CrowdFrame *after = &frames[(beat + 1u) % 96u];
    int moved = f->pose - after->pose;
    /* Always food in flight, never more than the game's three, two or more most beats. */
    assert(f->foods >= 1 && f->foods <= 3);
    if (f->foods >= 2) ++two_or_more;
    assert(moved >= -1 && moved <= 1); /* one pose per beat, across the period wrap too */
    /* Olive throws exactly while a food leaves her hands, and only blows a kiss while still. */
    for (lane = 0u; lane < GAME_LANES; ++lane) if (f->stage[lane] == 0) ++throws;
    assert(f->throwing == (f->stage[0] == 0 || f->stage[1] == 0 || f->stage[2] == 0 || f->stage[3] == 0));
    assert(!(f->throwing && f->kiss));
    kiss_beats += (unsigned)f->kiss;
    /* The catch flash is Popeye under food at its last position, and only then. */
    {
      int under = -1;
      for (lane = 0u; lane < GAME_LANES; ++lane)
        if (f->stage[lane] == 4 && game_lane_pose((uint8_t)lane) == (unsigned)f->pose) under = (int)lane;
      assert(f->catching == (under >= 0));
      if (f->catching) lanes_caught |= 1u << under;
    }
    /* Every food Popeye is due to catch finds him there: no food reaches position 5 unmet. */
    for (lane = 0u; lane < GAME_LANES; ++lane)
      if (f->stage[lane] == 4) assert(game_lane_pose((uint8_t)lane) == (unsigned)f->pose);
    /* Brutus: five idle beats, two wind-up, one strike, then the other side. */
    if (f->attack == GAME_ATTACK_STRIKE) {
      unsigned threatened = f->side == GAME_LEFT ? 0u : 3u;
      assert(before->attack == GAME_ATTACK_WINDUP && before->side == f->side);
      assert(after->attack == GAME_ATTACK_IDLE && after->side != f->side);
      assert(f->pose != (f->side == GAME_LEFT ? 0 : 4)); /* Popeye is never hit */
      /* can_launch / strike_conflicts_cargo: no landing in that lane on the strike or the beat before. */
      assert(f->stage[threatened] != 4 && before->stage[threatened] != 4);
      sides_struck |= 1u << f->side;
    }
    if (f->attack == GAME_ATTACK_WINDUP && before->attack == GAME_ATTACK_IDLE)
      assert(after->attack == GAME_ATTACK_WINDUP && frames[(beat + 2u) % 96u].attack == GAME_ATTACK_STRIKE);
    /* Landings keep the game's spacing: two foods are never nearer in time than Popeye's travel. */
    for (lane = 0u; lane < GAME_LANES; ++lane) {
      unsigned other;
      for (other = lane + 1u; other < GAME_LANES; ++other) {
        int a = (int)game_lane_pose((uint8_t)lane), b = (int)game_lane_pose((uint8_t)other);
        int gap = f->stage[lane] - f->stage[other];
        if (f->stage[lane] < 0 || f->stage[other] < 0) continue;
        assert(gap != 0 && (gap < 0 ? -gap : gap) >= (a < b ? b - a : a - b));
      }
    }
  }
  assert(two_or_more >= 96u * 3u / 4u);  /* crowded: two foods or more on at least three beats in four */
  assert(throws == 36u);                 /* twelve throws per 32 beats, one per beat at most */
  assert(kiss_beats == 12u);             /* two kisses of two beats each period */
  assert(sides_struck == 3u && lanes_caught == 0xFu); /* both sides strike and all four lanes are caught */
  /* Same frame, same picture: the pattern repeats every 32 beats and across the counter's wrap. */
  face_scene_crowd(&idle, &local, false, 5u, 7u);
  face_scene_crowd(&scene, &local, false, 5u + 32u * 1000u, 7u);
  assert(same(&idle, &scene));
  face_scene_crowd(&idle, &local, false, UINT32_MAX, 7u);
  face_scene_crowd(&scene, &local, false, 31u, 7u);
  assert(same(&idle, &scene));
  face_scene_crowd(&idle, &local, false, 0u, 7u);
  face_scene_crowd(&scene, &local, false, 0u, 7u);
  assert(same(&idle, &scene));
}

static void test_crowded_arcade_activity_and_kiss(void) {
  struct tm local = at(14, 23, 0), hour = at(9, 0, 0), valentine = at(15, 27, 3);
  unsigned activity, beat, hour_kisses = 0u;
  Scene scene, plain;
  valentine.tm_mon = 1; valentine.tm_mday = 14;
  for (activity = 0u; activity < 8u; ++activity) {
    for (beat = 0u; beat < 64u; ++beat) {
      CrowdFrame f;
      face_scene_crowd(&scene, &local, true, beat, activity);
      f = read_crowd(&scene);
      if (!(activity & 1u)) assert(f.foods == 0 && !f.throwing && !f.kiss && !f.catching);
      if (!(activity & 2u)) assert(f.pose == 2 && !f.catching);
      if (!(activity & 4u)) assert(f.side == GAME_LEFT && f.attack == GAME_ATTACK_IDLE);
    }
  }
  /* On the hour and on 14 February Olive's kiss replaces the food, as in the other modes. */
  for (beat = 0u; beat < 64u; ++beat) {
    face_scene_crowd(&scene, &hour, false, beat, 7u);
    assert(scene_lit(&scene, SEG_OLIVE_KISS) && count_lit(&scene, SEG_CARGO, 20u) == 0u);
    assert(count_lit(&scene, SEG_BRUTUS, 6u) == 1u && count_lit(&scene, SEG_POPEYE, 7u) == 1u);
    hour_kisses += scene_lit(&scene, SEG_POPEYE_HEART) ? 1u : 0u;
    face_scene_crowd(&scene, &valentine, false, beat, 7u);
    assert(scene_lit(&scene, SEG_OLIVE_KISS) && count_lit(&scene, SEG_CARGO, 20u) == 0u);
    face_scene_crowd(&scene, &hour, false, beat, 6u); /* Olive still: no kiss */
    assert(!scene_lit(&scene, SEG_OLIVE_KISS));
  }
  assert(hour_kisses > 0u);
  /* Every minute of the day keeps its digits and the same time layout. */
  for (beat = 0u; beat < 1440u; ++beat) {
    FaceClockLayout a, b;
    struct tm t = at((int)beat / 60, (int)beat % 60, 0);
    face_scene(&plain, &t, beat % 2u != 0u, -1);
    face_scene_crowd(&scene, &t, beat % 2u != 0u, beat, 7u);
    face_clock_layout(&plain, &a); face_clock_layout(&scene, &b);
    assert(a.starts[0] == b.starts[0] && a.right == b.right && a.colon_x == b.colon_x);
  }
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
  test_still_scene();
  test_still_kiss();
  test_static_paths_unchanged();
  test_crowded_arcade();
  test_crowded_arcade_activity_and_kiss();
  puts("face tests passed");
  return 0;
}
