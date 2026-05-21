#include "gui_platform.h"
#include "raylib.h"
#include <stdio.h>
#include <stdarg.h>

void Gr_InitWindow(int w, int h, const char *title) { SetConfigFlags(FLAG_WINDOW_RESIZABLE); InitWindow(w, h, title); }
void Gr_CloseWindow(void) { CloseWindow(); }
int  Gr_WindowShouldClose(void) { return WindowShouldClose(); }
int  Gr_GetScreenW(void) { return GetScreenWidth(); }
int  Gr_GetScreenH(void) { return GetScreenHeight(); }
void Gr_SetMinSize(int w, int h) { SetWindowMinSize(w, h); }
void Gr_SetTargetFPS(int fps) { SetTargetFPS(fps); }
void Gr_BeginFrame(void) { BeginDrawing(); }
void Gr_EndFrame(void) { EndDrawing(); }
void Gr_Clear(GrColor c) { ClearBackground((Color){c.r,c.g,c.b,c.a}); }
float Gr_GetTime(void) { return (float)GetTime(); }
float Gr_GetFrameDelta(void) { return GetFrameTime(); }
int  Gr_FileExists(const char *path) { return FileExists(path); }

int  Gr_MouseX(void) { return (int)GetMousePosition().x; }
int  Gr_MouseY(void) { return (int)GetMousePosition().y; }
int  Gr_MousePressed(int btn) { return IsMouseButtonPressed(btn); }
int  Gr_MouseDown(int btn) { return IsMouseButtonDown(btn); }
int  Gr_GetWheel(void) {
    float w = GetMouseWheelMove();
    if (w > 0.0f) return 1;
    if (w < 0.0f) return -1;
    return 0;
}
int  Gr_KeyPressed(int key) { return IsKeyPressed(key); }
int  Gr_KeyDown(int key) { return IsKeyDown(key); }
int  Gr_GetChar(void) { return GetCharPressed(); }
const char *Gr_Clipboard(void) { return GetClipboardText(); }

int  Gr_FileDropped(void) { return IsFileDropped(); }
int  Gr_DropCount(void) { FilePathList d = LoadDroppedFiles(); int c = (int)d.count; UnloadDroppedFiles(d); return c; }
const char *Gr_DropPath(int i) {
    static char buf[4096];
    FilePathList d = LoadDroppedFiles();
    if (i >= 0 && i < (int)d.count && d.paths[i]) snprintf(buf, sizeof(buf), "%s", d.paths[i]);
    else buf[0] = '\0';
    UnloadDroppedFiles(d);
    return buf;
}
void Gr_DropFinish(void) { /* Raylib drop cleanup handled internally */ }

GrRect Gr_MeasureText(const char *text, int size) {
    int w = MeasureText(text, size);
    return (GrRect){0, 0, (float)w, (float)size};
}
void Gr_DrawText(const char *text, float x, float y, int size, GrColor c) { DrawText(text, (int)x, (int)y, size, (Color){c.r,c.g,c.b,c.a}); }
void Gr_DrawRect(GrRect r, GrColor c) { DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, (Color){c.r,c.g,c.b,c.a}); }
void Gr_DrawRectRounded(GrRect r, float rad, GrColor c) { DrawRectangleRounded((Rectangle){r.x,r.y,r.width,r.height}, rad, 8, (Color){c.r,c.g,c.b,c.a}); }
void Gr_DrawRectRoundedLines(GrRect r, float rad, GrColor c) { DrawRectangleRoundedLines((Rectangle){r.x,r.y,r.width,r.height}, rad, 8, (Color){c.r,c.g,c.b,c.a}); }
void Gr_DrawCircle(int cx, int cy, float rad, GrColor c) { DrawCircle(cx, cy, rad, (Color){c.r,c.g,c.b,c.a}); }
void Gr_ScissorBegin(int x, int y, int w, int h) { BeginScissorMode(x, y, w, h); }
void Gr_ScissorEnd(void) { EndScissorMode(); }
int  Gr_CheckPointInRect(GrRect r, float px, float py) { return CheckCollisionPointRec((Vector2){px, py}, (Rectangle){r.x,r.y,r.width,r.height}); }
int  Gr_CheckPointInCircle(float px, float py, float cx, float cy, float r) { return CheckCollisionPointCircle((Vector2){px, py}, (Vector2){cx, cy}, r); }

int Gr_MakeTextureRGBA(GrTexture *t, const unsigned char *rgba, int w, int h) {
    Image img = {0};
    img.data = (void *)rgba;
    img.width = w; img.height = h;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Texture2D tex = LoadTextureFromImage(img);
    if (tex.id == 0) return -1;
    t->id = tex.id;
    t->w = tex.width;
    t->h = tex.height;
    return 0;
}
void Gr_UpdateTexture(GrTexture *t, const unsigned char *rgba) {
    UpdateTexture((Texture2D){t->id, t->w, t->h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8}, rgba);
}
void Gr_DrawTexture(GrTexture *t, GrRect src, GrRect dst, GrColor tint) {
    DrawTexturePro((Texture2D){t->id, t->w, t->h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8},
                   (Rectangle){src.x,src.y,src.width,src.height},
                   (Rectangle){dst.x,dst.y,dst.width,dst.height},
                   (Vector2){0,0}, 0.0f, (Color){tint.r,tint.g,tint.b,tint.a});
}
void Gr_UnloadTexture(GrTexture *t) { UnloadTexture((Texture2D){t->id, t->w, t->h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8}); t->id = 0; }

#include <stdarg.h>
const char *Gr_FormatText(const char *fmt, ...) {
    static char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

GrFilePathList Gr_LoadDirectoryFiles(const char *dirPath) {
    FilePathList list = LoadDirectoryFiles(dirPath);
    GrFilePathList result;
    result.count = list.count;
    result.paths = list.paths;
    return result;
}

void Gr_UnloadDirectoryFiles(GrFilePathList list) {
    FilePathList rl_list;
    rl_list.capacity = list.count;
    rl_list.count = list.count;
    rl_list.paths = list.paths;
    UnloadDirectoryFiles(rl_list);
}

int Gr_IsPathFile(const char *path) {
    return IsPathFile(path);
}

const char* Gr_GetFileName(const char *filePath) {
    return GetFileName(filePath);
}

const char* Gr_GetWorkingDirectory(void) {
    return GetWorkingDirectory();
}

const char* Gr_GetPrevDirectoryPath(const char *dirPath) {
    return GetPrevDirectoryPath(dirPath);
}
