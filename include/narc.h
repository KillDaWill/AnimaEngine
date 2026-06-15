/**
 * @file narc.h
 * @brief Nintendo Archive Resource (.NARC) file format reading and unpacking.
 */

#ifndef NARC_H
#define NARC_H

#include "common.h"

/**
 * @brief Structure containing offsets and metadata representing an active .NARC archive file.
 */
typedef struct NarcArchive {
    const u8 *data;        /**< Pointer to raw binary source file data. */
    size_t size;           /**< Size of source data in bytes. */

    u32 btaf_offset;       /**< Start address of Block File Allocation Table (BTAF) block. */
    u32 btnf_offset;       /**< Start address of Block File Name Table (BTNF) block. */
    u32 gmif_offset;       /**< Start address of File Images (GMIF) block. */
    u32 gmif_data_offset;  /**< Start address of actual file payload contents. */

    u32 file_count;        /**< Number of individual files archived inside. */
} NarcArchive;

/**
 * @brief Offset and size boundaries of an individual member file within the NARC database.
 */
typedef struct NarcMemberRange {
    u32 start;             /**< Absolute start byte offset inside the game archive. */
    u32 end;               /**< Absolute end byte offset inside the game archive. */
    u32 size;              /**< Unpacked size in bytes. */
} NarcMemberRange;

/**
 * @brief Initializes and validates a NarcArchive from raw binary data.
 * @param narc Destination structure to populate.
 * @param data Pointer to raw file data.
 * @param size File size in bytes.
 * @return 0 on success; negative value on formatting or invalid signature errors.
 */
int Narc_Init(NarcArchive *narc, const u8 *data, size_t size);

/**
 * @brief Resolves the file offset boundaries of an archived member index.
 * @param narc Pointer to initialized archive.
 * @param member_id ID offset of target file.
 * @param out_range Destination container to write boundary coordinates.
 * @return 0 on success; negative value if member index is out of bounds.
 */
int Narc_GetMemberRange(
    const NarcArchive *narc,
    int member_id,
    NarcMemberRange *out_range
);

/**
 * @brief Copies/extracts an archived member from NARC into a separate buffer.
 * @param narc Pointer to initialized archive.
 * @param member_id ID offset of target file.
 * @param out_data Pointer to the destination buffer pointer, allocated inside this function.
 * @param out_size Pointer to store the size of the extracted member file.
 * @return 0 on success; negative value if extraction fails.
 */
int Narc_ExtractMember(
    const NarcArchive *narc,
    int member_id,
    u8 **out_data,
    size_t *out_size
);

/**
 * @brief Debug printer function writing NARC layout stats to console stdout.
 * @param narc Pointer to archive.
 */
void Narc_PrintInfo(const NarcArchive *narc);

#endif