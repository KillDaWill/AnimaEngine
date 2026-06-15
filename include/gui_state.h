/**
 * @file gui_state.h
 * @brief GUI global state tracking, rendering options, and control flow wrappers.
 */

#ifndef GUI_STATE_H
#define GUI_STATE_H

#include <stddef.h>
#include "gui_platform.h"
#include "anima_backend.h"
#include "pokemon_catalog.h"

#define GUI_TEXT_CAPACITY 4096       /**< Maximum size for ROM path storage. */
#define GUI_SEARCH_CAPACITY 128      /**< Maximum size for species search filter queries. */
#define GUI_STATUS_CAPACITY 512      /**< Maximum size for GUI footer messages. */
#define GUI_OUTPUT_ROOT "out"        /**< Default root output directory relative to project root. */
#define GUI_GIF_DELAY_MIN_CS 1       /**< Lower bound constraint for custom GIF frame duration. */
#define GUI_GIF_DELAY_MAX_CS 50      /**< Upper bound constraint for custom GIF frame duration. */
#define GUI_GIF_DELAY_DEFAULT_CS 5   /**< Fallback default GIF frame duration in centiseconds. */

/**
 * @brief Categorization of current preview viewport drawing state.
 */
typedef enum GuiPreviewMode {
    PREVIEW_GIF,            /**< Standard animated battle idle GIF loop. */
    PREVIEW_SPRITESHEET,    /**< Grid spritesheet composite. */
    PREVIEW_STATIC_IDLE,    /**< High-quality static preview (first frame). */
    PREVIEW_IDLE_BREAK,     /**< Rarely triggered secondary animation sequence. */
    PREVIEW_COMPOSED,       /**< Combined timeline loop merging idle + break. */
    PREVIEW_NMAR_ANIMATION, /**< Direct play of selected NMAR timeline record. */
    PREVIEW_NMCR_MAP        /**< Direct draw of chosen NMCR frame layout map. */
} GuiPreviewMode;

/**
 * @brief Texture and temporal data cache for animating species previews.
 */
typedef struct GuiPreview {
    GrTexture texture;          /**< Hardware-backed display texture containing active frame sheet. */
    unsigned char *frames;      /**< Raw unpacked CPU RGBA8888 color pixel data. */
    int loaded;                 /**< 1 if preview cache is valid and active; 0 otherwise. */
    int width;                  /**< Width of single animation cell. */
    int height;                 /**< Height of single animation cell. */
    int content_x;              /**< Crop box offset coordinate (horizontal). */
    int content_y;              /**< Crop box offset coordinate (vertical). */
    int content_width;          /**< Bounded content width. */
    int content_height;         /**< Bounded content height. */
    int frame_count;            /**< Total frames cache capacity. */
    int current_frame;          /**< Currently active frame index. */
    float frame_timer;          /**< Elapsed timer tracking time before next frame swap. */
    float seconds_per_frame;    /**< Target temporal rate between frame advances. */
} GuiPreview;

/**
 * @brief State variables and option flags for the main immediate-mode GUI editor.
 */
typedef struct GuiState {
    char rom_path[GUI_TEXT_CAPACITY];                    /**< Target NDS ROM file path string. */
    char search[GUI_SEARCH_CAPACITY];                    /**< Filtering query string for Pokemon list. */
    char status[GUI_STATUS_CAPACITY];                    /**< Console status text displayed in GUI statusbar. */
    int rom_ready;                                       /**< 1 if ROM was verified and filesystem parsed. */
    int rom_text_active;                                 /**< 1 if text entry cursor is inside the ROM path box. */
    int search_active;                                   /**< 1 if search text box holds keyboard focus. */
    int selected_dex_id;                                 /**< Active species index selection (1 to 649). */
    int list_scroll;                                     /**< Scroll offset inside species index sidebar. */
    GuiPreviewMode preview_mode;                         /**< Active display method. */
    GuiPreview preview;                                  /**< Cached preview buffer. */
    int is_shiny;                                        /**< Palette state: 0 = Normal, 1 = Shiny. */
    int gender;                                          /**< Gender state: 0 = Male, 1 = Female. */
    int form_index;                                      /**< Active form offset. */
    int is_back;                                         /**< Camera state: 0 = Front, 1 = Back. */
    int form_dropdown_open;                              /**< 1 if forms selection modal is drawn. */
    int form_dropdown_scroll;                            /**< Vertical scroll inside forms dropdown list. */
    int asset_dropdown_open;                             /**< 1 if asset selection modal is drawn. */
    int asset_dropdown_scroll;                           /**< Vertical scroll inside asset dropdown list. */
    int selected_asset;                                  /**< User selected index in discovered assets list. */
    int asset_count;                                     /**< Amount of valid assets found for species. */
    AnimaPreviewAssetInfo assets[ANIMA_MAX_PREVIEW_ASSETS]; /**< Discovered asset metadata array. */
    int has_female;                                      /**< 1 if current species has female specific assets. */
    int gif_delay_cs;                                    /**< Timing rate. */
    int is_sequel;                                       /**< 1 if ROM detected as Black 2 or White 2. */
} GuiState;

/**
 * @brief Initializes default options inside a GuiState structure.
 * @param state Pointer to state to initialize.
 */
void GuiState_Init(GuiState *state);

/**
 * @brief Validates NDS ROM header and initializes internal directory mappings.
 * @param state Global GUI state pointer.
 * @param path Path to NDS ROM file.
 * @return 0 on success; negative value on validation or load error.
 */
