/**
 * @file nitro_util.h
 * @brief Common section parsing and binary parsing helpers matching Nintendo Nitro specifications.
 */

#ifndef NITRO_UTIL_H
#define NITRO_UTIL_H

#include "common.h"

/**
 * @brief Verifies if the buffer starts with the target 4-character magic signature.
 * @param data Data buffer pointer.
 * @param size Buffer size in bytes.
 * @param magic 4-character format magic string (e.g. "NCLR").
 * @return 1 if magic matches; 0 otherwise.
 */
int Nitro_HasMagic(const u8 *data, size_t size, const char *magic);

/**
 * @brief Searches for a section starting with a specific 4-character magic string.
 * @param data Raw file data buffer.
 * @param size Buffer size in bytes.
 * @param magic Section magic identifier to search for (e.g. "PLTT", "CHAR").
 * @param out_offset Destination pointer to store the section start offset.
 * @param out_size Destination pointer to store the section size in bytes.
 * @return 0 on success; negative value if the section is not found.
 */
int Nitro_FindSection(
    const u8 *data,
    size_t size,
    const char *magic,
    size_t *out_offset,
    u32 *out_size
);

/**
 * @brief Safely reads the first 4 bytes of a buffer and formats them as a null-terminated string.
 * @param data Data buffer.
 * @param size Data size.
 * @param out Destination char array buffer (must be at least 5 bytes).
 */
void Nitro_GetPrintableMagic(const u8 *data, size_t size, char out[5]);

/**
 * @brief Math helper returning absolute value of an integer.
 * @param value Integer value.
 * @return Absolute integer value.
 */
int Nitro_AbsInt(int value);

/**
 * @brief Safely reads a little-endian signed 32-bit value.
 * @param p Pointer to byte stream.
 * @return Signed 32-bit integer.
 */
s32 Nitro_ReadS32LE(const u8 *p);

/**
 * @brief Safely reads a little-endian signed 16-bit value.
 * @param p Pointer to byte stream.
 * @return Signed 16-bit integer.
 */
s16 Nitro_ReadS16LE(const u8 *p);

#endif

