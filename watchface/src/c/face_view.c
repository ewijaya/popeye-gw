#include "face_view.h"

#include "data.h"
#include "face.h"
#include "segments.h"

#include <stdio.h>
#include <string.h>

typedef char CompleteSegmentArt[(SEGMENT_ART_COUNT == SEG_COUNT) ? 1 : -1];

static GFont s_info_font;
static GBitmap *s_backdrop, *s_sheet, *s_art[SEG_COUNT], *s_ghost_art[28];
static GColor s_lit_palette[2], s_ghost_palette[2];
/* Keep the clock legible even if artwork allocation fails before configure. */
static GColor s_background = { .argb = GColorWhiteARGB8 };
static GColor s_foreground = { .argb = GColorBlackARGB8 };
static GColor s_accent = { .argb = GColorOrangeARGB8 };
static GColor s_ghost = { .argb = GColorLightGrayARGB8 };
static GColor s_original_palette[16];
static unsigned s_palette_count;
static bool s_loaded, s_configured;
static FaceSettings s_colors;

/* A small fixed pixel font gives the panel two readable single-line rows.
 * Column bits are top-to-bottom, seven pixels tall; no font or heap requests
 * occur in the animation loop. ASCII lowercase is rendered as uppercase. */
static const uint8_t s_font[95][5] = {
  {0,0,0,0,0}, {0,0,95,0,0}, {0,7,0,7,0}, {20,127,20,127,20},
  {36,42,127,42,18}, {35,19,8,100,98}, {54,73,85,34,80}, {0,5,3,0,0},
  {0,28,34,65,0}, {0,65,34,28,0}, {20,8,62,8,20}, {8,8,62,8,8},
  {0,80,48,0,0}, {8,8,8,8,8}, {0,96,96,0,0}, {32,16,8,4,2},
  {62,81,73,69,62}, {0,66,127,64,0}, {66,97,81,73,70}, {33,65,69,75,49},
  {24,20,18,127,16}, {39,69,69,69,57}, {60,74,73,73,48}, {1,113,9,5,3},
  {54,73,73,73,54}, {6,73,73,41,30}, {0,54,54,0,0}, {0,86,54,0,0},
  {8,20,34,65,0}, {20,20,20,20,20}, {0,65,34,20,8}, {2,1,81,9,6},
  {50,73,121,65,62}, {126,17,17,17,126}, {127,73,73,73,54}, {62,65,65,65,34},
  {127,65,65,34,28}, {127,73,73,73,65}, {127,9,9,9,1}, {62,65,73,73,122},
  {127,8,8,8,127}, {0,65,127,65,0}, {32,64,65,63,1}, {127,8,20,34,65},
  {127,64,64,64,64}, {127,2,12,2,127}, {127,4,8,16,127}, {62,65,65,65,62},
  {127,9,9,9,6}, {62,65,81,33,94}, {127,9,25,41,70}, {70,73,73,73,49},
  {1,1,127,1,1}, {63,64,64,64,63}, {31,32,64,32,31}, {63,64,56,64,63},
  {99,20,8,20,99}, {7,8,112,8,7}, {97,81,73,69,67},
  {0,127,65,65,0}, {2,4,8,16,32}, {0,65,65,127,0},
  {4,2,1,2,4}, {64,64,64,64,64}, {0,1,2,4,0},
  [91] = {0,8,54,65,0}, [92] = {0,0,127,0,0},
  [93] = {0,65,54,8,0}, [94] = {8,4,8,16,8}
};

/* Compact seven-pixel Japanese weekday/date glyphs, stored by row. */
static const struct { unsigned code; uint8_t rows[7]; } s_japanese[] = {
  {0x65e5, {0x3e,0x22,0x22,0x3e,0x22,0x22,0x3e}}, /* 日 */
  {0x6708, {0x3e,0x22,0x3e,0x22,0x3e,0x22,0x46}}, /* 月 */
  {0x706b, {0x08,0x2a,0x2a,0x08,0x14,0x22,0x41}}, /* 火 */
  {0x6c34, {0x08,0x0a,0x7c,0x18,0x2a,0x49,0x18}}, /* 水 */
  {0x6728, {0x08,0x08,0x7f,0x1c,0x2a,0x49,0x08}}, /* 木 */
  {0x91d1, {0x08,0x14,0x22,0x7f,0x08,0x2a,0x7f}}, /* 金 */
  {0x571f, {0x08,0x08,0x3e,0x08,0x08,0x08,0x7f}}  /* 土 */
};

