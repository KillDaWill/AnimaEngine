/**
 * @file nds_fat.h
 * @brief Nintendo DS File Allocation Table (FAT) parser structure.
 */

#ifndef NDS_FAT_H
#define NDS_FAT_H

#include "common.h"
#include "nds_header.h"

/**
 * @brief Represents offset boundaries of an individual file in the NDS ROM filesystem.
 */
typedef struct NdsFatRange {
    u32 start; /**< Start byte offset inside ROM file. */
    u32 end;   /**< End byte offset inside ROM file. */
    u32 size;  /**< Size of the file in bytes. */
} NdsFatRange;

/**
 * @brief Reads boundary coordinates of a specific file ID from the NDS ROM FAT.
 * @param rom Pointer to loaded NDS ROM buffer.
 * @param rom_size ROM size in bytes.
 * @param header Pointer to parsed ROM header containing FAT offset configurations.
 * @param file_id Target file index identifier.
 * @param out_range Destination structure to populate.
 * @return 0 on success; negative value on out-of-bounds parameters.
 */
int NdsFat_GetRange(
    const u8 *rom,
    size_t rom_size,
    const NdsHeader *header,
    int file_id,
    NdsFatRange *out_range
);

#endif