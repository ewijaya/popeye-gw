#ifndef POPEYE_GW_ORIENTATION_H
#define POPEYE_GW_ORIENTATION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* A compact 200-square UI is centred in the 228-wide landscape canvas.
 * Convert one ARGB8 row into a rotated 2-bit palette bitmap; palette order
 * is clear, black, white, dark green. Caller clears destination before row zero. */
void orientation_pack_row(uint8_t *destination, size_t stride,
                          const uint8_t source[200], unsigned row, bool buttons_bottom);
/* With buttons along the bottom, physical Down is on the left. Mapping both
 * press and release before game/menu handling preserves the Swap preference. */
bool orientation_logical_up(bool physical_up, bool buttons_bottom);
#endif
