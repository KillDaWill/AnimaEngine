#include "ppm.h"

static void WritePixel(FILE *f, const RgbaColor *px)
{
    u8 rgb[3];

    if (px->a == 0) {
        rgb[0] = 255;
        rgb[1] = 0;
        rgb[2] = 255;
    } else {
        rgb[0] = px->r;
        rgb[1] = px->g;
        rgb[2] = px->b;
    }

    fwrite(rgb, 1, 3, f);
}

int Ppm_WriteRgbImage(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height
)
{
    return Ppm_WriteRgbImageScaled(path, pixels, width, height, 1);
}

int Ppm_WriteRgbImageScaled(
    const char *path,
    const RgbaColor *pixels,
    int width,
    int height,
    int scale
)
{
    FILE *f;
    int x;
    int y;
    int sx;
    int sy;
    int out_width;
    int out_height;

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

    fprintf(f, "P6\n%d %d\n255\n", out_width, out_height);

    for (y = 0; y < height; y++) {
        for (sy = 0; sy < scale; sy++) {
            for (x = 0; x < width; x++) {
                const RgbaColor *px;

                px = &pixels[y * width + x];

                for (sx = 0; sx < scale; sx++) {
                    WritePixel(f, px);
                }
            }
        }
    }

    fclose(f);
    return 0;
}