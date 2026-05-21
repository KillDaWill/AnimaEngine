#include "gui_app.h"
#include "gui_platform.h"
#include "gui_state.h"
#include "gui_view_rom.h"
#include "gui_view_browser.h"
#include <stdlib.h>

int GuiApp_Run(void)
{
    GuiState *state;

    state = malloc(sizeof(*state));
    if (state == NULL) {
        return 1;
    }
    GuiState_Init(state);

    Gr_InitWindow(1120, 720, "AnimaEngine Sprite Scraper");
    Gr_SetMinSize(1120, 720);
    Gr_SetTargetFPS(60);

    while (!Gr_WindowShouldClose()) {
        GuiState_UpdatePreview(&state->preview);
        Gr_BeginFrame();
        if (!state->rom_ready) GuiView_DrawRomScreen(state);
        else                   GuiView_DrawMainScreen(state);
        Gr_EndFrame();
    }

    GuiState_UnloadPreview(&state->preview);
    free(state);
    Gr_CloseWindow();
    return 0;
}
