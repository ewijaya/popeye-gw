#ifndef POPEYE_GW_THEME_H
#define POPEYE_GW_THEME_H

#include <stdbool.h>
#include <stdint.h>

/* LCD color themes. Classic shows the artwork exactly as authored. Ivory
 * matches the companion watchface (watchface/src/c/face_view.c theme 1):
 * background #FFFFAA, ink #550000, scenery tinted toward accent #AA5500.
 * Colors are Pebble GColor8 bytes (aarrggbb, two bits per channel). */
typedef enum { THEME_CLASSIC = 0, THEME_IVORY = 1, THEME_COUNT } Theme;

typedef struct { uint8_t background, foreground, accent; } ThemeColors;

/* Returns false for Classic, which keeps the original artwork untouched. */
bool theme_colors(Theme theme, ThemeColors *colors);
/* GColorFromHEX: each 8-bit channel keeps its top two bits; alpha opaque. */
uint8_t theme_from_hex(uint32_t hex);
/* Watchface blend(): weight/divisor of b mixed into a, scaled by 85 and
 * quantized through GColorFromRGB. */
uint8_t theme_blend(uint8_t a, uint8_t b, unsigned weight, unsigned divisor);
/* Watchface backdrop_color() for color artwork: white to background, black
 * to ink, any other color blended one quarter toward the accent. Fully
 * transparent pixels stay transparent. */
uint8_t theme_backdrop(uint8_t original, const ThemeColors *colors);
/* Segment and character ink: transparent stays clear, anything else is ink. */
uint8_t theme_ink(uint8_t original, const ThemeColors *colors);
/* Fill a 256-entry lookup from original GColor8 bytes to themed bytes. */
void theme_backdrop_lookup(uint8_t lookup[256], const ThemeColors *colors);

#endif
