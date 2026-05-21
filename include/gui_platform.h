/**
 * @file gui_platform.h
 * @brief Platform-independent renderer and OS abstraction layer.
 *
 * This module defines the color, rectangle, texture, and windowing abstractions used by the
 * GUI browser application. Implementing this interface allows swapping backends (e.g. Raylib, SDL, OpenGL).
 */

#ifndef GUI_PLATFORM_H
#define GUI_PLATFORM_H

/**
 * @brief Abstract color representation using 8-bit red, green, blue, and alpha values.
 */
typedef struct GrColor  { unsigned char r, g, b, a; } GrColor;

/**
 * @brief Abstract rectangle bounding box in 2D coordinate space.
 */
typedef struct GrRect   { float x, y, width, height; } GrRect;

/**
 * @brief Abstract texture representation mapped to graphics card registers.
 */
typedef struct GrTexture { unsigned int id; int w, h; } GrTexture;

/**
 * @brief Initialises the application window and renderer context.
 * @param w Window width.
 * @param h Window height.
 * @param title UTF-8 window title.
 */
void Gr_InitWindow(int w, int h, const char *title);

/**
 * @brief Terminates the windowing subsystem and cleans up remaining hardware resources.
 */
void Gr_CloseWindow(void);

/**
 * @brief Checks if the OS window closing event (close button, Alt+F4) has been triggered.
 * @return 1 if window should terminate; 0 otherwise.
 */
int  Gr_WindowShouldClose(void);

/**
 * @brief Gets the current window canvas width in pixels.
 * @return Width in pixels.
 */
int  Gr_GetScreenW(void);

/**
 * @brief Gets the current window canvas height in pixels.
 * @return Height in pixels.
 */
int  Gr_GetScreenH(void);

/**
 * @brief resticts the minimum allowable window sizing for resizable targets.
 * @param w Minimum width limit.
 * @param h Minimum height limit.
 */
void Gr_SetMinSize(int w, int h);

/**
 * @brief resticts the rendering execution speed to the target frames per second.
 * @param fps Target FPS (e.g. 60).
 */
void Gr_SetTargetFPS(int fps);

/**
 * @brief Signals the rendering backend that a new frame has started (flushes draw calls).
 */
void Gr_BeginFrame(void);

/**
 * @brief Signals the rendering backend that a frame has finished (swaps framebuffers).
 */
void Gr_EndFrame(void);

/**
 * @brief Clears the entire active framebuffer with a solid background color.
 * @param c Target background color.
 */
void Gr_Clear(GrColor c);

/**
 * @brief Returns total time elapsed in seconds since the window context started.
 * @return Time in seconds.
 */
float Gr_GetTime(void);

/**
 * @brief Returns the duration time step in seconds between the previous frame and this frame.
 * @return Frame time step in seconds.
 */
float Gr_GetFrameDelta(void);

/**
 * @brief Checks if a file path exists and is accessible.
 * @param path System path string.
 * @return 1 if file is accessible; 0 otherwise.
 */
int  Gr_FileExists(const char *path);

/**
 * @brief Abstracted directory list container.
 */
typedef struct GrFilePathList {
    unsigned int count;     /**< Amount of filepaths inside the list. */
    char **paths;           /**< Array of path strings. */
} GrFilePathList;

/**
 * @brief Loads all filenames/directories inside a specified folder.
 * @param dirPath Folder path string.
 * @return Struct containing paths and count.
 */
GrFilePathList Gr_LoadDirectoryFiles(const char *dirPath);

/**
 * @brief Deallocates directory list memory allocations.
 * @param list Direct container to clean.
 */
void           Gr_UnloadDirectoryFiles(GrFilePathList list);

/**
 * @brief Checks whether the given path represents a file or a directory.
 * @param path Target path string.
 * @return 1 if it is a file; 0 if directory.
 */
int            Gr_IsPathFile(const char *path);

/**
 * @brief Extracts the file name pointer (excluding folders) from a path string.
 * @param filePath Complete path string.
 * @return Pointer within filePath starting at the base file name.
 */
const char*    Gr_GetFileName(const char *filePath);

/**
 * @brief Gets current process working directory (CWD) path.
 * @return Pointer to internal static path string.
 */
const char*    Gr_GetWorkingDirectory(void);

/**
 * @brief Resolves parent directory path.
 * @param dirPath Complete directory path.
 * @return Pointer to parent path string.
 */
const char*    Gr_GetPrevDirectoryPath(const char *dirPath);

