#include "sprite_composer.h"
#include "coords.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_GLOBAL_OAMS 2048
#define COMPOSITE_MARGIN 8

static double NanrScaleToDouble(int scale)
{
    if (scale == 0) {
        return 1.0;
    }

    return (double)scale / (double)NANR_SCALE_ONE;
}

static void Composer_SetParentTransform(
    GlobalOam *goam,
    const ComposerTransform *parent_transform
)
{
    if (goam == NULL) {
        return;
    }

    if (parent_transform == NULL) {
        goam->parent_rotation = 0;
        goam->parent_scale_x = NANR_SCALE_ONE;
        goam->parent_scale_y = NANR_SCALE_ONE;
        goam->parent_translate_x = 0;
        goam->parent_translate_y = 0;
        return;
    }

    goam->parent_rotation = parent_transform->rotation;
    goam->parent_scale_x = parent_transform->scale_x;
    goam->parent_scale_y = parent_transform->scale_y;
    goam->parent_translate_x = parent_transform->translate_x;
    goam->parent_translate_y = parent_transform->translate_y;
}

static void ParentTransformPoint(
    const GlobalOam *goam,
    double child_x,
    double child_y,
    double *out_x,
    double *out_y
)
{
    double angle;
    double sin_a;
    double cos_a;
    double scale_x;
    double scale_y;
    double scaled_x;
    double scaled_y;

    angle = ((double)goam->parent_rotation) * ((2.0 * M_PI) / NANR_ROTATION_FULL);
    sin_a = sin(angle);
    cos_a = cos(angle);
    scale_x = NanrScaleToDouble(goam->parent_scale_x);
    scale_y = NanrScaleToDouble(goam->parent_scale_y);
    scaled_x = child_x * scale_x;
    scaled_y = child_y * scale_y;

    *out_x = (double)goam->parent_translate_x + scaled_x * cos_a - scaled_y * sin_a;
    *out_y = (double)goam->parent_translate_y + scaled_x * sin_a + scaled_y * cos_a;
}

static void ParentInverseTransformPoint(
    const GlobalOam *goam,
    double world_x,
    double world_y,
    double *out_child_x,
    double *out_child_y
)
{
    double angle;
    double sin_a;
    double cos_a;
    double scale_x;
    double scale_y;
    double dx;
    double dy;

    angle = ((double)goam->parent_rotation) * ((2.0 * M_PI) / NANR_ROTATION_FULL);
    sin_a = sin(angle);
    cos_a = cos(angle);
    scale_x = NanrScaleToDouble(goam->parent_scale_x);
    scale_y = NanrScaleToDouble(goam->parent_scale_y);
    dx = world_x - (double)goam->parent_translate_x;
    dy = world_y - (double)goam->parent_translate_y;

    *out_child_x = (dx * cos_a + dy * sin_a) / scale_x;
    *out_child_y = (-dx * sin_a + dy * cos_a) / scale_y;
}

static int RoundDoubleToInt(double value)
{
    if (value >= 0.0) {
        return (int)floor(value + 0.5);
    }

    return (int)ceil(value - 0.5);
}

static void GlobalOamTransformPoint(
    const GlobalOam *goam,
    double local_x,
    double local_y,
    double *out_x,
    double *out_y
)
{
    double angle;
    double sin_a;
    double cos_a;
    double scale_x;
    double scale_y;
    double scaled_x;
    double scaled_y;
    double child_x;
    double child_y;

    angle = ((double)goam->rotation) * ((2.0 * M_PI) / NANR_ROTATION_FULL);
    sin_a = sin(angle);
    cos_a = cos(angle);
    scale_x = NanrScaleToDouble(goam->scale_x);
    scale_y = NanrScaleToDouble(goam->scale_y);
    scaled_x = local_x * scale_x;
    scaled_y = local_y * scale_y;

    child_x = (double)goam->record_x + (double)goam->translate_x +
        scaled_x * cos_a - scaled_y * sin_a;
    child_y = (double)goam->record_y + (double)goam->translate_y +
        scaled_x * sin_a + scaled_y * cos_a;
    ParentTransformPoint(goam, child_x, child_y, out_x, out_y);
}

static void GlobalOamInverseTransformPoint(
    const GlobalOam *goam,
    double world_x,
    double world_y,
    double *out_local_x,
    double *out_local_y
)
{
    double angle;
    double sin_a;
    double cos_a;
    double scale_x;
    double scale_y;
    double dx;
    double dy;
    double child_x;
    double child_y;
    double unrotated_x;
    double unrotated_y;

    angle = ((double)goam->rotation) * ((2.0 * M_PI) / NANR_ROTATION_FULL);
    sin_a = sin(angle);
    cos_a = cos(angle);
    scale_x = NanrScaleToDouble(goam->scale_x);
    scale_y = NanrScaleToDouble(goam->scale_y);

    ParentInverseTransformPoint(goam, world_x, world_y, &child_x, &child_y);
    dx = child_x - (double)goam->record_x - (double)goam->translate_x;
    dy = child_y - (double)goam->record_y - (double)goam->translate_y;
    unrotated_x = dx * cos_a + dy * sin_a;
    unrotated_y = -dx * sin_a + dy * cos_a;

    *out_local_x = unrotated_x / scale_x;
    *out_local_y = unrotated_y / scale_y;
}