static int pixel_text_draw(GContext *ctx, const char *text, int x, int y, int width) {
  int pen = 0;
  while (*text != '\0') {
    unsigned col, row, i, bytes = 1;
    unsigned char c = (unsigned char)*text;
    const uint8_t *jp = NULL;
    if ((c & 0xf0u) == 0xe0u && text[1] && text[2] &&
        ((unsigned char)text[1] & 0xc0u) == 0x80u &&
        ((unsigned char)text[2] & 0xc0u) == 0x80u) {
      unsigned code = ((c & 15u) << 12) | (((unsigned char)text[1] & 63u) << 6) |
                      ((unsigned char)text[2] & 63u);
      bytes = 3;
      for (i = 0; i < sizeof(s_japanese)/sizeof(s_japanese[0]); ++i)
        if (s_japanese[i].code == code) { jp = s_japanese[i].rows; break; }
    }
    int advance = jp ? 8 : 6;
    if (pen + advance > width) break;
    if (text[bytes] && pen + advance + 6 > width) {
      if (ctx) for (col = 0; col < 3; ++col)
        graphics_fill_rect(ctx, GRect(x + pen + (int)col * 2,
          y + 6, 1, 1), 0, GCornerNone);
      return pen + 5;
    }
    if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
    if (c < 32u || c > 126u) c = '?';
    for (col = 0; col < (jp ? 7u : 5u); ++col) {
      for (row = 0; row < 7; ++row) {
        bool lit = jp ? (jp[row] & (1u << (6u-col))) != 0 : (s_font[c-32u][col] >> row & 1u) != 0;
        if (lit && ctx) {
          graphics_fill_rect(ctx, GRect(x + pen + (int)col, y + (int)row, 1, 1),
                             0, GCornerNone);
        }
      }
    }
    pen += advance;
    text += bytes;
  }
  return pen > 0 ? pen - 1 : 0;
}

static void pixel_text(GContext *ctx, const char *text, int x, int y, int width) {
  pixel_text_draw(ctx, text, x, y, width);
}

/* Native-size sans-serif outlines avoid uneven fractional pixel scaling. */
static GFont info_font(void) {
  return s_info_font ? s_info_font : fonts_get_system_font(FONT_KEY_GOTHIC_14);
}

static int info_text_width(const char *text, int width) {
  return graphics_text_layout_get_content_size(text, info_font(),
      GRect(0, 0, width, 18), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentRight).w;
}

