#include "png_pipeline.h"
#include "file_util.h"
#include "png_writer.h"

int PngPipeline_TilePreview(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
)
{
    u8 *ncgr_data;
    size_t ncgr_size;
    u8 *nclr_data;
    size_t nclr_size;

    NclrPalette palette;
    NcgrImage image;

    RgbaColor *pixels;
    int width;
    int height;

    ncgr_data = NULL;
    nclr_data = NULL;
    pixels = NULL;

    if (File_ReadAll(ncgr_path, &ncgr_data, &ncgr_size) != 0) {
        fprintf(stderr, "Warning: could not read NCGR %s\n", ncgr_path);
        return -1;
    }

    if (File_ReadAll(nclr_path, &nclr_data, &nclr_size) != 0) {
        fprintf(stderr, "Warning: could not read NCLR %s\n", nclr_path);
        free(ncgr_data);
        return -1;
    }

    if (Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        fprintf(stderr, "Warning: could not parse NCLR %s\n", nclr_path);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &image) != 0) {
        fprintf(stderr, "Warning: could not parse NCGR %s\n", ncgr_path);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    printf("Generating tile preview:\n");
    printf("  NCGR: %s\n", ncgr_path);
    printf("  NCLR: %s\n", nclr_path);
    printf("  OUT:  %s\n", out_path);

    Ncgr_PrintInfo(&image);
    Nclr_PrintInfo(&palette);

    if (Ncgr_RenderTilesToImage(
            &image,
            &palette,
            0,
            &pixels,
            &width,
            &height
        ) != 0) {
        fprintf(stderr, "Warning: could not render tiles.\n");
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Png_WriteRgbaImageScaled(out_path, pixels, width, height, 3) != 0) {
        fprintf(stderr, "Warning: could not write preview %s\n", out_path);
        free(pixels);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    printf("  preview size: %dx%d\n", width, height);
    printf("\n");

    free(pixels);
    free(ncgr_data);
    free(nclr_data);

    return 0;
}

int PngPipeline_CropToAlpha(
    const RgbaColor *pixels,
    int width,
    int height,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int x;
    int y;
    RgbaColor *cropped;

    if (pixels == NULL || out_pixels == NULL || out_width == NULL || out_height == NULL) {
        return -1;
    }

    min_x = width;
    min_y = height;
    max_x = -1;
    max_y = -1;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            const RgbaColor *px;

            px = &pixels[y * width + x];

            if (px->a != 0) {
                if (x < min_x) {
                    min_x = x;
                }
                if (y < min_y) {
                    min_y = y;
                }
                if (x > max_x) {
                    max_x = x;
                }
                if (y > max_y) {
                    max_y = y;
                }
            }
        }
    }

    if (max_x < min_x || max_y < min_y) {
        return -1;
    }

    *out_width = max_x - min_x + 1;
    *out_height = max_y - min_y + 1;

    cropped = malloc((size_t)(*out_width) * (size_t)(*out_height) * sizeof(RgbaColor));
    if (cropped == NULL) {
        return -1;
    }

    for (y = 0; y < *out_height; y++) {
        memcpy(
            cropped + ((size_t)y * (size_t)(*out_width)),
            pixels + ((size_t)(min_y + y) * (size_t)width) + min_x,
            (size_t)(*out_width) * sizeof(RgbaColor)
        );
    }

    *out_pixels = cropped;
    return 0;
}

