/**
 * @file coords.h
 * @brief Coordinates file parsing structures and functions for character offset rendering.
 */

#ifndef COORDS_H
#define COORDS_H

#include "common.h"

#define COORD_RECORD_WORD_COUNT 12 /**< Number of 32-bit words in a raw coordinate record. */

/**
 * @brief Representation of a single sprite layout frame coordinate record.
 */
typedef struct CoordRecord {
    s32 raw_words[COORD_RECORD_WORD_COUNT]; /**< Raw header data from ROM file. */
    s32 offset_x;                           /**< Horizontal render offset. */
    s32 offset_y;                           /**< Vertical render offset. */
    s32 source_width;                       /**< Width of the source sprite graphic. */
    s32 source_height;                      /**< Height of the source sprite graphic. */
    s32 source_x;                           /**< Horizontal source coordinate in texture page. */
    s32 source_y;                           /**< Vertical source coordinate in texture page. */
} CoordRecord;

/**
 * @brief Collection of coordinate records parsed from a coordinates file.
 */
typedef struct CoordFile {
    int record_count;     /**< Number of records contained in this file structure. */
    CoordRecord *records; /**< Array of CoordRecord objects. */
} CoordFile;

/**
 * @brief Parses coordinate records from raw binary data.
 * @param data Pointer to the binary source data buffer.
 * @param size Size of the data buffer in bytes.
 * @param out Pointer to the CoordFile structure to populate.
 * @return 0 on success; negative value on error.
 */
int Coord_Parse(const u8 *data, size_t size, CoordFile *out);

/**
 * @brief Deallocates all resources held inside a CoordFile structure.
 * @param coords Pointer to the CoordFile structure to clear.
 */
void Coord_Free(CoordFile *coords);

#endif

