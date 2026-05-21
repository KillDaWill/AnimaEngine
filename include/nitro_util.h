#ifndef NITRO_UTIL_H
#define NITRO_UTIL_H

#include "common.h"

int Nitro_HasMagic(const u8 *data, size_t size, const char *magic);

int Nitro_FindSection(
    const u8 *data,
    size_t size,
    const char *magic,
    size_t *out_offset,
    u32 *out_size
);

void Nitro_GetPrintableMagic(const u8 *data, size_t size, char out[5]);

int Nitro_AbsInt(int value);

s32 Nitro_ReadS32LE(const u8 *p);
s16 Nitro_ReadS16LE(const u8 *p);

#endif
