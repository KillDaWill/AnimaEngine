#ifndef NDS_FNT_H
#define NDS_FNT_H

#include "common.h"

#define NDS_FNT_ROOT_DIR_ID 0xF000

int NdsFnt_FindFileId(
    const u8 *fnt,
    size_t fnt_size,
    const char *path,
    int *out_file_id
);

#endif