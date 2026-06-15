/**
 * @file nds_fnt.h
 * @brief Nintendo DS File Name Table (FNT) directory structure and path finder.
 */

#ifndef NDS_FNT_H
#define NDS_FNT_H

#include "common.h"

#define NDS_FNT_ROOT_DIR_ID 0xF000 /**< Base directory identifier index representing the filesystem root (/). */

/**
 * @brief Scans the FNT structures to resolve the file ID matching a virtual filesystem path.
 * @param fnt Pointer to raw binary File Name Table buffer.
 * @param fnt_size Size of FNT buffer in bytes.
 * @param path Virtual filesystem path string (e.g. "/a/0/0/4").
 * @param out_file_id Destination pointer to store the matching file ID.
 * @return 0 on success; negative value on file path resolution failure.
 */
int NdsFnt_FindFileId(
    const u8 *fnt,
    size_t fnt_size,
    const char *path,
    int *out_file_id
);

#endif