int GuiState_LoadAndValidateRom(GuiState *state, const char *path);

/**
 * @brief Sets statusbar notification message.
 * @param state Global GUI state pointer.
 * @param text Format string.
 */
void GuiState_SetStatus(GuiState *state, const char *text);

/**
 * @brief Helper utility to safely copy a string with bounding limit constraints.
 * @param dst Destination buffer.
 * @param dst_size Destination capacity limit in bytes.
 * @param src Source string.
 */
void GuiState_CopyText(char *dst, size_t dst_size, const char *src);

/**
 * @brief Extracts file basename from full absolute or relative path.
 * @param path File path.
 * @return Pointer to start of basename within path string.
 */
const char *GuiState_BaseName(const char *path);

/**
 * @brief Sanitizes folder name by stripping out illegal character blocks.
 * @param name Source directory folder name.
 * @param out Destination buffer.
 * @param out_size Size of destination buffer.
 */
void GuiState_SanitizePath(const char *name, char *out, size_t out_size);

/**
 * @brief Builds directory path where current species assets will be written.
 * @param entry Pointer to Pokemon catalog entry.
 * @param buf Output path buffer.
 * @param sz Size of output path buffer.
 */
void GuiState_BuildOutputDir(const PokemonCatalogEntry *entry, char *buf, size_t sz);

/**
 * @brief Checks if a Pokemon species matches the search filtering text query.
 * @param entry Pointer to Pokemon catalog entry.
 * @param query Search query text filter.
 * @return 1 on match; 0 otherwise.
 */
int GuiState_EntryMatchesQuery(const PokemonCatalogEntry *entry, const char *query);

/**
 * @brief Retrieves the species catalog entry matching index.
 * @param dex_id National Dex ID (1 to 649).
 * @return Catalog entry pointer.
 */
const PokemonCatalogEntry *GuiState_SelectedEntry(int dex_id);

/**
 * @brief Retrieves form count for a species.
 * @param state Global GUI state pointer.
 * @param dex_id National Dex ID.
 * @return Amount of alternative forms (at least 1).
 */
int GuiState_GetFormCount(const GuiState *state, int dex_id);

/**
 * @brief Retrieves user-friendly label name of alternate form index.
 * @param state Global GUI state pointer.
 * @param dex_id National Dex ID.
 * @param form_idx Form index.
 * @return Read-only form name label.
 */
const char *GuiState_GetFormName(const GuiState *state, int dex_id, int form_idx);

/**
 * @brief Advances active viewport frame index based on real-world elapsed frame delta time.
 * @param p Pointer to active preview structure to advance.
 */
void GuiState_UpdatePreview(GuiPreview *p);

/**
 * @brief Deallocates texture cache and color frames stored inside preview struct.
 * @param p Pointer to preview struct.
 */
void GuiState_UnloadPreview(GuiPreview *p);

/**
 * @brief Retrieves the current custom GIF frame delay.
 * @param state Global GUI state pointer.
 * @return Delay in centiseconds.
 */
int GuiState_GifDelayCs(const GuiState *state);

/**
 * @brief Sets the custom GIF frame delay.
 * @param state Global GUI state pointer.
 * @param delay_cs Custom delay in centiseconds.
 */
void GuiState_SetGifDelayCs(GuiState *state, int delay_cs);

/**
 * @brief Attempts to load the preview for a Pokemon catalog entry.
 * @param state Global GUI state pointer.
 * @param entry Pokemon catalog entry.
 * @return 0 on success; negative value on failure.
 */
int GuiState_TryLoadPreview(GuiState *state, const PokemonCatalogEntry *entry);

/**
 * @brief Scans ROM files to discover all asset variants for selected species.
 * @param state Global GUI state pointer.
 * @param entry Pokemon catalog entry.
 * @return 0 on success; negative value on error.
 */
int GuiState_RefreshAssets(GuiState *state, const PokemonCatalogEntry *entry);

/**
 * @brief Performs batch export run for currently selected Pokemon.
 * @param state Global GUI state.
 * @param entry Catalog entry.
 * @param export_spritesheets 1 to generate sheets.
 * @param export_static 1 to generate static first-frame.
 * @param export_gif 1 to write standard GIF.
 * @param export_idle_break 1 to write break GIF.
 * @param export_composed 1 to write merged loop.
 */
void GuiState_RunExport(
    GuiState *state,
    const PokemonCatalogEntry *entry,
    int export_spritesheets,
    int export_static,
    int export_gif,
    int export_idle_break,
    int export_composed
);

/**
 * @brief Exports the currently active preview asset view to its default output file path.
 * @param state Global GUI state.
 * @param entry Catalog entry.
 */
void GuiState_ExportCurrent(
    GuiState *state,
    const PokemonCatalogEntry *entry
);

/**
 * @brief Raw dumps individual compressed Nitro NARC blocks for selected species to disk.
 * @param state Global GUI state.
 * @param entry Catalog entry.
 */
void GuiState_ExportDsFiles(
    GuiState *state,
    const PokemonCatalogEntry *entry
);

/**
 * @brief Recursively renders and writes all assets discovered for species.
 * @param state Global GUI state.
 * @param entry Catalog entry.
 */
void GuiState_ExportAllAssets(
    GuiState *state,
    const PokemonCatalogEntry *entry
);

#endif
