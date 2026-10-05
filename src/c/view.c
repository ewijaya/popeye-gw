#include "view.h"

/* M2 placeholder geometry for the PRD 9.2 segments. M3 replaces it with the
 * generated segment table; nothing outside this file depends on pixels. */
#define POSE_X(pose) (36 + 32 * (pose))
#define DIGIT_W 14
#define DIGIT_H 20
#define DIGIT_Y 2

static const int16_t s_lane_x[GAME_LANES] = { POSE_X(0), POSE_X(1), POSE_X(3), POSE_X(4) };
static const int16_t s_digit_x[SCENE_DIGITS] = { 108, 126, 150, 168 };

static Layer *s_layer;
static Scene s_scene;
static ViewOverlay s_overlay;

static GRect digit_rect(unsigned digit, unsigned bar) {
  int16_t x = s_digit_x[digit];
  switch (bar) {
    case 0: return GRect(x + 2, DIGIT_Y, DIGIT_W - 4, 2);
    case 1: return GRect(x + DIGIT_W - 2, DIGIT_Y + 2, 2, 7);
    case 2: return GRect(x + DIGIT_W - 2, DIGIT_Y + 11, 2, 7);
    case 3: return GRect(x + 2, DIGIT_Y + DIGIT_H - 2, DIGIT_W - 4, 2);
    case 4: return GRect(x, DIGIT_Y + 11, 2, 7);
    case 5: return GRect(x, DIGIT_Y + 2, 2, 7);
    default: return GRect(x + 2, DIGIT_Y + 9, DIGIT_W - 4, 2);
  }
}

static GRect segment_rect(unsigned seg) {
  if (seg < SEG_MAE_THROW) return GRect(s_lane_x[seg - SEG_MAE_READY] - 9, 38, 9, 24);
  if (seg < SEG_MAE_BELL) return GRect(s_lane_x[seg - SEG_MAE_THROW] + 1, 34, 9, 20);
  if (seg < SEG_CARGO) return GRect(4 + 10 * (seg - SEG_MAE_BELL), 38, 9, 24);
  if (seg < SEG_FINN) {
    unsigned lane = (seg - SEG_CARGO) / GAME_CARGO_STEPS;
    unsigned stage = (seg - SEG_CARGO) % GAME_CARGO_STEPS;
    return GRect(s_lane_x[lane] - 5, 72 + 16 * stage, 10, 10);
  }
  if (seg < SEG_FINN_DIZZY_LEFT) return GRect(POSE_X(seg - SEG_FINN) - 6, 152, 12, 24);
  if (seg == SEG_FINN_DIZZY_LEFT) return GRect(POSE_X(0) - 9, 158, 18, 14);
  if (seg == SEG_FINN_DIZZY_RIGHT) return GRect(POSE_X(4) - 9, 158, 18, 14);
  if (seg == SEG_FINN_CATCH) return GRect(90, 148, 20, 3);
  if (seg < SEG_GRIZZLE) return GRect(s_lane_x[seg - SEG_SPLASH] - 8, 192, 16, 6);
  if (seg < SEG_RING) {
    unsigned side = (seg - SEG_GRIZZLE) / 3u;
    unsigned phase = (seg - SEG_GRIZZLE) % 3u;
    static const GRect left[3] = {
      { { 2, 150 }, { 12, 24 } }, { { 2, 128 }, { 12, 18 } }, { { 14, 160 }, { 14, 6 } }
    };
    GRect rect = left[phase];
    if (side == 1u) rect.origin.x = (int16_t)(200 - rect.origin.x - rect.size.w);
    return rect;
  }
  if (seg < SEG_RING_HALF) return GRect(8 + 20 * (seg - SEG_RING), 204, 16, 16);
  if (seg == SEG_RING_HALF) return GRect(68, 204, 8, 16);
  if (seg < SEG_COLON) return digit_rect((seg - SEG_DIGIT) / 7u, (seg - SEG_DIGIT) % 7u);
  switch (seg) {
    case SEG_COLON: return GRect(145, 7, 2, 10);
    case SEG_AM: return GRect(186, 4, 10, 6);
    case SEG_PM: return GRect(186, 14, 10, 6);
    case SEG_GAME_A: return GRect(4, 0, 40, 12);
    case SEG_GAME_B: return GRect(4, 11, 40, 12);
    case SEG_BELL: return GRect(50, 5, 12, 14);
    case SEG_GULL: return GRect(66, 7, 16, 10);
    default: return GRect(86, 3, 16, 16); /* SEG_HI */
  }
}

static const char *segment_label(unsigned seg) {
  switch (seg) {
    case SEG_GAME_A: return "GAME A";
    case SEG_GAME_B: return "GAME B";
    case SEG_HI: return "HI";
    default: return NULL;
  }
}

static void draw_backdrop(GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, 24), GPoint(199, 24));
  graphics_draw_line(ctx, GPoint(0, 30), GPoint(199, 30));   /* deck rail */
  graphics_draw_line(ctx, GPoint(24, 180), GPoint(176, 180)); /* rowboat */
  graphics_draw_line(ctx, GPoint(24, 180), GPoint(34, 188));
  graphics_draw_line(ctx, GPoint(176, 180), GPoint(166, 188));
  graphics_draw_line(ctx, GPoint(34, 188), GPoint(166, 188));
  graphics_draw_rect(ctx, GRect(0, 176, 18, 10));             /* piers */
  graphics_draw_rect(ctx, GRect(182, 176, 18, 10));
  graphics_draw_line(ctx, GPoint(0, 190), GPoint(199, 190));  /* waterline */
}

static void draw_segment(GContext *ctx, unsigned seg, bool lit) {
  GRect rect = segment_rect(seg);
  const char *label = segment_label(seg);
  GColor colour = lit ? GColorBlack : GColorMediumAquamarine;
  if (label != NULL) {
    graphics_context_set_text_color(ctx, colour);
    graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_09), rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else if (lit) {
    graphics_context_set_fill_color(ctx, colour);
    graphics_fill_rect(ctx, rect, 0, GCornerNone);
  } else {
    graphics_context_set_stroke_color(ctx, colour);
    graphics_draw_rect(ctx, rect);
  }
}

static void draw_overlay(GContext *ctx) {
  static const char *const titles[] = { NULL, "Harbor Catch", "Paused", "Game over" };
  static const char *const hints[] = {
    NULL,
    "Select: Game A\nHold Select: Game B",
    "Select: resume\nBack: quit",
    "Select: play again\nHold Select: other mode"
  };
  GRect box = GRect(16, 78, 168, 60);
  if (s_overlay == VIEW_OVERLAY_NONE) return;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, box, 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, box, 4);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, titles[s_overlay], fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(box.origin.x, box.origin.y, box.size.w, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, hints[s_overlay], fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(box.origin.x, box.origin.y + 22, box.size.w, box.size.h - 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void update_proc(Layer *layer, GContext *ctx) {
  unsigned seg;
  graphics_context_set_fill_color(ctx, GColorMintGreen);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  draw_backdrop(ctx);
  for (seg = 0u; seg < SEG_COUNT; ++seg) draw_segment(ctx, seg, scene_lit(&s_scene, seg));
  draw_overlay(ctx);
}

void view_init(Layer *parent) {
  s_layer = layer_create(layer_get_bounds(parent));
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(parent, s_layer);
}

void view_deinit(void) {
  layer_destroy(s_layer);
  s_layer = NULL;
}

void view_show(const Scene *scene, ViewOverlay overlay) {
  s_scene = *scene;
  s_overlay = overlay;
  layer_mark_dirty(s_layer);
}
