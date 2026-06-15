/**
 * @file gif_writer.h
 * @brief GIF compilation engine utilizing LZW encoding.
 */

#ifndef GIF_WRITER_H
#define GIF_WRITER_H

#include "common.h"
#include "ppm.h"

/**
 * @brief Encodes and writes a sequence of indexed frames to a GIF file.
 * @param path Path to write the final GIF file.
 * @param frames Pointer to 8-bit indexed frame pixel buffer. Size: (width * height * frame_count).
 * @param frame_count Total number of frames in sequence.
 * @param width Canvas width in pixels.
 * @param height Canvas height in pixels.
 * @param palette Pointer to an array of colors defining the color map (palette).
 * @param palette_count Total number of entries in color palette (up to 256).
 * @param transparent_index Index in the palette mapping transparency, or -1.
 * @param delay_cs Base duration between frames in centiseconds (1/100ths of a second).
 * @param loop_count Number of loops (0 = infinite).
 * @param scale Bilinear scale multiplier applied to output image sizes.
 * @return 0 on success; negative value on file or encoding failure.
 */
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

