/**
 * @file nitro_guess.h
 * @brief Auto-detection routines matching magic signatures to Nintendo Nitro formats.
 */

#ifndef NITRO_GUESS_H
#define NITRO_GUESS_H

#include "common.h"

/**
 * @brief Identifiers for Nintendo Nitro SDK proprietary file formats.
 */
typedef enum NitroFileType {
    NITRO_UNKNOWN,            /**< Unrecognized binary block. */
    NITRO_NCGR,               /**< Character Graphic Resource (tile graphics). */
    NITRO_NCLR,               /**< Color Resource (palettes). */
    NITRO_NCER,               /**< Cell Resource (sprite OAM cell compositions). */
    NITRO_NANR,               /**< Animation Resource (timeline frame definitions). */
    NITRO_NMCR,               /**< Multi-Cell Resource (sprite map layout mapping). */
    NITRO_NMAR,               /**< Multi-Animation Resource (timelines/actions metadata). */
    NITRO_RAW_GFX_OR_SPECIAL, /**< Raw unheadered graphics block or specialized structures. */
    NITRO_EMPTY               /**< Empty or zero-padded segment. */
} NitroFileType;

/**
 * @brief Auto-detects the Nitro file format of a data stream by inspecting its magic signature.
 * @param data Pointer to binary data buffer.
 * @param size Data size in bytes.
 * @param block_offset Byte index offset to begin inspection (often 0 or 4).
 * @return Detected Nitro format identifier.
 */
NitroFileType NitroGuess_Detect(
    const u8 *data,
    size_t size,
    int block_offset
);

/**
 * @brief Returns a human-readable name string for a NitroFileType.
 * @param type Nitro format identifier.
 * @return Read-only string description of file type.
 */
const char *NitroGuess_Name(NitroFileType type);

/**
 * @brief Returns the standard file extension string for a NitroFileType.
 * @param type Nitro format identifier.
 * @return Read-only standard file extension string (e.g. ".nclr").
 */
const char *NitroGuess_Extension(NitroFileType type);

#endif