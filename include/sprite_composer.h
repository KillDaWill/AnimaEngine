/**
 * @file sprite_composer.h
 * @brief Layout compositing, affine transformations, and frame rendering engine.
 *
 * This module performs sprite compositing (OAM blitting) by reading cell layout tables (NCER),
 * mapping tiles (NCGR) and palettes (NCLR), applying animation keyframes (NANR), and combining
 * multi-sprite scene positioning records (NMCR) and timeline events (NMAR).
 */

#ifndef SPRITE_COMPOSER_H
#define SPRITE_COMPOSER_H

#include "common.h"
#include "ppm.h"
#include "ncer.h"
#include "nanr.h"
#include "nmcr.h"
#include "nmar.h"
#include "ncgr.h"
#include "nclr.h"
#include "coords.h"

/**
 * @brief Assembled global OAM record with complete transformations applied.
 */
typedef struct GlobalOam {
    int record_x;            /**< Raw horizontal position from NCER. */
    int record_y;            /**< Raw vertical position from NCER. */
    int draw_x;              /**< Resolved pixel-space rendering X coordinate. */
    int draw_y;              /**< Resolved pixel-space rendering Y coordinate. */
    int tile_index;          /**< Character tile graphics offset. */
    int width;               /**< Sprite layout width in pixels. */
    int height;              /**< Sprite layout height in pixels. */
    int palette_bank;        /**< Color palette slot index. */
    int flip_h;              /**< Horizontal mirror flag (1 = mirror, 0 = normal). */
    int flip_v;              /**< Vertical mirror flag (1 = mirror, 0 = normal). */
    int priority;            /**< Z-priority mapping value. */
    int original_index;      /**< Index of the OAM block in the NCER cell. */
    int rotation;            /**< Element rotation angle in degrees. */
    int scale_x;             /**< Horizontal scaling ratio (fixed-point or percentage). */
    int scale_y;             /**< Vertical scaling ratio. */
    int translate_x;         /**< Horizontal translation offset. */
    int translate_y;         /**< Vertical translation offset. */
    int parent_rotation;     /**< Propagated timeline/parent rotation. */
    int parent_scale_x;      /**< Propagated timeline/parent horizontal scale. */
    int parent_scale_y;      /**< Propagated timeline/parent vertical scale. */
    int parent_translate_x;  /**< Propagated timeline/parent horizontal translation. */
    int parent_translate_y;  /**< Propagated timeline/parent vertical translation. */
} GlobalOam;

/**
 * @brief Affine transformation parameters passed down during scene orchestration.
 */
typedef struct ComposerTransform {
    int rotation;            /**< Rotation angle in degrees. */
    int scale_x;             /**< Horizontal scaling factor. */
    int scale_y;             /**< Vertical scaling factor. */
    int translate_x;         /**< Horizontal translation offset. */
    int translate_y;         /**< Vertical translation offset. */
} ComposerTransform;

/**
 * @brief Fills the RGBA canvas with transparent pixels.
 * @param pixels Target pixel buffer.
 * @param width Canvas width.
 * @param height Canvas height.
 */
void Composer_ClearPixels(RgbaColor *pixels, int width, int height);

/**
 * @brief Calculates the extreme bounding box coordinates for a specific animation frame tick.
 * @param ncer Parsed Cell file.
 * @param nanr Parsed Animation sequence.
 * @param map Screen layout composite map.
 * @param tick Current animation frame index/tick.
 * @param out_min_x Output for minimum horizontal bound.
 * @param out_min_y Output for minimum vertical bound.
 * @param out_max_x Output for maximum horizontal bound.
 * @param out_max_y Output for maximum vertical bound.
 * @param coords Battle coordinates offset configuration (unused in placement checks).
 */
void Composer_ComputeBounds(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    int tick,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords
);

/**
 * @brief Bounding box calculator with parent timeline transformations.
 * @param ncer Cell details.
 * @param nanr Animation details.
 * @param map Map layout.
 * @param tick Animation frame tick.
 * @param out_min_x Minimum X coordinate bound output.
 * @param out_min_y Minimum Y coordinate bound output.
 * @param out_max_x Maximum X coordinate bound output.
 * @param out_max_y Maximum Y coordinate bound output.
 * @param coords Battle coordinates.
 * @param parent_transform Active parent scaling and rotation matrices.
 */
