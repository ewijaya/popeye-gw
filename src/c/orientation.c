#include "orientation.h"

bool orientation_logical_up(bool physical_up, bool buttons_bottom) {
  return physical_up != buttons_bottom;
}

void orientation_pack_row(uint8_t *out, size_t stride, const uint8_t source[200], unsigned row,
                          bool buttons_bottom) {
  unsigned x;
  if (row >= 200u || stride < 50u) return;
  for (x = 0; x < 200u; ++x) {
    unsigned column = buttons_bottom ? row : 199u - row;
    unsigned target_row = buttons_bottom ? 199u - x : x;
    /* ARGB8: black, white, dark green (#005500); magenta is transparent.
     * Retain the header accent in the existing fourth palette entry. */
    unsigned index = source[x] == 0xc0u ? 1u : source[x] == 0xffu ? 2u :
                     source[x] == 0xc4u ? 3u : 0u;
    out[target_row * stride + column / 4u] |= (uint8_t)(index << (6u - 2u * (column % 4u)));
  }
}
