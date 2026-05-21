#ifndef NCER_H
#define NCER_H

#include "common.h"
#include "ncgr.h"
#include "nclr.h"
#include "ppm.h"

typedef struct NcerOam {
    int x;
    int y;

    int width;
    int height;

    int shape;
    int size;

    int tile_index;
    int palette;
    int priority;

    int flip_h;
    int flip_v;
    int affine;
    int double_size;
    int obj_mode;
    int affine_index;

    u16 attr0;
    u16 attr1;
    u16 attr2;
} NcerOam;

typedef struct NcerCell {
    int oam_count;
    int cell_attr;
    u32 raw_oam_offset;
    NcerOam *oams;
} NcerCell;

typedef struct NcerFile {
    int cell_count;
    NcerCell *cells;
} NcerFile;

int Ncer_Parse(
    const u8 *data,
    size_t size,
    NcerFile *out_ncer
);

void Ncer_Free(NcerFile *ncer);

void Ncer_PrintInfo(const NcerFile *ncer);

void Ncer_GetOamDrawOrigin(
    const NcerOam *oam,
    int *out_x,
    int *out_y
);

int Ncer_RenderCellToImage(
    const NcerFile *ncer,
    int cell_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
);

int Ncer_RenderCellToImageWithTileStride(
    const NcerFile *ncer,
    int cell_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int tile_stride,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
);

void Ncer_CellBounds(
    const NcerCell *cell,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y
);

#endif
