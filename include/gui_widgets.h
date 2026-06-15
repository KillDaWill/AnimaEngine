/**
 * @file gui_widgets.h
 * @brief Custom immediate-mode graphical widget drawing routines.
 */

#ifndef GUI_WIDGETS_H
#define GUI_WIDGETS_H

#include <stddef.h>
#include "gui_platform.h"

/**
 * @brief Renders a custom clickable button.
 * @param bounds Button position and size.
 * @param label Text printed on the button.
 * @param enabled 1 to allow clicks, 0 to draw grayed out and ignore input.
 * @return 1 if the button was clicked during this frame; 0 otherwise.
 */
int  Gr_DrawButton(GrRect bounds, const char *label, int enabled);

/**
 * @brief Renders a standard circular radio button with associated text label.
 * @param x Horizontal center coordinate.
 * @param y Vertical center coordinate.
 * @param selected 1 if active, 0 otherwise.
 * @param label Text printed next to the radio button.
 * @return 1 if the radio button was clicked during this frame; 0 otherwise.
 */
int  Gr_DrawRadioButton(float x, float y, int selected, const char *label);

/**
 * @brief Renders an interactive input text box.
 * @param bounds Text box coordinates.
 * @param text Buffer storing active characters (mutated in place).
 * @param cap Maximum capacity of the text buffer.
 * @param active 1 if text box has keyboard focus, 0 otherwise.
 * @param placeholder Text displayed when the buffer is empty.
 * @return 1 if keyboard focus state changed or text was edited; 0 otherwise.
 */
int  Gr_DrawTextBox(GrRect bounds, char *text, size_t cap, int active, const char *placeholder);

/**
 * @brief Utility helper to draw text aligned within a bounding box.
 * @param bounds Outer bounding rectangle.
 * @param text Message to draw.
 * @param font_size Height of font characters in pixels.
 * @param color Text color.
 */
void Gr_DrawTextInBox(GrRect bounds, const char *text, int font_size, GrColor color);

/**
 * @brief Draws a slide switch toggle.
 * @param x Left coordinate.
 * @param y Top coordinate.
 * @param w Width.
 * @param h Height.
 * @param active 1 if switch is turned on, 0 if turned off.
 * @param label Toggle description label.
 * @return 1 if switch state was toggled during this frame; 0 otherwise.
 */
int  Gr_DrawCustomSwitch(float x, float y, float w, float h, int active, const char *label);

/**
 * @brief Renders a segmented selector bar containing multiple text labels.
 * @param bounds Segment container boundaries.
 * @param opts Array of label strings representing selectable segments.
 * @param count Number of options.
 * @param selected Currently active selection index.
 * @return Newly selected segment index, or the original index if no change occurred.
 */
int  Gr_DrawSegmentedControl(GrRect bounds, const char *opts[], int count, int selected);

/**
 * @brief Draws a custom drop-down menu selection modal.
 * @param bounds Outer selector box boundaries.
 * @param label Display text of the active selection when closed.
 * @param items Array of options to display when expanded.
 * @param count Number of items in array.
 * @param selected Pointer to selection index value (mutated in place).
 * @param open Pointer to boolean toggle tracking expansion state (mutated in place).
 * @return 1 if selection changed; 0 otherwise.
 */
int  Gr_DrawCustomDropdown(GrRect bounds, const char *label, const char *items[], int count, int *selected, int *open);

/**
 * @brief Extended custom dropdown featuring scrollbars for long list selections.
 * @param bounds Outer selector box boundaries.
 * @param label Display text of the active selection when closed.
 * @param items Array of options to display when expanded.
 * @param count Number of items in array.
 * @param selected Pointer to selection index value (mutated in place).
 * @param open Pointer to boolean toggle tracking expansion state (mutated in place).
 * @param scroll Pointer to scroll offset value (mutated in place).
 * @param max_visible Maximum items to draw before triggering scrollbars.
 * @return 1 if selection changed; 0 otherwise.
 */
int  Gr_DrawCustomDropdownScrollable(GrRect bounds, const char *label, const char *items[], int count, int *selected, int *open, int *scroll, int max_visible);

#endif

