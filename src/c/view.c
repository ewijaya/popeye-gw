#include "view.h"

#include "segments.h"
#include "segments_landscape.h"
#include "orientation.h"

/* Pixel placement is generated from the complete art/segments inventory. */
typedef char CompleteSegmentArt[(SEGMENT_ART_COUNT == SEG_COUNT) ? 1 : -1];

static Layer *s_layer;
static GBitmap *s_backdrop;
static GBitmap *s_ui;
static GColor s_ui_palette[4];
static bool s_landscape, s_loaded_landscape, s_loaded_ghosts;
static GBitmap *s_sheet;
static GBitmap *s_art[SEG_COUNT];
static Scene s_scene;
static ViewOverlay s_overlay;
static bool s_art_ready;
static bool s_show_ghosts, s_save_error, s_panel_visible;
static ViewPanel s_panel;

static void draw_text(GContext *ctx, const char *text, GRect rect, bool bold,
                      GTextAlignment alignment) {
  graphics_draw_text(ctx, text,
      fonts_get_system_font(bold ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14),
      rect, GTextOverflowModeWordWrap, alignment, NULL);
}

static void draw_panel(GContext *ctx) {
  unsigned row;
  unsigned first = s_panel.selected > 3 ? (unsigned)s_panel.selected - 3u : 0u;
  int start = s_landscape ? 33 : 45;
  int spacing = s_landscape ? 31 : 34;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(0, 0, 200, s_landscape ? 200 : 228), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  draw_text(ctx, s_panel.title, GRect(8, 7, 184, 25), true, GTextAlignmentCenter);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(8, s_landscape ? 30 : 36), GPoint(192, s_landscape ? 30 : 36));
  for (row = first; row < s_panel.count && row < first + 4u; ++row) {
    bool selected = (int)row == s_panel.selected;
    GRect box = GRect(8, start + (row - first) * spacing, 184, spacing - 2);
    graphics_context_set_fill_color(ctx, selected ? GColorBlack : GColorWhite);
    graphics_fill_rect(ctx, box, 2, GCornersAll);
    graphics_context_set_text_color(ctx, selected ? GColorWhite : GColorBlack);
    draw_text(ctx, s_panel.rows[row], GRect(13, box.origin.y + 3, 174, 28), true,
              GTextAlignmentLeft);
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  draw_text(ctx, s_panel.footer, GRect(8, s_landscape ? 163 : 187, 184, 37), false, GTextAlignmentCenter);
}

static void draw_segment(GContext *ctx, unsigned seg) {
  const SegmentArt *art = s_loaded_landscape ? &landscape_art[seg] : &segment_art[seg];
  graphics_draw_bitmap_in_rect(ctx, s_art[seg], GRect(art->x, art->y, art->w, art->h));
}

static void draw_overlay(GContext *ctx) {
  static const char *const titles[] = { NULL, "Popeye G&W", "Paused", "Game over", "Alarm" };
  static const char *const hints[] = {
    NULL,
    "Select: Game A\nHold Select: Game B",
    "Select: resume\nBack: quit",
    "Select: play again\nHold Select: other mode",
    "Any button: stop"
  };
  GRect box = GRect(16, s_landscape ? 65 : 78, 168, 60);
  if (s_overlay == VIEW_OVERLAY_NONE) return;
  if (s_overlay == VIEW_OVERLAY_CLOCK) {
    graphics_context_set_text_color(ctx, GColorBlack);
    draw_text(ctx, "Popeye G&W", GRect(24, s_landscape ? 72 : 83, 152, 24), true, GTextAlignmentCenter);
    draw_text(ctx, "Select: A   Hold: B", GRect(24, s_landscape ? 94 : 106, 152, 20), false, GTextAlignmentCenter);
    draw_text(ctx, s_landscape ? "Left: scores   Right: menu" : "Up: scores   Down: menu",
              GRect(8, s_landscape ? 181 : 209, 184, 19), false, GTextAlignmentCenter);
    return;
  }
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

static void draw_notice(GContext *ctx) {
  if (!s_save_error) return;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(0, s_landscape ? 180 : 207, 200, 21), 0, GCornerNone);
  draw_text(ctx, "Save failed - open menu", GRect(0, s_landscape ? 180 : 207, 200, 21),
            false, GTextAlignmentCenter);
}

static bool capture_ui(Layer *layer, GContext *ctx) {
  GBitmap *frame = graphics_capture_frame_buffer_format(ctx, GBitmapFormat8Bit);
  unsigned y;
  uint8_t row[200];
  GPoint origin = layer_convert_point_to_screen(layer, GPoint(0, 0));
  uint8_t *out = gbitmap_get_data(s_ui);
  size_t stride = gbitmap_get_bytes_per_row(s_ui);
  memset(out, 0, stride * 200u);
  if (frame == NULL) return false;
  for (y = 0; y < 200u; ++y) {
    int screen_y = origin.y + (int)y;
    unsigned x;
    memset(row, GColorMagenta.argb, sizeof(row));
    if (screen_y >= 0 && screen_y < 228) {
      GBitmapDataRowInfo info = gbitmap_get_data_row_info(frame, (uint16_t)screen_y);
      for (x = 0; x < 200u; ++x) {
        int screen_x = origin.x + (int)x;
        if (screen_x >= info.min_x && screen_x <= info.max_x) row[x] = info.data[screen_x];
      }
    }
    orientation_pack_row(out, stride, row, y);
  }
  graphics_release_frame_buffer(ctx, frame);
  return true;
}

