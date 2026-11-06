#pragma once
#include <stdint.h>

// A horizontal band of the 2bpp colour framebuffer with its own four RGB565 colours.
// The colour screens are laid out in rows (status row, value row, progress bar, footer),
// so giving each row its own palette lifts the 4-colour limit to 4 colours *per row*
// without any extra framebuffer RAM. Bands are sorted by yStart; the first band must
// start at y = 0 and covers rows up to the next band's yStart (the last one to the bottom).
// Index 0 of every band is that band's background (clearScreenColor() writes zeros).
struct ColorBand {
  uint8_t  yStart;
  uint16_t colors[4];
};
