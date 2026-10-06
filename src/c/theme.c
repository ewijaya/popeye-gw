#include "theme.h"

#define CH_B(c) ((unsigned)(c) & 3u)
#define CH_G(c) (((unsigned)(c) >> 2) & 3u)
#define CH_R(c) (((unsigned)(c) >> 4) & 3u)
#define CH_A(c) (((unsigned)(c) >> 6) & 3u)

/* GColorFromRGBA(red, green, blue, 255): (uint8_t)channel >> 6, opaque. */
static uint8_t from_rgb(unsigned red, unsigned green, unsigned blue) {
  return (uint8_t)(3u << 6 | ((unsigned)(uint8_t)red >> 6) << 4 |
                   ((unsigned)(uint8_t)green >> 6) << 2 | ((unsigned)(uint8_t)blue >> 6));
}

uint8_t theme_from_hex(uint32_t hex) {
  return from_rgb((hex >> 16) & 0xffu, (hex >> 8) & 0xffu, hex & 0xffu);
}

bool theme_colors(Theme theme, ThemeColors *colors) {
  if (theme != THEME_IVORY) return false;
  colors->background = theme_from_hex(UINT32_C(0xFFFFAA));
  colors->foreground = theme_from_hex(UINT32_C(0x550000));
  colors->accent = theme_from_hex(UINT32_C(0xAA5500));
  return true;
}

uint8_t theme_blend(uint8_t a, uint8_t b, unsigned weight, unsigned divisor) {
  return from_rgb(((CH_R(a) * (divisor - weight) + CH_R(b) * weight) * 85u) / divisor,
                  ((CH_G(a) * (divisor - weight) + CH_G(b) * weight) * 85u) / divisor,
                  ((CH_B(a) * (divisor - weight) + CH_B(b) * weight) * 85u) / divisor);
}

uint8_t theme_backdrop(uint8_t original, const ThemeColors *colors) {
  if (CH_A(original) == 0u) return original;
  if (CH_R(original) == 3u && CH_G(original) == 3u && CH_B(original) == 3u) return colors->background;
  if (CH_R(original) == 0u && CH_G(original) == 0u && CH_B(original) == 0u) return colors->foreground;
  return theme_blend(original, colors->accent, 1u, 4u);
}

uint8_t theme_ink(uint8_t original, const ThemeColors *colors) {
  return CH_A(original) == 0u ? original : colors->foreground;
}

void theme_backdrop_lookup(uint8_t lookup[256], const ThemeColors *colors) {
  unsigned i;
  for (i = 0u; i < 256u; ++i) lookup[i] = theme_backdrop((uint8_t)i, colors);
}