void Composer_ComputeBoundsWithTransform(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    int tick,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords,
    const ComposerTransform *parent_transform
);

/**
 * @brief Calculates bounding box encapsulating a full range of animation frames.
 * @param ncer Cell metadata.
 * @param nanr Animation sequence.
 * @param map Map layout.
 * @param start_frame Beginning index.
 * @param frame_count Amount of frames to scan.
 * @param out_min_x Output minimum horizontal coordinate.
 * @param out_min_y Output minimum vertical coordinate.
 * @param out_max_x Output maximum horizontal coordinate.
 * @param out_max_y Output maximum vertical coordinate.
 * @param coords Coordinate metadata anchors.
 * @param delay_cs Base frame duration delay in centiseconds.
 */
void Composer_ComputeBoundsRange(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    int start_frame,
    int frame_count,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords,
    int delay_cs
);

/**
 * @brief Composites a frame into an indexed 8-bit color-indexed layout canvas.
 * @param ncer Cells file.
 * @param nanr Animations sequence.
 * @param map Layout mapping.
 * @param ncgr Character tile image data.
 * @param palette Palettes array.
 * @param tick Active frame tick.
 * @param tile_stride Target tile stride (e.g. 32).
 * @param min_x Minimum horizontal coordinate anchor.
 * @param min_y Minimum vertical coordinate anchor.
 * @param width Target canvas rendering width.
 * @param height Target canvas rendering height.
 * @param margin Pixel safety margin size.
 * @param out_indices Preallocated output buffer to receive color index bytes.
 * @param coords Coordinate metadata.
 * @return 0 on success; negative on rendering exception.
 */
int Composer_RenderFrameIndexed(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int tick,
    int tile_stride,
    int min_x,
    int min_y,
    int width,
    int height,
    int margin,
    u8 *out_indices,
    const CoordFile *coords
);

/**
 * @brief Composites a transformed frame into an 8bpp color-indexed layout canvas.
 * @param ncer Cells file.
 * @param nanr Animations file.
 * @param map Map layout.
 * @param ncgr Tile pixels.
 * @param palette Palettes bank.
 * @param tick Active tick.
 * @param tile_stride NCGR stride.
 * @param min_x Horizontal canvas rendering offset.
 * @param min_y Vertical canvas rendering offset.
 * @param width Canvas width.
 * @param height Canvas height.
 * @param margin Canvas safety padding.
 * @param out_indices Output destination buffer for indexed color mapping.
 * @param coords Coordinates anchors.
 * @param parent_transform Active timeline transforms.
 * @return 0 on success; negative on rendering failure.
 */
int Composer_RenderFrameIndexedWithTransform(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int tick,
    int tile_stride,
    int min_x,
    int min_y,
    int width,
    int height,
    int margin,
    u8 *out_indices,
    const CoordFile *coords,
    const ComposerTransform *parent_transform
);

/**
 * @brief Renders a composite frame directly into a 32-bit RGBA pixel array.
 * @param ncer Cell details.
 * @param nanr Animation frames.
 * @param map Map configurations.
 * @param ncgr Tile pixels.
 * @param palette Palette registers.
 * @param tick Frame tick index.
 * @param tile_stride NCGR tile width stride.
 * @param label Debug output identifier.
 * @param out_pixels Pointer filled with newly allocated RGBA pixel buffer on success.
 * @param out_width Width of generated RGBA frame.
 * @param out_height Height of generated RGBA frame.
 * @param coords Coordinate metadata anchors.
 * @return 0 on success; negative on rendering exception.
 */
int Composer_RenderFrameRgba(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int tick,
    int tile_stride,
    const char *label,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height,
    const CoordFile *coords
);

/**
 * @brief Crops transparent margins from index-compiled spritesheet animation loops.
 * @param frames Multi-frame sequential 8bpp color buffer.
 * @param frame_count Number of frames inside the buffer.
 * @param width Raw source canvas width.
 * @param height Raw source canvas height.
 * @param out_frames Newly allocated cropped multi-frame indexed buffer output.
 * @param out_width Width of the cropped bounding box.
 * @param out_height Height of the cropped bounding box.
 * @return 0 on success; negative on invalid bounds or memory exception.
 */
