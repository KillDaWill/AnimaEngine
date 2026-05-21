#include "gui_state.h"
#include "file_util.h"
#include "gif_writer.h"
#include "nds_header.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void GuiState_Init(GuiState *state)
{
    memset(state, 0, sizeof(*state));
    state->selected_dex_id = 1;
    state->preview_mode = PREVIEW_GIF;
    state->gif_delay_cs = GUI_GIF_DELAY_DEFAULT_CS;
    GuiState_SetStatus(state, "Select a ROM");
}

int GuiState_LoadAndValidateRom(GuiState *state, const char *path)
{
    if (state == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        GuiState_SetStatus(state, "Error: Could not open ROM file.");
        return -1;
    }
    u8 header_bytes[0x200];
    size_t read_bytes = fread(header_bytes, 1, 0x200, f);
    fclose(f);
    if (read_bytes < 0x200) {
        GuiState_SetStatus(state, "Error: ROM file is too small.");
        return -1;
    }
    NdsHeader header;
    if (NdsHeader_Parse(header_bytes, 0x200, &header) != 0) {
        GuiState_SetStatus(state, "Error: Failed to parse NDS header.");
        return -1;
    }
    if (!NdsHeader_IsValidGame(&header)) {
        char err[256];
        snprintf(err, sizeof(err), "Error: Code %.4s not Pokémon B/W or B/W 2 ROM.", header.game_code);
        GuiState_SetStatus(state, err);
        return -1;
    }
    state->is_sequel = NdsHeader_IsSequel(&header);
    GuiState_CopyText(state->rom_path, sizeof(state->rom_path), path);
    state->rom_ready = 1;
    GuiState_SetStatus(state, "ROM parsed successfully");
    return 0;
}

void GuiState_SetStatus(GuiState *state, const char *text)
{
    GuiState_CopyText(state->status, sizeof(state->status), text);
}

void GuiState_CopyText(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) return;
    if (src == NULL) { dst[0] = '\0'; return; }
    if (dst == src) return;
    snprintf(dst, dst_size, "%s", src);
}

const char *GuiState_BaseName(const char *path)
{
    const char *base, *p;
    if (path == NULL) return "";
    base = path;
    for (p = path; *p; p++) if (*p == '/' || *p == '\\') base = p + 1;
    return base;
}

void GuiState_SanitizePath(const char *name, char *out, size_t out_size)
{
    size_t i, j;
    int last_sep;
    if (out == NULL || out_size == 0) return;
    j = 0; last_sep = 1;
    for (i = 0; name && name[i] && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c)) { out[j++] = (char)tolower(c); last_sep = 0; }
        else if (!last_sep) { out[j++] = '_'; last_sep = 1; }
    }
    if (j > 0 && out[j-1] == '_') j--;
    if (j == 0 && out_size > 1) out[j++] = 'p';
    out[j] = '\0';
}

static void GuiState_SanitizeDisplayPath(const char *name, char *out, size_t out_size)
{
    size_t i, j;
    int last_sep;
    if (out == NULL || out_size == 0) return;
    j = 0; last_sep = 1;
    for (i = 0; name && name[i] && j + 1 < out_size; i++) {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c)) { out[j++] = (char)c; last_sep = 0; }
        else if (!last_sep) { out[j++] = '_'; last_sep = 1; }
    }
    if (j > 0 && out[j-1] == '_') j--;
    if (j == 0 && out_size > 1) out[j++] = 'P';
    out[j] = '\0';
}

void GuiState_BuildOutputDir(const PokemonCatalogEntry *entry, char *buf, size_t sz)
{
    char slug[128];
    if (entry == NULL || buf == NULL || sz == 0) return;
    GuiState_SanitizeDisplayPath(entry->name, slug, sizeof(slug));
    snprintf(buf, sz, "%s/pokedex%03d_%s", GUI_OUTPUT_ROOT, entry->dex_id, slug);
}

