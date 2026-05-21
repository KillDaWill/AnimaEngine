#include "gif_writer.h"

#define GIF_COMPAT_MIN_FAST_DELAY_CS 5

typedef struct GifBitWriter {
    FILE *f;
    u8 block[255];
    int block_len;
    unsigned int bit_buffer;
    int bit_count;
} GifBitWriter;

static void WriteU16(FILE *f, int value)
{
    fputc(value & 0xFF, f);
    fputc((value >> 8) & 0xFF, f);
}

static void ResolveCompatibleTiming(
    int requested_delay_cs,
    int *out_frame_step,
    int *out_delay_cs
)
{
    int frame_step;
    int delay_cs;

    frame_step = 1;
    delay_cs = requested_delay_cs < 0 ? 0 : requested_delay_cs;

    if (requested_delay_cs > 0 &&
        requested_delay_cs < GIF_COMPAT_MIN_FAST_DELAY_CS) {
        frame_step = (GIF_COMPAT_MIN_FAST_DELAY_CS + requested_delay_cs - 1) /
                     requested_delay_cs;
        if (frame_step < 1) frame_step = 1;
        delay_cs = requested_delay_cs * frame_step;
    }

    if (out_frame_step != NULL) *out_frame_step = frame_step;
    if (out_delay_cs != NULL) *out_delay_cs = delay_cs;
}

static int ColorTableSize(int color_count)
{
    int size;

    size = 2;
    while (size < color_count && size < 256) {
        size <<= 1;
    }

    return size;
}

static int BitsForColorTable(int table_size)
{
    int bits;
    int value;

    bits = 0;
    value = 1;
    while (value < table_size) {
        value <<= 1;
        bits++;
    }

    if (bits < 1) {
        bits = 1;
    }

    return bits;
}

static int GifBitWriter_FlushBlock(GifBitWriter *writer)
{
    if (writer->block_len <= 0) {
        return 0;
    }

    fputc(writer->block_len, writer->f);
    if (fwrite(writer->block, 1, (size_t)writer->block_len, writer->f) !=
        (size_t)writer->block_len) {
        return -1;
    }

    writer->block_len = 0;
    return 0;
}

static int GifBitWriter_WriteByte(GifBitWriter *writer, u8 value)
{
    writer->block[writer->block_len++] = value;

    if (writer->block_len == 255) {
        return GifBitWriter_FlushBlock(writer);
    }

    return 0;
}

static int GifBitWriter_WriteCode(
    GifBitWriter *writer,
    int code,
    int code_size
)
{
    writer->bit_buffer |= ((unsigned int)code << writer->bit_count);
    writer->bit_count += code_size;

    while (writer->bit_count >= 8) {
        if (GifBitWriter_WriteByte(writer, (u8)(writer->bit_buffer & 0xFF)) != 0) {
            return -1;
        }

        writer->bit_buffer >>= 8;
        writer->bit_count -= 8;
    }

    return 0;
}

static int GifBitWriter_Finish(GifBitWriter *writer)
{
    if (writer->bit_count > 0) {
        if (GifBitWriter_WriteByte(writer, (u8)(writer->bit_buffer & 0xFF)) != 0) {
            return -1;
        }

        writer->bit_buffer = 0;
        writer->bit_count = 0;
    }

    if (GifBitWriter_FlushBlock(writer) != 0) {
        return -1;
    }

    fputc(0, writer->f);
    return 0;
}

static int WriteLzwLiteralImageData(
    FILE *f,
    const u8 *frame,
    int width,
    int height,
    int scale,
    int min_code_size
)
{
    GifBitWriter writer;
    int clear_code;
    int end_code;
    int code_size;
    int literal_limit;
    int emitted_literals;
    int out_width;
    int out_height;
    int y;

    if (scale <= 0) {
        scale = 1;
    }

    clear_code = 1 << min_code_size;
    end_code = clear_code + 1;
    code_size = min_code_size + 1;
    literal_limit = (1 << code_size) - end_code - 3;
    if (literal_limit < 1) {
        literal_limit = 1;
    }

    fputc(min_code_size, f);

    memset(&writer, 0, sizeof(writer));
    writer.f = f;

    if (GifBitWriter_WriteCode(&writer, clear_code, code_size) != 0) {
        return -1;
    }

    emitted_literals = 0;
    out_width = width * scale;
    out_height = height * scale;

    for (y = 0; y < out_height; y++) {
        int x;
        int src_y;

        src_y = y / scale;

        for (x = 0; x < out_width; x++) {
            int src_x;
            int color_index;

            if (emitted_literals >= literal_limit) {
                if (GifBitWriter_WriteCode(&writer, clear_code, code_size) != 0) {
                    return -1;
                }
                emitted_literals = 0;
            }

            src_x = x / scale;
            color_index = frame[src_y * width + src_x];

            if (GifBitWriter_WriteCode(&writer, color_index, code_size) != 0) {
                return -1;
            }

            emitted_literals++;
        }
    }

    if (GifBitWriter_WriteCode(&writer, end_code, code_size) != 0) {
        return -1;
    }

    return GifBitWriter_Finish(&writer);
}

