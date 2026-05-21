#include "nitro_util.h"
#include "file_util.h"

int Nitro_HasMagic(const u8 *data, size_t size, const char *magic)
{
    if (data == NULL || size < 4) {
        return 0;
    }

    return memcmp(data, magic, 4) == 0;
}

int Nitro_FindSection(
    const u8 *data,
    size_t size,
    const char *magic,
    size_t *out_offset,
    u32 *out_size
)
{
    size_t pos;

    if (data == NULL || out_offset == NULL || out_size == NULL) {
        return -1;
    }

    pos = 0x10;

    while (pos + 8 <= size) {
        u32 section_size;

        section_size = ReadU32LE(data + pos + 4);

        if (section_size < 8 || pos + section_size > size) {
            return -1;
        }

        if (memcmp(data + pos, magic, 4) == 0) {
            *out_offset = pos;
            *out_size = section_size;
            return 0;
        }

        pos += section_size;
    }

    return -1;
}

void Nitro_GetPrintableMagic(const u8 *data, size_t size, char out[5])
{
    int i;

    for (i = 0; i < 4; i++) {
        if ((size_t)i >= size) {
            out[i] = '.';
        } else if (data[i] >= 32 && data[i] <= 126) {
            out[i] = (char)data[i];
        } else {
            out[i] = '.';
        }
    }

    out[4] = '\0';
}

int Nitro_AbsInt(int value)
{
    return value < 0 ? -value : value;
}

s32 Nitro_ReadS32LE(const u8 *p)
{
    return (s32)ReadU32LE(p);
}

s16 Nitro_ReadS16LE(const u8 *p)
{
    return (s16)ReadU16LE(p);
}