static int StrContainsInsensitive(const char *hay, const char *needle)
{
    size_t hl, nl, i, j;
    if (!hay || !needle) return 0;
    hl = strlen(hay); nl = strlen(needle);
    if (nl == 0) return 1;
    if (nl > hl) return 0;
    for (i = 0; i <= hl - nl; i++) {
        int ok = 1;
        for (j = 0; j < nl; j++) {
            if (tolower((unsigned char)hay[i+j]) != tolower((unsigned char)needle[j])) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

int GuiState_EntryMatchesQuery(const PokemonCatalogEntry *entry, const char *query)
{
    char dex[16], padded[16];
    if (!entry || !query || !query[0]) return 1;
    snprintf(dex, sizeof(dex), "%d", entry->dex_id);
    snprintf(padded, sizeof(padded), "%03d", entry->dex_id);
    return StrContainsInsensitive(entry->name, query) ||
           StrContainsInsensitive(dex, query) ||
           StrContainsInsensitive(padded, query);
}

const PokemonCatalogEntry *GuiState_SelectedEntry(int dex_id)
{
    const PokemonCatalogEntry *e = PokemonCatalog_FindByDexId(dex_id);
    return e ? e : PokemonCatalog_FindByDexId(1);
}

int GuiState_GetFormCount(const GuiState *state, int dex_id)
{
    int is_sequel = (state != NULL) && state->is_sequel;

    if (is_sequel) {
        switch (dex_id) {
            case 641: return 2; // Tornadus
            case 642: return 2; // Thundurus
            case 643: return 2; // Landorus
            case 646: return 3; // Kyurem
            case 647: return 2; // Keldeo
        }
    }

    switch (dex_id) {
        case 201: return 28; case 351: return 4;  case 386: return 4;
        case 412: return 3;  case 413: return 3;  case 421: return 2;
        case 422: return 2;  case 423: return 2;  case 479: return 6;
        case 487: return 2;  case 492: return 2;  case 550: return 2;
        case 555: return 2;  case 585: return 4;  case 586: return 4;
        case 648: return 2;  case 649: return 5;  default:  return 1;
    }
}

const char *GuiState_GetFormName(const GuiState *state, int dex_id, int form_idx)
{
    if (dex_id == 201) {
        static const char *f[] = {"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","!","?"};
        if (form_idx >= 0 && form_idx < 28) return f[form_idx];
        return "A";
    }

    if (form_idx == 0) return "Normal";

    int is_sequel = (state != NULL) && state->is_sequel;
    if (is_sequel) {
        switch (dex_id) {
            case 641: if (form_idx == 1) return "Therian"; break;
            case 642: if (form_idx == 1) return "Therian"; break;
            case 643: if (form_idx == 1) return "Therian"; break;
            case 646: if (form_idx == 1) return "Black Kyurem"; if (form_idx == 2) return "White Kyurem"; break;
            case 647: if (form_idx == 1) return "Resolute"; break;
        }
    }

    switch (dex_id) {
        case 351: if (form_idx==1) return "Sunny"; if (form_idx==2) return "Rainy"; if (form_idx==3) return "Snowy"; break;
        case 386: if (form_idx==1) return "Attack"; if (form_idx==2) return "Defense"; if (form_idx==3) return "Speed"; break;
        case 412: case 413: if (form_idx==1) return "Sandy"; if (form_idx==2) return "Trash"; break;
        case 421: if (form_idx==1) return "Sunshine"; break;
        case 422: case 423: if (form_idx==1) return "East Sea"; break;
        case 479: if (form_idx==1) return "Heat"; if (form_idx==2) return "Wash"; if (form_idx==3) return "Frost"; if (form_idx==4) return "Fan"; if (form_idx==5) return "Mow"; break;
        case 487: if (form_idx==1) return "Origin"; break;
        case 492: if (form_idx==1) return "Sky"; break;
        case 550: if (form_idx==1) return "Blue-Striped"; break;
        case 555: if (form_idx==1) return "Zen"; break;
        case 585: case 586: if (form_idx==1) return "Summer"; if (form_idx==2) return "Autumn"; if (form_idx==3) return "Winter"; break;
        case 648: if (form_idx==1) return "Pirouette"; break;
        case 649: if (form_idx==1) return "Shock"; if (form_idx==2) return "Burn"; if (form_idx==3) return "Chill"; if (form_idx==4) return "Douse"; break;
        default: break;
    }
    return "Normal";
}

static void GuiState_BuildPreviewOptions(GuiState *state, AnimaPreviewOptions *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->gender = state->gender;
    opts->is_shiny = state->is_shiny;
    opts->form_index = state->form_index;
    opts->is_back = state->is_back;
    opts->animation_index = -1;
    opts->map_index = -1;
    opts->gif_delay_cs = GuiState_GifDelayCs(state);
}

static const AnimaPreviewAssetInfo *GuiState_CurrentAsset(const GuiState *state)
{
    if (state == NULL || state->asset_count <= 0 ||
        state->selected_asset < 0 || state->selected_asset >= state->asset_count) {
        return NULL;
    }

    return &state->assets[state->selected_asset];
}

int GuiState_RefreshAssets(GuiState *state, const PokemonCatalogEntry *entry)
{
    AnimaPreviewOptions opts;
    AnimaPreviewAssetInfo previous;
    int previous_valid;
    int i;
    int best;

    if (state == NULL || entry == NULL || !state->rom_ready) {
        return -1;
    }

    previous_valid = 0;
    memset(&previous, 0, sizeof(previous));
    if (state->asset_count > 0 &&
        state->selected_asset >= 0 &&
        state->selected_asset < state->asset_count) {
        previous = state->assets[state->selected_asset];
        previous_valid = 1;
    } else {
        previous.type = (AnimaPreviewAssetType)state->preview_mode;
        previous.animation_index = -1;
        previous.map_index = -1;
    }

    GuiState_BuildPreviewOptions(state, &opts);
    if (AnimaBackend_ListPreviewAssets(
            state->rom_path,
            entry->dex_id,
            &opts,
            state->assets,
            ANIMA_MAX_PREVIEW_ASSETS,
            &state->asset_count
        ) != 0 || state->asset_count <= 0) {
        state->asset_count = 0;
        state->selected_asset = 0;
        state->asset_dropdown_scroll = 0;
        return -1;
    }

    best = 0;
    for (i = 0; i < state->asset_count; i++) {
        if (previous_valid &&
            state->assets[i].type == previous.type &&
            state->assets[i].animation_index == previous.animation_index &&
            state->assets[i].map_index == previous.map_index) {
            best = i;
            break;
        }
        if (!previous_valid && state->assets[i].type == previous.type) {
            best = i;
            break;
        }
    }

    state->selected_asset = best;
    state->preview_mode = (GuiPreviewMode)state->assets[best].type;
    if (state->asset_dropdown_scroll > state->selected_asset) {
        state->asset_dropdown_scroll = state->selected_asset;
    }
    if (state->asset_dropdown_scroll < 0) {
        state->asset_dropdown_scroll = 0;
    }
    return 0;
}

void GuiState_UpdatePreview(GuiPreview *p)
{
    if (!p || !p->loaded || p->frame_count <= 1) return;
    p->frame_timer += Gr_GetFrameDelta();
    if (p->frame_timer < p->seconds_per_frame) return;
    p->frame_timer = 0.0f;
    p->current_frame = (p->current_frame + 1) % p->frame_count;
    size_t fs = (size_t)p->width * (size_t)p->height * 4u;
    Gr_UpdateTexture(&p->texture, p->frames + ((size_t)p->current_frame * fs));
}

int GuiState_GifDelayCs(const GuiState *state)
{
    int delay_cs;

    if (state == NULL) {
        return GUI_GIF_DELAY_DEFAULT_CS;
    }

    delay_cs = state->gif_delay_cs;
    if (delay_cs < GUI_GIF_DELAY_MIN_CS) delay_cs = GUI_GIF_DELAY_MIN_CS;
    if (delay_cs > GUI_GIF_DELAY_MAX_CS) delay_cs = GUI_GIF_DELAY_MAX_CS;
    return delay_cs;
}

void GuiState_SetGifDelayCs(GuiState *state, int delay_cs)
{
    if (state == NULL) return;

    if (delay_cs < GUI_GIF_DELAY_MIN_CS) delay_cs = GUI_GIF_DELAY_MIN_CS;
    if (delay_cs > GUI_GIF_DELAY_MAX_CS) delay_cs = GUI_GIF_DELAY_MAX_CS;
    state->gif_delay_cs = delay_cs;

    if (state->preview.loaded && state->preview.frame_count > 1) {
        state->preview.seconds_per_frame = (float)delay_cs / 100.0f;
        state->preview.frame_timer = 0.0f;
    }
}

void GuiState_UnloadPreview(GuiPreview *p)
{
    if (!p || !p->loaded) return;
    Gr_UnloadTexture(&p->texture);
    free(p->frames);
    memset(p, 0, sizeof(*p));
}

static void GuiState_SetPreviewContentBounds(GuiPreview *p)
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int frame;

    if (p == NULL || p->frames == NULL ||
        p->width <= 0 || p->height <= 0 || p->frame_count <= 0) {
        return;
    }

    min_x = p->width;
    min_y = p->height;
    max_x = -1;
    max_y = -1;

    for (frame = 0; frame < p->frame_count; frame++) {
        const unsigned char *rgba;
        int y;

        rgba = p->frames + ((size_t)frame * (size_t)p->width * (size_t)p->height * 4u);
        for (y = 0; y < p->height; y++) {
            int x;
            for (x = 0; x < p->width; x++) {
                size_t alpha_index = (((size_t)y * (size_t)p->width) + (size_t)x) * 4u + 3u;
                if (rgba[alpha_index] != 0) {
                    if (x < min_x) min_x = x;
                    if (y < min_y) min_y = y;
                    if (x > max_x) max_x = x;
                    if (y > max_y) max_y = y;
                }
            }
        }
    }

    if (max_x < min_x || max_y < min_y) {
        p->content_x = 0;
        p->content_y = 0;
        p->content_width = p->width;
        p->content_height = p->height;
        return;
    }

    p->content_x = min_x;
    p->content_y = min_y;
    p->content_width = max_x - min_x + 1;
    p->content_height = max_y - min_y + 1;
}

static int LoadPreviewFromBackend(GuiState *state, const PokemonCatalogEntry *entry)
{
    AnimaIdlePreview idle_preview;
    AnimaPreviewOptions opts;
    GrTexture tex;

    if (!state || !entry || !state->rom_ready) return -1;
    GuiState_BuildPreviewOptions(state, &opts);

    memset(&idle_preview, 0, sizeof(idle_preview));
    if (AnimaBackend_LoadIdlePreviewExt(state->rom_path, entry->dex_id, &opts, &idle_preview) != 0 ||
        !idle_preview.rgba || idle_preview.width <= 0 || idle_preview.height <= 0 || idle_preview.frame_count <= 0) {
        AnimaIdlePreview_Free(&idle_preview); return -1;
    }
    if (Gr_MakeTextureRGBA(&tex, idle_preview.rgba, idle_preview.width, idle_preview.height) != 0) {
        AnimaIdlePreview_Free(&idle_preview); return -1;
    }
    GuiState_UnloadPreview(&state->preview);
    state->preview.texture = tex;
    state->preview.frames = idle_preview.rgba;
    state->preview.loaded = 1;
    state->preview.width = idle_preview.width;
    state->preview.height = idle_preview.height;
    state->preview.frame_count = idle_preview.frame_count;
    state->preview.current_frame = 0;
    state->preview.frame_timer = 0.0f;
    state->preview.seconds_per_frame = (float)GuiState_GifDelayCs(state) / 100.0f;
    if (state->preview.seconds_per_frame <= 0.0f) state->preview.seconds_per_frame = 0.08f;
    GuiState_SetPreviewContentBounds(&state->preview);
    idle_preview.rgba = NULL;
    return 0;
}

static int LoadPngPreview(GuiState *state, const PokemonCatalogEntry *entry, int backend_func(GuiState*,const PokemonCatalogEntry*,AnimaIdlePreview*))
{
    AnimaIdlePreview preview;
    GrTexture tex;
    memset(&preview, 0, sizeof(preview));
    if (backend_func(state, entry, &preview) != 0 || !preview.rgba || preview.width <= 0 || preview.height <= 0) {
        AnimaIdlePreview_Free(&preview); return -1;
    }
    if (Gr_MakeTextureRGBA(&tex, preview.rgba, preview.width, preview.height) != 0) {
        AnimaIdlePreview_Free(&preview); return -1;
    }
    GuiState_UnloadPreview(&state->preview);
    state->preview.texture = tex;
    state->preview.frames = preview.rgba;
    state->preview.loaded = 1;
    state->preview.width = preview.width;
    state->preview.height = preview.height;
    state->preview.frame_count = 1;
    state->preview.current_frame = 0;
    state->preview.frame_timer = 0.0f;
    state->preview.seconds_per_frame = 0.0f;
    GuiState_SetPreviewContentBounds(&state->preview);
    preview.rgba = NULL;
    return 0;
}

static int LoadAnimatedPreview(GuiState *state, const PokemonCatalogEntry *entry, int backend_func(GuiState*,const PokemonCatalogEntry*,AnimaIdlePreview*))
{
    AnimaIdlePreview preview;
    GrTexture tex;
    memset(&preview, 0, sizeof(preview));
    if (backend_func(state, entry, &preview) != 0 || !preview.rgba || preview.width <= 0 || preview.height <= 0 || preview.frame_count <= 0) {
        AnimaIdlePreview_Free(&preview); return -1;
    }
    if (Gr_MakeTextureRGBA(&tex, preview.rgba, preview.width, preview.height) != 0) {
        AnimaIdlePreview_Free(&preview); return -1;
    }
    GuiState_UnloadPreview(&state->preview);
    state->preview.texture = tex;
    state->preview.frames = preview.rgba;
    state->preview.loaded = 1;
    state->preview.width = preview.width;
    state->preview.height = preview.height;
    state->preview.frame_count = preview.frame_count;
    state->preview.current_frame = 0;
    state->preview.frame_timer = 0.0f;
    state->preview.seconds_per_frame = (float)GuiState_GifDelayCs(state) / 100.0f;
    if (state->preview.seconds_per_frame <= 0.0f) state->preview.seconds_per_frame = 0.08f;
    GuiState_SetPreviewContentBounds(&state->preview);
    preview.rgba = NULL;
    return 0;
}

static int LoadSpritesheet(GuiState *state, const PokemonCatalogEntry *entry, AnimaIdlePreview *out)
{
    AnimaPreviewOptions opts; GuiState_BuildPreviewOptions(state, &opts);
    return AnimaBackend_LoadSpritesheetPreviewExt(state->rom_path, entry->dex_id, &opts, out);
}
static int LoadStaticIdle(GuiState *state, const PokemonCatalogEntry *entry, AnimaIdlePreview *out)
{
    AnimaPreviewOptions opts; GuiState_BuildPreviewOptions(state, &opts);
    return AnimaBackend_LoadIdlePreviewExt(state->rom_path, entry->dex_id, &opts, out);
}
static int LoadBreak(GuiState *state, const PokemonCatalogEntry *entry, AnimaIdlePreview *out)
{
    AnimaPreviewOptions opts; GuiState_BuildPreviewOptions(state, &opts);
    return AnimaBackend_LoadIdleBreakPreviewExt(state->rom_path, entry->dex_id, &opts, out);
}
static int LoadComposed(GuiState *state, const PokemonCatalogEntry *entry, AnimaIdlePreview *out)
{
    AnimaPreviewOptions opts;
    const AnimaPreviewAssetInfo *asset = GuiState_CurrentAsset(state);
    GuiState_BuildPreviewOptions(state, &opts);
    if (asset != NULL) {
        opts.animation_index = asset->animation_index;
        opts.map_index = asset->map_index;
    }
    return AnimaBackend_LoadComposedPreviewExt(state->rom_path, entry->dex_id, -1, &opts, out);
}
static int LoadNmarAnimation(GuiState *state, const PokemonCatalogEntry *entry, AnimaIdlePreview *out)
{
    AnimaPreviewOptions opts;
    const AnimaPreviewAssetInfo *asset = GuiState_CurrentAsset(state);
    if (asset == NULL) return -1;
    GuiState_BuildPreviewOptions(state, &opts);
    opts.animation_index = asset->animation_index;
    return AnimaBackend_LoadNmarAnimationPreviewExt(state->rom_path, entry->dex_id, asset->animation_index, &opts, out);
}
static int LoadNmcrMap(GuiState *state, const PokemonCatalogEntry *entry, AnimaIdlePreview *out)
{
    AnimaPreviewOptions opts;
    const AnimaPreviewAssetInfo *asset = GuiState_CurrentAsset(state);
    if (asset == NULL) return -1;
    GuiState_BuildPreviewOptions(state, &opts);
    opts.map_index = asset->map_index;
    return AnimaBackend_LoadNmcrMapPreviewExt(state->rom_path, entry->dex_id, asset->map_index, &opts, out);
}

int GuiState_TryLoadPreview(GuiState *state, const PokemonCatalogEntry *entry)
{
    if (!state || !entry) return -1;
    state->has_female = AnimaBackend_HasFemaleSprite(state->rom_path, entry->dex_id);
    if (state->gender == 1 && !state->has_female) state->gender = 0;
    GuiState_UnloadPreview(&state->preview);
    if (GuiState_RefreshAssets(state, entry) != 0) {
        GuiState_SetStatus(state, "No preview assets found");
        return -1;
    }

    switch (state->preview_mode) {
        case PREVIEW_GIF:
            GuiState_SetStatus(state, "Loading idle preview...");
            if (LoadPreviewFromBackend(state, entry) == 0) { GuiState_SetStatus(state, "Idle preview loaded"); return 0; }
            GuiState_SetStatus(state, "Failed to load idle preview"); return -1;
        case PREVIEW_SPRITESHEET:
            GuiState_SetStatus(state, "Loading spritesheet...");
            if (LoadPngPreview(state, entry, LoadSpritesheet) == 0) { GuiState_SetStatus(state, "Spritesheet loaded"); return 0; }
            GuiState_SetStatus(state, "Failed to load spritesheet"); return -1;
        case PREVIEW_STATIC_IDLE:
            GuiState_SetStatus(state, "Loading static idle...");
            if (LoadPngPreview(state, entry, LoadStaticIdle) == 0) { GuiState_SetStatus(state, "Static idle loaded"); return 0; }
            GuiState_SetStatus(state, "Failed to load static idle"); return -1;
        case PREVIEW_IDLE_BREAK:
            GuiState_SetStatus(state, "Loading idle break...");
            if (LoadAnimatedPreview(state, entry, LoadBreak) == 0) { GuiState_SetStatus(state, "Idle break loaded"); return 0; }
            GuiState_SetStatus(state, "No idle break found"); return -1;
        case PREVIEW_COMPOSED:
            GuiState_SetStatus(state, "Loading composed animation...");
            if (LoadAnimatedPreview(state, entry, LoadComposed) == 0) { GuiState_SetStatus(state, "Composed animation loaded"); return 0; }
            GuiState_SetStatus(state, "Failed to load composed"); return -1;
        case PREVIEW_NMAR_ANIMATION: {
            const AnimaPreviewAssetInfo *asset = GuiState_CurrentAsset(state);
            GuiState_SetStatus(state, "Loading animation...");
            if (LoadAnimatedPreview(state, entry, LoadNmarAnimation) == 0) {
                GuiState_SetStatus(state, asset != NULL ? Gr_FormatText("%s loaded", asset->display_name) : "Animation loaded");
                return 0;
            }
            GuiState_SetStatus(state, "Failed to load animation"); return -1;
        }
        case PREVIEW_NMCR_MAP: {
            const AnimaPreviewAssetInfo *asset = GuiState_CurrentAsset(state);
            GuiState_SetStatus(state, "Loading map animation...");
            if (LoadAnimatedPreview(state, entry, LoadNmcrMap) == 0) {
                GuiState_SetStatus(state, asset != NULL ? Gr_FormatText("%s loaded", asset->display_name) : "Map animation loaded");
                return 0;
            }
            GuiState_SetStatus(state, "Failed to load map animation"); return -1;
        }
        default: return -1;
    }
}

void GuiState_RunExport(
    GuiState *state, const PokemonCatalogEntry *entry,
    int export_spritesheets, int export_static, int export_gif,
    int export_idle_break, int export_composed)
{
    AnimaExtractOptions options;
    char out_dir[ANIMA_PATH_BUFFER_SIZE];
    char status[GUI_STATUS_CAPACITY];
    if (!state || !entry || !state->rom_ready) return;

    GuiState_BuildOutputDir(entry, out_dir, sizeof(out_dir));
    snprintf(status, sizeof(status), "Extracting #%03d %s...", entry->dex_id, entry->name);
    GuiState_SetStatus(state, status);

    AnimaExtractOptions_Init(&options);
    options.export_spritesheets = export_spritesheets;
    options.export_static_previews = export_static;
    options.export_gifs = export_gif;
    options.export_idle_break_gifs = export_idle_break;
    options.export_composed_gifs = export_composed;
    options.export_json = 1;
    options.form_index = state->form_index;
    options.gif_options.side = state->is_back ? GIF_SIDE_BACK : GIF_SIDE_FRONT;
    options.gif_options.palette = state->is_shiny ? GIF_PALETTE_SHINY : GIF_PALETTE_NORMAL;
    options.gif_options.scale = 4;
    options.gif_options.delay_cs = GUI_GIF_DELAY_DEFAULT_CS;
    options.gif_options.playback_delay_cs = GuiState_GifDelayCs(state);

    int result = AnimaBackend_ExtractPokemon(state->rom_path, entry->dex_id, out_dir, &options);
    if (result == 0) {
        GuiState_TryLoadPreview(state, entry);
        snprintf(status, sizeof(status), "Exported full folder to: %s", out_dir);
    } else {
        snprintf(status, sizeof(status), "Error extracting #%03d %s", entry->dex_id, entry->name);
    }
    GuiState_SetStatus(state, status);
}

void GuiState_ExportDsFiles(GuiState *state, const PokemonCatalogEntry *entry)
{
    AnimaExtractOptions options;
    char out_dir[ANIMA_PATH_BUFFER_SIZE];
    char status[GUI_STATUS_CAPACITY];
    int result;

    if (!state || !entry || !state->rom_ready) return;

    GuiState_BuildOutputDir(entry, out_dir, sizeof(out_dir));
    snprintf(status, sizeof(status), "Extracting DS files for #%03d %s...", entry->dex_id, entry->name);
    GuiState_SetStatus(state, status);

    AnimaExtractOptions_Init(&options);
    options.export_spritesheets = 0;
    options.export_static_previews = 0;
    options.export_gifs = 0;
    options.export_idle_break_gifs = 0;
    options.export_composed_gifs = 0;
    options.export_json = 1;
    options.form_index = state->form_index;

    result = AnimaBackend_ExtractPokemon(state->rom_path, entry->dex_id, out_dir, &options);
    if (result == 0) {
        snprintf(status, sizeof(status), "Exported species DS files to: %s/nds_files", out_dir);
    } else {
        snprintf(status, sizeof(status), "Error extracting DS files for #%03d %s", entry->dex_id, entry->name);
    }
    GuiState_SetStatus(state, status);
}

void GuiState_ExportAllAssets(GuiState *state, const PokemonCatalogEntry *entry)
{
    AnimaExtractOptions options;
    char out_dir[ANIMA_PATH_BUFFER_SIZE];
    char status[GUI_STATUS_CAPACITY];
    int result;

    if (!state || !entry || !state->rom_ready) return;

    GuiState_BuildOutputDir(entry, out_dir, sizeof(out_dir));
    snprintf(status, sizeof(status), "Exporting all assets for #%03d %s...", entry->dex_id, entry->name);
    GuiState_SetStatus(state, status);

    AnimaExtractOptions_Init(&options);
    options.export_spritesheets = 1;
    options.export_static_previews = 1;
    options.export_gifs = 1;
    options.export_idle_break_gifs = 1;
    options.export_composed_gifs = 1;
    options.export_json = 1;
    options.form_index = state->form_index;
    options.gif_options.side = GIF_SIDE_BOTH;
    options.gif_options.palette = GIF_PALETTE_BOTH;
    options.gif_options.scale = 4;
    options.gif_options.delay_cs = GUI_GIF_DELAY_DEFAULT_CS;
    options.gif_options.playback_delay_cs = GuiState_GifDelayCs(state);

    result = AnimaBackend_ExtractPokemon(state->rom_path, entry->dex_id, out_dir, &options);
    if (result == 0) {
        GuiState_TryLoadPreview(state, entry);
        snprintf(status, sizeof(status), "Exported all assets to: %s", out_dir);
    } else {
        snprintf(status, sizeof(status), "Error exporting all assets for #%03d %s", entry->dex_id, entry->name);
    }
    GuiState_SetStatus(state, status);
}

static int GuiState_ExportLoadedPreviewGif(const GuiState *state, const char *out_path)
{
    const GuiPreview *preview;
    RgbaColor palette[256];
    char parent_dir[ANIMA_PATH_BUFFER_SIZE];
    u8 *indices;
    size_t pixel_count;
    size_t i;
    int palette_count;
    int transparent_index;
    int delay_cs;
    char *last_slash;
    int result;
    size_t frame_pixels;

    if (state == NULL || out_path == NULL) {
        return -1;
    }

    preview = &state->preview;
    if (!preview->loaded || preview->frames == NULL ||
        preview->width <= 0 || preview->height <= 0 ||
        preview->frame_count <= 0) {
        return -1;
    }

    snprintf(parent_dir, sizeof(parent_dir), "%s", out_path);
    last_slash = strrchr(parent_dir, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        if (File_MkdirRecursive(parent_dir) != 0) {
            return -1;
        }
    }

    frame_pixels = (size_t)preview->width * (size_t)preview->height;
    pixel_count = frame_pixels * (size_t)preview->frame_count;
    if (frame_pixels == 0 ||
        pixel_count / frame_pixels != (size_t)preview->frame_count) {
        return -1;
    }

    indices = malloc(pixel_count);
    if (indices == NULL) {
        return -1;
    }

    palette_count = 0;
    transparent_index = -1;

    for (i = 0; i < pixel_count; i++) {
        const unsigned char *rgba;
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
        int found;
        int c;

        rgba = preview->frames + (i * 4u);
        r = rgba[0];
        g = rgba[1];
        b = rgba[2];
        a = rgba[3];

        if (a == 0) {
            if (transparent_index < 0) {
                if (palette_count >= 256) {
                    free(indices);
                    return -1;
                }
                transparent_index = palette_count;
                palette[palette_count].r = 0;
                palette[palette_count].g = 0;
                palette[palette_count].b = 0;
                palette[palette_count].a = 0;
                palette_count++;
            }
            indices[i] = (u8)transparent_index;
            continue;
        }

        found = -1;
        for (c = 0; c < palette_count; c++) {
            if (palette[c].r == r &&
                palette[c].g == g &&
                palette[c].b == b &&
                palette[c].a == a) {
                found = c;
                break;
            }
        }

        if (found < 0) {
            if (palette_count >= 256) {
                free(indices);
                return -1;
            }
            found = palette_count;
            palette[palette_count].r = r;
            palette[palette_count].g = g;
            palette[palette_count].b = b;
            palette[palette_count].a = a;
            palette_count++;
        }

        indices[i] = (u8)found;
    }

    delay_cs = GuiState_GifDelayCs(state);
    result = Gif_WriteIndexed(
        out_path,
        indices,
        preview->frame_count,
        preview->width,
        preview->height,
        palette,
        palette_count,
        transparent_index,
        delay_cs,
        0,
        4
    );

    free(indices);
    return result;
}

void GuiState_ExportCurrent(GuiState *state, const PokemonCatalogEntry *entry)
{
    char out_path[ANIMA_PATH_BUFFER_SIZE];
    char species_name[128], form_name[128];
    char side_name[32], gender_name[32], palette_name[32], type_name[32];
    char out_dir[ANIMA_PATH_BUFFER_SIZE], folder_name[64];
    const char *ext = ".png";

    if (!state || !entry || !state->rom_ready) return;

    GuiState_SanitizePath(entry->name, species_name, sizeof(species_name));
    GuiState_SanitizePath(GuiState_GetFormName(state, entry->dex_id, state->form_index), form_name, sizeof(form_name));
    snprintf(side_name, sizeof(side_name), "%s", state->is_back ? "back" : "front");
    snprintf(gender_name, sizeof(gender_name), "%s", state->gender == 1 ? "female" : "male");
    snprintf(palette_name, sizeof(palette_name), "%s", state->is_shiny ? "shiny" : "normal");

    const AnimaPreviewAssetInfo *asset = GuiState_CurrentAsset(state);
    GuiPreviewMode export_mode = asset != NULL ? (GuiPreviewMode)asset->type : state->preview_mode;

    GuiState_BuildOutputDir(entry, out_dir, sizeof(out_dir));

    switch (export_mode) {
        case PREVIEW_GIF:
            snprintf(type_name, sizeof(type_name), "idle");
            snprintf(folder_name, sizeof(folder_name), "animated_idle_gif");
            ext = ".gif";
            break;
        case PREVIEW_SPRITESHEET:
            snprintf(type_name, sizeof(type_name), "spritesheet");
            snprintf(folder_name, sizeof(folder_name), "spritesheet_png");
            break;
        case PREVIEW_STATIC_IDLE:
            snprintf(type_name, sizeof(type_name), "static");
            snprintf(folder_name, sizeof(folder_name), "static_png");
            break;
        case PREVIEW_IDLE_BREAK:
            snprintf(type_name, sizeof(type_name), "break");
            snprintf(folder_name, sizeof(folder_name), "idle_break_gif");
            ext = ".gif";
            break;
        case PREVIEW_COMPOSED:
            snprintf(type_name, sizeof(type_name), "composed");
            snprintf(folder_name, sizeof(folder_name), "composed_gif");
            ext = ".gif";
            break;
        case PREVIEW_NMAR_ANIMATION: {
            char label_slug[64];
            GuiState_SanitizePath(asset != NULL && asset->label[0] ? asset->label : "animation", label_slug, sizeof(label_slug));
            snprintf(type_name, sizeof(type_name), "animation_%02d_%s", asset != NULL ? asset->animation_index : 0, label_slug);
            snprintf(folder_name, sizeof(folder_name), "animation_gif");
            ext = ".gif";
            break;
        }
        case PREVIEW_NMCR_MAP:
            snprintf(type_name, sizeof(type_name), "map_%02d", asset != NULL ? asset->map_index : 0);
            snprintf(folder_name, sizeof(folder_name), "animation_gif");
            ext = ".gif";
            break;
        default:
            snprintf(type_name, sizeof(type_name), "unknown");
            snprintf(folder_name, sizeof(folder_name), "misc_assets");
            break;
    }

    snprintf(out_path, sizeof(out_path), "%s/%s/%s_%s_%s_%s_%s_%s%s",
             out_dir, folder_name, species_name, form_name, gender_name, palette_name, side_name, type_name, ext);

    GuiState_SetStatus(state, "Saving custom asset preview...");
    AnimaPreviewOptions opts; GuiState_BuildPreviewOptions(state, &opts);
    if (asset != NULL) {
        opts.animation_index = asset->animation_index;
        opts.map_index = asset->map_index;
    }
    int ret;
    if (ext[1] == 'g') {
        ret = GuiState_ExportLoadedPreviewGif(state, out_path);
        if (ret != 0) {
            ret = AnimaBackend_ExportCurrentAsset(state->rom_path, entry->dex_id, (int)export_mode, &opts, out_path);
        }
    } else {
        ret = AnimaBackend_ExportCurrentAsset(state->rom_path, entry->dex_id, (int)export_mode, &opts, out_path);
    }
    GuiState_SetStatus(state, ret == 0 ? Gr_FormatText("WYSIWYG Export OK (%dcs): %s", GuiState_GifDelayCs(state), out_path) : "Export failed!");
}