int Gif_WriteIndexed(
    const char *path,
    const u8 *frames,
    int frame_count,
    int width,
    int height,
    const RgbaColor *palette,
    int palette_count,
    int transparent_index,
    int delay_cs,
    int loop_count,
    int scale
)
{
    FILE *f;
    int table_size;
    int table_bits;
    int min_code_size;
    int out_width;
    int out_height;
    int frame_step;
    int output_delay_cs;
    int output_frame_count;
    int i;

    if (path == NULL || frames == NULL || frame_count <= 0 ||
        width <= 0 || height <= 0 || palette == NULL || palette_count <= 0) {
        return -1;
    }

    if (scale <= 0) {
        scale = 1;
    }

    out_width = width * scale;
    out_height = height * scale;

    if (out_width <= 0 || out_height <= 0 ||
        out_width > 65535 || out_height > 65535) {
        return -1;
    }

    ResolveCompatibleTiming(delay_cs, &frame_step, &output_delay_cs);
    output_frame_count = (frame_count + frame_step - 1) / frame_step;
    if (output_frame_count <= 0) {
        output_frame_count = 1;
    }
    if (frame_count > 1 && output_frame_count < 2) {
        output_frame_count = 2;
    }

    table_size = ColorTableSize(palette_count);
    table_bits = BitsForColorTable(table_size);
    min_code_size = table_bits;
    if (min_code_size < 2) {
        min_code_size = 2;
    }

    f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        return -1;
    }

    fwrite("GIF89a", 1, 6, f);
    WriteU16(f, out_width);
    WriteU16(f, out_height);
    fputc(0x80 | ((table_bits - 1) << 4) | (table_bits - 1), f);
    fputc(transparent_index >= 0 ? transparent_index : 0, f);
    fputc(0, f);

    for (i = 0; i < table_size; i++) {
        if (i < palette_count) {
            fputc(palette[i].r, f);
            fputc(palette[i].g, f);
            fputc(palette[i].b, f);
        } else {
            fputc(0, f);
            fputc(0, f);
            fputc(0, f);
        }
    }

    fputc(0x21, f);
    fputc(0xFF, f);
    fputc(11, f);
    fwrite("NETSCAPE2.0", 1, 11, f);
    fputc(3, f);
    fputc(1, f);
    WriteU16(f, loop_count < 0 ? 0 : loop_count);
    fputc(0, f);

    for (i = 0; i < output_frame_count; i++) {
        const u8 *frame;
        int source_frame;

        source_frame = i * frame_step;
        if (source_frame >= frame_count) {
            source_frame = frame_count - 1;
        }
        frame = frames + ((size_t)source_frame * (size_t)width * (size_t)height);

        fputc(0x21, f);
        fputc(0xF9, f);
        fputc(4, f);
        fputc(((2 & 0x07) << 2) | (transparent_index >= 0 ? 1 : 0), f);
        WriteU16(f, output_delay_cs);
        fputc(transparent_index >= 0 ? transparent_index : 0, f);
        fputc(0, f);

        fputc(0x2C, f);
        WriteU16(f, 0);
        WriteU16(f, 0);
        WriteU16(f, out_width);
        WriteU16(f, out_height);
        fputc(0, f);

        if (WriteLzwLiteralImageData(
                f,
                frame,
                width,
                height,
                scale,
                min_code_size
            ) != 0) {
            fclose(f);
            return -1;
        }
    }

    fputc(0x3B, f);

    if (fclose(f) != 0) {
        return -1;
    }

    return 0;
}
