#include "lz.h"

static u32 ReadU24LE(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16);
}

CompressionType Lz_Detect(const u8 *data, size_t size)
{
    if (data == NULL || size < 4) {
        return COMPRESSION_NONE;
    }

    if (data[0] == 0x10) {
        return COMPRESSION_LZ10;
    }

    if (data[0] == 0x11) {
        return COMPRESSION_LZ11;
    }

    return COMPRESSION_NONE;
}

const char *Lz_CompressionName(CompressionType type)
{
    switch (type) {
    case COMPRESSION_NONE:
        return "none";
    case COMPRESSION_LZ10:
        return "lz10";
    case COMPRESSION_LZ11:
        return "lz11";
    case COMPRESSION_UNKNOWN:
    default:
        return "unknown";
    }
}

static int Lz_Decompress10(
    const u8 *in_data,
    size_t in_size,
    u8 **out_data,
    size_t *out_size
)
{
    u32 decoded_size;
    u8 *out;
    size_t in_pos;
    size_t out_pos;

    if (in_size < 4 || in_data[0] != 0x10) {
        return -1;
    }

    decoded_size = ReadU24LE(in_data + 1);
    if (decoded_size == 0) {
        return -1;
    }

    out = malloc(decoded_size);
    if (out == NULL) {
        return -1;
    }

    in_pos = 4;
    out_pos = 0;

    while (out_pos < decoded_size) {
        u8 flags;
        int bit;

        if (in_pos >= in_size) {
            free(out);
            return -1;
        }

        flags = in_data[in_pos++];

        for (bit = 7; bit >= 0 && out_pos < decoded_size; bit--) {
            if ((flags & (1 << bit)) == 0) {
                if (in_pos >= in_size) {
                    free(out);
                    return -1;
                }

                out[out_pos++] = in_data[in_pos++];
            } else {
                u8 b1;
                u8 b2;
                u32 length;
                u32 disp;
                size_t copy_pos;
                u32 i;

                if (in_pos + 1 >= in_size) {
                    free(out);
                    return -1;
                }

                b1 = in_data[in_pos++];
                b2 = in_data[in_pos++];

                length = (b1 >> 4) + 3;
                disp = ((u32)(b1 & 0x0F) << 8) | b2;

                if (disp + 1 > out_pos) {
                    free(out);
                    return -1;
                }

                copy_pos = out_pos - disp - 1;

                for (i = 0; i < length && out_pos < decoded_size; i++) {
                    out[out_pos++] = out[copy_pos++];
                }
            }
        }
    }

    *out_data = out;
    *out_size = decoded_size;

    return 0;
}

static int Lz_Decompress11(
    const u8 *in_data,
    size_t in_size,
    u8 **out_data,
    size_t *out_size
)
{
    u32 decoded_size;
    u8 *out;
    size_t in_pos;
    size_t out_pos;

    if (in_size < 4 || in_data[0] != 0x11) {
        return -1;
    }

    decoded_size = ReadU24LE(in_data + 1);
    in_pos = 4;

    if (decoded_size == 0) {
        if (in_size < 8) {
            return -1;
        }

        decoded_size = (u32)in_data[4]
            | ((u32)in_data[5] << 8)
            | ((u32)in_data[6] << 16)
            | ((u32)in_data[7] << 24);

        in_pos = 8;
    }

    if (decoded_size == 0) {
        return -1;
    }

    out = malloc(decoded_size);
    if (out == NULL) {
        return -1;
    }

    out_pos = 0;

    while (out_pos < decoded_size) {
        u8 flags;
        int bit;

        if (in_pos >= in_size) {
            free(out);
            return -1;
        }

        flags = in_data[in_pos++];

        for (bit = 7; bit >= 0 && out_pos < decoded_size; bit--) {
            if ((flags & (1 << bit)) == 0) {
                if (in_pos >= in_size) {
                    free(out);
                    return -1;
                }

                out[out_pos++] = in_data[in_pos++];
            } else {
                u8 b1;
                u32 length;
                u32 disp;
                size_t copy_pos;
                u32 i;

                if (in_pos >= in_size) {
                    free(out);
                    return -1;
                }

                b1 = in_data[in_pos++];

                if ((b1 >> 4) == 0) {
                    u8 b2;
                    u8 b3;

                    if (in_pos + 1 >= in_size) {
                        free(out);
                        return -1;
                    }

                    b2 = in_data[in_pos++];
                    b3 = in_data[in_pos++];

                    length = (((u32)(b1 & 0x0F) << 4) | (b2 >> 4)) + 0x11;
                    disp = ((u32)(b2 & 0x0F) << 8) | b3;
                } else if ((b1 >> 4) == 1) {
                    u8 b2;
                    u8 b3;
                    u8 b4;

                    if (in_pos + 2 >= in_size) {
                        free(out);
                        return -1;
                    }

                    b2 = in_data[in_pos++];
                    b3 = in_data[in_pos++];
                    b4 = in_data[in_pos++];

                    length = (((u32)(b1 & 0x0F) << 12)
                           | ((u32)b2 << 4)
                           | (b3 >> 4)) + 0x111;

                    disp = ((u32)(b3 & 0x0F) << 8) | b4;
                } else {
                    u8 b2;

                    if (in_pos >= in_size) {
                        free(out);
                        return -1;
                    }

                    b2 = in_data[in_pos++];

                    length = (b1 >> 4) + 1;
                    disp = ((u32)(b1 & 0x0F) << 8) | b2;
                }

                if (disp + 1 > out_pos) {
                    free(out);
                    return -1;
                }

                copy_pos = out_pos - disp - 1;

                for (i = 0; i < length && out_pos < decoded_size; i++) {
                    out[out_pos++] = out[copy_pos++];
                }
            }
        }
    }

    *out_data = out;
    *out_size = decoded_size;

    return 0;
}

int Lz_Decompress(
    const u8 *in_data,
    size_t in_size,
    u8 **out_data,
    size_t *out_size,
    CompressionType *out_type
)
{
    CompressionType type;
    u8 *copy;

    if (out_data == NULL || out_size == NULL || out_type == NULL) {
        return -1;
    }

    *out_data = NULL;
    *out_size = 0;
    *out_type = COMPRESSION_NONE;

    if (in_data == NULL) {
        return -1;
    }

    type = Lz_Detect(in_data, in_size);
    *out_type = type;

    if (type == COMPRESSION_NONE) {
        copy = malloc(in_size == 0 ? 1 : in_size);
        if (copy == NULL) {
            return -1;
        }

        if (in_size > 0) {
            memcpy(copy, in_data, in_size);
        }

        *out_data = copy;
        *out_size = in_size;
        return 0;
    }

    if (type == COMPRESSION_LZ10) {
        return Lz_Decompress10(in_data, in_size, out_data, out_size);
    }

    if (type == COMPRESSION_LZ11) {
        return Lz_Decompress11(in_data, in_size, out_data, out_size);
    }

    return -1;
}