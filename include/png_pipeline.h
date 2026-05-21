#ifndef PNG_PIPELINE_H
#define PNG_PIPELINE_H

#include "common.h"
#include "ppm.h"
#include "ncgr.h"
#include "nclr.h"
#include "ncer.h"

#define PNG_PATH_BUFFER_SIZE 4096

int PngPipeline_TilePreview(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
);

int PngPipeline_CropToAlpha(
    const RgbaColor *pixels,
    int width,
    int height,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
);

int PngPipeline_CroppedSprite(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
);

int PngPipeline_ExactNcgr(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
);

int PngPipeline_CellPreviews(
    const char *ncgr_path,
    const char *ncer_path,
    const char *nclr_path,
    const char *out_dir
);

int PngPipeline_CombineSideBySide(
    const RgbaColor *front, int front_w, int front_h,
    const RgbaColor *back, int back_w, int back_h,
    RgbaColor **out_combined, int *out_combined_w, int *out_combined_h
);

int PngPipeline_CombineToGrid(
    const NcerFile *ncer,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    RgbaColor **out_combined, int *out_combined_w, int *out_combined_h
);

#endif