static void GlobalOamTransformedBounds(
    const GlobalOam *goam,
    double *out_min_x,
    double *out_min_y,
    double *out_max_x,
    double *out_max_y
)
{
    double corners_x[4];
    double corners_y[4];
    double min_x;
    double min_y;
    double max_x;
    double max_y;
    int i;

    corners_x[0] = (double)goam->draw_x;
    corners_y[0] = (double)goam->draw_y;
    corners_x[1] = (double)(goam->draw_x + goam->width);
    corners_y[1] = (double)goam->draw_y;
    corners_x[2] = (double)goam->draw_x;
    corners_y[2] = (double)(goam->draw_y + goam->height);
    corners_x[3] = (double)(goam->draw_x + goam->width);
    corners_y[3] = (double)(goam->draw_y + goam->height);

    min_x = 999999.0;
    min_y = 999999.0;
    max_x = -999999.0;
    max_y = -999999.0;

    for (i = 0; i < 4; i++) {
        double x;
        double y;

        GlobalOamTransformPoint(goam, corners_x[i], corners_y[i], &x, &y);

        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
    }

    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
}

static void ApplyType3CoordOffset(
    const NmcrRecord *record,
    const NanrFrame *frame,
    const NcerCell *cell,
    const CoordFile *coords,
    int *translate_x,
    int *translate_y
)
{
    (void)record;
    (void)frame;
    (void)cell;
    (void)coords;
    (void)translate_x;
    (void)translate_y;

    /* Gen V BW battle sprites already compose correctly from NMCR record
     * x/y plus NANR frame transforms. The coordinate block carries useful
     * source/anchor metadata, but treating its offset words as extra Type 3
     * translation duplicates placement data and breaks known references:
     * Venusaur, Nidorina, Pinsir, Eevee, and the Klink family. Keep this hook
     * intentionally inert unless a verified species proves a narrower need. */
}

static int BuildGlobalOamsForFrame(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    int tick,
    const char *label,
    GlobalOam *global_oams,
    int max_global_oams,
    const CoordFile *coords,
    const ComposerTransform *parent_transform
)
{
    int i;
    int global_oam_count;

    global_oam_count = 0;

    for (i = 0; i < map->record_count; i++) {
        const NmcrRecord *record;
        NanrFrame frame;
        int cell_id;
        const NcerCell *cell;
        int translate_x;
        int translate_y;
        int j;

        record = &map->records[i];
        cell_id = Nanr_GetResolvedCellIdAtTick(nanr, record->animation_index, tick, &frame);
        translate_x = frame.translate_x;
        translate_y = frame.translate_y;

        if (cell_id < 0 || cell_id >= ncer->cell_count) {
            if (label != NULL) {
                printf("  [%s] record %d: anim=%d -> cell=%d (INVALID, skipped)\n",
                       label, i, record->animation_index, cell_id);
            }
            continue;
        }

        cell = &ncer->cells[cell_id];
        ApplyType3CoordOffset(record, &frame, cell, coords, &translate_x, &translate_y);

        if (label != NULL) {
            printf("  [%s] record %d: anim=%d -> cell=%d x=%d y=%d rot=%d tx=%d ty=%d oam=%d\n",
                   label, i, record->animation_index, cell_id,
                   record->x, record->y, frame.rotation,
                   translate_x, translate_y,
                   cell->oam_count);
        }

        for (j = 0; j < cell->oam_count; j++) {
            GlobalOam *goam;
            const NcerOam *oam;

            if (global_oam_count >= max_global_oams) {
                continue;
            }

            goam = &global_oams[global_oam_count];
            oam = &cell->oams[j];

            memset(goam, 0, sizeof(*goam));
            Composer_SetParentTransform(goam, parent_transform);
            Ncer_GetOamDrawOrigin(oam, &goam->draw_x, &goam->draw_y);
            goam->record_x = record->x;
            goam->record_y = record->y;
            goam->tile_index = oam->tile_index;
            goam->width = oam->width;
            goam->height = oam->height;
            goam->palette_bank = oam->palette;
            goam->flip_h = oam->flip_h;
            goam->flip_v = oam->flip_v;
            goam->priority = oam->priority;
            goam->original_index = global_oam_count;
            goam->rotation = frame.rotation;
            goam->scale_x = frame.scale_x;
            goam->scale_y = frame.scale_y;
            goam->translate_x = translate_x;
            goam->translate_y = translate_y;


            global_oam_count++;
        }
    }

    return global_oam_count;
}

static void SortGlobalOams(GlobalOam *global_oams, int global_oam_count)
{
    int i;

    for (i = 0; i < global_oam_count - 1; i++) {
        int j;
        for (j = 0; j < global_oam_count - 1 - i; j++) {
            int swap = 0;
            if (global_oams[j].priority < global_oams[j + 1].priority) {
                swap = 1;
            } else if (global_oams[j].priority == global_oams[j + 1].priority) {
                if (global_oams[j].original_index < global_oams[j + 1].original_index) {
                    swap = 1;
                }
            }
            if (swap) {
                GlobalOam tmp = global_oams[j];
                global_oams[j] = global_oams[j + 1];
                global_oams[j + 1] = tmp;
            }
        }
    }
}

