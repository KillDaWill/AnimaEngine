#include "ncer.h"
#include "file_util.h"
#include "nitro_util.h"

static int DecodeSignedX(int raw)
{
    raw &= 0x1FF;

    if (raw >= 256) {
        raw -= 512;
    }

    return raw;
}

static int DecodeSignedY(int raw)
{
    raw &= 0xFF;

    if (raw >= 128) {
        raw -= 256;
    }

    return raw;
}

static int DecodeObjSize(int shape, int size, int *out_w, int *out_h)
{
    static const int widths[3][4] = {
        { 8, 16, 32, 64 },
        { 16, 32, 32, 64 },
        { 8, 8, 16, 32 }
    };

    static const int heights[3][4] = {
        { 8, 16, 32, 64 },
        { 8, 8, 16, 32 },
        { 16, 32, 32, 64 }
    };

    if (shape < 0 || shape > 2 || size < 0 || size > 3) {
        return -1;
    }

    *out_w = widths[shape][size];
    *out_h = heights[shape][size];

    return 0;
}

static int DecodeOam(const u8 *data, size_t size, size_t offset, NcerOam *out_oam)
{
    u16 attr0;
    u16 attr1;
    u16 attr2;
    int shape;
    int obj_size;
    int width;
    int height;

    if (offset + 6 > size) {
        return -1;
    }

    attr0 = ReadU16LE(data + offset + 0);
    attr1 = ReadU16LE(data + offset + 2);
    attr2 = ReadU16LE(data + offset + 4);

    shape = (attr0 >> 14) & 0x03;
    obj_size = (attr1 >> 14) & 0x03;

    if (DecodeObjSize(shape, obj_size, &width, &height) != 0) {
        return -1;
    }

    out_oam->attr0 = attr0;
    out_oam->attr1 = attr1;
    out_oam->attr2 = attr2;
    out_oam->x = DecodeSignedX(attr1 & 0x01FF);
    out_oam->y = DecodeSignedY(attr0 & 0x00FF);
    out_oam->shape = shape;
    out_oam->size = obj_size;
    out_oam->width = width;
    out_oam->height = height;
    out_oam->tile_index = attr2 & 0x03FF;
    out_oam->priority = (attr2 >> 10) & 0x03;
    out_oam->palette = (attr2 >> 12) & 0x0F;
    out_oam->affine = (attr0 >> 8) & 1;
    out_oam->double_size = out_oam->affine ? ((attr0 >> 9) & 1) : 0;
    out_oam->obj_mode = (attr0 >> 10) & 0x03;
    out_oam->affine_index = (attr1 >> 9) & 0x1F;
    out_oam->flip_h = out_oam->affine ? 0 : ((attr1 >> 12) & 1);
    out_oam->flip_v = out_oam->affine ? 0 : ((attr1 >> 13) & 1);

    return 0;
}

void Ncer_GetOamDrawOrigin(
    const NcerOam *oam,
    int *out_x,
    int *out_y
)
{
    int x;
    int y;

    if (oam == NULL || out_x == NULL || out_y == NULL) {
        return;
    }

    x = oam->x;
    y = oam->y;

    if (oam->affine && oam->double_size) {
        x += oam->width / 2;
        y += oam->height / 2;
    }

    *out_x = x;
    *out_y = y;
}

static int SortOamsByPriority(const NcerOam *oams, int count, int *indices)
{
    int i;
    int j;

    if (count > 128) return -1;

    for (i = 0; i < count; i++) {
        indices[i] = i;
    }

    for (i = 0; i < count - 1; i++) {
        for (j = 0; j < count - 1 - i; j++) {
            if (oams[indices[j]].priority < oams[indices[j + 1]].priority) {
                int tmp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = tmp;
            } else if (oams[indices[j]].priority == oams[indices[j + 1]].priority &&
                       indices[j] < indices[j + 1]) {
                int tmp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = tmp;
            }
        }
    }

    return 0;
}

