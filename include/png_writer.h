/**
 * @file png_writer.h
 * @brief libpng file output utility wrapper.
 */

#ifndef PNG_WRITER_H
#define PNG_WRITER_H

#include "common.h"
#include "ppm.h"

/**
 * @brief Writes an RGBA pixel buffer to a standard PNG file.
 * @param path File path to save the PNG.
 * @param pixels Source RGBA pixels buffer.
 * @param width Width of image.
 * @param height Height of image.
 * @return 0 on success; negative value on file open or libpng initialization error.
 */
int Png_WriteRgbaImage(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height
);

/**
 * @brief Writes an RGBA pixel buffer scaled by a scaling factor to a PNG.
 * @param path File path to save.
 * @param pixels Source RGBA pixels.
 * @param width Canvas width before scaling.
 * @param height Canvas height before scaling.
 * @param scale Bilinear scale multiplier.
 * @return 0 on success; negative value on error.
 */
int Png_WriteRgbaImageScaled(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height,
    int scale
);

#endif

