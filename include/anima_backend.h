/**
 * @file anima_backend.h
 * @brief High-level orchestration API for Nintendo DS Pokemon Black/White asset extraction.
 *
 * This module coordinates NDS ROM filesystem reading, NARC parsing, LZ decompression,
 * and calling the sub-parsers (NCGR, NCLR, NCER, NANR, NMCR, NMAR) to assemble and
 * export sprite sheets, static previews, composed animations, and manifests.
 */

#ifndef ANIMA_BACKEND_H
#define ANIMA_BACKEND_H

#include <stddef.h>
#include "gif_pipeline.h"

#define ANIMA_PATH_BUFFER_SIZE 4096      /**< Buffer size for absolute and relative paths. */
#define ANIMA_BW_TILE_STRIDE 32          /**< Hardcoded width stride of NCGR tiles for Gen 5 Pokegra NARCs. */
#define ANIMA_COMPOSITE_MARGIN 8         /**< Safety margin in pixels around composed canvas rendering bounds. */
#define ANIMA_POKEGRA_PATH "/a/0/0/4"    /**< Virtual NDS path to Pokegra NARC archive inside Black/White filesystem. */
#define ANIMA_MAX_PREVIEW_ASSETS 96      /**< Maximum amount of discoverable and previewable assets per species. */
#define ANIMA_PREVIEW_ASSET_NAME_SIZE 96 /**< Maximum label size of discovered preview assets. */

/**
 * @brief Categorization of discoverable/renderable assets for a species.
 */
typedef enum AnimaPreviewAssetType {
    ANIMA_PREVIEW_ASSET_IDLE_GIF = 0,            /**< Standard animated battle idle GIF. */
    ANIMA_PREVIEW_ASSET_SPRITESHEET_PNG = 1,     /**< Recomposed tile sheet containing indexed sprite frames. */
    ANIMA_PREVIEW_ASSET_STATIC_IDLE_PNG = 2,     /**< First idle frame static high-quality PNG. */
    ANIMA_PREVIEW_ASSET_IDLE_BREAK_GIF = 3,      /**< Secondary/rare idle animation played after repeated cycles. */
    ANIMA_PREVIEW_ASSET_COMPOSED_GIF = 4,        /**< Combined timeline loop merging repetitions of idle + break. */
    ANIMA_PREVIEW_ASSET_NMAR_ANIMATION_GIF = 5,  /**< Arbitrary direct NMAR labeled animation index preview. */
    ANIMA_PREVIEW_ASSET_NMCR_MAP_GIF = 6         /**< Direct rendering of individual NMCR sprite maps. */
} AnimaPreviewAssetType;

/**
 * @brief Configuration parameters for a batch extraction pipeline run.
 */
typedef struct AnimaExtractOptions {
    int export_spritesheets;       /**< 1 to enable spritesheet PNG generation; 0 otherwise. */
    int export_static_previews;    /**< 1 to write static first frame previews; 0 otherwise. */
    int export_gifs;               /**< 1 to export basic idle GIFs; 0 otherwise. */
    int export_idle_break_gifs;    /**< 1 to export break candidate animation GIFs; 0 otherwise. */
    int export_composed_gifs;      /**< 1 to export composed idle+break animations; 0 otherwise. */
    int export_json;               /**< 1 to write complete reconstruction JSON manifests; 0 otherwise. */
    int form_index;                /**< Selected alternate form index (e.g. Rotom, Deoxys, Giratina). */
    GifExportOptions gif_options;  /**< Detailed centisecond duration and looping constraints. */
} AnimaExtractOptions;

/**
 * @brief Raw RGBA frame buffer container used by both CLI rendering and GUI preview components.
 */