int Ncer_Parse(
    const u8 *data,
    size_t size,
    NcerFile *out_ncer
)
{
    size_t kbec_offset;
    u32 kbec_size;
    int cell_count;
    u32 cell_table_offset;
    size_t cell_table;
    size_t oam_base;
    NcerCell *cells;
    int i;

    if (data == NULL || out_ncer == NULL) {
        return -1;
    }

    memset(out_ncer, 0, sizeof(*out_ncer));

    if (size < 0x20 || !Nitro_HasMagic(data, size, "RECN")) {
        return -1;
    }

    if (Nitro_FindSection(data, size, "KBEC", &kbec_offset, &kbec_size) != 0) {
        return -1;
    }

    if (kbec_offset + kbec_size > size || kbec_size < 0x20) {
        return -1;
    }

    cell_count = ReadU16LE(data + kbec_offset + 0x08);
    cell_table_offset = ReadU32LE(data + kbec_offset + 0x0C);

    if (cell_count <= 0 || cell_count > 1024) {
        return -1;
    }

    if (cell_table_offset >= kbec_size) {
        return -1;
    }

    cell_table = kbec_offset + 8 + cell_table_offset;
    oam_base = cell_table + ((size_t)cell_count * 8);

    if (cell_table + ((size_t)cell_count * 8) > kbec_offset + kbec_size ||
        oam_base > kbec_offset + kbec_size) {
        return -1;
    }

    cells = calloc((size_t)cell_count, sizeof(NcerCell));
    if (cells == NULL) {
        return -1;
    }

    for (i = 0; i < cell_count; i++) {
        size_t entry_offset;
        size_t oam_offset;
        int j;

        entry_offset = cell_table + ((size_t)i * 8);

        cells[i].oam_count = ReadU16LE(data + entry_offset + 0);
        cells[i].cell_attr = ReadU16LE(data + entry_offset + 2);
        cells[i].raw_oam_offset = ReadU32LE(data + entry_offset + 4);

        if (cells[i].oam_count > 128) {
            Ncer_Free(&(NcerFile){ cell_count, cells });
            return -1;
        }

        oam_offset = oam_base + cells[i].raw_oam_offset;

        if (oam_offset + ((size_t)cells[i].oam_count * 6) > kbec_offset + kbec_size) {
            Ncer_Free(&(NcerFile){ cell_count, cells });
            return -1;
        }

        cells[i].oams = calloc((size_t)cells[i].oam_count, sizeof(NcerOam));
        if (cells[i].oams == NULL) {
            Ncer_Free(&(NcerFile){ cell_count, cells });
            return -1;
        }

        for (j = 0; j < cells[i].oam_count; j++) {
            if (DecodeOam(
                    data,
                    size,
                    oam_offset + ((size_t)j * 6),
                    &cells[i].oams[j]
                ) != 0) {
                Ncer_Free(&(NcerFile){ cell_count, cells });
                return -1;
            }
        }
    }

    out_ncer->cell_count = cell_count;
    out_ncer->cells = cells;

    return 0;
}

void Ncer_Free(NcerFile *ncer)
{
    int i;

    if (ncer == NULL) {
        return;
    }

    if (ncer->cells != NULL) {
        for (i = 0; i < ncer->cell_count; i++) {
            free(ncer->cells[i].oams);
        }

        free(ncer->cells);
    }

    ncer->cell_count = 0;
    ncer->cells = NULL;
}

void Ncer_PrintInfo(const NcerFile *ncer)
{
    int i;

    if (ncer == NULL) {
        return;
    }

    printf("NCER file:\n");
    printf("  cell count: %d\n", ncer->cell_count);

    for (i = 0; i < ncer->cell_count && i < 16; i++) {
        printf("  cell %03d: %d OAM entries attr=%d raw_oam_offset=0x%X\n",
               i,
               ncer->cells[i].oam_count,
               ncer->cells[i].cell_attr,
               ncer->cells[i].raw_oam_offset);
    }

    if (ncer->cell_count > 16) {
        printf("  ...\n");
    }

    printf("\n");
}

