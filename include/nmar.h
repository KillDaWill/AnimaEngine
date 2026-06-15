/**
 * @file nmar.h
 * @brief Nintendo Multi-Animation Resource (.NMAR) file parser.
 */

#ifndef NMAR_H
#define NMAR_H

#include "common.h"

/**
 * @brief Represents an individual layout mapping metadata entry record.
 */
typedef struct NmarEntry {
    int map_index;   /**< Associated NMCR cell map layout index. */
    u16 flags;       /**< Attributes/packing flags. */
    char label[32];  /**< Null-terminated identification tag string (e.g. "wait", "blink"). */
} NmarEntry;

/**
 * @brief Represents a single keyframe configuration block in a multi-animation sequence.
 */
typedef struct NmarFrame {
    u32 raw_element_offset; /**< Byte offset address of frame elements. */
    int map_index;          /**< NMCR layout index mapping to this keyframe. */
    int duration;           /**< Display duration of frame in ticks. */
    u16 marker;             /**< Transition indicator flags or event markers. */
    int transform_type;     /**< Type of transformation applied to frame (rotation/scaling). */
    int rotation;           /**< Fixed-point rotation angle. */
    int scale_x;            /**< Fixed-point horizontal scaling multiplier. */
    int scale_y;            /**< Fixed-point vertical scaling multiplier. */
    int translate_x;        /**< Keyframe horizontal pixel translate offset. */
    int translate_y;        /**< Keyframe vertical pixel translate offset. */
} NmarFrame;

/**
 * @brief Represents an individual animation timeline sequence block.
 */
typedef struct NmarAnimation {
    int frame_count;       /**< Total keyframes capacity. */
    int loop_start;        /**< Frame index where playback loop resets. */
    int playback_type;     /**< Playback method (forward, loop, reverse). */
    int format;            /**< Frame layout formats. */
    u32 raw_frame_offset;  /**< Virtual offset address of frame data block. */
    char label[32];        /**< Null-terminated name tag string. */
    NmarFrame *frames;     /**< Dynamic array of NmarFrame keyframes. */
} NmarAnimation;

/**
 * @brief Unpacked representation of a complete .NMAR resource.
 */
typedef struct NmarFile {
    int entry_count;             /**< Number of parsed header entries. */
    NmarEntry *entries;          /**< Array of NmarEntry structures. */
    int animation_count;         /**< Number of animation sequences contained. */
    NmarAnimation *animations;   /**< Array of NmarAnimation structures. */
} NmarFile;

/**
 * @brief Parses an unpacked .NMAR animation resource block from raw bytes.
 * @param data Raw binary input buffer.
 * @param size Input stream size in bytes.
 * @param out_nmar Destination NmarFile structure.
 * @return 0 on success; negative value on formatting or invalid signature errors.
 */
int Nmar_Parse(
    const u8 *data,
    size_t size,
    NmarFile *out_nmar
);

/**
 * @brief Resolves default idle layout map index.
 * @param nmar Pointer to parsed NMAR structure.
 * @return Map index, or 0 if not found.
 */
int Nmar_GetIdleMapIndex(const NmarFile *nmar);

/**
 * @brief Resolves rare idle break candidate layout map index.
 * @param nmar Pointer to parsed NMAR structure.
 * @param idle_map Base idle map index.
 * @param map_count Total number of maps available.
 * @return Alternative idle map index candidate, or base idle index if none.
 */
int Nmar_GetIdleBreakMapIndex(
    const NmarFile *nmar,
    int idle_map,
    int map_count
);

/**
 * @brief Resolves keyframe attributes at a specific tick timing step.
 * @param nmar Pointer to parsed NMAR structure.
 * @param animation_index Animation timeline index.
 * @param tick Target timing step.
 * @param out_frame Destination frame structure.
 * @return 0 on success; negative value on index errors.
 */
int Nmar_GetFrameAtTick(
    const NmarFile *nmar,
    int animation_index,
    int tick,
    NmarFrame *out_frame
);

/**
 * @brief Calculates total duration of a timeline sequence in ticks.
 * @param nmar Pointer to parsed NMAR structure.
 * @param animation_index Target animation sequence.
 * @return Total tick duration length.
 */
int Nmar_GetTotalDuration(
    const NmarFile *nmar,
    int animation_index
);

/**
 * @brief Deallocates all resources held inside an NmarFile structure.
 * @param nmar Pointer to structure.
 */
void Nmar_Free(NmarFile *nmar);

/**
 * @brief Debug prints NMAR layout stats to stdout.
 * @param nmar Pointer to parsed NMAR file.
 */
void Nmar_PrintInfo(const NmarFile *nmar);

#endif

