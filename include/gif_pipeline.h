#ifndef GIF_PIPELINE_H
#define GIF_PIPELINE_H

#include "common.h"
#include "ncer.h"
#include "nanr.h"
#include "nmcr.h"
#include "nmar.h"
#include "ncgr.h"
#include "nclr.h"
#include "coords.h"

#define GIF_PATH_BUFFER_SIZE 4096

typedef enum GifSideMode {
    GIF_SIDE_FRONT,
    GIF_SIDE_BACK,
    GIF_SIDE_BOTH
} GifSideMode;

typedef enum GifPaletteMode {
    GIF_PALETTE_NORMAL,
    GIF_PALETTE_SHINY,
    GIF_PALETTE_BOTH
} GifPaletteMode;

typedef enum GifEyeMode {
    GIF_EYE_OPEN,
    GIF_EYE_ALL
} GifEyeMode;

typedef struct GifExportOptions {
    int enabled;
    GifSideMode side;
    GifPaletteMode palette;
    GifEyeMode eye_mode;
    int scale;
    int delay_cs;
    int playback_delay_cs;
    int loop_count;
    int start_frame;
    int frame_count;
    int map_is_idle;
    int map_index;
    int nmar_animation_index;
} GifExportOptions;

void GifExportOptions_Init(GifExportOptions *options);

int GifPipeline_ExportIdle(
    const char *out_dir,
    const char *side_name,
    const char *animation_name,
    const char *palette_name,
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int default_idle_map,
    const GifExportOptions *options,
    int tile_stride,
    int margin,
    const int *union_min_x,
    const int *union_min_y,
    const int *union_max_x,
    const int *union_max_y,
    const CoordFile *coords
);

#endif
