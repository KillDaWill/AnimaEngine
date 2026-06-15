/**
 * @file nanr.h
 * @brief Nintendo Animation Resource (.NANR) structure parsing.
 */

#ifndef NANR_H
#define NANR_H

#include "common.h"

#define NANR_SCALE_ONE 4096            /**< Fixed-point scale representation of 1.0. */
#define NANR_ROTATION_FULL 65536.0     /**< Fixed-point representation of a full 360 degree rotation. */

/**
 * @brief Represents a single keyframe in an animation timeline.
 */
typedef struct NanrFrame {
    u32 raw_cell_value;     /**< Raw cell indices/values. */
    int cell_id;            /**< Target NCER cell layout index mapping to this keyframe. */
    int duration;           /**< Time to display this frame (in frames/ticks). */
    u16 marker;             /**< Sequence transition markers or event flags. */
    int transform_type;     /**< Type of transformation applied to frame (rotation/scaling). */
    int rotation;           /**< Fixed-point rotation angle. */
    int scale_x;            /**< Fixed-point horizontal scaling multiplier. */
    int scale_y;            /**< Fixed-point vertical scaling multiplier. */
    int translate_x;        /**< Frame offset translation (horizontal). */
    int translate_y;        /**< Frame offset translation (vertical). */
} NanrFrame;

/**
 * @brief Animation timeline sequence block containing multiple frames.
 */
typedef struct NanrAnimation {
    int frame_count;      /**< Total amount of keyframes in sequence. */
    int loop_start;       /**< Frame index where playback loops back. */
    int playback_type;    /**< Playing mode (forward, reverse, loop, ping-pong). */
    int format;           /**< Internal element storage format type flags. */
    u32 raw_frame_offset; /**< Virtual offset address of frame data block. */
    NanrFrame *frames;    /**< Dynamic array of NanrFrame structures. */
} NanrAnimation;

/**
 * @brief Unpacked representation of a complete .NANR resource file.
 */
typedef struct NanrFile {
    int animation_count;        /**< Number of animation sequences contained. */
    NanrAnimation *animations;  /**< Array of NanrAnimation structures. */
} NanrFile;

/**
 * @brief Parses an unpacked .NANR resource block from raw bytes.
 * @param data Pointer to binary file data buffer.
 * @param size Buffer size in bytes.
 * @param out_nanr Destination NanrFile structure.
 * @return 0 on success; negative value on invalid magic/formatting errors.
 */
int Nanr_Parse(const u8 *data, size_t size, NanrFile *out_nanr);

/**
 * @brief Deallocates all resources held inside a NanrFile structure.
 * @param nanr Pointer to structure to clear.
 */
void Nanr_Free(NanrFile *nanr);

/**
 * @brief Retrieves information for a specific keyframe in a timeline sequence.
 * @param nanr Pointer to parsed .NANR resource structure.
 * @param animation_index Target animation sequence index.
 * @param frame_index Frame index inside animation sequence.
 * @param out_frame Destination NanrFrame container to write results.
 * @return 0 on success; negative value on out-of-bounds index requests.
 */
int Nanr_GetResolvedCellId(
    const NanrFile *nanr,
    int animation_index,
    int frame_index,
    NanrFrame *out_frame
);

/**
 * @brief Resolves keyframe attributes at a specific tick timing step.
 * @param nanr Pointer to parsed .NANR resource structure.
 * @param animation_index Target animation sequence index.
 * @param tick Timing offset tick inside the timeline loop.
 * @param out_frame Destination NanrFrame container to write results.
 * @return 0 on success; negative value on out-of-bounds animation index.
 */
int Nanr_GetResolvedCellIdAtTick(
    const NanrFile *nanr,
    int animation_index,
    int tick,
    NanrFrame *out_frame
);

/**
 * @brief Debug printing routine to write parsed .NANR layout contents to stdout.
 * @param nanr Pointer to parsed .NANR resource.
 */
void Nanr_PrintInfo(const NanrFile *nanr);

#endif

