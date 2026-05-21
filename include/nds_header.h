#ifndef NDS_HEADER_H
#define NDS_HEADER_H

#include "common.h"

typedef struct NdsHeader {
    char game_title[13];
    char game_code[5];

    u32 fnt_offset;
    u32 fnt_size;

    u32 fat_offset;
    u32 fat_size;
} NdsHeader;

int NdsHeader_Parse(const u8 *rom, size_t rom_size, NdsHeader *out_header);
void NdsHeader_Print(const NdsHeader *header, size_t rom_size);

int NdsHeader_IsValidGame(const NdsHeader *header);
int NdsHeader_IsSequel(const NdsHeader *header);

#endif