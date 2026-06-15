/**
 * @file ncgr.h
 * @brief Nintendo Character Graphic Resource (.NCGR) 2D tile character graphics structures.
 */

#ifndef NCGR_H
#define NCGR_H

#include "common.h"
#include "nclr.h"

/**
 * @brief Container representing parsed NCGR image tile graphics.
 */
typedef struct NcgrImage {
    const u8 *tile_data;     /**< Pointer to raw indexed pixel character data. */
    size_t tile_data_size;   /**< Size of pixel data in bytes. */

    int bpp;                 /**< Bits per pixel (e.g. 4 for 16-color, 8 for 256-color). */
    int tile_count;          /**< Total number of 8x8 character tiles. */
    int width_tiles;         /**< Image layout width in 8-pixel tiles (often -1 for character lists). */
    int height_tiles;        /**< Image layout height in 8-pixel tiles (often -1 for character lists). */

    u8 *allocated_data;      /**< Pointer to locally allocated duplicate buffer (or NULL). */
} NcgrImage;

/**
 * @brief Parses character graphics tiles from raw binary stream.
 * @param data Raw binary input.
 * @param size Input stream size in bytes.
 * @param out_image Destination struct.
 * @return 0 on success; negative value on formatting or invalid signature errors.
 */
int Ncgr_Parse(
    const u8 *data,
    size_t size,
    NcgrImage *out_image
);

/**
 * @brief Debug prints character graphics metadata to stdout.
 * @param image Pointer to NCGR image.
 */
void Ncgr_PrintInfo(const NcgrImage *image);

/**
 * @brief Deallocates local buffers inside NcgrImage structure.
 * @param image Pointer to NCGR image.
 */
void Ncgr_Free(NcgrImage *image);

/**
 * @brief Retrieves the color index offset for a pixel coordinate inside a specific 8x8 tile.
 * @param image Pointer to NCGR image.
 * @param tile_index Index of the target tile.
 * @param x Horizontal pixel coordinate relative to tile origin (0 to 7).
 * @param y Vertical pixel coordinate relative to tile origin (0 to 7).
 * @return Palette color index value.
 */
int Ncgr_GetPixelIndex(
    const NcgrImage *image,
    int tile_index,
    int x,
    int y
);

/**
 * @brief Blits a sub-sprite object onto an RGBA canvas.
 * @param image Pointer to NCGR image.
 * @param palette Palette resource containing colors.
 * @param canvas Destination RGBA pixel canvas buffer.
 * @param canvas_width Width of destination canvas in pixels.
 * @param canvas_height Height of destination canvas in pixels.
 * @param dst_x Horizontal canvas offset coordinates to start rendering.
 * @param dst_y Vertical canvas offset coordinates to start rendering.
 * @param tile_index Initial NCGR tile index.
 * @param obj_width Width of active sub-sprite in pixels.
 * @param obj_height Height of active sub-sprite in pixels.
 * @param palette_index Palette page offset selection.
 * @param flip_h 1 to mirror graphics horizontally.
 * @param flip_v 1 to mirror graphics vertically.
 */
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

/**
 * @brief Blits a sub-sprite object onto canvas using specific width tile character stride constraints.
 * @param image Pointer to NCGR image.
 * @param palette Palette resource.
 * @param canvas Destination RGBA canvas buffer.
 * @param canvas_width Canvas width in pixels.
 * @param canvas_height Canvas height in pixels.
 * @param dst_x Horizontal canvas offset.
 * @param dst_y Vertical canvas offset.
 * @param tile_index Initial NCGR tile index.
 * @param obj_width Width of active sub-sprite.
 * @param obj_height Height of active sub-sprite.
 * @param palette_index Palette page offset.
 * @param flip_h 1 to mirror horizontally.
 * @param flip_v 1 to mirror vertically.
 * @param tile_stride Width stride (in tiles) of NCGR image data.
 */
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

/**
 * @brief Renders sequential list of tiles directly to a grid sheet RGBA image.
 * @param image Pointer to NCGR image.
 * @param palette Palette resource.
 * @param tiles_per_row Grid width columns.
 * @param out_pixels Output destination RGBA pixels buffer.
 * @param out_width Width of generated sheet image.
 * @param out_height Height of generated sheet image.
 * @return 0 on success; negative value on error.
 */
int Ncgr_RenderTilesToImage(
    const NcgrImage *image,
    const NclrPalette *palette,
    int tiles_per_row,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
);

#endif

