#include <assert.h>
#include <stdio.h>

#include "theme.h"

/* Reference oracle: the SDK's GColor8 union and conversion macros
 * (gcolor_definitions.h) with blend() and backdrop_color() copied verbatim
 * from the companion watchface (watchface/src/c/face_view.c), Ivory theme,
 * color artwork on, no custom colors or high contrast. */
typedef union {
  uint8_t argb;
  __extension__ struct { uint8_t b:2; uint8_t g:2; uint8_t r:2; uint8_t a:2; }; /* SDK layout */
} GColor;
#define GColorFromRGBA(red, green, blue, alpha) ((GColor){ \
  .b = (uint8_t)(blue) >> 6, .g = (uint8_t)(green) >> 6, \
  .r = (uint8_t)(red) >> 6, .a = (uint8_t)(alpha) >> 6, })
#define GColorFromRGB(red, green, blue) GColorFromRGBA(red, green, blue, 255)
#define GColorFromHEX(v) GColorFromRGB(((v) >> 16) & 0xff, ((v) >> 8) & 0xff, ((v) & 0xff))

static GColor s_background, s_foreground, s_accent;

static GColor blend(GColor a, GColor b, unsigned weight, unsigned divisor) {
  return GColorFromRGB(((a.r * (divisor - weight) + b.r * weight) * 85u) / divisor,
      ((a.g * (divisor - weight) + b.g * weight) * 85u) / divisor,
      ((a.b * (divisor - weight) + b.b * weight) * 85u) / divisor);
}

static GColor backdrop_color(GColor original) {
  if (original.r == 3u && original.g == 3u && original.b == 3u) return s_background;
  if (original.r == 0u && original.g == 0u && original.b == 0u) return s_foreground;
  return blend(original, s_accent, 1u, 4u); /* color_artwork with a colored theme */
}

int main(void) {
  ThemeColors colors;
  uint8_t lookup[256];
  unsigned i, opaque = 0u;
  s_background = GColorFromHEX(0xFFFFAA);
  s_foreground = GColorFromHEX(0x550000);
  s_accent = GColorFromHEX(0xAA5500);

  assert(!theme_colors(THEME_CLASSIC, &colors)); /* Classic keeps authored artwork. */
  assert(theme_colors(THEME_IVORY, &colors));
  /* #FFFFAA, #550000 and #AA5500 as opaque GColor8 bytes. */
  assert(colors.background == 0xFEu && colors.foreground == 0xD0u && colors.accent == 0xE4u);
  assert(colors.background == s_background.argb && colors.foreground == s_foreground.argb &&
         colors.accent == s_accent.argb);
  for (i = 0u; i < 256u; ++i)
    assert(theme_from_hex(((i & 0xc0u) << 16) | ((i & 0x30u) << 10) | ((i & 0x0cu) << 4)) ==
           GColorFromHEX(((i & 0xc0u) << 16) | ((i & 0x30u) << 10) | ((i & 0x0cu) << 4)).argb);

  /* Every possible color, through the oracle and the game's implementation. */
  theme_backdrop_lookup(lookup, &colors);
  for (i = 0u; i < 256u; ++i) {
    GColor original = { .argb = (uint8_t)i };
    unsigned weight, divisor;
    for (divisor = 1u; divisor <= 4u; ++divisor)
      for (weight = 0u; weight <= divisor; ++weight)
        assert(theme_blend((uint8_t)i, s_accent.argb, weight, divisor) ==
               blend(original, s_accent, weight, divisor).argb);
    if (original.a == 0u) {
      /* Transparency is preserved; the watchface's opaque backdrop never has it. */
      assert(lookup[i] == i && theme_ink((uint8_t)i, &colors) == i);
      continue;
    }
    assert(lookup[i] == backdrop_color(original).argb);
    assert(lookup[i] == theme_backdrop((uint8_t)i, &colors));
    assert(theme_ink((uint8_t)i, &colors) == s_foreground.argb);
    ++opaque;
  }
  /* Spot values: white, black, gray scenery and a baked ghost, orange hull, sea blue. */
  assert(theme_backdrop(0xFFu, &colors) == 0xFEu);
  assert(theme_backdrop(0xC0u, &colors) == 0xD0u);
  assert(theme_backdrop(0xEAu, &colors) == 0xE9u); /* #AAAAAA -> #AAAA55 */
  assert(theme_backdrop(0xF4u, &colors) == 0xF4u); /* #FF5500 stays #FF5500 */
  assert(theme_backdrop(0xCBu, &colors) == 0xCAu); /* #00AAFF -> #00AAAA */
  /* Recoloring derives from originals: applying it twice would differ. */
  assert(lookup[lookup[0xCBu]] != lookup[0xCBu]);
  printf("Theme tests passed: %u opaque colors match the watchface's Ivory backdrop_color and blend; "
         "64 transparent colors preserved\n", opaque);
  return 0;
}
