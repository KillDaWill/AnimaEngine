#ifndef NMCR_H
#define NMCR_H

#include "common.h"
#include "nanr.h"
#include "ncer.h"

typedef struct NmcrRecord {
    int animation_index;
    int x;
    int y;
    int flags;
} NmcrRecord;

typedef struct NmcrMap {
    int record_count;
    u32 raw_record_offset;
    NmcrRecord *records;
} NmcrMap;

typedef struct NmcrFile {
    int map_count;
    NmcrMap *maps;
} NmcrFile;

int Nmcr_Parse(const u8 *data, size_t size, NmcrFile *out_nmcr);

void Nmcr_Free(NmcrFile *nmcr);

int Nmcr_MaxFrameCount(const NmcrMap *map, const NanrFile *nanr);

int Nmcr_CountValidRecords(
    const NmcrMap *map,
    const NanrFile *nanr,
    const NcerFile *ncer,
    int frame_index
);

int Nmcr_ComputeBreakScore(
    const NmcrMap *idle_map,
    const NmcrMap *candidate_map,
    const NanrFile *nanr,
    const NcerFile *ncer
);

void Nmcr_PrintInfo(const NmcrFile *nmcr);

#endif
