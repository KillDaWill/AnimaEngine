/**
 * @file png_pipeline.h
 * @brief Sprite sheet grid composition and PNG drawing pipeline.
 */

#ifndef PNG_PIPELINE_H
#define PNG_PIPELINE_H

#include "common.h"
#include "ppm.h"
#include "ncgr.h"
#include "nclr.h"
#include "ncer.h"

#define PNG_PATH_BUFFER_SIZE 4096 /**< Maximum buffer size for generating output PNG file paths. */

/**
 * @brief Renders NCGR tiles in order to a standard PNG preview file.
 * @param ncgr_path Path to the NCGR file.
 * @param nclr_path Path to the NCLR palette file.
 * @param out_path Destination file path.
 * @return 0 on success; negative value on error.
 */
int PngPipeline_TilePreview(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
);

/**
 * @brief Crops transparent borders around a pixel frame buffer.
 * @param pixels Source RGBA pixels buffer.
 * @param width Input canvas width.
 * @param height Input canvas height.
 * @param out_pixels Output cropped RGBA buffer (allocated within function).
 * @param out_width Width of cropped image.
 * @param out_height Height of cropped image.
 * @return 0 on success; negative value on empty or transparent input.
 */
int PngPipeline_CropToAlpha(
    const RgbaColor *pixels,
    int width,
    int height,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
);

/**
 * @brief Renders the first cell frame from NCGR tiles, cropped to its alpha bounding box, and saves it.
 * @param ncgr_path Path to NCGR file.
 * @param nclr_path Path to NCLR file.
 * @param out_path Destination file path.
 * @return 0 on success; negative value on failure.
 */
int PngPipeline_CroppedSprite(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
);

/**
 * @brief Renders the exact tile character page layout defined by NCGR and exports as PNG.
 * @param ncgr_path Path to NCGR file.
 * @param nclr_path Path to NCLR file.
 * @param out_path Destination file path.
 * @return 0 on success; negative value on failure.
 */
int PngPipeline_ExactNcgr(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
);

/**
 * @brief Renders all individual cells in NCGR as static PNGs.
 * @param ncgr_path Path to NCGR file.
 * @param ncer_path Path to NCER cell resource.
 * @param nclr_path Path to NCLR palette file.
 * @param out_dir Directory path to write output PNGs.
 * @return 0 on success; negative value on failure.
 */
int PngPipeline_CellPreviews(
    const char *ncgr_path,
    const char *ncer_path,
    const char *nclr_path,
    const char *out_dir
);

/**
 * @brief Stitches two RGBA images side-by-side into a single combined buffer.
 * @param front Front frame RGBA pixels buffer.
 * @param front_w Front frame width.
 * @param front_h Front frame height.
 * @param back Back frame RGBA pixels buffer.
 * @param back_w Back frame width.
 * @param back_h Back frame height.
 * @param out_combined Destination combined RGBA pixels buffer (allocated within function).
 * @param out_combined_w Width of stitched image.
 * @param out_combined_h Height of stitched image.
 * @return 0 on success; negative value on memory failure.
 */
int PngPipeline_CombineSideBySide(
    const RgbaColor *front, int front_w, int front_h,
    const RgbaColor *back, int back_w, int back_h,
    RgbaColor **out_combined, int *out_combined_w, int *out_combined_h
);

/**
 * @brief Recomposes and stitches all frames defined in NCER cells side-by-side in a single grid sheet.
 * @param ncer NCER resource file.
 * @param ncgr NCGR graphics resource.
 * @param palette NCLR palette resource.
 * @param out_combined Destination combined RGBA pixels buffer (allocated within function).
 * @param out_combined_w Width of stitched spritesheet image.
 * @param out_combined_h Height of stitched spritesheet image.
 * @return 0 on success; negative value on failure.
 */
int PngPipeline_CombineToGrid(
    const NcerFile *ncer,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    RgbaColor **out_combined, int *out_combined_w, int *out_combined_h
);

#endif

