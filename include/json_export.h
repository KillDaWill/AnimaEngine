/**
 * @file json_export.h
 * @brief Metadata exporter for writing complete sprite extraction manifests in JSON format.
 */

#ifndef JSON_EXPORT_H
#define JSON_EXPORT_H

#include "common.h"
#include "ncer.h"
#include "nclr.h"

#define JSON_PATH_BUFFER_SIZE 4096 /**< Maximum buffer size for generating output JSON paths. */

/**
 * @brief Metadata describing a single member record parsed from an NARC archive.
 */
typedef struct JsonMemberInfo {
    int offset;                           /**< Absolute file offset of the member. */
    int member_id;                        /**< ID index of member inside the NARC. */
    size_t raw_size;                      /**< Size in bytes of compressed source member data. */
    size_t decoded_size;                  /**< Size in bytes of decompressed/decoded buffer. */
    char compression[32];                 /**< Description of compression format used (e.g. "LZ10"). */
    char magic[5];                        /**< Magic identifier signature (e.g. "RGCN"). */
    char type[32];                        /**< Categorized file type label (e.g. "ncgr", "nclr"). */
    char raw_path[JSON_PATH_BUFFER_SIZE]; /**< Physical filesystem destination path of raw dump. */
    char nds_path[JSON_PATH_BUFFER_SIZE]; /**< Virtual NDS path inside game ROM. */
} JsonMemberInfo;

/**
 * @brief Writes complete metadata manifest JSON for a Pokemon species.
 * @param json_dir Directory to write output manifest.
 * @param species National Dex ID.
 * @param base Member offset inside Pokegra NARC database.
 * @param members Array of member structures detailing source block elements.
 * @return 0 on success; negative value on file write failure.
 */
int Json_WriteManifest(
    const char *json_dir,
    int species,
    int base,
    const JsonMemberInfo members[20]
);

/**
 * @brief Stub helper to write debug palettes metadata.
 * @param out_dir Output directory.
 * @return 0 on success; negative value on failure.
 */
int Json_WritePalettes(const char *out_dir);

/**
 * @brief Stub helper to write cell layout configurations.
 * @param out_dir Output directory.
 * @param tile_stride Stride value.
 * @return 0 on success; negative value on failure.
 */
int Json_WriteCells(const char *out_dir, int tile_stride);

/**
 * @brief Stub helper to write animation timeline data.
 * @param out_dir Output directory.
 * @param tile_stride Stride value.
 * @return 0 on success; negative value on failure.
 */
int Json_WriteAnimation(const char *out_dir, int tile_stride);

#endif

