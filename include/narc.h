#ifndef NARC_H
#define NARC_H

#include "common.h"

typedef struct NarcArchive {
    const u8 *data;
    size_t size;

    u32 btaf_offset;
    u32 btnf_offset;
    u32 gmif_offset;
    u32 gmif_data_offset;

    u32 file_count;
} NarcArchive;

typedef struct NarcMemberRange {
    u32 start;
    u32 end;
    u32 size;
} NarcMemberRange;

int Narc_Init(NarcArchive *narc, const u8 *data, size_t size);

int Narc_GetMemberRange(
    const NarcArchive *narc,
    int member_id,
    NarcMemberRange *out_range
);

int Narc_ExtractMember(
    const NarcArchive *narc,
    int member_id,
    u8 **out_data,
    size_t *out_size
);

void Narc_PrintInfo(const NarcArchive *narc);

#endif