/**
 * @file nds_header.h
 * @brief Nintendo DS ROM header structure and format validation.
 */

#ifndef NDS_HEADER_H
#define NDS_HEADER_H

#include "common.h"

/**
 * @brief Representation of subset of core metadata field descriptors inside an NDS ROM header block.
 */
typedef struct NdsHeader {
    char game_title[13]; /**< Game title code string. */
    char game_code[5];   /**< Game ID serialization code (e.g. "IRBO" for Black, "IRAD" for White). */

    u32 fnt_offset;      /**< Start offset of File Name Table. */
    u32 fnt_size;        /**< Size of File Name Table in bytes. */

    u32 fat_offset;      /**< Start offset of File Allocation Table. */
    u32 fat_size;        /**< Size of File Allocation Table in bytes. */
} NdsHeader;

/**
 * @brief Parses essential header entries from a raw ROM buffer.
 * @param rom Pointer to binary ROM data.
 * @param rom_size ROM buffer size.
 * @param out_header Destination structure.
 * @return 0 on success; negative value on formatting or size constraint errors.
 */
int NdsHeader_Parse(const u8 *rom, size_t rom_size, NdsHeader *out_header);

/**
 * @brief Debug prints ROM header values to stdout.
 * @param header Pointer to header structure.
 * @param rom_size ROM size in bytes.
 */
void NdsHeader_Print(const NdsHeader *header, size_t rom_size);

/**
 * @brief Validates if the ROM matches known Gen 5 Pokemon game serial codes.
 * @param header Pointer to parsed header.
 * @return 1 if game code is Black/White/Black 2/White 2 (US/EU/JP variants); 0 otherwise.
 */
int NdsHeader_IsValidGame(const NdsHeader *header);

/**
 * @brief Checks if the target ROM is Black 2 or White 2.
 * @param header Pointer to parsed header.
 * @return 1 if game is Black 2 or White 2; 0 if Black or White.
 */
int NdsHeader_IsSequel(const NdsHeader *header);

#endif