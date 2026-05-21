#include "nitro_guess.h"
#include "nitro_util.h"

NitroFileType NitroGuess_Detect(
    const u8 *data,
    size_t size,
    int block_offset
)
{
    if (size == 0) {
        return NITRO_EMPTY;
    }

    if (Nitro_HasMagic(data, size, "RGCN")) {
        return NITRO_NCGR;
    }

    if (Nitro_HasMagic(data, size, "RLCN")) {
        return NITRO_NCLR;
    }

    if (Nitro_HasMagic(data, size, "RECN")) {
        return NITRO_NCER;
    }

    if (Nitro_HasMagic(data, size, "RNAN")) {
        return NITRO_NANR;
    }

    if (Nitro_HasMagic(data, size, "RCMN")) {
        return NITRO_NMCR;
    }

    if (Nitro_HasMagic(data, size, "RAMN")) {
        return NITRO_NMAR;
    }

    /*
     * BW Pokemon sprite blocks contain some members that are known by position
     * but do not always begin with a Nitro magic in the raw extracted bytes.
     *
     * Known useful offsets:
     *   +2  front graphics / parts data
     *   +5  front animation-related data, often expected NANR after decoding
     *   +8  extra coordinate/metadata
     *   +11 back graphics / parts candidate
     *   +14 back animation-related candidate
     *   +17 extra coordinate/metadata
     */
    switch (block_offset) {
    case 0:
    case 2:
    case 5:
    case 8:
    case 9:
    case 11:
    case 14:
    case 17:
        return NITRO_RAW_GFX_OR_SPECIAL;
    default:
        return NITRO_UNKNOWN;
    }
}

const char *NitroGuess_Name(NitroFileType type)
{
    switch (type) {
    case NITRO_NCGR:
        return "NCGR";
    case NITRO_NCLR:
        return "NCLR";
    case NITRO_NCER:
        return "NCER";
    case NITRO_NANR:
        return "NANR";
    case NITRO_NMCR:
        return "NMCR";
    case NITRO_NMAR:
        return "NMAR";
    case NITRO_RAW_GFX_OR_SPECIAL:
        return "RAW_OR_SPECIAL";
    case NITRO_EMPTY:
        return "EMPTY";
    case NITRO_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

const char *NitroGuess_Extension(NitroFileType type)
{
    switch (type) {
    case NITRO_NCGR:
        return "ncgr";
    case NITRO_NCLR:
        return "nclr";
    case NITRO_NCER:
        return "ncer";
    case NITRO_NANR:
        return "nanr";
    case NITRO_NMCR:
        return "nmcr";
    case NITRO_NMAR:
        return "nmar";
    case NITRO_RAW_GFX_OR_SPECIAL:
        return "rawspecial.bin";
    case NITRO_EMPTY:
        return "empty.bin";
    case NITRO_UNKNOWN:
    default:
        return "unknown.bin";
    }
}