static int SampleGlobalOamColorIndex(
    const NcgrImage *image,
    const NclrPalette *palette,
    const GlobalOam *goam,
    int src_x,
    int src_y,
    int tile_stride
)
{
    int tiles_per_row;
    int fetch_x;
    int fetch_y;
    int tile_x;
    int tile_y;
    int local_x;
    int local_y;
    int actual_tile;
    int color_index;
    int final_color_index;

    if (src_x < 0 || src_x >= goam->width ||
        src_y < 0 || src_y >= goam->height) {
        return -1;
    }

    tiles_per_row = tile_stride > 0 ? tile_stride : goam->width / 8;
    if (tiles_per_row <= 0) {
        return -1;
    }

    fetch_x = goam->flip_h ? (goam->width - 1 - src_x) : src_x;
    fetch_y = goam->flip_v ? (goam->height - 1 - src_y) : src_y;
    tile_x = fetch_x / 8;
    tile_y = fetch_y / 8;
    local_x = fetch_x & 7;
    local_y = fetch_y & 7;
    actual_tile = goam->tile_index + tile_y * tiles_per_row + tile_x;

    color_index = Ncgr_GetPixelIndex(image, actual_tile, local_x, local_y);
    if (color_index == 0) {
        return -1;
    }

    if (image->bpp == 4 && palette->color_count > 16) {
        final_color_index = goam->palette_bank * 16 + color_index;
    } else {
        final_color_index = color_index;
    }

    if (final_color_index < 0 || final_color_index >= palette->color_count) {
        final_color_index = color_index;

        if (final_color_index >= palette->color_count) {
            return -1;
        }
    }

    if (final_color_index < 0 || final_color_index > 255) {
        return -1;
    }

    return final_color_index;
}

static void BlitGlobalOamToRgbaCanvas(
    const NcgrImage *image,
    const NclrPalette *palette,
    RgbaColor *canvas,
    int canvas_width,
    int canvas_height,
    int world_origin_x,
    int world_origin_y,
    const GlobalOam *goam,
    int tile_stride
)
{
    double oam_min_x;
    double oam_min_y;
    double oam_max_x;
    double oam_max_y;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int y;

    GlobalOamTransformedBounds(goam, &oam_min_x, &oam_min_y, &oam_max_x, &oam_max_y);

    min_x = (int)floor(oam_min_x) - world_origin_x - 1;
    min_y = (int)floor(oam_min_y) - world_origin_y - 1;
    max_x = (int)ceil(oam_max_x) - world_origin_x + 1;
    max_y = (int)ceil(oam_max_y) - world_origin_y + 1;

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= canvas_width) max_x = canvas_width - 1;
    if (max_y >= canvas_height) max_y = canvas_height - 1;

    for (y = min_y; y <= max_y; y++) {
        int x;
        for (x = min_x; x <= max_x; x++) {
            double local_x;
            double local_y;
            int src_x;
            int src_y;
            int final_color_index;

            GlobalOamInverseTransformPoint(
                goam,
                (double)(world_origin_x + x),
                (double)(world_origin_y + y),
                &local_x,
                &local_y
            );

            src_x = RoundDoubleToInt(local_x - (double)goam->draw_x);
            src_y = RoundDoubleToInt(local_y - (double)goam->draw_y);

            final_color_index = SampleGlobalOamColorIndex(
                image,
                palette,
                goam,
                src_x,
                src_y,
                tile_stride
            );

            if (final_color_index >= 0) {
                canvas[y * canvas_width + x] = palette->colors[final_color_index];
            }
        }
    }
}

static void BlitGlobalOamToIndexedCanvas(
    const NcgrImage *image,
    const NclrPalette *palette,
    u8 *canvas,
    int canvas_width,
    int canvas_height,
    int world_origin_x,
    int world_origin_y,
    const GlobalOam *goam,
    int tile_stride
)
{
    double oam_min_x;
    double oam_min_y;
    double oam_max_x;
    double oam_max_y;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int y;

    GlobalOamTransformedBounds(goam, &oam_min_x, &oam_min_y, &oam_max_x, &oam_max_y);

    min_x = (int)floor(oam_min_x) - world_origin_x - 1;
    min_y = (int)floor(oam_min_y) - world_origin_y - 1;
    max_x = (int)ceil(oam_max_x) - world_origin_x + 1;
    max_y = (int)ceil(oam_max_y) - world_origin_y + 1;

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= canvas_width) max_x = canvas_width - 1;
    if (max_y >= canvas_height) max_y = canvas_height - 1;

    for (y = min_y; y <= max_y; y++) {
        int x;
        for (x = min_x; x <= max_x; x++) {
            double local_x;
            double local_y;
            int src_x;
            int src_y;
            int final_color_index;

            GlobalOamInverseTransformPoint(
                goam,
                (double)(world_origin_x + x),
                (double)(world_origin_y + y),
                &local_x,
                &local_y
            );

            src_x = RoundDoubleToInt(local_x - (double)goam->draw_x);
            src_y = RoundDoubleToInt(local_y - (double)goam->draw_y);

            final_color_index = SampleGlobalOamColorIndex(
                image,
                palette,
                goam,
                src_x,
                src_y,
                tile_stride
            );

            if (final_color_index >= 0) {
                canvas[y * canvas_width + x] = (u8)final_color_index;
            }
        }
    }
}

void Composer_ClearPixels(RgbaColor *pixels, int width, int height)
{
    int i;

    for (i = 0; i < width * height; i++) {
        pixels[i].r = 0;
        pixels[i].g = 0;
        pixels[i].b = 0;
        pixels[i].a = 0;
    }
}

