#include "orientation.h"

void orientation_pack_row(uint8_t *out, size_t stride, const uint8_t source[200], unsigned row) {
  unsigned x;
  if (row >= 200u || stride < 50u) return;
  for (x = 0; x < 200u; ++x) {
    unsigned column = 199u - row;
    /* UI drawing uses opaque black, white and magenta (transparent sentinel). */
    unsigned index = source[x] == 0xc0u ? 1u : source[x] == 0xffu ? 2u : 0u;
    out[x * stride + column / 4u] |= (uint8_t)(index << (6u - 2u * (column % 4u)));
  }
}
