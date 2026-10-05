#include "view.h"

#include "segments.h"

/* Pixel placement is generated from the complete art/segments inventory. */
typedef char CompleteSegmentArt[(SEGMENT_ART_COUNT == SEG_COUNT) ? 1 : -1];

static Layer *s_layer;
static GBitmap *s_backdrop;
static GBitmap *s_sheet;
static GBitmap *s_art[SEG_COUNT];
static Scene s_scene;
static ViewOverlay s_overlay;
static bool s_art_ready;

static void draw_segment(GContext *ctx, unsigned seg) {
  const SegmentArt *art = &segment_art[seg];
  graphics_draw_bitmap_in_rect(ctx, s_art[seg], GRect(art->x, art->y, art->w, art->h));
}

static void draw_overlay(GContext *ctx) {
  static const char *const titles[] = { NULL, "Popeye G&W", "Paused", "Game over" };
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
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  if (!s_art_ready) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "Unable to load artwork\nBack: exit",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(8, 76, 184, 70), GTextOverflowModeWordWrap,
                       GTextAlignmentCenter, NULL);
    return;
  }
  graphics_draw_bitmap_in_rect(ctx, s_backdrop, layer_get_bounds(layer));
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_text_color(ctx, GColorBlack);
  for (seg = 0u; seg < SEG_COUNT; ++seg) {
    if (scene_lit(&s_scene, seg)) draw_segment(ctx, seg);
  }
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  draw_overlay(ctx);
}

void view_init(Layer *parent) {
  unsigned seg;
  s_backdrop = gbitmap_create_with_resource(RESOURCE_ID_BACKDROP_GHOSTS);
  s_sheet = gbitmap_create_with_resource(RESOURCE_ID_SEGMENTS);
  s_art_ready = s_backdrop != NULL && s_sheet != NULL;
  if (s_art_ready) {
    for (seg = 0u; seg < SEG_COUNT; ++seg) {
      const SegmentArt *art = &segment_art[seg];
      s_art[seg] = gbitmap_create_as_sub_bitmap(s_sheet,
                      GRect(art->sheet_x, art->sheet_y, art->w, art->h));
      if (s_art[seg] == NULL) s_art_ready = false;
    }
  }
  if (!s_art_ready) APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to load complete segment art");
  s_layer = layer_create(layer_get_bounds(parent));
  if (s_layer == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to create art layer");
    return;
  }
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(parent, s_layer);
}

void view_deinit(void) {
  unsigned seg;
  if (s_layer != NULL) layer_destroy(s_layer);
  s_layer = NULL;
  for (seg = 0u; seg < SEG_COUNT; ++seg) {
    if (s_art[seg] != NULL) gbitmap_destroy(s_art[seg]);
    s_art[seg] = NULL;
  }
  if (s_sheet != NULL) gbitmap_destroy(s_sheet);
  if (s_backdrop != NULL) gbitmap_destroy(s_backdrop);
  s_sheet = NULL;
  s_backdrop = NULL;
  s_art_ready = false;
}

void view_show(const Scene *scene, ViewOverlay overlay) {
  s_scene = *scene;
  s_overlay = overlay;
  if (s_layer != NULL) layer_mark_dirty(s_layer);
}
