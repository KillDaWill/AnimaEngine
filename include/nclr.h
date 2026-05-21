#ifndef NCLR_H
#define NCLR_H

#include "common.h"
#include "ppm.h"

#define NCLR_MAX_COLORS 256

typedef struct NclrPalette {
    int color_count;
    u16 raw_colors[NCLR_MAX_COLORS];
    RgbaColor colors[NCLR_MAX_COLORS];
} NclrPalette;

int Nclr_Parse(
    const u8 *data,
    size_t size,
    NclrPalette *out_palette
);

void Nclr_PrintInfo(const NclrPalette *palette);

#endif
