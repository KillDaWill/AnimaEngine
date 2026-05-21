#include "narc.h"
#include "file_util.h"
#include "nitro_util.h"

int Narc_Init(NarcArchive *narc, const u8 *data, size_t size)
{
    size_t pos;
    int found_btaf;
    int found_btnf;
    int found_gmif;

    if (narc == NULL || data == NULL) {
        return -1;
    }

    if (size < 0x10) {
        return -1;
    }

    if (!Nitro_HasMagic(data, size, "NARC")) {
        return -1;
    }

    memset(narc, 0, sizeof(*narc));

    narc->data = data;
    narc->size = size;

    found_btaf = 0;
    found_btnf = 0;
    found_gmif = 0;

    /*
     * NARC header is normally 0x10 bytes.
     * After that come chunks:
     *   BTAF
     *   BTNF
     *   GMIF
     */
    pos = 0x10;

    while (pos + 8 <= size) {
        u32 chunk_size;

        chunk_size = ReadU32LE(data + pos + 4);

        if (chunk_size < 8) {
            return -1;
        }

        if (pos + chunk_size > size) {
            return -1;
        }

        if (Nitro_HasMagic(data + pos, size - pos, "BTAF")) {
            narc->btaf_offset = (u32)pos;
            found_btaf = 1;
        } else if (Nitro_HasMagic(data + pos, size - pos, "BTNF")) {
            narc->btnf_offset = (u32)pos;
            found_btnf = 1;
        } else if (Nitro_HasMagic(data + pos, size - pos, "GMIF")) {
            narc->gmif_offset = (u32)pos;
            narc->gmif_data_offset = (u32)(pos + 8);
            found_gmif = 1;
        }

        pos += chunk_size;
    }

    if (!found_btaf || !found_btnf || !found_gmif) {
        return -1;
    }

    /*
     * BTAF layout:
     *   0x00 magic "BTAF"
     *   0x04 section size
     *   0x08 file count, effectively u16 + reserved,
     *        reading u32 works when reserved is zero
     *   0x0C entries: start/end pairs
     */
    if (narc->btaf_offset + 0x0C > size) {
        return -1;
    }

    narc->file_count = ReadU32LE(data + narc->btaf_offset + 0x08);

    if (narc->file_count == 0) {
        return -1;
    }

    if (narc->btaf_offset + 0x0C + narc->file_count * 8 > size) {
        return -1;
    }

    return 0;
}

int Narc_GetMemberRange(
    const NarcArchive *narc,
    int member_id,
    NarcMemberRange *out_range
)
{
    size_t entry_offset;
    u32 rel_start;
    u32 rel_end;
    u32 abs_start;
    u32 abs_end;

    if (narc == NULL || out_range == NULL) {
        return -1;
    }

    if (member_id < 0 || (u32)member_id >= narc->file_count) {
        return -1;
    }

    entry_offset = (size_t)narc->btaf_offset + 0x0C + ((size_t)member_id * 8);

    if (entry_offset + 8 > narc->size) {
        return -1;
    }

    rel_start = ReadU32LE(narc->data + entry_offset + 0);
    rel_end   = ReadU32LE(narc->data + entry_offset + 4);

    if (rel_start > rel_end) {
        return -1;
    }

    abs_start = narc->gmif_data_offset + rel_start;
    abs_end   = narc->gmif_data_offset + rel_end;

    if ((size_t)abs_end > narc->size) {
        return -1;
    }

    out_range->start = abs_start;
    out_range->end = abs_end;
    out_range->size = abs_end - abs_start;

    return 0;
}

int Narc_ExtractMember(
    const NarcArchive *narc,
    int member_id,
    u8 **out_data,
    size_t *out_size
)
{
    NarcMemberRange range;
    u8 *data;

    if (out_data == NULL || out_size == NULL) {
        return -1;
    }

    *out_data = NULL;
    *out_size = 0;

    if (Narc_GetMemberRange(narc, member_id, &range) != 0) {
        return -1;
    }

    data = malloc(range.size);
    if (data == NULL) {
        return -1;
    }

    memcpy(data, narc->data + range.start, range.size);

    *out_data = data;
    *out_size = range.size;

    return 0;
}

void Narc_PrintInfo(const NarcArchive *narc)
{
    printf("NARC info:\n");
    printf("  size:             %zu bytes\n", narc->size);
    printf("  BTAF offset:      0x%08X\n", narc->btaf_offset);
    printf("  BTNF offset:      0x%08X\n", narc->btnf_offset);
    printf("  GMIF offset:      0x%08X\n", narc->gmif_offset);
    printf("  GMIF data offset: 0x%08X\n", narc->gmif_data_offset);
    printf("  file count:       %u\n", narc->file_count);
    printf("\n");
}