static void info_text(GContext *ctx, const char *text, int y, int width, int right) {
  graphics_context_set_text_color(ctx, s_foreground);
  graphics_draw_text(ctx, text, info_font(), GRect(right - width, y - 3, width, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static GColor blend(GColor a, GColor b, unsigned weight, unsigned divisor) {
  return GColorFromRGB(((a.r * (divisor - weight) + b.r * weight) * 85u) / divisor,
      ((a.g * (divisor - weight) + b.g * weight) * 85u) / divisor,
      ((a.b * (divisor - weight) + b.b * weight) * 85u) / divisor);
}

static bool colors_changed(const FaceSettings *settings) {
  return !s_configured || settings->theme != s_colors.theme ||
      settings->background_color != s_colors.background_color ||
      settings->segment_color != s_colors.segment_color ||
      settings->accent_color != s_colors.accent_color ||
      settings->custom_colors != s_colors.custom_colors ||
      settings->color_artwork != s_colors.color_artwork ||
      settings->high_contrast != s_colors.high_contrast;
}

static GColor backdrop_color(GColor original, const FaceSettings *settings) {
  if (original.r == 3u && original.g == 3u && original.b == 3u) return s_background;
  if (original.r == 0u && original.g == 0u && original.b == 0u) return s_foreground;
  if (!settings->color_artwork) {
    unsigned brightness = original.r + original.g + original.b;
    return settings->high_contrast ? s_foreground :
           blend(s_foreground, s_background, brightness / 3u, 4u);
  }
  /* Printed scenery keeps its recognizable nautical colors; colored themes
   * tint the print. */
  if (settings->custom_colors || settings->theme != 0)
    return blend(original, s_accent, 1u, 4u);
  return original;
}

static bool load_backdrop(void) {
  GBitmapFormat format;
  GColor *palette;
  unsigned i;
  if (s_backdrop != NULL) gbitmap_destroy(s_backdrop);
  s_backdrop = gbitmap_create_with_resource(RESOURCE_ID_BACKDROP);
  s_palette_count = 0u;
  if (s_backdrop == NULL) return false;
  format = gbitmap_get_format(s_backdrop);
  if (format == GBitmapFormat1BitPalette) s_palette_count = 2u;
  else if (format == GBitmapFormat2BitPalette) s_palette_count = 4u;
  else if (format == GBitmapFormat4BitPalette) s_palette_count = 16u;
  palette = gbitmap_get_palette(s_backdrop);
  for (i = 0u; palette != NULL && i < s_palette_count; ++i)
    s_original_palette[i] = palette[i];
  return format == GBitmapFormat8Bit || s_palette_count != 0u;
}

void face_view_configure(const FaceSettings *settings) {
  static const uint32_t backgrounds[] = { 0xFFFFFF, 0xFFFFAA, 0xAAFFAA, 0xFFAA55 };
  static const uint32_t foregrounds[] = { 0x000000, 0x550000, 0x005500, 0x550000 };
  static const uint32_t accents[] = { 0xFF5500, 0xAA5500, 0x00AA55, 0xAA0000 };
  unsigned theme = settings->theme >= 0 && settings->theme < 4 ? (unsigned)settings->theme : 1u;
  bool changed = colors_changed(settings);
  unsigned i;
  s_background = GColorFromHEX(settings->custom_colors ? (uint32_t)settings->background_color : backgrounds[theme]);
  s_foreground = GColorFromHEX(settings->custom_colors ? (uint32_t)settings->segment_color : foregrounds[theme]);
  s_accent = GColorFromHEX(settings->custom_colors ? (uint32_t)settings->accent_color : accents[theme]);
  if (settings->high_contrast) {
    s_foreground = s_background.r + s_background.g + s_background.b < 5 ? GColorWhite : GColorBlack;
    s_accent = s_foreground;
  }
  s_ghost = blend(s_background, s_foreground, settings->ghost_strength > 1 ? 2u : 1u, 3u);
  if (settings->ghost_strength == 1) {
    /* Round in the display's four-level channel space to avoid an orange
     * quantization bias in faint ink on Ivory. */
    s_ghost.r = (2u * s_background.r + s_foreground.r + 1u) / 3u;
    s_ghost.g = (2u * s_background.g + s_foreground.g + 1u) / 3u;
    s_ghost.b = (2u * s_background.b + s_foreground.b + 1u) / 3u;
  }
  for (i = 0u; i < 2u; ++i) {
    bool transparent = s_lit_palette[i].a == 0u;
    s_lit_palette[i] = transparent ? GColorClear : s_foreground;
    s_ghost_palette[i] = transparent ? GColorClear : s_ghost;
  }
  if (s_loaded && changed) {
    if (s_configured && !load_backdrop()) s_loaded = false;
    if (s_loaded && gbitmap_get_format(s_backdrop) == GBitmapFormat8Bit) {
      uint8_t lookup[256], *pixels = gbitmap_get_data(s_backdrop);
      GRect bounds = gbitmap_get_bounds(s_backdrop);
      size_t stride = gbitmap_get_bytes_per_row(s_backdrop);
      int x, y;
      for (i = 0u; i < 256u; ++i) lookup[i] = backdrop_color((GColor){ .argb = (uint8_t)i }, settings).argb;
      for (y = 0; y < bounds.size.h; ++y)
        for (x = 0; x < bounds.size.w; ++x)
          pixels[(size_t)y * stride + (size_t)x] = lookup[pixels[(size_t)y * stride + (size_t)x]];
    } else if (s_loaded) {
      GColor *palette = gbitmap_get_palette(s_backdrop);
      for (i = 0u; i < s_palette_count; ++i) palette[i] = backdrop_color(s_original_palette[i], settings);
    }
  }
  s_colors = *settings;
  s_configured = true;
}

void face_view_deinit(void) {
  unsigned i;
  if (s_info_font) fonts_unload_custom_font(s_info_font);
  s_info_font = NULL;
  for (i = 0u; i < 28u; ++i) {
    if (s_ghost_art[i] != NULL) gbitmap_destroy(s_ghost_art[i]);
    s_ghost_art[i] = NULL;
  }
  for (i = 0u; i < SEG_COUNT; ++i) {
    if (s_art[i] != NULL) gbitmap_destroy(s_art[i]);
    s_art[i] = NULL;
  }
  if (s_sheet != NULL) gbitmap_destroy(s_sheet);
  if (s_backdrop != NULL) gbitmap_destroy(s_backdrop);
  s_backdrop = s_sheet = NULL;
  s_loaded = s_configured = false;
}

bool face_view_init(void) {
  unsigned i;
  GColor *palette;
  FaceSettings defaults;
  face_view_deinit();
  if (!load_backdrop()) { face_view_deinit(); return false; }
  s_sheet = gbitmap_create_with_resource(RESOURCE_ID_SEGMENTS);
  if (s_sheet == NULL || gbitmap_get_format(s_sheet) != GBitmapFormat1BitPalette) {
    face_view_deinit(); return false;
  }
  palette = gbitmap_get_palette(s_sheet);
  if (palette == NULL) { face_view_deinit(); return false; }
  for (i = 0u; i < 2u; ++i) s_lit_palette[i] = s_ghost_palette[i] = palette[i];
  for (i = 0u; i < SEG_COUNT; ++i) {
    const SegmentArt *art = &segment_art[i];
    s_art[i] = gbitmap_create_as_sub_bitmap(s_sheet, GRect(art->sheet_x, art->sheet_y, art->w, art->h));
    if (s_art[i] == NULL) { face_view_deinit(); return false; }
    gbitmap_set_palette(s_art[i], s_lit_palette, false);
    if (i >= SEG_DIGIT && i < SEG_DIGIT + 28u) {
      s_ghost_art[i - SEG_DIGIT] = gbitmap_create_as_sub_bitmap(s_sheet,
          GRect(art->sheet_x, art->sheet_y, art->w, art->h));
      if (s_ghost_art[i - SEG_DIGIT] == NULL) { face_view_deinit(); return false; }
      gbitmap_set_palette(s_ghost_art[i - SEG_DIGIT], s_ghost_palette, false);
    }
  }
  s_info_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_INFO_SANS_15));
  s_loaded = true;
  settings_defaults(&defaults);
  face_view_configure(&defaults);
  return s_loaded;
}

/* Emery has only four levels per color channel. A solid lighter gray would
 * round into the background, so keep one pixel in four for faint LCD ink.
 * The pattern stays fixed on screen and needs no extra bitmaps or timers. */
static void soften_ghost(GContext *ctx, GRect rect) {
  int x, y;
  if (s_colors.ghost_strength != 1) return;
  graphics_context_set_fill_color(ctx, s_background);
  for (y = rect.origin.y; y < rect.origin.y + rect.size.h; ++y) {
    for (x = rect.origin.x; x < rect.origin.x + rect.size.w; ++x) {
      if ((x + y) % 4 != 0)
        graphics_fill_rect(ctx, GRect(x, y, 1, 1), 0, GCornerNone);
    }
  }
}

static void draw_segment(GContext *ctx, unsigned segment, bool ghost) {
  const SegmentArt *art = &segment_art[segment];
  GRect rect = GRect(art->x, art->y, art->w, art->h);
  graphics_draw_bitmap_in_rect(ctx, ghost ? s_ghost_art[segment - SEG_DIGIT] : s_art[segment],
      rect);
  if (ghost) soften_ghost(ctx, rect);
}

static int large_clock(GContext *ctx, const Scene *scene, bool ghosts) {
  unsigned digit, bar;
  const int top = 8;
  FaceClockLayout layout;
  /* Narrow 1s and the AM/PM label participate in the same centered layout. */
  face_clock_layout(scene, &layout);
  for (digit = 0u; digit < 4u; ++digit) {
    int x = layout.starts[digit];
    if (layout.widths[digit] == 4) {
      graphics_context_set_fill_color(ctx, s_foreground);
      graphics_fill_rect(ctx, GRect(x, top + 4, 4, 11), 1, GCornersAll);
      graphics_fill_rect(ctx, GRect(x, top + 17, 4, 11), 1, GCornersAll);
      continue;
    }
    GRect bars[7] = {
      GRect(x + 4, top + 1, 24, 4), GRect(x + 28, top + 4, 4, 11),
      GRect(x + 28, top + 17, 4, 11), GRect(x + 4, top + 27, 24, 4),
      GRect(x, top + 17, 4, 11), GRect(x, top + 4, 4, 11), GRect(x + 4, top + 14, 24, 4)
    };
    for (bar = 0u; bar < 7u; ++bar) {
      bool lit = scene_lit(scene, SEG_DIGIT + digit * 7u + bar);
      if (lit || ghosts) {
        graphics_context_set_fill_color(ctx, lit ? s_foreground : s_ghost);
        graphics_fill_rect(ctx, bars[bar], 1, GCornersAll);
        if (!lit) soften_ghost(ctx, bars[bar]);
      }
    }
  }
  graphics_context_set_fill_color(ctx, s_foreground);
  if (scene_lit(scene, SEG_COLON)) {
    graphics_fill_rect(ctx, GRect(layout.colon_x, top + 8, 4, 4), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(layout.colon_x, top + 21, 4, 4), 0, GCornerNone);
  }
  if (scene_lit(scene, SEG_AM)) pixel_text(ctx, "AM", layout.meridiem_x, top + 20, 12);
  if (scene_lit(scene, SEG_PM)) pixel_text(ctx, "PM", layout.meridiem_x, top + 20, 12);
  return layout.right;
}

static void battery_icon(GContext *ctx, int x, int y, int percent, int width) {
  int bar, lit = percent == 0 ? 0 : (percent + 24) / 25;
  int cell = (width - 3) / 4;
  graphics_context_set_stroke_color(ctx, s_foreground);
  graphics_draw_rect(ctx, GRect(x, y, width, 7));
  graphics_fill_rect(ctx, GRect(x + width, y + 2, 2, 3), 0, GCornerNone);
  for (bar = 0; bar < lit; ++bar)
    graphics_fill_rect(ctx, GRect(x + 2 + bar * cell, y + 2, cell - 1, 3), 0, GCornerNone);
}

/* Compact lightning bolt, drawn as pixel steps to match the LCD artwork. */
static void charging_icon(GContext *ctx, int x, int y) {
  graphics_fill_rect(ctx, GRect(x + 4, y, 3, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 2, y + 2, 4, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x, y + 4, 7, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 2, y + 6, 3, 2), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 1, y + 8, 2, 2), 0, GCornerNone);
}

