/**
 * @file ppm.h
 * @brief Portable Pixmap (PPM) format writer utilities and color structures.
 */

#ifndef PPM_H
#define PPM_H

#include "common.h"

/**
 * @brief 32-bit RGBA color representation struct.
 */
typedef struct RgbaColor {
    u8 r; /**< Red color component value (0 to 255). */
    u8 g; /**< Green color component value (0 to 255). */
    u8 b; /**< Blue color component value (0 to 255). */
    u8 a; /**< Alpha transparency channel value (0 = Fully Transparent, 255 = Opaque). */
} RgbaColor;

/**
 * @brief Exports an RGBA buffer to standard PPM format.
 * @param path File path destination.
 * @param pixels Source RGBA pixels.
 * @param width Image width.
 * @param height Image height.
 * @return 0 on success; negative value on file open error.
 */
int Ppm_WriteRgbImage(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height
);

/**
 * @brief Exports an RGBA buffer scaled by an integer factor to standard PPM.
 * @param path File path destination.
 * @param pixels Source RGBA pixels.
 * @param width Canvas width before scaling.
 * @param height Canvas height before scaling.
 * @param scale Integer scaling multiplier.
 * @return 0 on success; negative value on file open error.
 */
int Ppm_WriteRgbImageScaled(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height,
    int scale
);

#endif