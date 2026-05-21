#ifndef GIF_WRITER_H
#define GIF_WRITER_H

#include "common.h"
#include "ppm.h"

int Gif_WriteIndexed(
    const char *path,
    const u8 *frames,
    int frame_count,
    int width,
    int height,
    const RgbaColor *palette,
    int palette_count,
    int transparent_index,
    int delay_cs,
    int loop_count,
    int scale
);

#endif
