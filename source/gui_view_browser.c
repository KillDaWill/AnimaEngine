#include "gui_view_browser.h"
#include "gui_widgets.h"
#include <math.h>

#define GR_WHITE ((GrColor){255,255,255,255})

static GrColor Gc(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { GrColor c; c.r=r;c.g=g;c.b=b;c.a=a; return c; }

static void DrawPokemonRow(GrRect row, const PokemonCatalogEntry *entry, int selected, int hovered)
{
    GrColor fill = selected ? Gc(124,58,237,100) : hovered ? Gc(30,41,59,120) : Gc(0,0,0,0);
    Gr_DrawRect(row, fill);
    Gr_DrawRect((GrRect){row.x, row.y+row.height-1, row.width, 1}, Gc(51,65,85,100));
    Gr_DrawText(Gr_FormatText("#%03d", entry->dex_id), row.x+14, row.y+11, 14, Gc(148,163,184,255));
    Gr_DrawText(entry->name, row.x+82, row.y+9, 16, GR_WHITE);
    if (selected) Gr_DrawRect((GrRect){row.x, row.y, 4, row.height}, Gc(6,182,212,255));
}

static void DrawPreviewPanel(GrRect bounds, GuiPreview *preview)
{
    Gr_DrawRectRounded(bounds, 0.04f, Gc(30,41,59,150));
    Gr_DrawRectRoundedLines(bounds, 0.04f, Gc(71,85,105,255));

    if (!preview || !preview->loaded) {
        const char *t = "No preview active"; int fs = 18;
        GrRect m = Gr_MeasureText(t, fs);
        Gr_DrawText(t, bounds.x + (bounds.width - m.width)*0.5f, bounds.y + (bounds.height - m.height)*0.5f, fs, Gc(148,163,184,255));
        return;
    }

    int content_x = preview->content_width > 0 ? preview->content_x : 0;
    int content_y = preview->content_height > 0 ? preview->content_y : 0;
    int content_w = preview->content_width > 0 ? preview->content_width : preview->width;
    int content_h = preview->content_height > 0 ? preview->content_height : preview->height;
    float top_off = 58.0f;
    float scale = fminf((bounds.width - 32.0f) / (float)content_w, (bounds.height - top_off - 24.0f) / (float)content_h);
    if (scale > 5.0f) scale = 5.0f;
    float dw = (float)content_w * scale, dh = (float)content_h * scale;
    GrRect src = {(float)content_x,(float)content_y,(float)content_w,(float)content_h};
    GrRect dst = {bounds.x + (bounds.width - dw)*0.5f, bounds.y + top_off + (bounds.height - top_off - dh)*0.5f, dw, dh};

    int cs = 24, rows = (int)(bounds.height/cs)+2, cols = (int)(bounds.width/cs)+2;
    Gr_ScissorBegin((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            Gr_DrawRect((GrRect){bounds.x + c*cs, bounds.y + r*cs, cs, cs}, ((r+c)%2==0) ? Gc(15,23,42,255) : Gc(30,41,59,255));
    Gr_ScissorEnd();
    Gr_DrawRectRoundedLines(bounds, 0.04f, Gc(71,85,105,255));
    Gr_DrawTexture(&preview->texture, src, dst, GR_WHITE);
}

static void DrawGifSpeedControl(GuiState *state, GrRect bounds, int enabled)
{
    int delay_cs = GuiState_GifDelayCs(state);
    int speed_percent = (GUI_GIF_DELAY_DEFAULT_CS * 100 + delay_cs / 2) / delay_cs;
    float button_w = bounds.height;
    float gap = 8.0f;
    float value_w = bounds.width - (button_w * 2.0f) - (gap * 2.0f);
    GrRect minus;
    GrRect value;
    GrRect plus;

    if (value_w < 64.0f) value_w = 64.0f;
    minus = (GrRect){bounds.x, bounds.y, button_w, bounds.height};
    value = (GrRect){bounds.x + button_w + gap, bounds.y, value_w, bounds.height};
    plus = (GrRect){value.x + value.width + gap, bounds.y, button_w, bounds.height};

    if (Gr_DrawButton(minus, "-", enabled && delay_cs < GUI_GIF_DELAY_MAX_CS)) {
        GuiState_SetGifDelayCs(state, delay_cs + 1);
        delay_cs = GuiState_GifDelayCs(state);
        speed_percent = (GUI_GIF_DELAY_DEFAULT_CS * 100 + delay_cs / 2) / delay_cs;
    }

    Gr_DrawRectRounded(value, 0.15f, Gc(15,23,42,180));
    Gr_DrawRectRoundedLines(value, 0.15f, Gc(71,85,105,255));
    Gr_DrawTextInBox(value, Gr_FormatText("%d%% / %dcs", speed_percent, delay_cs), 14, enabled ? GR_WHITE : Gc(100,116,139,255));

    if (Gr_DrawButton(plus, "+", enabled && delay_cs > GUI_GIF_DELAY_MIN_CS)) {
        GuiState_SetGifDelayCs(state, delay_cs - 1);
    }
}

static int PreviewModeUsesGifSpeed(GuiPreviewMode mode)
{
    return mode == PREVIEW_GIF ||
        mode == PREVIEW_IDLE_BREAK ||
        mode == PREVIEW_COMPOSED ||
        mode == PREVIEW_NMAR_ANIMATION ||
        mode == PREVIEW_NMCR_MAP;
}

void GuiView_DrawMainScreen(GuiState *state)
{
    const PokemonCatalogEntry *entries;
    int entry_count;
    int matches[700], match_count = 0;
    int i, visible_rows;
    const PokemonCatalogEntry *selected;

    entries = PokemonCatalog_GetEntries(&entry_count);
    for (i = 0; i < entry_count && match_count < 700; i++)
        if (GuiState_EntryMatchesQuery(&entries[i], state->search)) matches[match_count++] = i;

    Gr_Clear(Gc(15,23,42,255));
    Gr_DrawRect((GrRect){0,0,(float)Gr_GetScreenW(),64}, Gc(10,15,30,255));
    Gr_DrawRect((GrRect){0,63,(float)Gr_GetScreenW(),1}, Gc(6,182,212,255));
    Gr_DrawText("AnimaEngine Studio", 28, 18, 22, GR_WHITE);
    Gr_DrawText(GuiState_BaseName(state->rom_path), 250, 22, 14, Gc(6,182,212,255));

    GrRect search_box = {20,84,320,40};
    GrRect list_rect  = {20,136,320,554};
    GrRect details_rect = {360,84,740,606};
    GrRect preview_rect = {380,150,480,380};

    state->search_active = Gr_DrawTextBox(search_box, state->search, sizeof(state->search), state->search_active, "Search species name or number...");

    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();
    visible_rows = (int)(list_rect.height / 38.0f);
    if (Gr_CheckPointInRect(list_rect, mx, my)) { int w = Gr_GetWheel(); if (w > 0) state->list_scroll -= 3; else if (w < 0) state->list_scroll += 3; }
    if (state->list_scroll < 0) state->list_scroll = 0;
    if (state->list_scroll > match_count - visible_rows) state->list_scroll = match_count - visible_rows;
    if (state->list_scroll < 0) state->list_scroll = 0;

    Gr_DrawRectRounded(list_rect, 0.03f, Gc(30,41,59,150));
    Gr_DrawRectRoundedLines(list_rect, 0.03f, Gc(71,85,105,150));
    Gr_ScissorBegin((int)list_rect.x, (int)list_rect.y, (int)list_rect.width, (int)list_rect.height);
    for (i = 0; i < visible_rows && i + state->list_scroll < match_count; i++) {
        int idx = matches[i + state->list_scroll];
        const PokemonCatalogEntry *entry = &entries[idx];
        GrRect row = {list_rect.x+1, list_rect.y+1 + i*38.0f, list_rect.width-2, 38};
        if (Gr_CheckPointInRect(row, mx, my) && Gr_MousePressed(0)) {
            state->selected_dex_id = entry->dex_id;
            state->form_index = 0;
            state->form_dropdown_scroll = 0;
            GuiState_TryLoadPreview(state, entry);
        }
        DrawPokemonRow(row, entry, entry->dex_id == state->selected_dex_id, Gr_CheckPointInRect(row, mx, my));
    }
    Gr_ScissorEnd();

    selected = GuiState_SelectedEntry(state->selected_dex_id);
    Gr_DrawRectRounded(details_rect, 0.02f, Gc(30,41,59,100));
    Gr_DrawRectRoundedLines(details_rect, 0.02f, Gc(71,85,105,255));
    Gr_DrawText(Gr_FormatText("#%03d", selected->dex_id), 380, 106, 20, Gc(6,182,212,255));
    Gr_DrawText(selected->name, 450, 100, 28, GR_WHITE);

    DrawPreviewPanel(preview_rect, &state->preview);

    float opt_x = 880, opt_w = 200;
    float opt_y = 150;
    float form_dropdown_y = 0.0f;
    int form_count = 0;
    int form_selector_active = 0;
    int export_controls_enabled = 0;

    Gr_DrawText("PREVIEW SIDE", (int)opt_x, (int)opt_y, 11, Gc(148,163,184,255));
    const char *side_opts[] = {"Front View", "Back View"};
    int side_clicked = Gr_DrawSegmentedControl((GrRect){opt_x,opt_y+18,opt_w,32}, side_opts, 2, state->is_back ? 1 : 0);
    if (side_clicked >= 0) { state->is_back = side_clicked; GuiState_TryLoadPreview(state, selected); }
    opt_y += 62;

    if (state->has_female) {
        Gr_DrawText("GENDER GROUPING", (int)opt_x, (int)opt_y, 11, Gc(148,163,184,255));
        const char *gender_opts[] = {"Male", "Female"};
        int g = Gr_DrawSegmentedControl((GrRect){opt_x,opt_y+18,opt_w,32}, gender_opts, 2, state->gender);
        if (g >= 0) { state->gender = g; GuiState_TryLoadPreview(state, selected); }
        opt_y += 62;
    }

    Gr_DrawCustomSwitch(opt_x, opt_y+8, 54, 26, state->is_shiny, "Shiny Variant");
    GrRect switch_area = {opt_x, opt_y+8, 54+140, 26};
    if (!state->is_shiny && Gr_CheckPointInRect(switch_area, mx, my) && Gr_MousePressed(0)) { state->is_shiny = 1; GuiState_TryLoadPreview(state, selected); }
    else if (state->is_shiny && Gr_CheckPointInRect(switch_area, mx, my) && Gr_MousePressed(0)) { state->is_shiny = 0; GuiState_TryLoadPreview(state, selected); }
    opt_y += 58;

    form_count = GuiState_GetFormCount(state, selected->dex_id);
    if (form_count > 1) {
        Gr_DrawText("SPECIES FORM BLOCK", (int)opt_x, (int)opt_y, 11, Gc(148,163,184,255));
        form_dropdown_y = opt_y + 18;
        opt_y += 50;
    } else {
        state->form_dropdown_open = 0;
    }

    form_selector_active = (form_count > 1 && state->form_dropdown_open);
    export_controls_enabled = state->rom_ready && !form_selector_active;

    if (PreviewModeUsesGifSpeed(state->preview_mode)) {
        Gr_DrawText("GIF SPEED", (int)opt_x, (int)opt_y, 11, Gc(148,163,184,255));
        DrawGifSpeedControl(state, (GrRect){opt_x,opt_y+18,opt_w,32}, export_controls_enabled);
        opt_y += 62;
    }

    Gr_DrawText("EXPORT", 380, 550, 11, Gc(148,163,184,255));
    if (Gr_DrawButton((GrRect){380,570,220,40}, "Export Current Preview", export_controls_enabled)) GuiState_ExportCurrent(state, selected);
    if (Gr_DrawButton((GrRect){620,570,220,40}, "Export DS Files", export_controls_enabled)) GuiState_ExportDsFiles(state, selected);
    if (Gr_DrawButton((GrRect){860,570,220,40}, "Export All Assets", export_controls_enabled)) GuiState_ExportAllAssets(state, selected);

    Gr_DrawText(state->status, 380, 662, 14, Gc(6,182,212,255));

    /* Form dropdown (rendered last to overlay properly) */
    if (form_count > 1) {
        const char *form_items[32];
        for (int f = 0; f < form_count && f < 32; f++) form_items[f] = GuiState_GetFormName(state, selected->dex_id, f);
        int ai = state->form_index; if (ai >= form_count) { ai = 0; state->form_index = 0; }
        int drop_open = state->form_dropdown_open;
        if (Gr_DrawCustomDropdownScrollable(
                (GrRect){opt_x,form_dropdown_y,opt_w,32},
                "Select Form",
                form_items,
                form_count,
                &ai,
                &drop_open,
                &state->form_dropdown_scroll,
                6
            ) >= 0) {
            state->form_index = ai; GuiState_TryLoadPreview(state, selected);
        }
        state->form_dropdown_open = drop_open;
        if (drop_open) state->asset_dropdown_open = 0;
    } else state->form_dropdown_open = 0;

    /* Asset preview mode dropdown */
    const char *asset_names[ANIMA_MAX_PREVIEW_ASSETS];
    int asset_count;
    int am;
    int ao;

    if (state->asset_count <= 0) {
        GuiState_RefreshAssets(state, selected);
    }
    asset_count = state->asset_count;
    if (asset_count > ANIMA_MAX_PREVIEW_ASSETS) asset_count = ANIMA_MAX_PREVIEW_ASSETS;
    for (int a = 0; a < asset_count; a++) {
        asset_names[a] = state->assets[a].display_name;
    }
    am = state->selected_asset;
    if (am < 0 || am >= asset_count) am = 0;
    ao = state->asset_dropdown_open;
    Gr_DrawText("PREVIEW ASSET TYPE", 380, 150, 11, Gc(148,163,184,255));
    if (Gr_DrawCustomDropdownScrollable((GrRect){380,168,480,32}, "Select Asset", asset_names, asset_count, &am, &ao, &state->asset_dropdown_scroll, 8) >= 0) {
        state->selected_asset = am;
        state->preview_mode = (GuiPreviewMode)state->assets[am].type;
        GuiState_TryLoadPreview(state, selected);
    }
    state->asset_dropdown_open = ao;
    if (ao) state->form_dropdown_open = 0;
}
