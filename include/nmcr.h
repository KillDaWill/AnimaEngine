/**
 * @file nmcr.h
 * @brief Nintendo Multi-Cell Resource (.NMCR) layout mapping parser.
 */

#ifndef NMCR_H
#define NMCR_H

#include "common.h"
#include "nanr.h"
#include "ncer.h"

/**
 * @brief Represents a reference linking a sub-animation timeline to cell layout transforms.
 */
typedef struct NmcrRecord {
    int animation_index; /**< Target sub-animation index within .NANR file. */
    int x;               /**< Horizontal display coordinate translation. */
    int y;               /**< Vertical display coordinate translation. */
    int flags;           /**< Layout attribute flags. */
} NmcrRecord;

/**
 * @brief Multi-cell layout composition group containing multiple records.
 */
typedef struct NmcrMap {
    int record_count;       /**< Total reference records count. */
    u32 raw_record_offset;  /**< Position offset of record table block. */
    NmcrRecord *records;    /**< Dynamic array of NmcrRecord. */
} NmcrMap;

/**
 * @brief Unpacked representation of a complete .NMCR resource.
 */
typedef struct NmcrFile {
    int map_count;  /**< Number of parsed map layout compositions. */
    NmcrMap *maps;  /**< Array of NmcrMap structures. */
} NmcrFile;

/**
 * @brief Parses an unpacked .NMCR layout mapping resource block from raw bytes.
 * @param data Raw binary input buffer.
 * @param size Input stream size in bytes.
 * @param out_nmcr Destination NmcrFile structure.
 * @return 0 on success; negative value on formatting or invalid signature errors.
 */
int Nmcr_Parse(const u8 *data, size_t size, NmcrFile *out_nmcr);

/**
 * @brief Deallocates all resources held inside an NmcrFile structure.
 * @param nmcr Pointer to structure.
 */
void Nmcr_Free(NmcrFile *nmcr);

/**
 * @brief Computes maximum frames required to play a composite layout.
 * @param map Pointer to cell layout map.
 * @param nanr Pointer to parsed .NANR timeline sequences.
 * @return Total frame duration count.
 */
int Nmcr_MaxFrameCount(const NmcrMap *map, const NanrFile *nanr);

/**
 * @brief Counts valid active cell records inside layout mapping for specified frame index.
 * @param map Pointer to cell layout map.
 * @param nanr Pointer to parsed .NANR timeline.
 * @param ncer Pointer to parsed .NCER cells.
 * @param frame_index Frame index request.
 * @return Number of active drawing sub-cells.
 */
int Nmcr_CountValidRecords(
    const NmcrMap *map,
    const NanrFile *nanr,
    const NcerFile *ncer,
    int frame_index
);

/**
 * @brief Measures similarity index difference scores to identify rare break animation map candidate.
 * @param idle_map Base idle layout map.
 * @param candidate_map Alternative layout candidate.
 * @param nanr Pointer to parsed .NANR timeline.
 * @param ncer Pointer to parsed .NCER cells.
 * @return Score difference value (lower score implies higher layout similarity).
 */
int Nmcr_ComputeBreakScore(
    const NmcrMap *idle_map,
    const NmcrMap *candidate_map,
    const NanrFile *nanr,
    const NcerFile *ncer
);

/**
 * @brief Debug prints NMCR layout stats to stdout.
 * @param nmcr Pointer to parsed NMCR.
 */
void Nmcr_PrintInfo(const NmcrFile *nmcr);

#endif

