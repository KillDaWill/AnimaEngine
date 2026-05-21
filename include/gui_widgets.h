#ifndef GUI_WIDGETS_H
#define GUI_WIDGETS_H

#include <stddef.h>
#include "gui_platform.h"

int  Gr_DrawButton(GrRect bounds, const char *label, int enabled);
int  Gr_DrawRadioButton(float x, float y, int selected, const char *label);
int  Gr_DrawTextBox(GrRect bounds, char *text, size_t cap, int active, const char *placeholder);
void Gr_DrawTextInBox(GrRect bounds, const char *text, int font_size, GrColor color);
int  Gr_DrawCustomSwitch(float x, float y, float w, float h, int active, const char *label);
int  Gr_DrawSegmentedControl(GrRect bounds, const char *opts[], int count, int selected);
int  Gr_DrawCustomDropdown(GrRect bounds, const char *label, const char *items[], int count, int *selected, int *open);
int  Gr_DrawCustomDropdownScrollable(GrRect bounds, const char *label, const char *items[], int count, int *selected, int *open, int *scroll, int max_visible);

#endif
