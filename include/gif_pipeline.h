/**
 * @file gif_pipeline.h
 * @brief High-level animation assembly and LZW GIF compilation pipeline.
 */

#ifndef GIF_PIPELINE_H
#define GIF_PIPELINE_H

#include "common.h"
#include "ncer.h"
#include "nanr.h"
#include "nmcr.h"
#include "nmar.h"
#include "ncgr.h"
#include "nclr.h"
#include "coords.h"

#define GIF_PATH_BUFFER_SIZE 4096 /**< Maximum buffer size for generating output GIF filenames. */

/**
 * @brief Sprite perspective/camera rendering selection.
 */
typedef enum GifSideMode {
    GIF_SIDE_FRONT, /**< Front view (as seen by player in battle). */
    GIF_SIDE_BACK,  /**< Back view (as seen by player for their own Pokemon). */
    GIF_SIDE_BOTH   /**< Process/export both front and back views. */
} GifSideMode;

/**
 * @brief Color variant selection.
 */
typedef enum GifPaletteMode {
    GIF_PALETTE_NORMAL, /**< Default species colors. */
    GIF_PALETTE_SHINY,  /**< Alternating rare colors. */
    GIF_PALETTE_BOTH    /**< Process/export both normal and shiny variants. */
} GifPaletteMode;

/**
 * @brief Unused/legacy eye blinking rendering selection.
 */
typedef enum GifEyeMode {
    GIF_EYE_OPEN, /**< Keep eyes open. */
    GIF_EYE_ALL   /**< Process all states. */
} GifEyeMode;

/**
 * @brief Detailed parameters configuring animation exporting behavior.
 */
typedef struct GifExportOptions {
    int enabled;              /**< Whether this format is enabled for current export. */
    GifSideMode side;         /**< Perspective selection. */
    GifPaletteMode palette;   /**< Palette color variant selection. */
    GifEyeMode eye_mode;      /**< Blinking frame option. */
    int scale;                /**< Bilinear pixel scaling multiplier (e.g. 1, 2, 4). */
    int delay_cs;             /**< Fixed override frame duration in centiseconds. */
    int playback_delay_cs;    /**< Overridden playback duration of final frame in centiseconds. */
    int loop_count;           /**< GIF repeat/loop count (0 = infinite). */
    int start_frame;          /**< Initial frame index to begin export. */
    int frame_count;          /**< Number of animation frames to render. */
    int map_is_idle;          /**< 1 if mapping should default to catalog-based idle animations. */
    int map_index;            /**< Target individual NMCR map index. */
    int nmar_animation_index; /**< Target individual NMAR animation timeline sequence index. */
} GifExportOptions;

/**
 * @brief Initializes default values for a GifExportOptions structure.
 * @param options Pointer to options struct to modify.
 */
void GifExportOptions_Init(GifExportOptions *options);

/**
 * @brief Assembles and exports a composed animated GIF based on provided resources.
 * @param out_dir Directory path to write output file.
 * @param side_name Label used in output filename (e.g., "front", "back").
 * @param animation_name Label representing animation sequence (e.g., "idle", "break").
 * @param palette_name Label for color variant (e.g., "normal", "shiny").
 * @param ncer Pointer to the parsed cell resource file (.NCER).
 * @param nanr Pointer to the parsed animation resource file (.NANR).
 * @param nmcr Pointer to the parsed multi-cell mapping layout (.NMCR).
 * @param nmar Pointer to the parsed multi-animation sequence (.NMAR).
 * @param ncgr Pointer to the parsed character graphics tiles (.NCGR).
 * @param palette Pointer to the parsed color palette resource (.NCLR).
 * @param default_idle_map Default layout index for species' base idle frame.
 * @param options Export configurations.
 * @param tile_stride Width stride (in tiles) of NCGR image data.
 * @param margin Safety boundary width in pixels around the composed animation.
 * @param union_min_x Precalculated bounding box minimum horizontal coordinate.
 * @param union_min_y Precalculated bounding box minimum vertical coordinate.
 * @param union_max_x Precalculated bounding box maximum horizontal coordinate.
 * @param union_max_y Precalculated bounding box maximum vertical coordinate.
 * @param coords Optional CoordFile pointers to adjust horizontal/vertical offsets.
 * @return 0 on success; negative value on error.
 */
int GifPipeline_ExportIdle(
    const char *out_dir,
    const char *side_name,
    const char *animation_name,
    const char *palette_name,
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int default_idle_map,
    const GifExportOptions *options,
    int tile_stride,
    int margin,
    const int *union_min_x,
    const int *union_min_y,
    const int *union_max_x,
    const int *union_max_y,
    const CoordFile *coords
);

#endif