static void update_proc(Layer *layer, GContext *ctx) {
  unsigned seg;
  bool ui_ready = false;
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  if (s_landscape && s_ui != NULL &&
      (s_panel_visible || s_overlay != VIEW_OVERLAY_NONE || s_save_error)) {
    /* Native fonts are drawn once into a square scratch area, then packed into
     * a 10 KB rotated transparent bitmap. No allocation in the update loop. */
    graphics_context_set_fill_color(ctx, GColorMagenta);
    graphics_fill_rect(ctx, GRect(0, 0, 200, 200), 0, GCornerNone);
    if (s_panel_visible) draw_panel(ctx);
    else { draw_overlay(ctx); draw_notice(ctx); }
    ui_ready = capture_ui(layer, ctx);
  }
  if (s_panel_visible) {
    if (s_landscape && ui_ready) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, s_ui, GRect(0, 14, 200, 200));
    } else draw_panel(ctx);
    return;
  }
  if (!s_art_ready) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorBlack);
    draw_text(ctx, "Unable to load artwork\nBack: exit", GRect(8, 76, 184, 70),
              true, GTextAlignmentCenter);
    return;
  }
  graphics_draw_bitmap_in_rect(ctx, s_backdrop, layer_get_bounds(layer));
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  for (seg = 0u; seg < SEG_COUNT; ++seg) {
    if (scene_lit(&s_scene, seg)) draw_segment(ctx, seg);
  }
  if (s_landscape && ui_ready) graphics_draw_bitmap_in_rect(ctx, s_ui, GRect(0, 14, 200, 200));
  else { graphics_context_set_compositing_mode(ctx, GCompOpAssign); draw_overlay(ctx); draw_notice(ctx); }
}

static void unload_scene(void) {
  unsigned seg;
  for (seg = 0; seg < SEG_COUNT; ++seg) {
    if (s_art[seg] != NULL) gbitmap_destroy(s_art[seg]);
    s_art[seg] = NULL;
  }
  if (s_sheet != NULL) gbitmap_destroy(s_sheet);
  if (s_backdrop != NULL) gbitmap_destroy(s_backdrop);
  s_sheet = s_backdrop = NULL;
  s_art_ready = false;
}

static void load_scene(void) {
  unsigned seg;
  uint32_t backdrop_id;
  unload_scene();
  s_loaded_landscape = s_landscape;
  s_loaded_ghosts = s_show_ghosts;
  backdrop_id = s_landscape ? (s_show_ghosts ? RESOURCE_ID_BACKDROP_GHOSTS_LANDSCAPE : RESOURCE_ID_BACKDROP_LANDSCAPE)
                            : (s_show_ghosts ? RESOURCE_ID_BACKDROP_GHOSTS : RESOURCE_ID_BACKDROP);
  s_backdrop = gbitmap_create_with_resource(backdrop_id);
  s_sheet = gbitmap_create_with_resource(s_landscape ? RESOURCE_ID_SEGMENTS_LANDSCAPE : RESOURCE_ID_SEGMENTS);
  s_art_ready = s_backdrop != NULL && s_sheet != NULL;
  if (s_art_ready) {
    for (seg = 0; seg < SEG_COUNT; ++seg) {
      const SegmentArt *art = s_loaded_landscape ? &landscape_art[seg] : &segment_art[seg];
      s_art[seg] = gbitmap_create_as_sub_bitmap(s_sheet,
                      GRect(art->sheet_x, art->sheet_y, art->w, art->h));
      if (s_art[seg] == NULL) s_art_ready = false;
    }
  }
  if (!s_art_ready) APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to load complete segment art");
}

void view_init(Layer *parent, bool ghosts) {
  s_show_ghosts = ghosts;
  s_ui_palette[0] = s_ui_palette[3] = GColorClear;
  s_ui_palette[1] = GColorBlack;
  s_ui_palette[2] = GColorWhite;
  s_ui = gbitmap_create_blank_with_palette(GSize(200, 200), GBitmapFormat2BitPalette, s_ui_palette, false);
  if (s_ui == NULL) APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to allocate rotated UI");
  load_scene();
  s_layer = layer_create(layer_get_bounds(parent));
  if (s_layer == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to create art layer");
    return;
  }
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(parent, s_layer);
}

void view_deinit(void) {
  if (s_layer != NULL) layer_destroy(s_layer);
  s_layer = NULL;
  unload_scene();
  if (s_ui != NULL) gbitmap_destroy(s_ui);
  s_ui = NULL;
}

void view_set_landscape(bool landscape) {
  s_landscape = landscape;
}

void view_show(const Scene *scene, ViewOverlay overlay, bool ghosts, bool save_error) {
  s_panel_visible = false;
  s_show_ghosts = ghosts;
  if (s_loaded_landscape != s_landscape || s_loaded_ghosts != ghosts) load_scene();
  s_save_error = save_error;
  s_scene = *scene;
  s_overlay = overlay;
  if (s_layer != NULL) layer_mark_dirty(s_layer);
}

void view_panel(const ViewPanel *panel) {
  s_panel = *panel;
  s_panel_visible = true;
  if (s_layer != NULL) layer_mark_dirty(s_layer);
}
