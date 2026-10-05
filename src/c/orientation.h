#ifndef POPEYE_GW_ORIENTATION_H
#define POPEYE_GW_ORIENTATION_H
#include <stddef.h>
#include <stdint.h>
/* A compact 200-square UI is centred in the 228-wide landscape canvas.
 * Convert one ARGB8 row into a clockwise 2-bit palette bitmap; palette order
 * is clear, black, white, clear. Caller clears destination before row zero. */
void orientation_pack_row(uint8_t *destination, size_t stride,
                          const uint8_t source[200], unsigned row);
#endif
