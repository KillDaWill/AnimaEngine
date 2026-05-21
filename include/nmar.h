#ifndef NMAR_H
#define NMAR_H

#include "common.h"

typedef struct NmarEntry {
    int map_index;
    u16 flags;
    char label[32];
} NmarEntry;

typedef struct NmarFrame {
    u32 raw_element_offset;
    int map_index;
    int duration;
    u16 marker;
    int transform_type;
    int rotation;
    int scale_x;
    int scale_y;
    int translate_x;
    int translate_y;
} NmarFrame;

typedef struct NmarAnimation {
    int frame_count;
    int loop_start;
    int playback_type;
    int format;
    u32 raw_frame_offset;
    char label[32];
    NmarFrame *frames;
} NmarAnimation;

typedef struct NmarFile {
    int entry_count;
    NmarEntry *entries;
    int animation_count;
    NmarAnimation *animations;
} NmarFile;

int Nmar_Parse(
    const u8 *data,
    size_t size,
    NmarFile *out_nmar
);

int Nmar_GetIdleMapIndex(const NmarFile *nmar);

int Nmar_GetIdleBreakMapIndex(
    const NmarFile *nmar,
    int idle_map,
    int map_count
);

int Nmar_GetFrameAtTick(
    const NmarFile *nmar,
    int animation_index,
    int tick,
    NmarFrame *out_frame
);

int Nmar_GetTotalDuration(
    const NmarFile *nmar,
    int animation_index
);

void Nmar_Free(NmarFile *nmar);
void Nmar_PrintInfo(const NmarFile *nmar);

#endif