int Composer_CropIndexedFrames(
    const u8 *frames,
    int frame_count,
    int width,
    int height,
    u8 **out_frames,
    int *out_width,
    int *out_height
);

/**
 * @brief Computes combined bounding box that fits two animation sets sharing a unified canvas space.
 * @param ncer Cell details.
 * @param nanr Animation details.
 * @param map_a First map configuration.
 * @param map_a_frame_count Frame count of map A.
 * @param map_b Second map configuration.
 * @param map_b_frame_count Frame count of map B.
 * @param out_min_x Output minimum X bound.
 * @param out_min_y Output minimum Y bound.
 * @param out_max_x Output maximum X bound.
 * @param out_max_y Output maximum Y bound.
 * @param coords Coordinate metadata anchors.
 * @param delay_cs Base timeline centisecond step.
 */
void Composer_ComputeUnionBounds(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map_a,
    int map_a_frame_count,
    const NmcrMap *map_b,
    int map_b_frame_count,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords,
    int delay_cs
);

/**
 * @brief Renders a complete loop merging a base idle animation and a break sequence.
 * @param ncer Cell tables.
 * @param nanr Animation sequences.
 * @param idle_map Battle idle composite map.
 * @param break_map Battle break candidate map.
 * @param ncgr Tile pixels.
 * @param palette Palette data.
 * @param idle_repetitions Pre-calculated iterations of the idle loop before playing break.
 * @param tile_stride Target tile stride.
 * @param margin Safety boundary padding.
 * @param out_frames Newly allocated cropped multi-frame indexed index buffer.
 * @param out_frame_count Frame count of generated loop.
 * @param out_width Bounded canvas width.
 * @param out_height Bounded canvas height.
 * @param coords Coordinates anchors.
 * @param delay_cs Centisecond duration.
 * @return 0 on success; negative on rendering failure.
 */
int Composer_RenderComposedAnimation(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *idle_map,
    const NmcrMap *break_map,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int idle_repetitions,
    int tile_stride,
    int margin,
    u8 **out_frames,
    int *out_frame_count,
    int *out_width,
    int *out_height,
    const CoordFile *coords,
    int delay_cs
);

/**
 * @brief Timeline-driven composed loop renderer that applies full parent translation transforms.
 * @param ncer Cell layouts.
 * @param nanr Animation sequences.
 * @param nmcr Direct layouts map file.
 * @param nmar High-level animation timeline tracks.
 * @param idle_map Default idle map.
 * @param break_map Labeled break map.
 * @param idle_animation_index Labeled idle NMAR index.
 * @param break_animation_index Labeled break NMAR index.
 * @param ncgr Indexed tile sheet.
 * @param palette Color palettes.
 * @param idle_repetitions Desired loops of idle sequence.
 * @param tile_stride NCGR stride.
 * @param margin Bounding padding.
 * @param out_frames Pointer to cropped multi-frame index buffer output.
 * @param out_frame_count Frame count of compiled timeline loop.
 * @param out_width Width of bounding canvas.
 * @param out_height Height of bounding canvas.
 * @param coords Coordinates anchors.
 * @param delay_cs Timeline time step.
 * @return 0 on success; negative on rendering failure.
 */
int Composer_RenderComposedAnimationTimeline(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    const NmcrMap *idle_map,
    const NmcrMap *break_map,
    int idle_animation_index,
    int break_animation_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int idle_repetitions,
    int tile_stride,
    int margin,
    u8 **out_frames,
    int *out_frame_count,
    int *out_width,
    int *out_height,
    const CoordFile *coords,
    int delay_cs
);

/**
 * @brief Computes optimal number of idle loops before transition so loop length is ~3 seconds.
 * @param nanr Animation frames sequence.
 * @param idle_map Default battle idle map.
 * @return Repetitions value (clamped between 1 and 10).
 */
int Composer_ComputeIdleRepetitions(
    const NanrFile *nanr,
    const NmcrMap *idle_map
);

/**
 * @brief Timeline-driven optimal idle loops counter before playing break sequence.
 * @param nmar Timeline track details.
 * @param animation_index Labeled idle animation timeline track index.
 * @return Repetitions count (clamped 1 to 10).
 */
int Composer_ComputeNmarIdleRepetitions(
    const NmarFile *nmar,
    int animation_index
);

#endif