typedef struct AnimaIdlePreview {
    int width;                     /**< Frame buffer canvas width. */
    int height;                    /**< Frame buffer canvas height. */
    int frame_count;               /**< Number of frames stored sequentially in rgba. */
    int delay_cs;                  /**< Default inter-frame duration in centiseconds. */
    unsigned char *rgba;           /**< Raw RGBA8888 byte buffer (width * height * 4 * frame_count). */
} AnimaIdlePreview;

/**
 * @brief Configuration context specifying target variant rendering parameters.
 */
typedef struct AnimaPreviewOptions {
    int gender;          /**< Gender variant selection (0 = Male, 1 = Female). */
    int is_shiny;        /**< Shiny palette switch (0 = Normal, 1 = Shiny). */
    int form_index;      /**< Alternate form index within Pokegra form block. */
    int is_back;         /**< Camera perspective (0 = Front battle view, 1 = Back view). */
    int animation_index; /**< Specific NMAR timeline index to target in asset preview. */
    int map_index;       /**< Specific NMCR layout index to target in asset preview. */
    int gif_delay_cs;    /**< Target delay in centiseconds (0 or negative falls back to catalog). */
} AnimaPreviewOptions;

/**
 * @brief Metadata record detailing a discovered asset available for preview and export.
 */
typedef struct AnimaPreviewAssetInfo {
    AnimaPreviewAssetType type;                       /**< Category type of the asset. */
    int animation_index;                              /**< Relevant animation index or -1. */
    int map_index;                                    /**< Relevant map layout index or -1. */
    int frame_count;                                  /**< Estimated frame length. */
    char label[32];                                   /**< Raw timeline string label (e.g., "wait"). */
    char display_name[ANIMA_PREVIEW_ASSET_NAME_SIZE]; /**< Human-readable asset description. */
} AnimaPreviewAssetInfo;

/**
 * @brief Initialises default options for extraction.
 * @param options Pointer to options struct to populate.
 */
void AnimaExtractOptions_Init(AnimaExtractOptions *options);

/**
 * @brief Performs a full asset extraction pipeline for a specific Pokemon species.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID of the Pokemon (1 to 649).
 * @param out_dir Root directory where asset files will be exported.
 * @param options Extraction parameters selecting formats and blocks.
 * @return 0 on success; negative values indicate internal parsing/write failures.
 */
int AnimaBackend_ExtractPokemon(
    const char *rom_path,
    int species,
    const char *out_dir,
    const AnimaExtractOptions *options
);

/**
 * @brief Helper to generate output path for front normal battle idle GIF.
 * @param out_dir Root output directory.
 * @param buffer Output path container.
 * @param buffer_size Size of destination path buffer.
 * @return 0 on success; negative on buffer overflow.
 */
