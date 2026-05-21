#include "json_export.h"
#include "file_util.h"
#include "nitro_util.h"
#include "nanr.h"
#include "nmcr.h"
#include "nmar.h"
#include "ncgr.h"
#include "ppm.h"

#define JSON_DIR_NDS_FILES "nds_files"
#define JSON_DIR_OUTPUT "reconstruction_json"

static void JsonString(FILE *f, const char *s)
{
    fputc('"', f);

    while (s != NULL && *s != '\0') {
        unsigned char c;

        c = (unsigned char)*s++;

        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c == '\n') {
            fputs("\\n", f);
        } else if (c == '\r') {
            fputs("\\r", f);
        } else if (c == '\t') {
            fputs("\\t", f);
        } else if (c < 32) {
            fprintf(f, "\\u%04x", c);
        } else {
            fputc(c, f);
        }
    }

    fputc('"', f);
}

static void WriteRawWords(FILE *f, const u8 *data, size_t size, int max_words)
{
    int word_count;
    int i;

    word_count = (int)(size / 4);
    if (word_count > max_words) {
        word_count = max_words;
    }

    fprintf(f, "[");
    for (i = 0; i < word_count; i++) {
        fprintf(f, "%s%u", i == 0 ? "" : ", ", ReadU32LE(data + ((size_t)i * 4)));
    }
    fprintf(f, "]");
}

