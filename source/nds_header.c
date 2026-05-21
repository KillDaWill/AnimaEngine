#include "nds_header.h"
#include "file_util.h"

int NdsHeader_Parse(const u8 *rom, size_t rom_size, NdsHeader *out_header)
{
    if (rom == NULL || out_header == NULL) {
        return -1;
    }

    if (rom_size < 0x200) {
        return -1;
    }

    memcpy(out_header->game_title, rom + 0x00, 12);
    out_header->game_title[12] = '\0';

    memcpy(out_header->game_code, rom + 0x0C, 4);
    out_header->game_code[4] = '\0';

    out_header->fnt_offset = ReadU32LE(rom + 0x40);
    out_header->fnt_size   = ReadU32LE(rom + 0x44);

    out_header->fat_offset = ReadU32LE(rom + 0x48);
    out_header->fat_size   = ReadU32LE(rom + 0x4C);

    if (rom_size > 0x200) {
        if ((size_t)out_header->fnt_offset + out_header->fnt_size > rom_size) {
            return -1;
        }

        if ((size_t)out_header->fat_offset + out_header->fat_size > rom_size) {
            return -1;
        }
    }

    return 0;
}

void NdsHeader_Print(const NdsHeader *header, size_t rom_size)
{
    printf("ROM size: %zu bytes\n", rom_size);
    printf("\n");

    printf("Game title: %.12s\n", header->game_title);
    printf("Game code: %.4s\n", header->game_code);
    printf("\n");

    printf("NDS filesystem header:\n");
    printf("  FNT offset: 0x%08X (%u)\n", header->fnt_offset, header->fnt_offset);
    printf("  FNT size:   0x%08X (%u)\n", header->fnt_size, header->fnt_size);
    printf("  FAT offset: 0x%08X (%u)\n", header->fat_offset, header->fat_offset);
    printf("  FAT size:   0x%08X (%u)\n", header->fat_size, header->fat_size);
    printf("\n");
}

int NdsHeader_IsValidGame(const NdsHeader *header)
{
    if (header == NULL) return 0;
    if (header->game_code[0] != 'I' || header->game_code[1] != 'R') {
        return 0;
    }
    char c = header->game_code[2];
    if (c == 'A' || c == 'B' || c == 'C' || c == 'D' || c == 'E') {
        return 1;
    }
    return 0;
}

int NdsHeader_IsSequel(const NdsHeader *header)
{
    if (header == NULL || !NdsHeader_IsValidGame(header)) return 0;
    char c = header->game_code[2];
    return (c == 'C' || c == 'D' || c == 'E');
}