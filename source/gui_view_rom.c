#include "gui_view_rom.h"
#include "gui_widgets.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define IS_WINDOWS 1
#define SYSTEM_ROOT "C:/"
#else
#define IS_WINDOWS 0
#define SYSTEM_ROOT "/"
#endif

#define MAX_EXPLORER_ENTRIES 1024

typedef struct {
    char name[256];
    int is_dir;
} ExplorerEntry;

static ExplorerEntry explorer_entries[MAX_EXPLORER_ENTRIES];
static int explorer_entry_count = 0;

static char current_dir[4096] = "";
static char explorer_search[256] = "";
static int explorer_search_active = 0;
static int explorer_scroll = 0;
static int selected_idx = -1;
static int explorer_initialized = 0;

static float last_click_time = 0.0f;
static int last_clicked_row = -1;

static GrColor Gc(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    GrColor c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

static int CompareExplorerEntries(const void *a, const void *b)
{
    const ExplorerEntry *ea = (const ExplorerEntry *)a;
    const ExplorerEntry *eb = (const ExplorerEntry *)b;

    // ".." always goes first
    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return 1;

    // Directories go before files
    if (ea->is_dir != eb->is_dir) {
        return eb->is_dir - ea->is_dir;
    }

    // Alphabetical sort (case-insensitive)
    const char *sa = ea->name;
    const char *sb = eb->name;
    while (*sa && *sb) {
        int ca = tolower((unsigned char)*sa);
        int cb = tolower((unsigned char)*sb);
        if (ca != cb) return ca - cb;
        sa++;
        sb++;
    }
    return tolower((unsigned char)*sa) - tolower((unsigned char)*sb);
}

static void RefreshExplorerDir(const char *path)
{
    GrFilePathList files = Gr_LoadDirectoryFiles(path);

    explorer_entry_count = 0;

    // Always add ".." first if not at root
    if (strcmp(path, SYSTEM_ROOT) != 0 && strcmp(path, "/") != 0) {
        strcpy(explorer_entries[explorer_entry_count].name, "..");
        explorer_entries[explorer_entry_count].is_dir = 1;
        explorer_entry_count++;
    }

    for (unsigned int i = 0; i < files.count && explorer_entry_count < MAX_EXPLORER_ENTRIES; i++) {
        const char *full_path = files.paths[i];
        const char *name = Gr_GetFileName(full_path);

        // Skip "." and ".." (we added ".." manually)
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }

        // Filter out hidden files/folders (starting with .)
        if (name[0] == '.' && strcmp(name, "..") != 0) {
            continue;
        }

        int is_dir = !Gr_IsPathFile(full_path);

        strncpy(explorer_entries[explorer_entry_count].name, name, sizeof(explorer_entries[explorer_entry_count].name) - 1);
        explorer_entries[explorer_entry_count].name[sizeof(explorer_entries[explorer_entry_count].name) - 1] = '\0';
        explorer_entries[explorer_entry_count].is_dir = is_dir;

        explorer_entry_count++;
    }
    Gr_UnloadDirectoryFiles(files);

    // Sort the entries
    if (explorer_entry_count > 0) {
        int start_idx = (strcmp(explorer_entries[0].name, "..") == 0) ? 1 : 0;
        int count_to_sort = explorer_entry_count - start_idx;
        if (count_to_sort > 0) {
            qsort(&explorer_entries[start_idx], count_to_sort, sizeof(ExplorerEntry), CompareExplorerEntries);
        }
    }
}

static int CaseInsensitiveContains(const char *haystack, const char *needle)
{
    if (!needle || !*needle) return 1;
    if (!haystack) return 0;

    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);
    if (needle_len > haystack_len) return 0;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

static void ExplorerGoUp(void)
{
    if (strcmp(current_dir, SYSTEM_ROOT) == 0 || strcmp(current_dir, "/") == 0) return;
    const char *prev = Gr_GetPrevDirectoryPath(current_dir);
    if (prev) {
        strncpy(current_dir, prev, sizeof(current_dir) - 1);
        current_dir[sizeof(current_dir) - 1] = '\0';
    } else {
        char *last_slash = strrchr(current_dir, '/');
        if (!last_slash) last_slash = strrchr(current_dir, '\\');
        if (last_slash) {
            if (last_slash == current_dir) {
                strcpy(current_dir, SYSTEM_ROOT);
            } else {
                *last_slash = '\0';
            }
        }
    }
    RefreshExplorerDir(current_dir);
    selected_idx = -1;
    explorer_scroll = 0;
}