void Composer_ComputeBoundsWithTransform(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    int tick,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords,
    const ComposerTransform *parent_transform
)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int i;

    min_x = 999999;
    min_y = 999999;
    max_x = -999999;
    max_y = -999999;

    for (i = 0; i < map->record_count; i++) {
        const NmcrRecord *record;
        NanrFrame frame;
        int cell_id;
        const NcerCell *cell;
        int translate_x;
        int translate_y;
        int j;

        record = &map->records[i];
        cell_id = Nanr_GetResolvedCellIdAtTick(nanr, record->animation_index, tick, &frame);
        translate_x = frame.translate_x;
        translate_y = frame.translate_y;

        if (cell_id < 0 || cell_id >= ncer->cell_count) {
            continue;
        }

        cell = &ncer->cells[cell_id];
        ApplyType3CoordOffset(record, &frame, cell, coords, &translate_x, &translate_y);

        for (j = 0; j < cell->oam_count; j++) {
            const NcerOam *oam;
            GlobalOam goam;
            double oam_min_x;
            double oam_min_y;
            double oam_max_x;
            double oam_max_y;

            memset(&goam, 0, sizeof(goam));
            Composer_SetParentTransform(&goam, parent_transform);
            oam = &cell->oams[j];
            Ncer_GetOamDrawOrigin(oam, &goam.draw_x, &goam.draw_y);
            goam.record_x = record->x;
            goam.record_y = record->y;
            goam.width = oam->width;
            goam.height = oam->height;
            goam.rotation = frame.rotation;
            goam.scale_x = frame.scale_x;
            goam.scale_y = frame.scale_y;
            goam.translate_x = translate_x;
            goam.translate_y = translate_y;


            GlobalOamTransformedBounds(
                &goam,
                &oam_min_x,
                &oam_min_y,
                &oam_max_x,
                &oam_max_y
            );

            if ((int)floor(oam_min_x) < min_x) min_x = (int)floor(oam_min_x);
            if ((int)floor(oam_min_y) < min_y) min_y = (int)floor(oam_min_y);
            if ((int)ceil(oam_max_x) > max_x) max_x = (int)ceil(oam_max_x);
            if ((int)ceil(oam_max_y) > max_y) max_y = (int)ceil(oam_max_y);
        }
    }

    if (min_x == 999999) {
        min_x = 0;
        min_y = 0;
        max_x = 1;
        max_y = 1;
    }

    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
}

void Composer_ComputeBounds(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    int tick,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords
)
{
    Composer_ComputeBoundsWithTransform(
        ncer,
        nanr,
        map,
        tick,
        out_min_x,
        out_min_y,
        out_max_x,
        out_max_y,
        coords,
        NULL
    );
}

void Composer_ComputeBoundsRange(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    int start_frame,
    int frame_count,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords,
    int delay_cs
)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int i;

    min_x = 999999;
    min_y = 999999;
    max_x = -999999;
    max_y = -999999;

    for (i = 0; i < frame_count; i++) {
        int frame_min_x;
        int frame_min_y;
        int frame_max_x;
        int frame_max_y;
        int tick = ((start_frame + i) * delay_cs * 60) / 100;

        Composer_ComputeBounds(
            ncer,
            nanr,
            map,
            tick,
            &frame_min_x,
            &frame_min_y,
            &frame_max_x,
            &frame_max_y,
            coords
        );

        if (frame_min_x < min_x) min_x = frame_min_x;
        if (frame_min_y < min_y) min_y = frame_min_y;
        if (frame_max_x > max_x) max_x = frame_max_x;
        if (frame_max_y > max_y) max_y = frame_max_y;
    }

    if (min_x == 999999) {
        min_x = 0;
        min_y = 0;
        max_x = 1;
        max_y = 1;
    }

    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
}

int Composer_RenderFrameIndexedWithTransform(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int tick,
    int tile_stride,
    int min_x,
    int min_y,
    int width,
    int height,
    int margin,
    u8 *out_indices,
    const CoordFile *coords,
    const ComposerTransform *parent_transform
)
{
    GlobalOam global_oams[MAX_GLOBAL_OAMS];
    int global_oam_count;
    int i;

    if (out_indices == NULL || width <= 0 || height <= 0) {
        return -1;
    }

    memset(out_indices, 0, (size_t)width * (size_t)height);

    global_oam_count = BuildGlobalOamsForFrame(
        ncer,
        nanr,
        map,
        tick,
        NULL,
        global_oams,
        MAX_GLOBAL_OAMS,
        coords,
        parent_transform
    );

    SortGlobalOams(global_oams, global_oam_count);

    for (i = 0; i < global_oam_count; i++) {
        BlitGlobalOamToIndexedCanvas(
            ncgr,
            palette,
            out_indices,
            width,
            height,
            min_x - margin,
            min_y - margin,
            &global_oams[i],
            tile_stride
        );
    }

    return 0;
}

int Composer_RenderFrameIndexed(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int tick,
    int tile_stride,
    int min_x,
    int min_y,
    int width,
    int height,
    int margin,
    u8 *out_indices,
    const CoordFile *coords
)
{
    return Composer_RenderFrameIndexedWithTransform(
        ncer,
        nanr,
        map,
        ncgr,
        palette,
        tick,
        tile_stride,
        min_x,
        min_y,
        width,
        height,
        margin,
        out_indices,
        coords,
        NULL
    );
}