int AnimaBackend_BuildFrontNormalGifPath(
    const char *out_dir,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Helper to generate output path for front normal idle break battle GIF.
 * @param out_dir Root output directory.
 * @param buffer Output path container.
 * @param buffer_size Size of destination path buffer.
 * @return 0 on success; negative on buffer overflow.
 */
int AnimaBackend_BuildFrontNormalIdleBreakGifPath(
    const char *out_dir,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Helper to generate output path for static normal front static preview PNG.
 * @param out_dir Root output directory.
 * @param buffer Output path container.
 * @param buffer_size Size of destination path buffer.
 * @return 0 on success; negative on buffer overflow.
 */
int AnimaBackend_BuildFrontNormalStaticPath(
    const char *out_dir,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Verifies whether the specified species contains dedicated gender variant graphics.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID of the Pokemon.
 * @return 1 if female-specific member sprites exist; 0 otherwise.
 */
int AnimaBackend_HasFemaleSprite(const char *rom_path, int species);

/**
 * @brief High-level loader to construct a basic idle preview (Base front normal).
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param out_preview Output container to hold RGBA buffer.
 * @return 0 on success; negative on parsing/decompress errors.
 */
int AnimaBackend_LoadIdlePreview(
    const char *rom_path,
    int species,
    AnimaIdlePreview *out_preview
);

/**
 * @brief Extended loader for basic idle previews targeting custom shiny, perspective, or form variables.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param opts Rendering configurations.
 * @param out_preview Output container to hold RGBA buffer.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadIdlePreviewExt(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
);

/**
 * @brief Deallocates memory within an AnimaIdlePreview container.
 * @param preview Pointer to preview struct to clean.
 */
void AnimaIdlePreview_Free(AnimaIdlePreview *preview);

/**
 * @brief High-level loader to build the spritesheet PNG preview buffer (Base front normal).
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param out_preview Output container.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadSpritesheetPreview(
    const char *rom_path,
    int species,
    AnimaIdlePreview *out_preview
);

/**
 * @brief Extended spritesheet preview builder with complete variant option mappings.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param opts Rendering configurations.
 * @param out_preview Output container.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadSpritesheetPreviewExt(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
);

/**
 * @brief High-level loader to build the idle break variant preview buffer.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param out_preview Output container.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadIdleBreakPreview(
    const char *rom_path,
    int species,
    AnimaIdlePreview *out_preview
);

/**
 * @brief Extended idle break preview builder supporting shiny and perspective parameters.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param opts Rendering configurations.
 * @param out_preview Output container.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadIdleBreakPreviewExt(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
);

/**
 * @brief High-level loader to build unified composed idle-to-break animation loop.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param idle_repetitions Pre-calculated or target number of idle iterations before break.
 * @param out_preview Output container.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadComposedPreview(
    const char *rom_path,
    int species,
    int idle_repetitions,
    AnimaIdlePreview *out_preview
);

/**
 * @brief Extended composed idle-to-break preview builder supporting complete variant selections.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param idle_repetitions Target number of idle iterations before break.
 * @param opts Rendering configurations.
 * @param out_preview Output container.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadComposedPreviewExt(
    const char *rom_path,
    int species,
    int idle_repetitions,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
);

/**
 * @brief Builds custom NMAR-selected animation preview from direct timeline records.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param animation_index Direct NMAR animation timeline sequence index.
 * @param opts Rendering configurations.
 * @param out_preview Output container.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadNmarAnimationPreviewExt(
    const char *rom_path,
    int species,
    int animation_index,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
);

/**
 * @brief Builds direct static NMCR-selected composite layout preview frame.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param map_index Direct NMCR map record index.
 * @param opts Rendering configurations.
 * @param out_preview Output container.
 * @return 0 on success; negative on failure.
 */
int AnimaBackend_LoadNmcrMapPreviewExt(
    const char *rom_path,
    int species,
    int map_index,
    const AnimaPreviewOptions *opts,
    AnimaIdlePreview *out_preview
);

/**
 * @brief Scans active Nitro headers in Pokegra member block to discover and populate all available assets.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param opts Discovery configurations (side, gender, form, shiny context).
 * @param out_assets Array pointer to populate with discovered asset info.
 * @param max_assets Capacity limit of the out_assets array.
 * @param out_count Output pointer filled with total count of discovered assets.
 * @return 0 on success; negative on parsing failure.
 */
int AnimaBackend_ListPreviewAssets(
    const char *rom_path,
    int species,
    const AnimaPreviewOptions *opts,
    AnimaPreviewAssetInfo *out_assets,
    int max_assets,
    int *out_count
);

/**
 * @brief Renders and exports the active target preview buffer to a local file.
 * @param rom_path Path to the Pokemon Black/White .nds ROM.
 * @param species National Dex ID.
 * @param preview_mode Selected preview type matching AnimaPreviewAssetType.
 * @param opts Rendering configurations.
 * @param out_path Path to output destination file (PNG or GIF).
 * @return 0 on success; negative on write or export failure.
 */
int AnimaBackend_ExportCurrentAsset(
    const char *rom_path,
    int species,
    int preview_mode,
    const AnimaPreviewOptions *opts,
    const char *out_path
);

#endif