static void ExplorerNavigateTo(const char *subdir)
{
    char temp[4096];
    int len = strlen(current_dir);
    if (len > 0 && (current_dir[len - 1] == '/' || current_dir[len - 1] == '\\')) {
        snprintf(temp, sizeof(temp), "%s%s", current_dir, subdir);
    } else {
        snprintf(temp, sizeof(temp), "%s/%s", current_dir, subdir);
    }
    strncpy(current_dir, temp, sizeof(current_dir) - 1);
    current_dir[sizeof(current_dir) - 1] = '\0';
    RefreshExplorerDir(current_dir);
    selected_idx = -1;
    explorer_scroll = 0;
}

void GuiView_DrawRomScreen(GuiState *state)
{
    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();

    // 1. Initialize explorer state if not done
    if (!explorer_initialized) {
        const char *cwd = Gr_GetWorkingDirectory();
        if (cwd) {
            strncpy(current_dir, cwd, sizeof(current_dir) - 1);
            current_dir[sizeof(current_dir) - 1] = '\0';
        } else {
            strcpy(current_dir, ".");
        }
        RefreshExplorerDir(current_dir);
        explorer_initialized = 1;
    }

    // 2. Filter list based on search
    int filtered_indices[MAX_EXPLORER_ENTRIES];
    int filtered_count = 0;
    for (int i = 0; i < explorer_entry_count; i++) {
        if (CaseInsensitiveContains(explorer_entries[i].name, explorer_search)) {
            filtered_indices[filtered_count++] = i;
        }
    }

    // 3. Clear and Draw Top Header Bar
    Gr_Clear(Gc(15, 23, 42, 255));
    Gr_DrawRect((GrRect){0, 0, (float)Gr_GetScreenW(), 72}, Gc(10, 15, 30, 255));
    Gr_DrawRect((GrRect){0, 71, (float)Gr_GetScreenW(), 1}, Gc(6, 182, 212, 255));
    Gr_DrawText("AnimaEngine Studio", 28, 20, 22, Gc(255, 255, 255, 255));
    Gr_DrawText("NDS ROM Selection & File Browser", 260, 26, 14, Gc(148, 163, 184, 255));

    // 4. Sidebar Panel (x=20, y=92, width=280, height=608)
    GrRect sidebar_rect = {20, 92, 280, 608};
    Gr_DrawRectRounded(sidebar_rect, 0.03f, Gc(30, 41, 59, 150));
    Gr_DrawRectRoundedLines(sidebar_rect, 0.03f, Gc(71, 85, 105, 150));

    Gr_DrawText("QUICK DIRECTORIES", 36, 112, 11, Gc(148, 163, 184, 255));

    // Quick Shortcut: CWD
    if (Gr_DrawButton((GrRect){36, 132, 248, 36}, "📍 Current Folder", 1)) {
        const char *cwd = Gr_GetWorkingDirectory();
        if (cwd) {
            strncpy(current_dir, cwd, sizeof(current_dir) - 1);
            current_dir[sizeof(current_dir) - 1] = '\0';
        } else {
            strcpy(current_dir, ".");
        }
        RefreshExplorerDir(current_dir);
        selected_idx = -1;
        explorer_scroll = 0;
    }

    // Quick Shortcut: Home (cross-platform home directories support)
    const char *home_env = getenv("HOME");
    if (!home_env) {
        home_env = getenv("USERPROFILE");
    }
    if (Gr_DrawButton((GrRect){36, 178, 248, 36}, "🏠 Home Directory", home_env != NULL)) {
        if (home_env) {
            strncpy(current_dir, home_env, sizeof(current_dir) - 1);
            current_dir[sizeof(current_dir) - 1] = '\0';
            RefreshExplorerDir(current_dir);
            selected_idx = -1;
            explorer_scroll = 0;
        }
    }

    // Quick Shortcut: Root System
    if (Gr_DrawButton((GrRect){36, 224, 248, 36}, IS_WINDOWS ? "💻 System Root ( C:/ )" : "💻 Root System ( / )", 1)) {
        strcpy(current_dir, SYSTEM_ROOT);
        RefreshExplorerDir(current_dir);
        selected_idx = -1;
        explorer_scroll = 0;
    }

    Gr_DrawRect((GrRect){36, 280, 248, 1}, Gc(51, 65, 85, 255));

    // File search/filter
    Gr_DrawText("SEARCH / FILTER", 36, 300, 11, Gc(148, 163, 184, 255));
    explorer_search_active = Gr_DrawTextBox((GrRect){36, 320, 248, 40}, explorer_search, sizeof(explorer_search), explorer_search_active, "Search in folder...");

    Gr_DrawRect((GrRect){36, 380, 248, 1}, Gc(51, 65, 85, 255));

    // Drag and drop zone
    Gr_DrawText("DRAG & DROP ZONE", 36, 400, 11, Gc(148, 163, 184, 255));
    GrRect drop_zone = {36, 420, 248, 140};
    Gr_DrawRectRounded(drop_zone, 0.08f, Gc(15, 23, 42, 100));
    Gr_DrawRectRoundedLines(drop_zone, 0.08f, Gc(71, 85, 105, 120));
    Gr_DrawText("Drop ROM file (.nds)", 60, 455, 13, Gc(148, 163, 184, 255));
    Gr_DrawText("onto this window", 76, 480, 13, Gc(148, 163, 184, 255));
    Gr_DrawText("to load immediately", 64, 505, 13, Gc(148, 163, 184, 255));

    // 5. Main File Explorer Panel (x=320, y=92, width=780, height=508)
    GrRect explorer_rect = {320, 92, 780, 508};
    Gr_DrawRectRounded(explorer_rect, 0.02f, Gc(30, 41, 59, 100));
    Gr_DrawRectRoundedLines(explorer_rect, 0.02f, Gc(71, 85, 105, 255));

    // Current Folder header
    Gr_DrawRectRounded((GrRect){321, 93, 778, 46}, 0.02f, Gc(15, 23, 42, 180));
    Gr_DrawRect((GrRect){320, 139, 780, 1}, Gc(71, 85, 105, 255));

    Gr_ScissorBegin(336, 93, 748, 46);
    Gr_DrawText(Gr_FormatText("📁  %s", current_dir), 336, 108, 14, Gc(6, 182, 212, 255));
    Gr_ScissorEnd();

    // 6. Scrollable File List Row Rendering
    GrRect list_rect = {320, 140, 780, 460};
    int visible_rows = 12;
    float row_h = 38.0f;

    // Handle mouse wheel scrolling
    if (Gr_CheckPointInRect(list_rect, mx, my)) {
        int w = Gr_GetWheel();
        if (w > 0) explorer_scroll -= 3;
        else if (w < 0) explorer_scroll += 3;
    }
    if (explorer_scroll < 0) explorer_scroll = 0;
    if (explorer_scroll > filtered_count - visible_rows) explorer_scroll = filtered_count - visible_rows;
    if (explorer_scroll < 0) explorer_scroll = 0;

    Gr_ScissorBegin((int)list_rect.x, (int)list_rect.y, (int)list_rect.width, (int)list_rect.height);
    for (int i = 0; i < visible_rows && i + explorer_scroll < filtered_count; i++) {
        int idx = filtered_indices[i + explorer_scroll];
        ExplorerEntry entry = explorer_entries[idx];
        GrRect row = {list_rect.x + 1, list_rect.y + 1 + i * row_h, list_rect.width - 2, row_h};

        int hovered = Gr_CheckPointInRect(row, mx, my);
        int selected = (selected_idx == idx);

        if (selected) {
            Gr_DrawRect(row, Gc(124, 58, 237, 100)); // Highlight selected file
        } else if (hovered) {
            Gr_DrawRect(row, Gc(30, 41, 59, 180));
        }

        Gr_DrawRect((GrRect){row.x, row.y + row_h - 1, row.width, 1}, Gc(51, 65, 85, 100));

        // Draw Badge for File/Directory Type
        GrRect badge = {row.x + 14, row.y + 7, 52, 24};
        GrColor badge_bg;
        GrColor badge_text_color = {255, 255, 255, 255};
        const char *badge_text = "FILE";

        int is_rom = (!entry.is_dir && strstr(entry.name, ".nds") != NULL);

        if (entry.is_dir) {
            badge_bg = Gc(99, 102, 241, 255); // Indigo for directories
            badge_text = "DIR";
        } else if (is_rom) {
            badge_bg = Gc(6, 182, 212, 255); // Cyan for NDS ROMs
            badge_text = "ROM";
        } else {
            badge_bg = Gc(71, 85, 105, 255); // Slate gray for other files
        }

        Gr_DrawRectRounded(badge, 0.2f, badge_bg);
        GrRect badge_m = Gr_MeasureText(badge_text, 11);
        Gr_DrawText(badge_text, badge.x + (badge.width - badge_m.width)*0.5f, badge.y + (badge.height - badge_m.height)*0.5f, 11, badge_text_color);

        // Filename
        GrColor text_color = (is_rom || entry.is_dir) ? (GrColor){255, 255, 255, 255} : Gc(148, 163, 184, 255);
        Gr_ScissorBegin((int)(row.x + 84), (int)row.y, (int)(row.width - 94), (int)row.height);
        Gr_DrawText(entry.name, row.x + 84, row.y + 11, 15, text_color);
        Gr_ScissorEnd();

        // Left vertical cyan bar for selection
        if (selected) {
            Gr_DrawRect((GrRect){row.x, row.y, 4, row.height}, Gc(6, 182, 212, 255));
        }

        // Navigation / Selection
        if (hovered && Gr_MousePressed(0)) {
            float now = Gr_GetTime();
            int is_double_click = (last_clicked_row == idx && (now - last_click_time) < 0.3f);
            last_click_time = now;
            last_clicked_row = idx;

            if (entry.is_dir) {
                if (strcmp(entry.name, "..") == 0) {
                    ExplorerGoUp();
                } else {
                    ExplorerNavigateTo(entry.name);
                }
                break;
            } else {
                int len = strlen(current_dir);
                if (len > 0 && (current_dir[len - 1] == '/' || current_dir[len - 1] == '\\')) {
                    snprintf(state->rom_path, sizeof(state->rom_path), "%s%s", current_dir, entry.name);
                } else {
                    snprintf(state->rom_path, sizeof(state->rom_path), "%s/%s", current_dir, entry.name);
                }
                selected_idx = idx;

                if (is_double_click && is_rom) {
                    GuiState_LoadAndValidateRom(state, state->rom_path);
                }
            }
        }
    }
    Gr_ScissorEnd();

    // Scrollbar Thumb
    if (filtered_count > visible_rows) {
        float track_h = list_rect.height - 10.0f;
        float thumb_h = track_h * ((float)visible_rows / (float)filtered_count);
        float thumb_y = list_rect.y + 5.0f;
        if (thumb_h < 18.0f) thumb_h = 18.0f;
        int max_scroll = filtered_count - visible_rows;
        if (max_scroll > 0) {
            thumb_y += (track_h - thumb_h) * ((float)explorer_scroll / (float)max_scroll);
        }
        Gr_DrawRect((GrRect){list_rect.x + list_rect.width - 6.0f, thumb_y, 4.0f, thumb_h}, Gc(100, 116, 139, 220));
    }

    // 7. Bottom Raw Path Input & Actions
    GrRect path_box = {320, 612, 580, 40};
    GrRect use_button = {920, 612, 180, 40};

    state->rom_text_active = Gr_DrawTextBox(path_box, state->rom_path, sizeof(state->rom_path), state->rom_text_active, "Absolute path to .nds ROM file");

    int rom_exists = (state->rom_path[0] != '\0' && Gr_FileExists(state->rom_path));
    if (Gr_DrawButton(use_button, "Load ROM", rom_exists)) {
        GuiState_LoadAndValidateRom(state, state->rom_path);
    }

    // Status Message under input box
    Gr_DrawText(state->status, 324, 668, 14, Gc(6, 182, 212, 255));

    // 8. Drag and Drop file handler
    if (Gr_FileDropped()) {
        int n = Gr_DropCount();
        if (n > 0) {
            const char *p = Gr_DropPath(0);
            if (p) {
                GuiState_CopyText(state->rom_path, sizeof(state->rom_path), p);

                // Auto-navigate explorer to parent folder of dropped file
                char parent_dir[4096];
                strncpy(parent_dir, p, sizeof(parent_dir) - 1);
                parent_dir[sizeof(parent_dir) - 1] = '\0';
                char *last_sl = strrchr(parent_dir, '/');
                if (!last_sl) last_sl = strrchr(parent_dir, '\\');
                if (last_sl) {
                    if (last_sl == parent_dir) {
                        strcpy(current_dir, SYSTEM_ROOT);
                    } else {
                        *last_sl = '\0';
                        strcpy(current_dir, parent_dir);
                    }
                    RefreshExplorerDir(current_dir);
                    explorer_scroll = 0;
                    selected_idx = -1;
                }
            }
        }
        Gr_DropFinish();
        if (Gr_FileExists(state->rom_path)) {
            GuiState_LoadAndValidateRom(state, state->rom_path);
        }
    }
}
