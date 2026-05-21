#ifndef NANR_H
#define NANR_H

#include "common.h"

#define NANR_SCALE_ONE 4096
#define NANR_ROTATION_FULL 65536.0

typedef struct NanrFrame {
    u32 raw_cell_value;
    int cell_id;
    int duration;
    u16 marker;
    int transform_type;
    int rotation;
    int scale_x;
    int scale_y;
    int translate_x;
    int translate_y;
} NanrFrame;

typedef struct NanrAnimation {
    int frame_count;
    int loop_start;
    int playback_type;
    int format;
    u32 raw_frame_offset;
    NanrFrame *frames;
} NanrAnimation;

typedef struct NanrFile {
    int animation_count;
    NanrAnimation *animations;
} NanrFile;

int Nanr_Parse(const u8 *data, size_t size, NanrFile *out_nanr);

void Nanr_Free(NanrFile *nanr);

int Nanr_GetResolvedCellId(
    const NanrFile *nanr,
    int animation_index,
    int frame_index,
    NanrFrame *out_frame
);

int Nanr_GetResolvedCellIdAtTick(
    const NanrFile *nanr,
    int animation_index,
    int tick,
    NanrFrame *out_frame
);

void Nanr_PrintInfo(const NanrFile *nanr);

#endif