int Composer_RenderFrameRgba(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int tick,
    int tile_stride,
    const char *label,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height,
    const CoordFile *coords
)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    int margin;
    int i;
    RgbaColor *pixels;
    GlobalOam global_oams[MAX_GLOBAL_OAMS];
    int global_oam_count;

    if (ncer == NULL || nanr == NULL || map == NULL ||
        ncgr == NULL || palette == NULL ||
        out_pixels == NULL || out_width == NULL || out_height == NULL) {
        return -1;
    }

    Composer_ComputeBounds(
        ncer,
        nanr,
        map,
        tick,
        &min_x,
        &min_y,
        &max_x,
        &max_y,
        coords
    );

    margin = COMPOSITE_MARGIN;
    width = (max_x - min_x) + margin * 2;
    height = (max_y - min_y) + margin * 2;

    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        return -1;
    }

    pixels = malloc((size_t)width * (size_t)height * sizeof(RgbaColor));
    if (pixels == NULL) {
        return -1;
    }

    Composer_ClearPixels(pixels, width, height);

    global_oam_count = BuildGlobalOamsForFrame(
        ncer,
        nanr,
        map,
        tick,
        label,
        global_oams,
        MAX_GLOBAL_OAMS,
        coords,
        NULL
    );
    SortGlobalOams(global_oams, global_oam_count);

    for (i = 0; i < global_oam_count; i++) {
        BlitGlobalOamToRgbaCanvas(
            ncgr,
            palette,
            pixels,
            width,
            height,
            min_x - margin,
            min_y - margin,
            &global_oams[i],
            tile_stride
        );
    }

    *out_pixels = pixels;
    *out_width = width;
    *out_height = height;

    return 0;
}

int Composer_CropIndexedFrames(
    const u8 *frames,
    int frame_count,
    int width,
    int height,
    u8 **out_frames,
    int *out_width,
    int *out_height
)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int frame_index;
    u8 *cropped;

    if (frames == NULL || out_frames == NULL ||
        out_width == NULL || out_height == NULL ||
        frame_count <= 0 || width <= 0 || height <= 0) {
        return -1;
    }

    min_x = width;
    min_y = height;
    max_x = -1;
    max_y = -1;

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        const u8 *frame;
        int y;

        frame = frames + ((size_t)frame_index * (size_t)width * (size_t)height);

        for (y = 0; y < height; y++) {
            int x;
            for (x = 0; x < width; x++) {
                if (frame[y * width + x] != 0) {
                    if (x < min_x) min_x = x;
                    if (y < min_y) min_y = y;
                    if (x > max_x) max_x = x;
                    if (y > max_y) max_y = y;
                }
            }
        }
    }

    if (max_x < min_x || max_y < min_y) {
        *out_width = 1;
        *out_height = 1;
        cropped = calloc((size_t)frame_count, 1);
        if (cropped == NULL) {
            return -1;
        }
        *out_frames = cropped;
        return 0;
    }

    *out_width = max_x - min_x + 1;
    *out_height = max_y - min_y + 1;

    cropped = malloc(
        (size_t)frame_count *
        (size_t)(*out_width) *
        (size_t)(*out_height)
    );
    if (cropped == NULL) {
        return -1;
    }

    for (frame_index = 0; frame_index < frame_count; frame_index++) {
        const u8 *src_frame;
        u8 *dst_frame;
        int y;

        src_frame = frames + ((size_t)frame_index * (size_t)width * (size_t)height);
        dst_frame = cropped + ((size_t)frame_index * (size_t)(*out_width) * (size_t)(*out_height));

        for (y = 0; y < *out_height; y++) {
            memcpy(
                dst_frame + ((size_t)y * (size_t)(*out_width)),
                src_frame + ((size_t)(min_y + y) * (size_t)width) + min_x,
                (size_t)(*out_width)
            );
        }
    }

    *out_frames = cropped;
    return 0;
}

void Composer_ComputeUnionBounds(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *map_a,
    int map_a_frame_count,
    const NmcrMap *map_b,
    int map_b_frame_count,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords,
    int delay_cs
)
{
    int min_a_x, min_a_y, max_a_x, max_a_y;
    int min_b_x, min_b_y, max_b_x, max_b_y;
    int min_x, min_y, max_x, max_y;

    min_x = 999999; min_y = 999999;
    max_x = -999999; max_y = -999999;

    if (map_a_frame_count > 0) {
        Composer_ComputeBoundsRange(ncer, nanr, map_a, 0, map_a_frame_count,
                                    &min_a_x, &min_a_y, &max_a_x, &max_a_y, coords, delay_cs);
        if (min_a_x < min_x) min_x = min_a_x;
        if (min_a_y < min_y) min_y = min_a_y;
        if (max_a_x > max_x) max_x = max_a_x;
        if (max_a_y > max_y) max_y = max_a_y;
    }

    if (map_b_frame_count > 0) {
        Composer_ComputeBoundsRange(ncer, nanr, map_b, 0, map_b_frame_count,
                                    &min_b_x, &min_b_y, &max_b_x, &max_b_y, coords, delay_cs);
        if (min_b_x < min_x) min_x = min_b_x;
        if (min_b_y < min_y) min_y = min_b_y;
        if (max_b_x > max_x) max_x = max_b_x;
        if (max_b_y > max_y) max_y = max_b_y;
    }

    if (min_x == 999999) { min_x = 0; min_y = 0; max_x = 1; max_y = 1; }

    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
}

