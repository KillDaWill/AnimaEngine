#include "ncgr.h"
#include "file_util.h"
#include "nitro_util.h"

int Ncgr_Parse(
    const u8 *data,
    size_t size,
    NcgrImage *out_image
)
{
    size_t pos;
    u32 section_size;
    u16 width_tiles;
    u16 height_tiles;
    u32 bit_depth;
    u32 tile_data_size;
    size_t tile_data_offset;

    if (data == NULL || out_image == NULL) {
        return -1;
    }

    if (size < 0x30) {
        return -1;
    }

    if (!Nitro_HasMagic(data, size, "RGCN")) {
        return -1;
    }

    memset(out_image, 0, sizeof(*out_image));

    pos = 0x10;

    if (pos + 0x20 > size) {
        return -1;
    }

    if (!Nitro_HasMagic(data + pos, size - pos, "RAHC")) {
        return -1;
    }

    section_size = ReadU32LE(data + pos + 0x04);
    width_tiles = ReadU16LE(data + pos + 0x08);
    height_tiles = ReadU16LE(data + pos + 0x0A);
    bit_depth = ReadU32LE(data + pos + 0x0C);
    tile_data_size = ReadU32LE(data + pos + 0x18);
    tile_data_offset = pos + 0x20;

    if (section_size < 0x20) {
        return -1;
    }

    if (tile_data_size == 0 || tile_data_offset + tile_data_size > size) {
        tile_data_size = (u32)(size - tile_data_offset);
    }

    out_image->tile_data = data + tile_data_offset;
    out_image->tile_data_size = tile_data_size;
    out_image->width_tiles = width_tiles;
    out_image->height_tiles = height_tiles;

    if (bit_depth == 3) {
        out_image->bpp = 4;
        out_image->tile_count = (int)(tile_data_size / 32);
    } else if (bit_depth == 4) {
        out_image->bpp = 8;
        out_image->tile_count = (int)(tile_data_size / 64);
    } else {
        return -1;
    }

    u32 character_type = ReadU32LE(data + pos + 0x14);
    if (character_type == 1 && out_image->bpp == 4) {
        int width_tiles_lin = 32;
        int width_pixels = width_tiles_lin * 8;

        if (out_image->width_tiles != 32) {
            printf("NCGR de-interleave: header width_tiles=%d, using stride=%d\n",
                   (int)out_image->width_tiles, width_tiles_lin);
        }

        u8 *tiled_data = calloc((size_t)out_image->tile_count, 32);
        if (tiled_data != NULL) {
            size_t total_pixels = out_image->tile_data_size * 2;
            u8 *flat_pixels = malloc(total_pixels);
            if (flat_pixels != NULL) {
                for (size_t b = 0; b < out_image->tile_data_size; b++) {
                    u8 val = out_image->tile_data[b];
                    flat_pixels[b * 2] = val & 0x0F;
                    flat_pixels[b * 2 + 1] = val >> 4;
                }

                for (int tile_idx = 0; tile_idx < out_image->tile_count; tile_idx++) {
                    int tile_x = tile_idx % width_tiles_lin;
                    int tile_y = tile_idx / width_tiles_lin;
                    for (int py = 0; py < 8; py++) {
                        for (int px = 0; px < 8; px++) {
                            int lx = tile_x * 8 + px;
                            int ly = tile_y * 8 + py;
                            size_t flat_idx = (size_t)ly * width_pixels + (size_t)lx;
                            if (flat_idx >= total_pixels) break;
                            u8 pixel_val = flat_pixels[flat_idx];

                            int tiled_pixel_idx = tile_idx * 64 + py * 8 + px;
                            int byte_idx = tiled_pixel_idx / 2;
                            int nibble = tiled_pixel_idx % 2;
                            if (nibble == 0) {
                                tiled_data[byte_idx] |= (pixel_val & 0x0F);
                            } else {
                                tiled_data[byte_idx] |= ((pixel_val & 0x0F) << 4);
                            }
                        }
                    }
                }
                free(flat_pixels);
                out_image->allocated_data = tiled_data;
                out_image->tile_data = tiled_data;
                out_image->width_tiles = 32;
            } else {
                free(tiled_data);
            }
        }
    }

    return 0;
}

void Ncgr_Free(NcgrImage *image)
{
    if (image != NULL && image->allocated_data != NULL) {
        free(image->allocated_data);
        image->allocated_data = NULL;
    }
}

void Ncgr_PrintInfo(const NcgrImage *image)
{
    printf("NCGR image:\n");
    printf("  bpp:            %d\n", image->bpp);
    printf("  dimensions:     %dx%d tiles\n", image->width_tiles, image->height_tiles);
    printf("  tile data size: %zu\n", image->tile_data_size);
    printf("  tile count:     %d\n", image->tile_count);
    printf("\n");
}

static int GetPixelIndex4bpp(const u8 *tile, int x, int y)
{
    int pixel_index;
    u8 value;

    pixel_index = y * 8 + x;
    value = tile[pixel_index / 2];

    if ((pixel_index & 1) == 0) {
        return value & 0x0F;
    }

    return value >> 4;
}

static int GetPixelIndex8bpp(const u8 *tile, int x, int y)
{
    return tile[y * 8 + x];
}

