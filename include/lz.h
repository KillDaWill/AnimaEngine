#ifndef LZ_H
#define LZ_H

#include "common.h"

typedef enum CompressionType {
    COMPRESSION_NONE,
    COMPRESSION_LZ10,
    COMPRESSION_LZ11,
    COMPRESSION_UNKNOWN
} CompressionType;

CompressionType Lz_Detect(const u8 *data, size_t size);

const char *Lz_CompressionName(CompressionType type);

int Lz_Decompress(
    const u8 *in_data,
    size_t in_size,
    u8 **out_data,
    size_t *out_size,
    CompressionType *out_type
);

#endif