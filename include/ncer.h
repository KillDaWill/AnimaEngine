/**
 * @file ncer.h
 * @brief Nintendo Cell Resource (.NCER) parser representing individual 2D sprite composite elements.
 */

#ifndef NCER_H
#define NCER_H

#include "common.h"
#include "ncgr.h"
#include "nclr.h"
#include "ppm.h"

/**
 * @brief Represents a single Object Attribute Memory (OAM) hardware sub-sprite element.
 */
typedef struct NcerOam {
    int x;                /**< Horizontal offset relative to cell origin. */
    int y;                /**< Vertical offset relative to cell origin. */

    int width;            /**< Resolved sub-sprite width in pixels. */
    int height;           /**< Resolved sub-sprite height in pixels. */

    int shape;            /**< Hardware shape format (0 = Square, 1 = Wide, 2 = Tall). */
    int size;             /**< Shape dimensions indicator (0 to 3). */

    int tile_index;       /**< Index address offset within NCGR tile character graphics. */
    int palette;          /**< Palette page index selection (0 to 15). */
    int priority;         /**< Rendering layers priorities indicator (0 = Top). */

    int flip_h;           /**< 1 if flipped horizontally, 0 otherwise. */
    int flip_v;           /**< 1 if flipped vertically, 0 otherwise. */
    int affine;           /**< 1 if affine rotation/scaling is enabled, 0 otherwise. */
    int double_size;      /**< 1 if double rendering bounding box bounds are active, 0 otherwise. */
    int obj_mode;         /**< Rendering object style (normal, transparent, window). */
    int affine_index;     /**< Offset index to the affine matrix table (0 to 31). */

    u16 attr0;            /**< Raw OAM Attribute 0 word read from file. */
    u16 attr1;            /**< Raw OAM Attribute 1 word read from file. */
    u16 attr2;            /**< Raw OAM Attribute 2 word read from file. */
} NcerOam;

/**
 * @brief A composite cell sprite made of multiple sub-sprite hardware OAM entries.
 */
typedef struct NcerCell {
    int oam_count;        /**< Number of hardware OAM elements comprising this cell. */
    int cell_attr;        /**< Internal formatting/packing attributes. */
    u32 raw_oam_offset;   /**< Position offset of OAM table block. */
    NcerOam *oams;        /**< Dynamic array of OAM segments. */
} NcerCell;

/**
 * @brief Unpacked representation of a complete .NCER resource.
 */
typedef struct NcerFile {
    int cell_count;       /**< Total number of cells. */
    NcerCell *cells;      /**< Array of NcerCell structures. */
} NcerFile;

/**
 * @brief Parses an unpacked .NCER cell layout block from raw bytes.
 * @param data Pointer to raw binary file data buffer.
 * @param size Buffer size in bytes.
 * @param out_ncer Destination structure.
 * @return 0 on success; negative value on formatting or invalid signature errors.
 */
int Ncer_Parse(
    const u8 *data,
    size_t size,
    NcerFile *out_ncer
);

/**
 * @brief Deallocates all resources held inside an NcerFile structure.
 * @param ncer Pointer to structure.
 */
void Ncer_Free(NcerFile *ncer);

/**
 * @brief Prints parsed .NCER layout metadata to stdout.
 * @param ncer Pointer to parsed NCER file.
 */
void Ncer_PrintInfo(const NcerFile *ncer);

/**
 * @brief Calculates resolved horizontal and vertical origins for drawing an OAM.
 * @param oam Sub-sprite structure.
 * @param out_x Output coordinate (horizontal).
 * @param out_y Output coordinate (vertical).
 */
void Ncer_GetOamDrawOrigin(
    const NcerOam *oam,
    int *out_x,
    int *out_y
);

/**
 * @brief Renders a composite cell into a raw RGBA pixels buffer.
 * @param ncer NCER resource.
 * @param cell_index Index of the cell to draw.
 * @param ncgr NCGR tile graphic resource.
 * @param palette NCLR palette resource.
 * @param out_pixels Output pointer to destination buffer, allocated inside this function.
 * @param out_width Width of rendered cell.
 * @param out_height Height of rendered cell.
 * @return 0 on success; negative value on failure.
 */
int Ncer_RenderCellToImage(
    const NcerFile *ncer,
    int cell_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
);

/**
 * @brief Renders a composite cell using specific tile character width stride formatting constraints.
 * @param ncer NCER resource.
 * @param cell_index Index of the cell to draw.
 * @param ncgr NCGR tile graphic resource.
 * @param palette NCLR palette resource.
 * @param tile_stride Width stride (in tiles) of NCGR image data.
 * @param out_pixels Output pointer to destination buffer.
 * @param out_width Width of rendered cell.
 * @param out_height Height of rendered cell.
 * @return 0 on success; negative value on failure.
 */
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

/**
 * @brief Computes bounding box limits for a cell.
 * @param cell Cell structure.
 * @param out_min_x Minimum horizontal coordinate.
 * @param out_min_y Minimum vertical coordinate.
 * @param out_max_x Maximum horizontal coordinate.
 * @param out_max_y Maximum vertical coordinate.
 */
void Ncer_CellBounds(
    const NcerCell *cell,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y
);

#endif