static int Composer_NmarAnimationIsUsable(
    const NmarFile *nmar,
    const NmcrFile *nmcr,
    int animation_index
)
{
    const NmarAnimation *animation;
    int i;

    if (nmar == NULL || nmcr == NULL || nmar->animations == NULL ||
        animation_index < 0 || animation_index >= nmar->animation_count) {
        return 0;
    }

    animation = &nmar->animations[animation_index];
    if (animation->frame_count <= 0 || animation->frames == NULL) {
        return 0;
    }

    for (i = 0; i < animation->frame_count; i++) {
        int map_index = animation->frames[i].map_index;
        if (map_index < 0 || map_index >= nmcr->map_count ||
            nmcr->maps[map_index].record_count <= 0) {
            return 0;
        }
    }

    return 1;
}

static int Composer_NmarFrameCount(
    const NmarFile *nmar,
    const NmcrFile *nmcr,
    const NanrFile *nanr,
    int animation_index,
    int delay_cs
)
{
    const NmarAnimation *animation;
    int total_ticks;
    int ticks_per_frame;
    int frame_count;
    int i;

    total_ticks = Nmar_GetTotalDuration(nmar, animation_index);
    if (total_ticks <= 0) {
        return 0;
    }

    ticks_per_frame = (delay_cs * 60) / 100;
    if (ticks_per_frame <= 0) {
        ticks_per_frame = 1;
    }

    frame_count = (total_ticks + ticks_per_frame - 1) / ticks_per_frame;
    if (frame_count <= 0) {
        frame_count = 1;
    }
    if (frame_count > 4096) {
        frame_count = 4096;
    }

    if (nmar != NULL && nmcr != NULL && nanr != NULL &&
        nmar->animations != NULL &&
        animation_index >= 0 && animation_index < nmar->animation_count) {
        animation = &nmar->animations[animation_index];
        for (i = 0; i < animation->frame_count; i++) {
            int map_index;
            int map_frame_count;

            map_index = animation->frames[i].map_index;
            if (map_index < 0 || map_index >= nmcr->map_count) {
                continue;
            }

            map_frame_count = Nmcr_MaxFrameCount(&nmcr->maps[map_index], nanr);
            if (map_frame_count > frame_count) {
                frame_count = map_frame_count;
            }
        }
    }

    return frame_count;
}

static void Composer_TransformFromNmarFrame(
    const NmarFrame *frame,
    ComposerTransform *out_transform
)
{
    if (out_transform == NULL) {
        return;
    }

    out_transform->rotation = 0;
    out_transform->scale_x = NANR_SCALE_ONE;
    out_transform->scale_y = NANR_SCALE_ONE;
    out_transform->translate_x = 0;
    out_transform->translate_y = 0;

    if (frame == NULL) {
        return;
    }

    out_transform->rotation = frame->rotation;
    out_transform->scale_x = frame->scale_x;
    out_transform->scale_y = frame->scale_y;
    out_transform->translate_x = frame->translate_x;
    out_transform->translate_y = frame->translate_y;
}

static int Composer_ComputeTimelineOrMapBounds(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    const NmcrMap *fallback_map,
    int animation_index,
    int use_timeline,
    int frame_count,
    int delay_cs,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y,
    const CoordFile *coords
)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int i;

    if (frame_count <= 0 || out_min_x == NULL || out_min_y == NULL ||
        out_max_x == NULL || out_max_y == NULL) {
        return -1;
    }

    if (!use_timeline) {
        if (fallback_map == NULL) {
            return -1;
        }
        Composer_ComputeBoundsRange(
            ncer, nanr, fallback_map, 0, frame_count,
            out_min_x, out_min_y, out_max_x, out_max_y,
            coords, delay_cs
        );
        return 0;
    }

    min_x = 999999;
    min_y = 999999;
    max_x = -999999;
    max_y = -999999;

    for (i = 0; i < frame_count; i++) {
        int tick;
        int map_index;
        int frame_min_x;
        int frame_min_y;
        int frame_max_x;
        int frame_max_y;
        NmarFrame nmar_frame;
        ComposerTransform parent_transform;

        tick = (i * delay_cs * 60) / 100;
        map_index = Nmar_GetFrameAtTick(nmar, animation_index, tick, &nmar_frame);
        if (map_index < 0 || map_index >= nmcr->map_count) {
            return -1;
        }

        Composer_TransformFromNmarFrame(&nmar_frame, &parent_transform);
        Composer_ComputeBoundsWithTransform(
            ncer,
            nanr,
            &nmcr->maps[map_index],
            tick,
            &frame_min_x,
            &frame_min_y,
            &frame_max_x,
            &frame_max_y,
            coords,
            &parent_transform
        );

        if (frame_min_x < min_x) min_x = frame_min_x;
        if (frame_min_y < min_y) min_y = frame_min_y;
        if (frame_max_x > max_x) max_x = frame_max_x;
        if (frame_max_y > max_y) max_y = frame_max_y;
    }

    if (min_x == 999999) {
        return -1;
    }

    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
    return 0;
}

