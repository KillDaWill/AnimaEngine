#ifndef NITRO_GUESS_H
#define NITRO_GUESS_H

#include "common.h"

typedef enum NitroFileType {
    NITRO_UNKNOWN,
    NITRO_NCGR,
    NITRO_NCLR,
    NITRO_NCER,
    NITRO_NANR,
    NITRO_NMCR,
    NITRO_NMAR,
    NITRO_RAW_GFX_OR_SPECIAL,
    NITRO_EMPTY
} NitroFileType;

NitroFileType NitroGuess_Detect(
    const u8 *data,
    size_t size,
    int block_offset
);

const char *NitroGuess_Name(NitroFileType type);
const char *NitroGuess_Extension(NitroFileType type);

#endif