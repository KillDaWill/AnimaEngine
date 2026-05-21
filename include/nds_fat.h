#ifndef NDS_FAT_H
#define NDS_FAT_H

#include "common.h"
#include "nds_header.h"

typedef struct NdsFatRange {
    u32 start;
    u32 end;
    u32 size;
} NdsFatRange;

int NdsFat_GetRange(
    const u8 *rom,
    size_t rom_size,
    const NdsHeader *header,
    int file_id,
    NdsFatRange *out_range
);

#endif