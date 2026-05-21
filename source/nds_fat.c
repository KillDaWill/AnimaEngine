#include "nds_fat.h"
#include "file_util.h"

int NdsFat_GetRange(
    const u8 *rom,
    size_t rom_size,
    const NdsHeader *header,
    int file_id,
    NdsFatRange *out_range
)
{
    size_t file_count;
    size_t entry_offset;
    u32 start;
    u32 end;

    if (rom == NULL || header == NULL || out_range == NULL) {
        return -1;
    }

    if (file_id < 0) {
        return -1;
    }

    file_count = header->fat_size / 8;

    if ((size_t)file_id >= file_count) {
        return -1;
    }

    entry_offset = (size_t)header->fat_offset + ((size_t)file_id * 8);

    if (entry_offset + 8 > rom_size) {
        return -1;
    }

    start = ReadU32LE(rom + entry_offset + 0);
    end   = ReadU32LE(rom + entry_offset + 4);

    if (start > end) {
        return -1;
    }

    if ((size_t)end > rom_size) {
        return -1;
    }

    out_range->start = start;
    out_range->end = end;
    out_range->size = end - start;

    return 0;
}