int Ncer_RenderCellToImage(
    const NcerFile *ncer,
    int cell_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
)
{
    return Ncer_RenderCellToImageWithTileStride(
        ncer,
        cell_index,
        ncgr,
        palette,
        32,
        out_pixels,
        out_width,
        out_height
    );
}

int Ncer_RenderCellToImageWithTileStride(
    const NcerFile *ncer,
    int cell_index,
    const NcgrImage *ncgr,
    const NclrPalette *palette,
    int tile_stride,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
)
{
    const NcerCell *cell;
    int i;
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int width;
    int height;
    int margin;
    RgbaColor *pixels;
    int sorted[128];

    if (ncer == NULL || ncgr == NULL || palette == NULL ||
        out_pixels == NULL || out_width == NULL || out_height == NULL) {
        return -1;
    }

    if (cell_index < 0 || cell_index >= ncer->cell_count) {
        return -1;
    }

    cell = &ncer->cells[cell_index];

    if (cell->oam_count <= 0) {
        return -1;
    }

    min_x = 999999;
    min_y = 999999;
    max_x = -999999;
    max_y = -999999;

    for (i = 0; i < cell->oam_count; i++) {
        const NcerOam *oam;
        int draw_x;
        int draw_y;

        oam = &cell->oams[i];
        Ncer_GetOamDrawOrigin(oam, &draw_x, &draw_y);

        if (draw_x < min_x) {
            min_x = draw_x;
        }
        if (draw_y < min_y) {
            min_y = draw_y;
        }
        if (draw_x + oam->width > max_x) {
            max_x = draw_x + oam->width;
        }
        if (draw_y + oam->height > max_y) {
            max_y = draw_y + oam->height;
        }
    }

    margin = 8;
    width = (max_x - min_x) + margin * 2;
    height = (max_y - min_y) + margin * 2;

    if (width <= 0 || height <= 0 || width > 512 || height > 512) {
        return -1;
    }

    pixels = malloc((size_t)width * (size_t)height * sizeof(RgbaColor));
    if (pixels == NULL) {
        return -1;
    }

    for (i = 0; i < width * height; i++) {
        pixels[i].r = 0;
        pixels[i].g = 0;
        pixels[i].b = 0;
        pixels[i].a = 0;
    }

    SortOamsByPriority(cell->oams, cell->oam_count, sorted);

    for (i = 0; i < cell->oam_count; i++) {
        const NcerOam *oam;
        int dst_x;
        int dst_y;
        int draw_x;
        int draw_y;
        int idx;

        idx = sorted[i];
        oam = &cell->oams[idx];
        Ncer_GetOamDrawOrigin(oam, &draw_x, &draw_y);
        dst_x = (draw_x - min_x) + margin;
        dst_y = (draw_y - min_y) + margin;

        Ncgr_BlitObjToCanvasWithTileStride(
            ncgr,
            palette,
            pixels,
            width,
            height,
            dst_x,
            dst_y,
            oam->tile_index,
            oam->width,
            oam->height,
            oam->palette,
            oam->flip_h,
            oam->flip_v,
            tile_stride
        );
    }

    *out_pixels = pixels;
    *out_width = width;
    *out_height = height;

    return 0;
}

void Ncer_CellBounds(
    const NcerCell *cell,
    int *out_min_x,
    int *out_min_y,
    int *out_max_x,
    int *out_max_y
)
{
    int i;
    int min_x;
    int min_y;
    int max_x;
    int max_y;

    min_x = 999999;
    min_y = 999999;
    max_x = -999999;
    max_y = -999999;

    for (i = 0; i < cell->oam_count; i++) {
        const NcerOam *oam;
        int draw_x;
        int draw_y;

        oam = &cell->oams[i];
        Ncer_GetOamDrawOrigin(oam, &draw_x, &draw_y);

        if (draw_x < min_x) {
            min_x = draw_x;
        }
        if (draw_y < min_y) {
            min_y = draw_y;
        }
        if (draw_x + oam->width > max_x) {
            max_x = draw_x + oam->width;
        }
        if (draw_y + oam->height > max_y) {
            max_y = draw_y + oam->height;
        }
    }

    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
}