int Json_WriteManifest(
    const char *json_dir,
    int species,
    int base,
    const JsonMemberInfo members[20]
)
{
    char path[JSON_PATH_BUFFER_SIZE];
    FILE *f;
    int i;

    snprintf(path, sizeof(path), "%s/manifest.json", json_dir);

    f = fopen(path, "w");
    if (f == NULL) {
        perror(path);
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"format\": \"pokemon_bw_sprite_block\",\n");
    fprintf(f, "  \"reconstruction\": \"NMCR_NANR_NCER\",\n");
    fprintf(f, "  \"species\": %d,\n", species);
    fprintf(f, "  \"base_member\": %d,\n", base);
    fprintf(f, "  \"block_start\": %d,\n", base);
    fprintf(f, "  \"block_end\": %d,\n", base + 19);
    fprintf(f, "  \"folders\": {\n");
    fprintf(f, "    \"raw_members\": \"../raw_narc_members\",\n");
    fprintf(f, "    \"nds_files\": \"../nds_files\",\n");
    fprintf(f, "    \"reconstruction_json\": \".\",\n");
    fprintf(f, "    \"spritesheets\": \"../spritesheet_png\",\n");
    fprintf(f, "    \"static_images\": \"../static_png\",\n");
    fprintf(f, "    \"idle_gifs\": \"../animated_idle_gif\",\n");
    fprintf(f, "    \"idle_break_gifs\": \"../idle_break_gif\",\n");
    fprintf(f, "    \"composed_gifs\": \"../composed_gif\"\n");
    fprintf(f, "  },\n");
    fprintf(f, "  \"animation_reconstruction_notes\": [\n");
    fprintf(f, "    \"Use NCGR tiles with NCLR palettes and NCER cells for sprite parts.\",\n");
    fprintf(f, "    \"Apply NANR frame transforms through NMCR map records, then NMAR timing timelines when present.\",\n");
    fprintf(f, "    \"Coordinate files are metadata/source anchors; do not add their offsets as extra translations unless a species-specific test proves it.\"\n");
    fprintf(f, "  ],\n");
    fprintf(f, "  \"members\": [\n");

    for (i = 0; i < 20; i++) {
        const JsonMemberInfo *e;

        e = &members[i];

        fprintf(f, "    {\n");
        fprintf(f, "      \"offset\": %d,\n", e->offset);
        fprintf(f, "      \"member_id\": %d,\n", e->member_id);
        fprintf(f, "      \"raw_size\": %zu,\n", e->raw_size);
        fprintf(f, "      \"decoded_size\": %zu,\n", e->decoded_size);
        fprintf(f, "      \"compression\": ");
        JsonString(f, e->compression);
        fprintf(f, ",\n      \"magic\": ");
        JsonString(f, e->magic);
        fprintf(f, ",\n      \"type\": ");
        JsonString(f, e->type);
        fprintf(f, ",\n      \"raw_file\": ");
        JsonString(f, e->raw_path);
        fprintf(f, ",\n      \"nds_file\": ");
        JsonString(f, e->nds_path);
        fprintf(f, "\n    }%s\n", i == 19 ? "" : ",");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

static int WritePaletteOneJson(FILE *f, const char *name, const char *path)
{
    u8 *data;
    size_t size;
    NclrPalette palette;
    int i;

    data = NULL;
    size = 0;

    if (File_ReadAll(path, &data, &size) != 0) {
        return -1;
    }

    if (Nclr_Parse(data, size, &palette) != 0) {
        free(data);
        return -1;
    }

    fprintf(f, "    {\n");
    fprintf(f, "      \"name\": ");
    JsonString(f, name);
    fprintf(f, ",\n      \"source\": ");
    JsonString(f, path);
    fprintf(f, ",\n      \"color_count\": %d,\n", palette.color_count);
    fprintf(f, "      \"colors\": [\n");

    for (i = 0; i < palette.color_count; i++) {
        RgbaColor color;

        color = palette.colors[i];

        fprintf(f, "        {\"index\": %d, \"bgr555\": %u, \"rgba\": [%u, %u, %u, %u]}%s\n",
                i,
                palette.raw_colors[i],
                color.r,
                color.g,
                color.b,
                color.a,
                i == palette.color_count - 1 ? "" : ",");
    }

    fprintf(f, "      ]\n");
    fprintf(f, "    }");

    free(data);
    return 0;
}

int Json_WritePalettes(const char *out_dir)
{
    char json_path[JSON_PATH_BUFFER_SIZE];
    char normal_path[JSON_PATH_BUFFER_SIZE];
    char shiny_path[JSON_PATH_BUFFER_SIZE];
    FILE *f;

    snprintf(json_path, sizeof(json_path), "%s/%s/palettes.json", out_dir, JSON_DIR_OUTPUT);
    snprintf(normal_path, sizeof(normal_path), "%s/%s/palette_normal.nclr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(shiny_path, sizeof(shiny_path), "%s/%s/palette_shiny.nclr", out_dir, JSON_DIR_NDS_FILES);

    {
        char json_dir[JSON_PATH_BUFFER_SIZE];
        snprintf(json_dir, sizeof(json_dir), "%s/%s", out_dir, JSON_DIR_OUTPUT);
        File_MkdirRecursive(json_dir);
    }

    f = fopen(json_path, "w");
    if (f == NULL) {
        perror(json_path);
        return -1;
    }

    fprintf(f, "{\n  \"palettes\": [\n");
    if (WritePaletteOneJson(f, "normal", normal_path) != 0) {
        fclose(f);
        return -1;
    }
    fprintf(f, ",\n");
    if (WritePaletteOneJson(f, "shiny", shiny_path) != 0) {
        fclose(f);
        return -1;
    }
    fprintf(f, "\n  ]\n}\n");

    fclose(f);
    return 0;
}

static int WriteCellsJsonOne(const char *ncer_path, const char *json_path, const char *side, int tile_stride)
{
    u8 *data;
    size_t size;
    NcerFile ncer;
    FILE *f;
    int i;

    data = NULL;
    size = 0;
    memset(&ncer, 0, sizeof(ncer));

    if (File_ReadAll(ncer_path, &data, &size) != 0) {
        return -1;
    }

    if (Ncer_Parse(data, size, &ncer) != 0) {
        free(data);
        return -1;
    }

    f = fopen(json_path, "w");
    if (f == NULL) {
        perror(json_path);
        Ncer_Free(&ncer);
        free(data);
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"side\": ");
    JsonString(f, side);
    fprintf(f, ",\n  \"source\": ");
    JsonString(f, ncer_path);
    fprintf(f, ",\n  \"cell_count\": %d,\n", ncer.cell_count);
    fprintf(f, "  \"tile_stride\": %d,\n", tile_stride);
    fprintf(f, "  \"cells\": [\n");

    for (i = 0; i < ncer.cell_count; i++) {
        const NcerCell *cell;
        int min_x;
        int min_y;
        int max_x;
        int max_y;
        int j;

        cell = &ncer.cells[i];
        Ncer_CellBounds(cell, &min_x, &min_y, &max_x, &max_y);

        fprintf(f, "    {\n");
        fprintf(f, "      \"index\": %d,\n", i);
        fprintf(f, "      \"oam_count\": %d,\n", cell->oam_count);
        fprintf(f, "      \"cell_attr\": %d,\n", cell->cell_attr);
        fprintf(f, "      \"raw_oam_offset\": %u,\n", cell->raw_oam_offset);
        fprintf(f, "      \"bounds\": {\"min_x\": %d, \"min_y\": %d, \"max_x\": %d, \"max_y\": %d, \"width\": %d, \"height\": %d},\n",
                min_x, min_y, max_x, max_y, max_x - min_x, max_y - min_y);
        fprintf(f, "      \"png\": \"../cell_png/%s_cells/cell_%03d.png\",\n", side, i);
        fprintf(f, "      \"oam\": [\n");

        for (j = 0; j < cell->oam_count; j++) {
            const NcerOam *oam;
            int draw_x;
            int draw_y;

            oam = &cell->oams[j];
            Ncer_GetOamDrawOrigin(oam, &draw_x, &draw_y);
            fprintf(
                f,
                "        {\"index\": %d, \"x\": %d, \"y\": %d, \"draw_x\": %d, \"draw_y\": %d, \"width\": %d, \"height\": %d, \"tile_index\": %d, \"palette\": %d, \"priority\": %d, \"flip_h\": %d, \"flip_v\": %d, \"affine\": %d, \"double_size\": %d, \"obj_mode\": %d, \"affine_index\": %d, \"shape\": %d, \"size\": %d, \"attr0\": %u, \"attr1\": %u, \"attr2\": %u}%s\n",
                j,
                oam->x,
                oam->y,
                draw_x,
                draw_y,
                oam->width,
                oam->height,
                oam->tile_index,
                oam->palette,
                oam->priority,
                oam->flip_h,
                oam->flip_v,
                oam->affine,
                oam->double_size,
                oam->obj_mode,
                oam->affine_index,
                oam->shape,
                oam->size,
                oam->attr0,
                oam->attr1,
                oam->attr2,
                j == cell->oam_count - 1 ? "" : ","
            );
        }

        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", i == ncer.cell_count - 1 ? "" : ",");
    }

    fprintf(f, "  ]\n}\n");

    fclose(f);
    Ncer_Free(&ncer);
    free(data);

    return 0;
}

int Json_WriteCells(const char *out_dir, int tile_stride)
{
    char path[JSON_PATH_BUFFER_SIZE];
    char json[JSON_PATH_BUFFER_SIZE];

    {
        char json_dir[JSON_PATH_BUFFER_SIZE];
        snprintf(json_dir, sizeof(json_dir), "%s/%s", out_dir, JSON_DIR_OUTPUT);
        File_MkdirRecursive(json_dir);
    }

    snprintf(path, sizeof(path), "%s/%s/front_cells.ncer", out_dir, JSON_DIR_NDS_FILES);
    snprintf(json, sizeof(json), "%s/%s/front_cells.json", out_dir, JSON_DIR_OUTPUT);
    if (WriteCellsJsonOne(path, json, "front", tile_stride) != 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/%s/back_cells.ncer", out_dir, JSON_DIR_NDS_FILES);
    snprintf(json, sizeof(json), "%s/%s/back_cells.json", out_dir, JSON_DIR_OUTPUT);
    if (WriteCellsJsonOne(path, json, "back", tile_stride) != 0) {
        return -1;
    }

    return 0;
}

static void WriteNitroSectionsJson(FILE *f, const u8 *data, size_t size)
{
    size_t pos;
    int section_index;

    fprintf(f, "  \"nitro_header\": {\n");
    if (size >= 0x10) {
        char magic[5];

        Nitro_GetPrintableMagic(data, size, magic);
        fprintf(f, "    \"magic\": ");
        JsonString(f, magic);
        fprintf(f, ",\n    \"bom\": %u,\n", ReadU16LE(data + 4));
        fprintf(f, "    \"version\": %u,\n", ReadU16LE(data + 6));
        fprintf(f, "    \"declared_size\": %u,\n", ReadU32LE(data + 8));
        fprintf(f, "    \"header_size\": %u,\n", ReadU16LE(data + 12));
        fprintf(f, "    \"section_count\": %u\n", ReadU16LE(data + 14));
    }
    fprintf(f, "  },\n");

    fprintf(f, "  \"sections\": [\n");

    pos = 0x10;
    section_index = 0;
    while (pos + 8 <= size) {
        u32 section_size;
        char magic[5];

        section_size = ReadU32LE(data + pos + 4);
        if (section_size < 8 || pos + section_size > size) {
            break;
        }

        Nitro_GetPrintableMagic(data + pos, size - pos, magic);

        fprintf(f, "    {\"index\": %d, \"offset\": %zu, \"magic\": ", section_index, pos);
        JsonString(f, magic);
        fprintf(f, ", \"size\": %u, \"raw_words_preview\": ", section_size);
        WriteRawWords(f, data + pos, section_size, 64);
        fprintf(f, "}");

        pos += section_size;
        section_index++;
        fprintf(f, "%s\n", pos + 8 <= size ? "," : "");
    }

    fprintf(f, "  ]");
}

static int WriteCoordsJson(FILE *f, const u8 *data, size_t size)
{
    u32 declared_count;
    size_t header_size;
    size_t record_size;
    u32 record_count;
    u32 i;

    declared_count = size >= 4 ? ReadU32LE(data) : 0;
    header_size = 16;
    record_size = 48;

    if (size < header_size || declared_count == 0 ||
        header_size + ((size_t)declared_count * record_size) > size) {
        fprintf(f, "  \"coordinate_layout\": \"unknown\",\n");
        fprintf(f, "  \"raw_word_count\": %zu,\n", size / 4);
        fprintf(f, "  \"raw_words\": ");
        WriteRawWords(f, data, size, 512);
        return 0;
    }

    record_count = declared_count;

    fprintf(f, "  \"coordinate_layout\": \"header16_records48\",\n");
    fprintf(f, "  \"declared_record_count\": %u,\n", declared_count);
    fprintf(f, "  \"header_words\": ");
    WriteRawWords(f, data, header_size, 4);
    fprintf(f, ",\n  \"records\": [\n");

    for (i = 0; i < record_count; i++) {
        const u8 *record;

        record = data + header_size + ((size_t)i * record_size);

        fprintf(f,
                "    {\"index\": %u, \"offset_x\": %d, \"offset_y\": %d, "
                "\"source_width\": %d, \"source_height\": %d, "
                "\"source_x\": %d, \"source_y\": %d, \"raw_words\": ",
                i,
                Nitro_ReadS16LE(record + 0),
                Nitro_ReadS16LE(record + 44),
                Nitro_ReadS32LE(record + 4),
                Nitro_ReadS32LE(record + 8),
                Nitro_ReadS32LE(record + 12),
                Nitro_ReadS32LE(record + 16));
        WriteRawWords(f, record, record_size, 12);
        fprintf(f, "}%s\n", i == record_count - 1 ? "" : ",");
    }

    fprintf(f, "  ]");

    return 0;
}

static int WriteAnimationJsonOne(
    const char *json_path,
    const char *side,
    const char *nanr_path,
    const char *nmcr_path,
    const char *nmar_path,
    const char *coords_path,
    const char *ncer_path,
    const char *ncgr_path,
    const char *nclr_path,
    const char *composite_dir,
    int tile_stride
)
{
    u8 *nanr_data;
    u8 *nmcr_data;
    u8 *nmar_data;
    u8 *coords_data;
    u8 *ncer_data;
    u8 *ncgr_data;
    u8 *nclr_data;
    size_t nanr_size;
    size_t nmcr_size;
    size_t nmar_size;
    size_t coords_size;
    size_t ncer_size;
    size_t ncgr_size;
    size_t nclr_size;
    NanrFile nanr;
    NmcrFile nmcr;
    NcerFile ncer;
    NcgrImage ncgr;
    NclrPalette palette;
    NmarFile nmar;
    FILE *f;
    int i;
    int idle_map_index;

    nanr_data = NULL;
    nmcr_data = NULL;
    nmar_data = NULL;
    coords_data = NULL;
    ncer_data = NULL;
    ncgr_data = NULL;
    nclr_data = NULL;
    memset(&nanr, 0, sizeof(nanr));
    memset(&nmcr, 0, sizeof(nmcr));
    memset(&ncer, 0, sizeof(ncer));
    memset(&nmar, 0, sizeof(nmar));
    idle_map_index = 0;

    (void)composite_dir;

    if (File_ReadAll(nanr_path, &nanr_data, &nanr_size) != 0 ||
        File_ReadAll(nmcr_path, &nmcr_data, &nmcr_size) != 0 ||
        File_ReadAll(nmar_path, &nmar_data, &nmar_size) != 0 ||
        File_ReadAll(coords_path, &coords_data, &coords_size) != 0 ||
        File_ReadAll(ncer_path, &ncer_data, &ncer_size) != 0 ||
        File_ReadAll(ncgr_path, &ncgr_data, &ncgr_size) != 0 ||
        File_ReadAll(nclr_path, &nclr_data, &nclr_size) != 0) {
        free(nanr_data);
        free(nmcr_data);
        free(nmar_data);
        free(coords_data);
        free(ncer_data);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Nanr_Parse(nanr_data, nanr_size, &nanr) != 0 ||
        Nmcr_Parse(nmcr_data, nmcr_size, &nmcr) != 0 ||
        Ncer_Parse(ncer_data, ncer_size, &ncer) != 0 ||
        Ncgr_Parse(ncgr_data, ncgr_size, &ncgr) != 0 ||
        Nclr_Parse(nclr_data, nclr_size, &palette) != 0) {
        Nmar_Free(&nmar);
        Nanr_Free(&nanr);
        Nmcr_Free(&nmcr);
        Ncer_Free(&ncer);
        free(nanr_data);
        free(nmcr_data);
        free(nmar_data);
        free(coords_data);
        free(ncer_data);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    if (Nmar_Parse(nmar_data, nmar_size, &nmar) == 0) {
        idle_map_index = Nmar_GetIdleMapIndex(&nmar);
        if (idle_map_index < 0) idle_map_index = 0;
        Nmar_PrintInfo(&nmar);
        printf("  idle_map_index from NMAR: %d\n", idle_map_index);
    }

    f = fopen(json_path, "w");
    if (f == NULL) {
        perror(json_path);
        Nmar_Free(&nmar);
        Nanr_Free(&nanr);
        Nmcr_Free(&nmcr);
        Ncer_Free(&ncer);
        free(nanr_data);
        free(nmcr_data);
        free(nmar_data);
        free(coords_data);
        free(ncer_data);
        free(ncgr_data);
        free(nclr_data);
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"side\": ");
    JsonString(f, side);
    fprintf(f, ",\n  \"reconstruction\": \"NMCR_NANR_NCER\",\n");
    fprintf(f, "  \"idle_map_index\": %d,\n", idle_map_index);
    fprintf(f, "  \"tile_stride\": %d,\n", tile_stride);
    fprintf(f, "  \"sources\": {\n");
    fprintf(f, "    \"nanr\": ");
    JsonString(f, nanr_path);
    fprintf(f, ",\n    \"nmcr\": ");
    JsonString(f, nmcr_path);
    fprintf(f, ",\n    \"nmar\": ");
    JsonString(f, nmar_path);
    fprintf(f, ",\n    \"coords\": ");
    JsonString(f, coords_path);
    fprintf(f, ",\n    \"ncer\": ");
    JsonString(f, ncer_path);
    fprintf(f, ",\n    \"ncgr\": ");
    JsonString(f, ncgr_path);
    fprintf(f, ",\n    \"nclr\": ");
    JsonString(f, nclr_path);
    fprintf(f, "\n  },\n");

    fprintf(f, "  \"animations\": [\n");
    for (i = 0; i < nanr.animation_count; i++) {
        int j;

        fprintf(f, "    {\n");
        fprintf(f, "      \"index\": %d,\n", i);
        fprintf(f, "      \"frame_count\": %d,\n", nanr.animations[i].frame_count);
        fprintf(f, "      \"loop_start\": %d,\n", nanr.animations[i].loop_start);
        fprintf(f, "      \"playback_type\": %d,\n", nanr.animations[i].playback_type);
        fprintf(f, "      \"raw_frame_offset\": %u,\n", nanr.animations[i].raw_frame_offset);
        fprintf(f, "      \"frames\": [\n");

        for (j = 0; j < nanr.animations[i].frame_count; j++) {
            const NanrFrame *frame;

            frame = &nanr.animations[i].frames[j];
            fprintf(
                f,
                "        {\"index\": %d, \"cell_id\": %d, \"raw_cell_value\": %u, \"duration\": %d, \"marker\": %u, \"transform_type\": %d, \"rotation\": %d, \"scale_x\": %d, \"scale_y\": %d, \"translate_x\": %d, \"translate_y\": %d}%s\n",
                j,
                frame->cell_id,
                frame->raw_cell_value,
                frame->duration,
                frame->marker,
                frame->transform_type,
                frame->rotation,
                frame->scale_x,
                frame->scale_y,
                frame->translate_x,
                frame->translate_y,
                j == nanr.animations[i].frame_count - 1 ? "" : ","
            );
        }

        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", i == nanr.animation_count - 1 ? "" : ",");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"maps\": [\n");
    for (i = 0; i < nmcr.map_count; i++) {
        int j;
        int frame_count;

        frame_count = Nmcr_MaxFrameCount(&nmcr.maps[i], &nanr);

        fprintf(f, "    {\n");
        fprintf(f, "      \"index\": %d,\n", i);
        fprintf(f, "      \"record_count\": %d,\n", nmcr.maps[i].record_count);
        fprintf(f, "      \"raw_record_offset\": %u,\n", nmcr.maps[i].raw_record_offset);
        fprintf(f, "      \"composite_frame_count\": %d,\n", frame_count);
        fprintf(f, "      \"records\": [\n");

        for (j = 0; j < nmcr.maps[i].record_count; j++) {
            const NmcrRecord *record;

            record = &nmcr.maps[i].records[j];
            fprintf(
                f,
                "        {\"index\": %d, \"animation_index\": %d, \"x\": %d, \"y\": %d, \"flags\": %d}%s\n",
                j,
                record->animation_index,
                record->x,
                record->y,
                record->flags,
                j == nmcr.maps[i].record_count - 1 ? "" : ","
            );
        }

        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", i == nmcr.map_count - 1 ? "" : ",");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"nmar_raw\": {\n");
    fprintf(f, "    \"source\": ");
    JsonString(f, nmar_path);
    fprintf(f, ",\n    \"size\": %zu,\n", nmar_size);
    WriteNitroSectionsJson(f, nmar_data, nmar_size);
    fprintf(f, "\n  },\n");
    fprintf(f, "  \"coords_raw\": {\n");
    fprintf(f, "    \"source\": ");
    JsonString(f, coords_path);
    fprintf(f, ",\n    \"size\": %zu,\n", coords_size);
    WriteCoordsJson(f, coords_data, coords_size);
    fprintf(f, "\n  }\n");
    fprintf(f, "}\n");
    fclose(f);

    Nanr_Free(&nanr);
    Nmcr_Free(&nmcr);
    Nmar_Free(&nmar);
    Ncer_Free(&ncer);
    free(nanr_data);
    free(nmcr_data);
    free(nmar_data);
    free(coords_data);
    free(ncer_data);
    free(ncgr_data);
    free(nclr_data);

    return 0;
}

int Json_WriteAnimation(const char *out_dir, int tile_stride)
{
    char json_path[JSON_PATH_BUFFER_SIZE];
    char nanr_path[JSON_PATH_BUFFER_SIZE];
    char nmcr_path[JSON_PATH_BUFFER_SIZE];
    char nmar_path[JSON_PATH_BUFFER_SIZE];
    char coords_path[JSON_PATH_BUFFER_SIZE];
    char ncer_path[JSON_PATH_BUFFER_SIZE];
    char ncgr_path[JSON_PATH_BUFFER_SIZE];
    char nclr_path[JSON_PATH_BUFFER_SIZE];
    char composite_dir[JSON_PATH_BUFFER_SIZE];

    {
        char json_dir[JSON_PATH_BUFFER_SIZE];
        snprintf(json_dir, sizeof(json_dir), "%s/%s", out_dir, JSON_DIR_OUTPUT);
        File_MkdirRecursive(json_dir);
    }

    snprintf(json_path, sizeof(json_path), "%s/%s/front_animation.json", out_dir, JSON_DIR_OUTPUT);
    snprintf(nanr_path, sizeof(nanr_path), "%s/%s/front_anim.nanr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(nmcr_path, sizeof(nmcr_path), "%s/%s/front_map.nmcr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(nmar_path, sizeof(nmar_path), "%s/%s/front_timing.nmar", out_dir, JSON_DIR_NDS_FILES);
    snprintf(coords_path, sizeof(coords_path), "%s/%s/front_coords.bin", out_dir, JSON_DIR_NDS_FILES);
    snprintf(ncer_path, sizeof(ncer_path), "%s/%s/front_cells.ncer", out_dir, JSON_DIR_NDS_FILES);
    snprintf(ncgr_path, sizeof(ncgr_path), "%s/%s/front_sheet.ncgr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(nclr_path, sizeof(nclr_path), "%s/%s/palette_normal.nclr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(composite_dir, sizeof(composite_dir), "%s/%s/front_composites", out_dir, JSON_DIR_OUTPUT);

    if (WriteAnimationJsonOne(
            json_path,
            "front",
            nanr_path,
            nmcr_path,
            nmar_path,
            coords_path,
            ncer_path,
            ncgr_path,
            nclr_path,
            composite_dir,
            tile_stride
        ) != 0) {
        return -1;
    }

    snprintf(json_path, sizeof(json_path), "%s/%s/back_animation.json", out_dir, JSON_DIR_OUTPUT);
    snprintf(nanr_path, sizeof(nanr_path), "%s/%s/back_anim.nanr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(nmcr_path, sizeof(nmcr_path), "%s/%s/back_map.nmcr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(nmar_path, sizeof(nmar_path), "%s/%s/back_timing.nmar", out_dir, JSON_DIR_NDS_FILES);
    snprintf(coords_path, sizeof(coords_path), "%s/%s/back_coords.bin", out_dir, JSON_DIR_NDS_FILES);
    snprintf(ncer_path, sizeof(ncer_path), "%s/%s/back_cells.ncer", out_dir, JSON_DIR_NDS_FILES);
    snprintf(ncgr_path, sizeof(ncgr_path), "%s/%s/back_sheet.ncgr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(nclr_path, sizeof(nclr_path), "%s/%s/palette_normal.nclr", out_dir, JSON_DIR_NDS_FILES);
    snprintf(composite_dir, sizeof(composite_dir), "%s/%s/back_composites", out_dir, JSON_DIR_OUTPUT);

    if (WriteAnimationJsonOne(
            json_path,
            "back",
            nanr_path,
            nmcr_path,
            nmar_path,
            coords_path,
            ncer_path,
            ncgr_path,
            nclr_path,
            composite_dir,
            tile_stride
        ) != 0) {
        return -1;
    }

    return 0;
}