static void spinach_icon(GContext *ctx, int x, int y, int percent) {
  graphics_context_set_stroke_color(ctx, s_foreground);
  graphics_draw_rect(ctx, GRect(x, y, 10, 9));
  graphics_fill_rect(ctx, GRect(x + 2, y + 2, 6, percent > 0 ? 1 + percent * 4 / 100 : 0), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, s_background);
  graphics_fill_rect(ctx, GRect(x + 4, y + 2, 2, 3), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, s_foreground);
}

static void step_icon(GContext *ctx, int x, int y) {
  graphics_fill_rect(ctx, GRect(x, y + 1, 3, 5), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 3, y + 4, 6, 3), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x, y + 7, 9, 1), 0, GCornerNone);
}

static const char *weather_word(int code) {
  if (code == 0) return "SUN";
  if (code <= 3) return "CLOUD";
  if (code == 45 || code == 48) return "FOG";
  if (code >= 95) return "STORM";
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return "SNOW";
  return "RAIN";
}

static bool snowy(int code) {
  return (code >= 71 && code <= 77) || code == 85 || code == 86;
}

static void weather_icon(GContext *ctx, int x, int y, int code) {
  graphics_context_set_stroke_color(ctx, s_foreground);
  if (code < 0) {
    /* Thermometer identifies unavailable weather without inventing conditions. */
    graphics_draw_rect(ctx, GRect(x + 3, y, 3, 7));
    graphics_draw_circle(ctx, GPoint(x + 4, y + 8), 2);
    return;
  }
  if (code == 0) {
    graphics_draw_circle(ctx, GPoint(x + 4, y + 4), 2);
    graphics_fill_rect(ctx, GRect(x + 4, y, 1, 1), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(x + 4, y + 8, 1, 1), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(x, y + 4, 1, 1), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(x + 8, y + 4, 1, 1), 0, GCornerNone);
  } else {
    graphics_fill_rect(ctx, GRect(x + 1, y + 3, 8, 3), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(x + 3, y + 1, 4, 5), 0, GCornerNone);
    if (code == 45 || code == 48) {
      graphics_fill_rect(ctx, GRect(x, y + 7, 9, 1), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(x + 2, y + 9, 7, 1), 0, GCornerNone);
    } else if (snowy(code)) {
      graphics_fill_rect(ctx, GRect(x + 2, y + 7, 2, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(x + 6, y + 7, 2, 2), 0, GCornerNone);
    } else if (code >= 95) {
      graphics_fill_rect(ctx, GRect(x + 4, y + 6, 2, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(x + 3, y + 8, 2, 2), 0, GCornerNone);
    } else if (code > 3) {
      graphics_fill_rect(ctx, GRect(x + 2, y + 7, 1, 2), 0, GCornerNone);
      graphics_fill_rect(ctx, GRect(x + 6, y + 7, 1, 2), 0, GCornerNone);
    }
  }
}

static void calendar_icon(GContext *ctx, int x, int y) {
  graphics_context_set_stroke_color(ctx, s_foreground);
  graphics_draw_rect(ctx, GRect(x, y + 2, 10, 9));
  graphics_fill_rect(ctx, GRect(x, y + 4, 10, 1), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 2, y, 1, 4), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(x + 7, y, 1, 4), 0, GCornerNone);
}

/* Moving cargo can cross the information rows. Clear only behind what a row
 * shows, so food elsewhere in the row (such as its second stop) stays visible. */
static void clear_behind(GContext *ctx, int left, int y, int right) {
  graphics_context_set_fill_color(ctx, s_background);
  graphics_fill_rect(ctx, GRect(left - 2, y - 3, right - left + 2, 18), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, s_foreground);
}

static void draw_row(GContext *ctx, int row, int y, const FaceSettings *settings,
                     const FaceData *data, const struct tm *local, time_t now,
                     bool watch_24h, bool celebration, int right, bool compact) {
  char text[64];
  int x = 60, width = right - 60;
  int panel_width = width;
  int icon = 0, icon_value = 0, bar = -1;
  int percent = data->battery_percent < 0 ? 0 : data->battery_percent > 100 ? 100 : data->battery_percent;
  text[0] = '\0';
  graphics_context_set_fill_color(ctx, s_foreground);
  switch (row) {
    case FACE_ROW_DATE:
      data_format_date(text, sizeof(text), settings, local);
      break;
    case FACE_ROW_BATTERY: {
      if (settings->battery_threshold > 0 && percent >= settings->battery_threshold && !data->charging) return;
      int content_right = data->charging ? right - 10 : right;
      snprintf(text, sizeof(text), "%d%%", percent);
      int text_width = settings->battery_style == 1 ? 0 : info_text_width(text, content_right - x);
      int icon_right = content_right - text_width - 5;
      int bar_width = icon_right - x - 2;
      if (bar_width > 24) bar_width = 24;
      clear_behind(ctx, settings->battery_style == 1 ? content_right - 26 :
                   settings->battery_style == 2 ? icon_right - bar_width - 2 :
                   settings->battery_style == 3 ? content_right - text_width - 15 :
                   content_right - text_width, y, right);
      if (data->charging) charging_icon(ctx, right - 7, y);
      if (settings->battery_style == 1) {
        battery_icon(ctx, content_right - 26, y + 2, percent, 24);
        return;
      }
      info_text(ctx, text, y, text_width, content_right);
      if (settings->battery_style == 2)
        battery_icon(ctx, icon_right - bar_width - 2, y + 2, percent, bar_width);
      else if (settings->battery_style == 3)
        spinach_icon(ctx, content_right - text_width - 15, y + 1, percent);
      return;
    }
    case FACE_ROW_STEPS: {
      int goal = settings->step_goal > 0 ? settings->step_goal : 10000;
      int progress = data_step_percent(settings, data);
      icon = settings->step_style == 2 ? 1 : 2; icon_value = progress;
      width -= 13;
      if (!data->health_available) snprintf(text, sizeof(text), compact ? "--" : "STEPS --");
      else if (!compact && celebration && data->celebration_kind == FACE_CELEBRATION_STEPS)
        snprintf(text, sizeof(text), "GOAL REACHED!");
      else if (settings->step_style == 1) snprintf(text, sizeof(text), compact ? "%d%%" : "%d%% OF GOAL", progress);
      else snprintf(text, sizeof(text), compact ? "%ld" : "%ld STEPS", (long)data->steps);
      if (settings->step_style == 2 && data->health_available)
        bar = data->steps >= goal ? panel_width : data->steps <= 0 ? 0 : (int)((int64_t)data->steps * panel_width / goal);
      break;
    }
    case FACE_ROW_WEATHER:
      if (compact) { icon = 3; icon_value = -1; width -= 13; }
      if (!settings->weather_enabled) snprintf(text, sizeof(text), compact ? "--" : "WEATHER OFF");
      else if (data->weather_updated <= 0 || data->weather_unit != settings->weather_units)
        snprintf(text, sizeof(text), compact ? "--" : "WEATHER --");
      else {
        bool stale = data_weather_stale(settings, data, now);
        char unit = data->weather_unit == 1 ? 'F' : 'C';
        icon = 3; icon_value = data->weather_code; if (!compact) width -= 13;
        if (settings->weather_detail == 1)
          snprintf(text, sizeof(text), "%ld/%ld%c%s", (long)data->weather_high,
                   (long)data->weather_low, unit, stale ? "*" : "");
        else if (compact) snprintf(text, sizeof(text), "%ld%c%s", (long)data->weather_temp,
                                   unit, stale ? "*" : "");
        else snprintf(text, sizeof(text), "%ld%c %s%s", (long)data->weather_temp,
                      unit, weather_word(data->weather_code), stale ? "*" : "");
      }
      break;
    case FACE_ROW_WORLD: {
      struct tm world;
      if (!data_world_time(data, now, &world)) snprintf(text, sizeof(text), "%s --:--", settings->world_label);
      else {
        bool use24 = settings->world_format == 2 || (settings->world_format == 0 && watch_24h);
        int hour = world.tm_hour;
        char suffix[2] = "";
        if (!use24) { suffix[0] = hour < 12 ? 'A' : 'P'; hour %= 12; if (hour == 0) hour = 12; }
        snprintf(text, sizeof(text), "%s %02d:%02d%s%s", settings->world_label, hour,
                 world.tm_min, suffix, data_world_stale(data, now) ? "*" : "");
      }
      break;
    }
    case FACE_ROW_EVENT: {
      int days;
      if (compact) {
        icon = 4; width -= 13;
        if (!settings_event_days(settings, local, &days) || (days < 0 && !settings->event_elapsed))
          snprintf(text, sizeof(text), "--");
        else snprintf(text, sizeof(text), "%s%d", days < 0 ? "+" : "", days < 0 ? -days : days);
        break;
      }
      if (!settings_event_days(settings, local, &days)) snprintf(text, sizeof(text), "%s --", settings->event_label);
      else if (days == 0) snprintf(text, sizeof(text), "%s TODAY", settings->event_label);
      else if (days < 0 && !settings->event_elapsed) snprintf(text, sizeof(text), "%s PASSED", settings->event_label);
      else {
        char remaining[16];
        int available;
        snprintf(remaining, sizeof(remaining), "%s%dD", days < 0 ? "+" : "", days < 0 ? -days : days);
        available = 15 - (int)strlen(remaining);
        if (available < 1) available = 1;
        snprintf(text, sizeof(text), "%.*s %s", available, settings->event_label, remaining);
      }
      break;
    }
    default: return;
  }
  clear_behind(ctx, right - info_text_width(text, width) - (icon ? 13 : 0), y, right);
  if (bar >= 0) { /* the step meter spans the row under the text */
    graphics_context_set_fill_color(ctx, s_ghost);
    graphics_fill_rect(ctx, GRect(x, y + 13, panel_width, 2), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, s_accent);
    if (bar > 0) graphics_fill_rect(ctx, GRect(x, y + 13, bar, 2), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, s_foreground);
  }
  if (icon) {
    int icon_x = right - info_text_width(text, width) - 13;
    if (icon == 1) spinach_icon(ctx, icon_x, y + 1, icon_value);
    else if (icon == 2) step_icon(ctx, icon_x, y + 1);
    else if (icon == 3) weather_icon(ctx, icon_x, y + 1, icon_value);
    else calendar_icon(ctx, icon_x, y + 1);
  }
  info_text(ctx, text, y, width, right);
}

void face_view_draw(GContext *ctx, GRect bounds, const FaceSettings *settings,
                    const FaceData *data, const struct tm *local,
                    bool watch_24h, uint32_t frame, bool animate) {
  Scene scene;
  struct tm stable_local = *local, copy = stable_local;
  time_t now = mktime(&copy);
  bool use24 = settings->time_format == 2 || (settings->time_format == 0 && watch_24h);
  bool ghosts = settings->ghost_strength != 0 && !settings->high_contrast;
  bool celebration = settings->celebrate && data->celebration && animate;
  int row1 = settings->row1, row2 = settings->row2;
  unsigned segment;
  /* gmtime used by a world row may share libc's localtime storage. */
  local = &stable_local;
  if (settings->animation_mode == FACE_ANIMATION_STILL) {
    /* Never animated, so a paused game frame that changes only with the minute. */
    face_scene_still(&scene, local, use24);
  } else if (settings->animation_mode == FACE_ANIMATION_ARCADE && animate && !celebration) {
    face_scene_crowd(&scene, local, use24, frame, (unsigned)settings->character_activity);
  } else face_scene_active(&scene, local, use24, animate ? frame : 0u,
      animate ? (unsigned)settings->character_activity : 0u, celebration,
      (unsigned)settings->animation_pause);
  if (settings->blink_colon && local->tm_sec % 2 != 0)
    scene.bits[SEG_COLON / 32u] &= ~(UINT32_C(1) << (SEG_COLON % 32u));

  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  graphics_context_set_fill_color(ctx, s_configured ? s_background : GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  if (s_loaded) graphics_draw_bitmap_in_rect(ctx, s_backdrop, bounds);
  /* This replaces the unused game MISS register with a useful two-row panel. */
  graphics_context_set_fill_color(ctx, s_background);
  graphics_fill_rect(ctx, GRect(60, 28, 140, 50), 0, GCornerNone);
  if (settings->large_time || !s_loaded) graphics_fill_rect(ctx, GRect(0, 0, 200, 40), 0, GCornerNone);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  if (s_loaded) {
    if (!settings->large_time && ghosts) {
      for (segment = SEG_DIGIT; segment < SEG_DIGIT + 28u; ++segment)
        if (!scene_lit(&scene, segment)) draw_segment(ctx, segment, true);
    }
    for (segment = 0u; segment < SEG_COUNT; ++segment) {
      if (settings->large_time && segment >= SEG_DIGIT && segment <= SEG_PM) continue;
      if (scene_lit(&scene, segment)) draw_segment(ctx, segment, false);
    }
  }
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  /* Align information with the clock's right edge, including its AM/PM label. */
  int info_right = segment_art[SEG_DIGIT + 3u * 7u + 1u].x +
                   segment_art[SEG_DIGIT + 3u * 7u + 1u].w;
  if (settings->large_time || !s_loaded) info_right = large_clock(ctx, &scene, ghosts);
  bool rotated_second = false;
  if (settings->rotate_seconds > 0) {
    row1 = data_rotating_row(settings, local);
    rotated_second = row1 == settings->row2 && row1 != settings->row1;
    row2 = FACE_ROW_NONE;
  }
  draw_row(ctx, row1, settings->large_time || !s_loaded ? 42 : 33, settings, data, local, now, use24, celebration, info_right, rotated_second);
  draw_row(ctx, row2, settings->large_time || !s_loaded ? 61 : 52, settings, data, local, now, use24, celebration, info_right, true);
}
