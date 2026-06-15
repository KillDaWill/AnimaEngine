/**
 * @file nclr.h
 * @brief Nintendo Color Resource (.NCLR) palette parser.
 */

#ifndef NCLR_H
#define NCLR_H

#include "common.h"
#include "ppm.h"

#define NCLR_MAX_COLORS 256 /**< Maximum capacity constraint of a single NCLR palette page. */

/**
 * @brief Container representing parsed NCLR color palette data.
 */
typedef struct NclrPalette {
    int color_count;                   /**< Number of colors stored. */
    u16 raw_colors[NCLR_MAX_COLORS];   /**< Raw BGR555 color values read from NCLR. */
    RgbaColor colors[NCLR_MAX_COLORS]; /**< Resolved 32-bit RGBA color array. */
} NclrPalette;

/**
 * @brief Parses species color palette data from raw NCLR binary stream.
 * @param data Pointer to binary data.
 * @param size Buffer size in bytes.
 * @param out_palette Destination structure to write palette records.
 * @return 0 on success; negative value on formatting or invalid signature errors.
 */
int Nclr_Parse(
    const u8 *data,
    size_t size,
    NclrPalette *out_palette
);

/**
 * @brief Debug printing routine to write color palette metadata to stdout.
 * @param palette Pointer to parsed palette.
 */
void Nclr_PrintInfo(const NclrPalette *palette);

#endif