/** @brief Gets active cursor horizontal pixel index. */
int  Gr_MouseX(void);
/** @brief Gets active cursor vertical pixel index. */
int  Gr_MouseY(void);
/** @brief Checks if a mouse button was clicked. */
int  Gr_MousePressed(int btn);
/** @brief Checks if a mouse button is currently held down. */
int  Gr_MouseDown(int btn);
/** @brief Gets mouse wheel offset delta. */
int  Gr_GetWheel(void);
/** @brief Checks if keyboard key was clicked. */
int  Gr_KeyPressed(int key);
/** @brief Checks if keyboard key is being held. */
int  Gr_KeyDown(int key);
/** @brief Pops latest character pressed from keyboard queue. */
int  Gr_GetChar(void);
/** @brief Gets active system clipboard contents. */
const char *Gr_Clipboard(void);
/** @brief Checks if any files have been dropped onto the window area in this frame. */
int  Gr_FileDropped(void);
/** @brief Gets count of dropped files. */
int  Gr_DropCount(void);
/** @brief Gets path of dropped file by index. */
const char *Gr_DropPath(int i);
/** @brief Cleans up drag-and-drop platform handles. */
void Gr_DropFinish(void);

/**
 * @brief Computes width bounds for a text string under a target font height size.
 * @param text UTF-8 string.
 * @param size Font height in pixels.
 * @return Layout bounds rectangle.
 */
GrRect Gr_MeasureText(const char *text, int size);

/**
 * @brief Draws direct raster text on active canvas.
 * @param text Target string.
 * @param x Horizontal pixel position.
 * @param y Vertical pixel position.
 * @param size Font height in pixels.
 * @param c Font color.
 */
void   Gr_DrawText(const char *text, float x, float y, int size, GrColor c);

/**
 * @brief Draws solid rectangle.
 * @param r Layout bounds.
 * @param c Solid color.
 */
void   Gr_DrawRect(GrRect r, GrColor c);

/**
 * @brief Draws rounded corners solid rectangle.
 * @param r Layout bounds.
 * @param rad Corner rounding radius factor (0.0 to 1.0).
 * @param c Solid color.
 */
void   Gr_DrawRectRounded(GrRect r, float rad, GrColor c);

/**
 * @brief Draws lines of a rounded corners rectangle.
 * @param r Layout bounds.
 * @param rad Rounding factor.
 * @param c Line color.
 */
void   Gr_DrawRectRoundedLines(GrRect r, float rad, GrColor c);

/**
 * @brief Draws solid circle.
 * @param cx Center X index.
 * @param cy Center Y index.
 * @param r Circle radius in pixels.
 * @param c Solid color.
 */
void   Gr_DrawCircle(int cx, int cy, float r, GrColor c);

/**
 * @brief Sets a rectangular scissor clipping box on drawing calls.
 * @param x Origin X coordinate.
 * @param y Origin Y coordinate.
 * @param w Clipping area width.
 * @param h Clipping area height.
 */
void   Gr_ScissorBegin(int x, int y, int w, int h);

/**
 * @brief Disables scissor clipping bounding boxes.
 */
void   Gr_ScissorEnd(void);

/**
 * @brief Verifies collision point bounds inside a rectangle.
 */
int    Gr_CheckPointInRect(GrRect r, float px, float py);

/**
 * @brief Verifies collision bounds inside a circle.
 */
int    Gr_CheckPointInCircle(float px, float py, float cx, float cy, float r);

/**
 * @brief Allocates dynamic texture mapping using raw RGBA pixel arrays.
 * @param t Abstract texture destination container.
 * @param rgba Pixel bytes.
 * @param w Texture width.
 * @param h Texture height.
 * @return 0 on success; negative on failure.
 */
int  Gr_MakeTextureRGBA(GrTexture *t, const unsigned char *rgba, int w, int h);

/**
 * @brief Copies new raw RGBA pixel bytes directly into an active texture VRAM address.
 * @param t Active texture container.
 * @param rgba New pixel bytes (must match width * height * 4).
 */
void Gr_UpdateTexture(GrTexture *t, const unsigned char *rgba);

/**
 * @brief Draws a textured quad inside target coordinates.
 * @param t Texture register pointer.
 * @param src Source cropped boundaries within the texture.
 * @param dst Destination target placement boundaries.
 * @param tint Color multiplier overlay.
 */
void Gr_DrawTexture(GrTexture *t, GrRect src, GrRect dst, GrColor tint);

/**
 * @brief Deallocates hardware graphics memory registers used by a texture.
 * @param t Texture pointer to clean.
 */
void Gr_UnloadTexture(GrTexture *t);

/**
 * @brief Formats text string using printf-style layout arguments.
 * @param fmt printf-style format string.
 * @return Pointer to internal static buffer populated with the result.
 */
const char *Gr_FormatText(const char *fmt, ...);

#endif