static int Composer_RenderTimelineOrMapFrame(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    const NmcrMap *fallback_map,
    int animation_index,
    int use_timeline,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int frame_index,
    int delay_cs,
    int tile_stride,
    int min_x,
    int min_y,
    int width,
    int height,
    int margin,
    u8 *out_indices,
    const CoordFile *coords
)
{
    int tick;
    const NmcrMap *render_map;
    ComposerTransform parent_transform;
    ComposerTransform *parent_transform_ptr;

    tick = (frame_index * delay_cs * 60) / 100;
    render_map = fallback_map;
    parent_transform_ptr = NULL;

    if (use_timeline) {
        NmarFrame nmar_frame;
        int map_index;

        map_index = Nmar_GetFrameAtTick(nmar, animation_index, tick, &nmar_frame);
        if (map_index < 0 || map_index >= nmcr->map_count) {
            return -1;
        }

        render_map = &nmcr->maps[map_index];
        Composer_TransformFromNmarFrame(&nmar_frame, &parent_transform);
        parent_transform_ptr = &parent_transform;
    }

    if (render_map == NULL) {
        return -1;
    }

    return Composer_RenderFrameIndexedWithTransform(
        ncer,
        nanr,
        render_map,
        ncgr,
        palette,
        tick,
        tile_stride,
        min_x,
        min_y,
        width,
        height,
        margin,
        out_indices,
        coords,
        parent_transform_ptr
    );
}

int Composer_RenderComposedAnimation(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrMap *idle_map,
    const NmcrMap *break_map,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int idle_repetitions,
    int tile_stride,
    int margin,
    u8 **out_frames,
    int *out_frame_count,
    int *out_width,
    int *out_height,
    const CoordFile *coords,
    int delay_cs
)
{
    int idle_frame_count;
    int break_frame_count;
    int total_frames;
    int min_x, min_y, max_x, max_y;
    int width, height;
    size_t frame_pixels;
    u8 *frames;
    int i;

    idle_frame_count = Nmcr_MaxFrameCount(idle_map, nanr);
    break_frame_count = Nmcr_MaxFrameCount(break_map, nanr);
    if (idle_frame_count <= 0) idle_frame_count = 1;
    if (break_frame_count <= 0) break_frame_count = 1;
    if (idle_repetitions <= 0) idle_repetitions = 1;

    total_frames = idle_frame_count * idle_repetitions + break_frame_count;
    if (total_frames <= 0 || total_frames > 4096) {
        return -1;
    }

    Composer_ComputeUnionBounds(
        ncer, nanr,
        idle_map, idle_frame_count,
        break_map, break_frame_count,
        &min_x, &min_y, &max_x, &max_y,
        coords,
        delay_cs
    );

    // Force symmetric bounds on the X-axis so the horizontal pivot is perfectly centered
    {
        int abs_x = abs(min_x) > abs(max_x) ? abs(min_x) : abs(max_x);
        min_x = -abs_x;
        max_x = abs_x;
    }

    width = (max_x - min_x) + margin * 2;
    height = (max_y - min_y) + margin * 2;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        return -1;
    }

    frame_pixels = (size_t)width * (size_t)height;
    frames = calloc((size_t)total_frames * frame_pixels, 1);
    if (frames == NULL) {
        return -1;
    }

    for (i = 0; i < total_frames; i++) {
        u8 *frame;
        int tick;
        const NmcrMap *map;

        frame = frames + ((size_t)i * frame_pixels);

        if (i < idle_frame_count * idle_repetitions) {
            map = idle_map;
            tick = (i * delay_cs * 60) / 100;
        } else {
            map = break_map;
            int break_i = i - idle_frame_count * idle_repetitions;
            tick = (break_i * delay_cs * 60) / 100;
        }

        if (Composer_RenderFrameIndexed(
                ncer, nanr, map, ncgr, palette,
                tick,
                tile_stride,
                min_x, min_y, width, height, margin,
                frame,
                coords
            ) != 0) {
            free(frames);
            return -1;
        }
    }

    *out_frames = frames;
    *out_frame_count = total_frames;
    *out_width = width;
    *out_height = height;
    return 0;
}

