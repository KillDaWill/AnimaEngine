#ifndef GUI_STATE_H
#define GUI_STATE_H

#include <stddef.h>
#include "gui_platform.h"
#include "anima_backend.h"
#include "pokemon_catalog.h"

#define GUI_TEXT_CAPACITY 4096
#define GUI_SEARCH_CAPACITY 128
#define GUI_STATUS_CAPACITY 512
#define GUI_OUTPUT_ROOT "out"
#define GUI_GIF_DELAY_MIN_CS 1
#define GUI_GIF_DELAY_MAX_CS 50
#define GUI_GIF_DELAY_DEFAULT_CS 5

typedef enum GuiPreviewMode {
    PREVIEW_GIF,
    PREVIEW_SPRITESHEET,
    PREVIEW_STATIC_IDLE,
    PREVIEW_IDLE_BREAK,
    PREVIEW_COMPOSED,
    PREVIEW_NMAR_ANIMATION,
    PREVIEW_NMCR_MAP
} GuiPreviewMode;

typedef struct GuiPreview {
    GrTexture texture;
    unsigned char *frames;
    int loaded;
    int width;
    int height;
    int content_x;
    int content_y;
    int content_width;
    int content_height;
    int frame_count;
    int current_frame;
    float frame_timer;
    float seconds_per_frame;
} GuiPreview;

typedef struct GuiState {
    char rom_path[GUI_TEXT_CAPACITY];
    char search[GUI_SEARCH_CAPACITY];
    char status[GUI_STATUS_CAPACITY];
    int rom_ready;
    int rom_text_active;
    int search_active;
    int selected_dex_id;
    int list_scroll;
    GuiPreviewMode preview_mode;
    GuiPreview preview;
    int is_shiny;
    int gender;
    int form_index;
    int is_back;
    int form_dropdown_open;
    int form_dropdown_scroll;
    int asset_dropdown_open;
    int asset_dropdown_scroll;
    int selected_asset;
    int asset_count;
    AnimaPreviewAssetInfo assets[ANIMA_MAX_PREVIEW_ASSETS];
    int has_female;
    int gif_delay_cs;
    int is_sequel;
} GuiState;

void GuiState_Init(GuiState *state);
int GuiState_LoadAndValidateRom(GuiState *state, const char *path);
void GuiState_SetStatus(GuiState *state, const char *text);
void GuiState_CopyText(char *dst, size_t dst_size, const char *src);
const char *GuiState_BaseName(const char *path);
void GuiState_SanitizePath(const char *name, char *out, size_t out_size);
void GuiState_BuildOutputDir(const PokemonCatalogEntry *entry, char *buf, size_t sz);

int GuiState_EntryMatchesQuery(const PokemonCatalogEntry *entry, const char *query);
const PokemonCatalogEntry *GuiState_SelectedEntry(int dex_id);

int GuiState_GetFormCount(const GuiState *state, int dex_id);
const char *GuiState_GetFormName(const GuiState *state, int dex_id, int form_idx);

void GuiState_UpdatePreview(GuiPreview *p);
void GuiState_UnloadPreview(GuiPreview *p);
int GuiState_GifDelayCs(const GuiState *state);
void GuiState_SetGifDelayCs(GuiState *state, int delay_cs);
int GuiState_TryLoadPreview(GuiState *state, const PokemonCatalogEntry *entry);
int GuiState_RefreshAssets(GuiState *state, const PokemonCatalogEntry *entry);

void GuiState_RunExport(
    GuiState *state,
    const PokemonCatalogEntry *entry,
    int export_spritesheets,
    int export_static,
    int export_gif,
    int export_idle_break,
    int export_composed
);

void GuiState_ExportCurrent(
    GuiState *state,
    const PokemonCatalogEntry *entry
);

void GuiState_ExportDsFiles(
    GuiState *state,
    const PokemonCatalogEntry *entry
);

void GuiState_ExportAllAssets(
    GuiState *state,
    const PokemonCatalogEntry *entry
);

#endif
