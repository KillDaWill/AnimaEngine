#ifndef PNG_WRITER_H
#define PNG_WRITER_H

#include "common.h"
#include "ppm.h"

int Png_WriteRgbaImage(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height
);

int Png_WriteRgbaImageScaled(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height,
    int scale
);

#endif
