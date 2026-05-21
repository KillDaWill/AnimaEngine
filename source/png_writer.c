#include "png_writer.h"

/*
 * This workspace has a stale /usr/local/include/setjmp.h ahead of glibc's
 * header in GCC's default search path. Include glibc's header explicitly and
 * mark the stale guard so libpng does not pick the wrong one indirectly.
 */
#include "/usr/include/setjmp.h"
#ifndef _SETJMP_H_
#define _SETJMP_H_
#endif

#include <png.h>

int Png_WriteRgbaImage(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height
)
{
    return Png_WriteRgbaImageScaled(path, pixels, width, height, 1);
}

int Png_WriteRgbaImageScaled(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height,
    int scale
)
{
    FILE *f;
    png_structp png;
    png_infop info;
    png_bytep row;
    int out_width;
    int out_height;
    int y;

    if (path == NULL || pixels == NULL || width <= 0 || height <= 0) {
        return -1;
    }

    if (scale <= 0) {
        scale = 1;
    }

    out_width = width * scale;
    out_height = height * scale;

    f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        return -1;
    }

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        fclose(f);
        return -1;
    }

    info = png_create_info_struct(png);
    if (info == NULL) {
        png_destroy_write_struct(&png, NULL);
        fclose(f);
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return -1;
    }

    png_init_io(png, f);
    png_set_IHDR(
        png,
        info,
        (png_uint_32)out_width,
        (png_uint_32)out_height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    row = malloc((size_t)out_width * 4);
    if (row == NULL) {
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return -1;
    }

    for (y = 0; y < height; y++) {
        int sy;

        for (sy = 0; sy < scale; sy++) {
            int x;

            for (x = 0; x < width; x++) {
                const RgbaColor *px;
                int sx;

                px = &pixels[y * width + x];

                for (sx = 0; sx < scale; sx++) {
                    int out_x;

                    out_x = (x * scale) + sx;
                    row[(out_x * 4) + 0] = px->r;
                    row[(out_x * 4) + 1] = px->g;
                    row[(out_x * 4) + 2] = px->b;
                    row[(out_x * 4) + 3] = px->a;
                }
            }

            png_write_row(png, row);
        }
    }

    free(row);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(f);

    return 0;
}
