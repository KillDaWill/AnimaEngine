/**
 * @file lz.h
 * @brief Nintendo DS LZ10/LZ11 decompression implementation.
 */

#ifndef LZ_H
#define LZ_H

#include "common.h"

/**
 * @brief Categorization of compression headers found in NDS/Nitro file formats.
 */
typedef enum CompressionType {
    COMPRESSION_NONE,   /**< Uncompressed raw data block. */
    COMPRESSION_LZ10,   /**< Standard GBA/NDS LZSS compression format (0x10 magic). */
    COMPRESSION_LZ11,   /**< Extended higher-ratio LZSS compression format (0x11 magic). */
    COMPRESSION_UNKNOWN /**< Unsupported or invalid compression identifier. */
} CompressionType;

/**
 * @brief Auto-detects the compression format of a data buffer by inspecting its header byte.
 * @param data Pointer to raw data buffer.
 * @param size Buffer size in bytes.
 * @return Detected compression category.
 */
CompressionType Lz_Detect(const u8 *data, size_t size);

/**
 * @brief Returns a human-readable name string for a CompressionType enum.
 * @param type The compression type.
 * @return Read-only string description of compression algorithm.
 */
const char *Lz_CompressionName(CompressionType type);

/**
 * @brief Decompresses an LZ10 or LZ11 compressed data stream.
 * @param in_data Pointer to the raw compressed source buffer.
 * @param in_size Size of the compressed data stream in bytes.
 * @param out_data Pointer to the destination buffer pointer, allocated inside this function.
 * @param out_size Pointer to store the size of the decompressed output buffer.
 * @param out_type Optional output pointer to store the detected compression format.
 * @return 0 on success; negative value on invalid headers or decompression buffer overflow.
 */
int Lz_Decompress(
    const u8 *in_data,
    size_t in_size,
    u8 **out_data,
    size_t *out_size,
    CompressionType *out_type
);

#endif