int PngPipeline_CroppedSprite(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
)
{
    u8 *ncgr_data;
    size_t ncgr_size;
    u8 *nclr_data;
    size_t nclr_size;
    NclrPalette palette;
    NcgrImage image;
    RgbaColor *pixels;
    RgbaColor *cropped;
    int width;
    int height;
    int cropped_width;
    int cropped_height;

    ncgr_data = NULL;
    nclr_data = NULL;
    pixels = NULL;
    cropped = NULL;

    if (File_ReadAll(ncgr_path, &ncgr_data, &ncgr_size) != 0 ||
        File_ReadAll(nclr_path, &nclr_data, &nclr_size) != 0) {
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &image) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Ncgr_RenderTilesToImage(&image, &palette, 0, &pixels, &width, &height) != 0) {
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (PngPipeline_CropToAlpha(
            pixels,
            width,
            height,
            &cropped,
            &cropped_width,
            &cropped_height
        ) != 0) {
        free(pixels);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Png_WriteRgbaImageScaled(out_path, cropped, cropped_width, cropped_height, 4) != 0) {
        free(cropped);
        free(pixels);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    free(cropped);
    free(pixels);
    free(ncgr_data);
    free(nclr_data);

    return 0;
}

int PngPipeline_ExactNcgr(
    const char *ncgr_path,
    const char *nclr_path,
    const char *out_path
)
{
    u8 *ncgr_data;
    size_t ncgr_size;
    u8 *nclr_data;
    size_t nclr_size;
    NclrPalette palette;
    NcgrImage image;
    RgbaColor *pixels;
    int width;
    int height;

    ncgr_data = NULL;
    nclr_data = NULL;
    pixels = NULL;

    if (File_ReadAll(ncgr_path, &ncgr_data, &ncgr_size) != 0 ||
        File_ReadAll(nclr_path, &nclr_data, &nclr_size) != 0) {
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &image) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Ncgr_RenderTilesToImage(&image, &palette, 0, &pixels, &width, &height) != 0) {
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Png_WriteRgbaImage(out_path, pixels, width, height) != 0) {
        free(pixels);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    free(pixels);
    free(ncgr_data);
    free(nclr_data);

    return 0;
}

int PngPipeline_CellPreviews(
    const char *ncgr_path,
    const char *ncer_path,
    const char *nclr_path,
    const char *out_dir
)
{
    u8 *ncgr_data;
    size_t ncgr_size;
    u8 *ncer_data;
    size_t ncer_size;
    u8 *nclr_data;
    size_t nclr_size;

    NcgrImage ncgr;
    NcerFile ncer;
    NclrPalette palette;

    int i;

    ncgr_data = NULL;
    ncer_data = NULL;
    nclr_data = NULL;

    memset(&ncer, 0, sizeof(ncer));

    if (File_MkdirRecursive(out_dir) != 0) {
        fprintf(stderr, "Warning: could not create cell preview directory %s\n", out_dir);
        return -1;
    }

    if (File_ReadAll(ncgr_path, &ncgr_data, &ncgr_size) != 0) {
        fprintf(stderr, "Warning: could not read NCGR %s\n", ncgr_path);
        return -1;
    }

    if (File_ReadAll(ncer_path, &ncer_data, &ncer_size) != 0) {
        fprintf(stderr, "Warning: could not read NCER %s\n", ncer_path);
        free(ncgr_data);
        return -1;
    }

    if (File_ReadAll(nclr_path, &nclr_data, &nclr_size) != 0) {
        fprintf(stderr, "Warning: could not read NCLR %s\n", nclr_path);
        free(ncgr_data);
        free(ncer_data);
        return -1;
    }

    if (Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0) {
        fprintf(stderr, "Warning: could not parse NCGR %s\n", ncgr_path);
        free(ncgr_data);
        free(ncer_data);
        free(nclr_data);
        return -1;
    }

    if (Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        fprintf(stderr, "Warning: could not parse NCLR %s\n", nclr_path);
        free(ncgr_data);
        free(ncer_data);
        free(nclr_data);
        return -1;
    }

    if (Ncer_Parse(ncer_data, ncer_size, &ncer) != 0) {
        fprintf(stderr, "Warning: could not parse NCER %s\n", ncer_path);
        free(ncgr_data);
        free(ncer_data);
        free(nclr_data);
        return -1;
    }

    printf("Generating cell previews:\n");
    printf("  NCGR: %s\n", ncgr_path);
    printf("  NCER: %s\n", ncer_path);
    printf("  NCLR: %s\n", nclr_path);
    printf("  OUT:  %s\n", out_dir);

    Ncer_PrintInfo(&ncer);

    for (i = 0; i < ncer.cell_count; i++) {
        RgbaColor *pixels;
        int width;
        int height;
        char out_path[PNG_PATH_BUFFER_SIZE];

        pixels = NULL;

        if (Ncer_RenderCellToImage(
                &ncer,
                i,
                &ncgr,
                &palette,
                &pixels,
                &width,
                &height
            ) != 0) {
            fprintf(stderr, "Warning: could not render cell %d\n", i);
            continue;
        }

        snprintf(out_path, sizeof(out_path), "%s/cell_%03d.png", out_dir, i);

        if (Png_WriteRgbaImageScaled(out_path, pixels, width, height, 4) != 0) {
            fprintf(stderr, "Warning: could not write cell preview %s\n", out_path);
            free(pixels);
            continue;
        }

        free(pixels);
    }

    printf("\n");

    Ncer_Free(&ncer);
    free(ncgr_data);
    free(ncer_data);
    free(nclr_data);

    return 0;
}

int PngPipeline_CombineSideBySide(
    const RgbaColor *front, int front_w, int front_h,
    const RgbaColor *back, int back_w, int back_h,
    RgbaColor **out_combined, int *out_combined_w, int *out_combined_h
)
{
    int pad = 16;
    int margin = 8;
    int comb_w = front_w + back_w + pad + margin * 2;
    int comb_h = (front_h > back_h ? front_h : back_h) + margin * 2;
    int i;
    int y;
    int x;

    RgbaColor *comb = malloc((size_t)comb_w * (size_t)comb_h * sizeof(RgbaColor));
    if (comb == NULL) {
        return -1;
    }

    for (i = 0; i < comb_w * comb_h; i++) {
        comb[i].r = 0;
        comb[i].g = 0;
        comb[i].b = 0;
        comb[i].a = 0;
    }

    {
        int front_start_x = margin;
        int front_start_y = margin + (comb_h - margin * 2 - front_h) / 2;
        for (y = 0; y < front_h; y++) {
            for (x = 0; x < front_w; x++) {
                int dest_x = front_start_x + x;
                int dest_y = front_start_y + y;
                comb[dest_y * comb_w + dest_x] = front[y * front_w + x];
            }
        }
    }

    {
        int back_start_x = margin + front_w + pad;
        int back_start_y = margin + (comb_h - margin * 2 - back_h) / 2;
        for (y = 0; y < back_h; y++) {
            for (x = 0; x < back_w; x++) {
                int dest_x = back_start_x + x;
                int dest_y = back_start_y + y;
                comb[dest_y * comb_w + dest_x] = back[y * back_w + x];
            }
        }
    }

    *out_combined = comb;
    *out_combined_w = comb_w;
    *out_combined_h = comb_h;
    return 0;
}

int PngPipeline_CombineToGrid(
    const NcerFile *ncer,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    RgbaColor **out_combined, int *out_combined_w, int *out_combined_h
)
{
    int cell_count = ncer->cell_count;
    int cols;
    int rows;
    int block_size = 96;
    int padding = 4;
    int margin = 8;
    int sheet_w;
    int sheet_h;
    int i;
    RgbaColor *sheet;

    if (cell_count <= 0) {
        return -1;
    }

    cols = 4;
    rows = (cell_count + cols - 1) / cols;
    sheet_w = cols * block_size + (cols - 1) * padding + margin * 2;
    sheet_h = rows * block_size + (rows - 1) * padding + margin * 2;

    sheet = malloc((size_t)sheet_w * (size_t)sheet_h * sizeof(RgbaColor));
    if (sheet == NULL) {
        return -1;
    }

    for (i = 0; i < sheet_w * sheet_h; i++) {
        sheet[i].r = 0;
        sheet[i].g = 0;
        sheet[i].b = 0;
        sheet[i].a = 0;
    }

    for (i = 0; i < cell_count; i++) {
        RgbaColor *cell_pixels = NULL;
        int cell_w = 0, cell_h = 0;
        int col = i % cols;
        int row = i / cols;
        int block_x = margin + col * (block_size + padding);
        int block_y = margin + row * (block_size + padding);
        int start_x = block_x + (block_size - cell_w) / 2;
        int start_y = block_y + (block_size - cell_h) / 2;
        int y;
        int x;

        if (Ncer_RenderCellToImage(ncer, i, ncgr, palette, &cell_pixels, &cell_w, &cell_h) != 0) {
            continue;
        }

        start_x = block_x + (block_size - cell_w) / 2;
        start_y = block_y + (block_size - cell_h) / 2;

        for (y = 0; y < cell_h; y++) {
            for (x = 0; x < cell_w; x++) {
                int dest_x = start_x + x;
                int dest_y = start_y + y;
                if (dest_x >= 0 && dest_x < sheet_w && dest_y >= 0 && dest_y < sheet_h) {
                    sheet[dest_y * sheet_w + dest_x] = cell_pixels[y * cell_w + x];
                }
            }
        }

        free(cell_pixels);
    }

    *out_combined = sheet;
    *out_combined_w = sheet_w;
    *out_combined_h = sheet_h;
    return 0;
}
