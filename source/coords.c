#include "coords.h"
#include "file_util.h"
#include "nitro_util.h"

int Coord_Parse(const u8 *data, size_t size, CoordFile *out)
{
    u32 declared_count;
    size_t header_size;
    size_t record_size;
    u32 i;

    if (data == NULL || out == NULL) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    if (size < 16) {
        return -1;
    }

    declared_count = ReadU32LE(data);
    header_size = 16;
    record_size = 48;

    if (declared_count == 0 || declared_count > 4096) {
        return -1;
    }

    if (header_size + ((size_t)declared_count * record_size) > size) {
        return -1;
    }

    out->records = calloc((size_t)declared_count, sizeof(CoordRecord));
    if (out->records == NULL) {
        return -1;
    }

    out->record_count = (int)declared_count;

    for (i = 0; i < declared_count; i++) {
        const u8 *record;
        int j;

        record = data + header_size + ((size_t)i * record_size);
        for (j = 0; j < COORD_RECORD_WORD_COUNT; j++) {
            out->records[i].raw_words[j] = Nitro_ReadS32LE(record + ((size_t)j * 4));
        }

        out->records[i].offset_x = Nitro_ReadS16LE(record + 0);
        out->records[i].source_width = out->records[i].raw_words[1];
        out->records[i].source_height = out->records[i].raw_words[2];
        out->records[i].source_x = out->records[i].raw_words[3];
        out->records[i].source_y = out->records[i].raw_words[4];
        out->records[i].offset_y = Nitro_ReadS16LE(record + 44);
    }

    return 0;
}

void Coord_Free(CoordFile *coords)
{
    if (coords == NULL) return;
    free(coords->records);
    coords->records = NULL;
    coords->record_count = 0;
}
