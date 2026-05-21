#ifndef PPM_H
#define PPM_H

#include "common.h"

typedef struct RgbaColor {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} RgbaColor;

int Ppm_WriteRgbImage(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height
);

int Ppm_WriteRgbImageScaled(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height,
    int scale
);

#endif