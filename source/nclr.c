#include "nclr.h"
#include "file_util.h"
#include "nitro_util.h"

static RgbaColor Bgr555ToRgba(u16 value, int index)
{
    RgbaColor color;
    u8 r5;
    u8 g5;
    u8 b5;

    r5 = value & 0x1F;
    g5 = (value >> 5) & 0x1F;
    b5 = (value >> 10) & 0x1F;

    color.r = (r5 << 3) | (r5 >> 2);
    color.g = (g5 << 3) | (g5 >> 2);
    color.b = (b5 << 3) | (b5 >> 2);

    /*
     * In many indexed sprite formats, palette index 0 is transparent.
     * For preview, mark it as alpha 0.
     */
    color.a = (index == 0) ? 0 : 255;

    return color;
}

int Nclr_Parse(
    const u8 *data,
    size_t size,
    NclrPalette *out_palette
)
{
    size_t pos;
    size_t palette_data_offset;
    u32 palette_data_size;
    int color_count;
    int i;

    if (data == NULL || out_palette == NULL) {
        return -1;
    }

    if (size < 0x28) {
        return -1;
    }

    if (!Nitro_HasMagic(data, size, "RLCN")) {
        return -1;
    }

    memset(out_palette, 0, sizeof(*out_palette));

    /*
     * Generic Nitro header is 0x10 bytes.
     * The first section should be TTLP.
     */
    pos = 0x10;

    if (pos + 0x18 > size) {
        return -1;
    }

    if (!Nitro_HasMagic(data + pos, size - pos, "TTLP")) {
        return -1;
    }

    /*
     * TTLP layout used here:
     * +0x10: palette data size
     * +0x14: colors per palette / offset-like value in some docs
     *
     * In the Chimchar NCLR files, total size is 72 bytes:
     * 0x10 generic header + 0x18 TTLP header + 0x20 palette data.
     *
     * So palette data starts at 0x28.
     */
    palette_data_size = ReadU32LE(data + pos + 0x10);
    palette_data_offset = pos + 0x18;

    if (palette_data_size == 0 || palette_data_offset + palette_data_size > size) {
        /*
         * Fallback for small simple NCLR files.
         */
        palette_data_offset = 0x28;

        if (palette_data_offset > size) {
            return -1;
        }

        palette_data_size = (u32)(size - palette_data_offset);
    }

    color_count = (int)(palette_data_size / 2);

    if (color_count > NCLR_MAX_COLORS) {
        color_count = NCLR_MAX_COLORS;
    }

    out_palette->color_count = color_count;

    for (i = 0; i < color_count; i++) {
        u16 raw_color;

        raw_color = ReadU16LE(data + palette_data_offset + (i * 2));
        out_palette->raw_colors[i] = raw_color;
        out_palette->colors[i] = Bgr555ToRgba(raw_color, i);
    }

    return 0;
}

void Nclr_PrintInfo(const NclrPalette *palette)
{
    int i;

    printf("NCLR palette:\n");
    printf("  color count: %d\n", palette->color_count);

    for (i = 0; i < palette->color_count && i < 16; i++) {
        printf(
            "  %02d: rgba(%3u, %3u, %3u, %3u)\n",
            i,
            palette->colors[i].r,
            palette->colors[i].g,
            palette->colors[i].b,
            palette->colors[i].a
        );
    }

    printf("\n");
}
