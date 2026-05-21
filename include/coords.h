#ifndef COORDS_H
#define COORDS_H

#include "common.h"

#define COORD_RECORD_WORD_COUNT 12

typedef struct CoordRecord {
    s32 raw_words[COORD_RECORD_WORD_COUNT];
    s32 offset_x;
    s32 offset_y;
    s32 source_width;
    s32 source_height;
    s32 source_x;
    s32 source_y;
} CoordRecord;

typedef struct CoordFile {
    int record_count;
    CoordRecord *records;
} CoordFile;

int Coord_Parse(const u8 *data, size_t size, CoordFile *out);
void Coord_Free(CoordFile *coords);

#endif
