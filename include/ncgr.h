#ifndef NCGR_H
#define NCGR_H

#include "common.h"
#include "nclr.h"

typedef struct NcgrImage {
    const u8 *tile_data;
    size_t tile_data_size;

    int bpp;
    int tile_count;
    int width_tiles;
    int height_tiles;

    u8 *allocated_data;
} NcgrImage;

int Ncgr_Parse(
    const u8 *data,
    size_t size,
    NcgrImage *out_image
);

void Ncgr_PrintInfo(const NcgrImage *image);
void Ncgr_Free(NcgrImage *image);

int Ncgr_GetPixelIndex(
    const NcgrImage *image,
    int tile_index,
    int x,
    int y
);

void Ncgr_BlitObjToCanvas(
    const NcgrImage *image,
    const NclrPalette *palette,
    RgbaColor *canvas,
    int canvas_width,
    int canvas_height,
    int dst_x,
    int dst_y,
    int tile_index,
    int obj_width,
    int obj_height,
    int palette_index,
    int flip_h,
    int flip_v
);

void Ncgr_BlitObjToCanvasWithTileStride(
    const NcgrImage *image,
    const NclrPalette *palette,
    RgbaColor *canvas,
    int canvas_width,
    int canvas_height,
    int dst_x,
    int dst_y,
    int tile_index,
    int obj_width,
    int obj_height,
    int palette_index,
    int flip_h,
    int flip_v,
    int tile_stride
);

int Ncgr_RenderTilesToImage(
    const NcgrImage *image,
    const NclrPalette *palette,
    int tiles_per_row,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
);

#endif
