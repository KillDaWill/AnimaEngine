#ifndef JSON_EXPORT_H
#define JSON_EXPORT_H

#include "common.h"
#include "ncer.h"
#include "nclr.h"

#define JSON_PATH_BUFFER_SIZE 4096

typedef struct JsonMemberInfo {
    int offset;
    int member_id;
    size_t raw_size;
    size_t decoded_size;
    char compression[32];
    char magic[5];
    char type[32];
    char raw_path[JSON_PATH_BUFFER_SIZE];
    char nds_path[JSON_PATH_BUFFER_SIZE];
} JsonMemberInfo;

int Json_WriteManifest(
    const char *json_dir,
    int species,
    int base,
    const JsonMemberInfo members[20]
);

int Json_WritePalettes(const char *out_dir);

int Json_WriteCells(const char *out_dir, int tile_stride);

int Json_WriteAnimation(const char *out_dir, int tile_stride);

#endif