int Ncgr_GetPixelIndex(
    const NcgrImage *image,
    int tile_index,
    int x,
    int y
)
{
    const u8 *tile;

    if (image == NULL) {
        return 0;
    }

    if (tile_index < 0 || tile_index >= image->tile_count) {
        return 0;
    }

    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return 0;
    }

    if (image->bpp == 4) {
        tile = image->tile_data + tile_index * 32;
        return GetPixelIndex4bpp(tile, x, y);
    }

    if (image->bpp == 8) {
        tile = image->tile_data + tile_index * 64;
        return GetPixelIndex8bpp(tile, x, y);
    }

    return 0;
}

void Ncgr_BlitObjToCanvas(
    const NcgrImage *image,
    const NclrPalette *palette,
    RgbaColor *canvas,
    int canvas_width,
    int canvas_height,
    int dst_x,
    int dst_y,
    int tile_index,
    int obj_width,
    int obj_height,
    int palette_index,
    int flip_h,
    int flip_v
)
{
    Ncgr_BlitObjToCanvasWithTileStride(
        image,
        palette,
        canvas,
        canvas_width,
        canvas_height,
        dst_x,
        dst_y,
        tile_index,
        obj_width,
        obj_height,
        palette_index,
        flip_h,
        flip_v,
        0
    );
}

void Ncgr_BlitObjToCanvasWithTileStride(
    const NcgrImage *image,
    const NclrPalette *palette,
    RgbaColor *canvas,
    int canvas_width,
    int canvas_height,
    int dst_x,
    int dst_y,
    int tile_index,
    int obj_width,
    int obj_height,
    int palette_index,
    int flip_h,
    int flip_v,
    int tile_stride
)
{
    int x;
    int y;
    int tiles_per_row;

    if (image == NULL || palette == NULL || canvas == NULL) {
        return;
    }

    if (obj_width <= 0 || obj_height <= 0) {
        return;
    }

    tiles_per_row = tile_stride > 0 ? tile_stride : obj_width / 8;

    if (tiles_per_row <= 0) {
        return;
    }

    for (y = 0; y < obj_height; y++) {
        for (x = 0; x < obj_width; x++) {
            int src_x;
            int src_y;
            int tile_x;
            int tile_y;
            int local_x;
            int local_y;
            int actual_tile;
            int color_index;
            int final_color_index;
            int out_x;
            int out_y;

            src_x = flip_h ? (obj_width - 1 - x) : x;
            src_y = flip_v ? (obj_height - 1 - y) : y;

            tile_x = src_x / 8;
            tile_y = src_y / 8;

            local_x = src_x & 7;
            local_y = src_y & 7;

            actual_tile = tile_index + tile_y * tiles_per_row + tile_x;

            color_index = Ncgr_GetPixelIndex(image, actual_tile, local_x, local_y);

            if (color_index == 0) {
                continue;
            }

            if (image->bpp == 4 && palette->color_count > 16) {
                final_color_index = palette_index * 16 + color_index;
            } else {
                final_color_index = color_index;
            }

            if (final_color_index < 0 || final_color_index >= palette->color_count) {
                final_color_index = color_index;

                if (final_color_index >= palette->color_count) {
                    continue;
                }
            }

            out_x = dst_x + x;
            out_y = dst_y + y;

            if (out_x < 0 || out_x >= canvas_width ||
                out_y < 0 || out_y >= canvas_height) {
                continue;
            }

            canvas[out_y * canvas_width + out_x] = palette->colors[final_color_index];
        }
    }
}

int Ncgr_RenderTilesToImage(
    const NcgrImage *image,
    const NclrPalette *palette,
    int tiles_per_row,
    RgbaColor **out_pixels,
    int *out_width,
    int *out_height
)
{
    int tile_rows;
    int width;
    int height;
    int tile_index;
    RgbaColor *pixels;

    if (image == NULL || palette == NULL ||
        out_pixels == NULL || out_width == NULL || out_height == NULL) {
        return -1;
    }

    if (tiles_per_row <= 0) {
        tiles_per_row = image->width_tiles > 0 ? image->width_tiles : 16;
    }

    if (image->tile_count <= 0) {
        return -1;
    }

    tile_rows = (image->tile_count + tiles_per_row - 1) / tiles_per_row;

    width = tiles_per_row * 8;
    height = tile_rows * 8;

    pixels = malloc((size_t)width * (size_t)height * sizeof(RgbaColor));
    if (pixels == NULL) {
        return -1;
    }

    memset(pixels, 0, (size_t)width * (size_t)height * sizeof(RgbaColor));

    for (tile_index = 0; tile_index < image->tile_count; tile_index++) {
        int tile_x;
        int tile_y;
        int px;
        int py;

        tile_x = tile_index % tiles_per_row;
        tile_y = tile_index / tiles_per_row;

        for (py = 0; py < 8; py++) {
            for (px = 0; px < 8; px++) {
                int color_index;
                int dst_x;
                int dst_y;

                color_index = Ncgr_GetPixelIndex(image, tile_index, px, py);

                if (color_index >= palette->color_count) {
                    color_index = 0;
                }

                dst_x = tile_x * 8 + px;
                dst_y = tile_y * 8 + py;

                pixels[dst_y * width + dst_x] = palette->colors[color_index];
            }
        }
    }

    *out_pixels = pixels;
    *out_width = width;
    *out_height = height;

    return 0;
}