int Composer_RenderComposedAnimationTimeline(
    const NcerFile *ncer,
    const NanrFile *nanr,
    const NmcrFile *nmcr,
    const NmarFile *nmar,
    const NmcrMap *idle_map,
    const NmcrMap *break_map,
    int idle_animation_index,
    int break_animation_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int idle_repetitions,
    int tile_stride,
    int margin,
    u8 **out_frames,
    int *out_frame_count,
    int *out_width,
    int *out_height,
    const CoordFile *coords,
    int delay_cs
)
{
    int use_idle_timeline;
    int use_break_timeline;
    int idle_frame_count;
    int break_frame_count;
    int total_frames;
    int idle_min_x, idle_min_y, idle_max_x, idle_max_y;
    int break_min_x, break_min_y, break_max_x, break_max_y;
    int min_x, min_y, max_x, max_y;
    int width, height;
    size_t frame_pixels;
    u8 *frames;
    int i;

    if (ncer == NULL || nanr == NULL || nmcr == NULL ||
        ncgr == NULL || palette == NULL || out_frames == NULL ||
        out_frame_count == NULL || out_width == NULL || out_height == NULL ||
        nmcr->map_count <= 0) {
        return -1;
    }

    if (delay_cs <= 0) {
        delay_cs = 5;
    }

    use_idle_timeline = Composer_NmarAnimationIsUsable(nmar, nmcr, idle_animation_index);
    use_break_timeline = Composer_NmarAnimationIsUsable(nmar, nmcr, break_animation_index);

    if (!use_idle_timeline && idle_map == NULL) {
        return -1;
    }
    if (!use_break_timeline && break_map == NULL) {
        return -1;
    }

    idle_frame_count = use_idle_timeline ?
        Composer_NmarFrameCount(nmar, nmcr, nanr, idle_animation_index, delay_cs) :
        Nmcr_MaxFrameCount(idle_map, nanr);
    break_frame_count = use_break_timeline ?
        Composer_NmarFrameCount(nmar, nmcr, nanr, break_animation_index, delay_cs) :
        Nmcr_MaxFrameCount(break_map, nanr);

    if (idle_frame_count <= 0) idle_frame_count = 1;
    if (break_frame_count <= 0) break_frame_count = 1;
    if (idle_repetitions <= 0) {
        idle_repetitions = use_idle_timeline ?
            Composer_ComputeNmarIdleRepetitions(nmar, idle_animation_index) :
            Composer_ComputeIdleRepetitions(nanr, idle_map);
    }
    if (idle_repetitions <= 0) idle_repetitions = 1;

    total_frames = idle_frame_count * idle_repetitions + break_frame_count;
    if (total_frames <= 0 || total_frames > 4096) {
        return -1;
    }

    if (Composer_ComputeTimelineOrMapBounds(
            ncer, nanr, nmcr, nmar, idle_map, idle_animation_index,
            use_idle_timeline, idle_frame_count, delay_cs,
            &idle_min_x, &idle_min_y, &idle_max_x, &idle_max_y,
            coords
        ) != 0 ||
        Composer_ComputeTimelineOrMapBounds(
            ncer, nanr, nmcr, nmar, break_map, break_animation_index,
            use_break_timeline, break_frame_count, delay_cs,
            &break_min_x, &break_min_y, &break_max_x, &break_max_y,
            coords
        ) != 0) {
        return -1;
    }

    min_x = idle_min_x < break_min_x ? idle_min_x : break_min_x;
    min_y = idle_min_y < break_min_y ? idle_min_y : break_min_y;
    max_x = idle_max_x > break_max_x ? idle_max_x : break_max_x;
    max_y = idle_max_y > break_max_y ? idle_max_y : break_max_y;

    {
        int abs_x = abs(min_x) > abs(max_x) ? abs(min_x) : abs(max_x);
        min_x = -abs_x;
        max_x = abs_x;
    }

    width = (max_x - min_x) + margin * 2;
    height = (max_y - min_y) + margin * 2;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        return -1;
    }

    frame_pixels = (size_t)width * (size_t)height;
    frames = calloc((size_t)total_frames * frame_pixels, 1);
    if (frames == NULL) {
        return -1;
    }

    for (i = 0; i < total_frames; i++) {
        u8 *frame;
        int local_frame;
        int use_timeline;
        int animation_index;
        const NmcrMap *map;

        frame = frames + ((size_t)i * frame_pixels);

        if (i < idle_frame_count * idle_repetitions) {
            local_frame = i;
            use_timeline = use_idle_timeline;
            animation_index = idle_animation_index;
            map = idle_map;
        } else {
            local_frame = i - idle_frame_count * idle_repetitions;
            use_timeline = use_break_timeline;
            animation_index = break_animation_index;
            map = break_map;
        }

        if (Composer_RenderTimelineOrMapFrame(
                ncer, nanr, nmcr, nmar, map, animation_index, use_timeline,
                ncgr, palette, local_frame, delay_cs, tile_stride,
                min_x, min_y, width, height, margin, frame, coords
            ) != 0) {
            free(frames);
            return -1;
        }
    }

    *out_frames = frames;
    *out_frame_count = total_frames;
    *out_width = width;
    *out_height = height;
    return 0;
}

int Composer_ComputeIdleRepetitions(
    const NanrFile *nanr,
    const NmcrMap *idle_map
)
{
    int idle_duration_cs;
    int i;
    int reps;

    idle_duration_cs = 0;

    for (i = 0; i < idle_map->record_count; i++) {
        int anim_idx;
        int j;
        int anim_dur;

        anim_idx = idle_map->records[i].animation_index;
        if (anim_idx < 0 || anim_idx >= nanr->animation_count) continue;

        anim_dur = 0;
        for (j = 0; j < nanr->animations[anim_idx].frame_count; j++) {
            anim_dur += nanr->animations[anim_idx].frames[j].duration;
        }

        if (anim_dur > idle_duration_cs) {
            idle_duration_cs = anim_dur;
        }
    }

    if (idle_duration_cs <= 0) {
        return 3;
    }

    reps = (300 + idle_duration_cs / 2) / idle_duration_cs;
    if (reps < 1) reps = 1;
    if (reps > 10) reps = 10;

    return reps;
}

int Composer_ComputeNmarIdleRepetitions(
    const NmarFile *nmar,
    int animation_index
)
{
    int idle_duration_ticks;
    int reps;

    idle_duration_ticks = Nmar_GetTotalDuration(nmar, animation_index);
    if (idle_duration_ticks <= 0) {
        return 3;
    }

    reps = (300 + idle_duration_ticks / 2) / idle_duration_ticks;
    if (reps < 1) reps = 1;
    if (reps > 10) reps = 10;

    return reps;
}
