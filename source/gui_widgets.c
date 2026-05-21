#include "gui_widgets.h"
#include <string.h>

#define GR_WHITE ((GrColor){255,255,255,255})

static GrColor Gc(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { GrColor c; c.r=r;c.g=g;c.b=b;c.a=a; return c; }

int Gr_DrawButton(GrRect bounds, const char *label, int enabled)
{
    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();
    int hot = enabled && Gr_CheckPointInRect(bounds, mx, my);
    GrColor fill   = enabled ? Gc(30,41,59,230) : Gc(15,23,42,100);
    GrColor border = enabled ? Gc(71,85,105,255) : Gc(51,65,85,255);
    GrColor textc = enabled ? GR_WHITE : Gc(100,116,139,255);
    if (hot) { fill = Gc(51,65,85,255); border = Gc(6,182,212,255); }

    Gr_DrawRectRounded(bounds, 0.15f, fill);
    Gr_DrawRectRoundedLines(bounds, 0.15f, border);

    int fs = 16;
    GrRect m = Gr_MeasureText(label, fs);
    Gr_DrawText(label, bounds.x + (bounds.width - m.width)*0.5f, bounds.y + (bounds.height - m.height)*0.5f, fs, textc);
    return hot && Gr_MousePressed(0);
}

int Gr_DrawRadioButton(float x, float y, int selected, const char *label)
{
    float radius = 9.0f, inner = 5.0f;
    int fs = 16;
    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();
    int clicked = Gr_CheckPointInCircle(mx, my, x+radius, y+radius, radius+4.0f) && Gr_MousePressed(0);
    Gr_DrawCircle((int)(x+radius), (int)(y+radius), radius, Gc(176,188,204,255));
    if (selected) Gr_DrawCircle((int)(x+radius), (int)(y+radius), inner, Gc(209,55,66,255));
    Gr_DrawText(label, (int)(x+radius*2+8), (int)(y-2), fs, Gc(28,35,45,255));
    return clicked;
}

void Gr_DrawTextInBox(GrRect bounds, const char *text, int font_size, GrColor color)
{
    GrRect m = Gr_MeasureText(text, font_size);
    int dx = (int)bounds.x + 12;
    if (m.width > bounds.width - 24.0f) dx = (int)(bounds.x + bounds.width - 12.0f - m.width);
    Gr_ScissorBegin((int)bounds.x + 8, (int)bounds.y, (int)bounds.width - 16, (int)bounds.height);
    Gr_DrawText(text, (float)dx, bounds.y + (bounds.height - (float)font_size)*0.5f, font_size, color);
    Gr_ScissorEnd();
}

int Gr_DrawTextBox(GrRect bounds, char *text, size_t cap, int active, const char *placeholder)
{
    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();
    int clicked = Gr_CheckPointInRect(bounds, mx, my);
    if (Gr_MousePressed(0)) active = clicked;

    if (active) {
        int key;
        while ((key = Gr_GetChar()) > 0) {
            size_t len = strlen(text);
            if (key >= 32 && key <= 126 && len + 1 < cap) { text[len] = (char)key; text[len+1] = '\0'; }
        }
        if (Gr_KeyPressed(259)) { size_t len = strlen(text); if (len > 0) text[len-1] = '\0'; }
        if ((Gr_KeyDown(341) || Gr_KeyDown(345)) && Gr_KeyPressed(86)) {
            const char *clip = Gr_Clipboard();
            size_t len = strlen(text);
            while (clip && *clip && len + 1 < cap) {
                unsigned char c = (unsigned char)*clip++;
                if (c >= 32 && c <= 126) text[len++] = (char)c;
            }
            text[len] = '\0';
        }
    }

    GrColor bg = Gc(30,41,59,240);
    GrColor border = active ? Gc(6,182,212,255) : Gc(71,85,105,255);
    Gr_DrawRectRounded(bounds, 0.15f, bg);
    Gr_DrawRectRoundedLines(bounds, 0.15f, border);

    int fs = 16;
    if (!text[0] && placeholder) Gr_DrawTextInBox(bounds, placeholder, fs, Gc(148,163,184,255));
    else Gr_DrawTextInBox(bounds, text, fs, GR_WHITE);

    if (active && ((int)(Gr_GetTime()*2.0) % 2) == 0) {
        GrRect m = Gr_MeasureText(text, fs);
        int cx = (int)bounds.x + 12 + (int)m.width;
        if (m.width > bounds.width - 24.0f) cx = (int)(bounds.x + bounds.width - 12.0f);
        if (cx > (int)(bounds.x + bounds.width - 12.0f)) cx = (int)(bounds.x + bounds.width - 12.0f);
        Gr_DrawRect((GrRect){(float)cx, bounds.y+11.0f, 2.0f, bounds.height-22.0f}, Gc(6,182,212,255));
    }
    return active;
}

int Gr_DrawCustomSwitch(float x, float y, float w, float h, int active, const char *label)
{
    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();
    GrRect bounds = {x, y, w, h};
    int clicked = Gr_CheckPointInRect(bounds, mx, my) && Gr_MousePressed(0);
    if (clicked) active = !active;

    GrColor track = active ? Gc(124,58,237,255) : Gc(51,65,85,255);
    Gr_DrawRectRounded(bounds, 0.5f, track);
    float kr = (h - 4.0f) * 0.5f;
    float kx = active ? (x + w - kr - 2.0f) : (x + kr + 2.0f);
    float ky = y + h * 0.5f;
    Gr_DrawCircle((int)kx, (int)ky, kr, GR_WHITE);
    Gr_DrawText(label, (int)(x + w + 10), (int)(y + (h - 16.0f)*0.5f), 16, GR_WHITE);
    return clicked;
}

int Gr_DrawSegmentedControl(GrRect bounds, const char *opts[], int count, int selected)
{
    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();
    Gr_DrawRectRounded(bounds, 0.25f, Gc(30,41,59,255));
    Gr_DrawRectRoundedLines(bounds, 0.25f, Gc(71,85,105,255));
    float sw = bounds.width / (float)count;
    int clicked = -1;
    for (int i = 0; i < count; i++) {
        GrRect seg = {bounds.x + i*sw, bounds.y, sw, bounds.height};
        int hovered = Gr_CheckPointInRect(seg, mx, my);
        if (hovered && Gr_MousePressed(0)) clicked = i;
        if (i == selected) Gr_DrawRectRounded((GrRect){seg.x+2,seg.y+2,seg.width-4,seg.height-4}, 0.25f, Gc(124,58,237,255));
        int fs = 14;
        GrRect m = Gr_MeasureText(opts[i], fs);
        GrColor tc = (i == selected) ? GR_WHITE : Gc(148,163,184,255);
        Gr_DrawText(opts[i], seg.x + (sw - m.width)*0.5f, seg.y + (bounds.height - (float)fs)*0.5f, fs, tc);
    }
    return clicked;
}

int Gr_DrawCustomDropdown(GrRect bounds, const char *label, const char *items[], int count, int *selected, int *open)
{
    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();
    if (Gr_CheckPointInRect(bounds, mx, my) && Gr_MousePressed(0)) *open = !(*open);

    GrColor hbg = *open ? Gc(51,65,85,255) : Gc(30,41,59,255);
    GrColor border = *open ? Gc(6,182,212,255) : Gc(71,85,105,255);
    Gr_DrawRectRounded(bounds, 0.15f, hbg);
    Gr_DrawRectRoundedLines(bounds, 0.15f, border);

    const char *dt = (*selected >= 0 && *selected < count) ? items[*selected] : label;
    int fs = 15;
    Gr_DrawText(dt, (int)bounds.x + 12, (int)(bounds.y + (bounds.height - (float)fs)*0.5f), fs, GR_WHITE);

    const char *arrow = *open ? "\xe2\x96\xb2" : "\xe2\x96\xbc";
    GrRect aw = Gr_MeasureText(arrow, fs);
    Gr_DrawText(arrow, bounds.x + bounds.width - aw.width - 12, bounds.y + (bounds.height - (float)fs)*0.5f, fs, Gc(148,163,184,255));

    int clicked_item = -1;
    if (*open && count > 0) {
        float ih = bounds.height - 4.0f;
        float mh = ih * (float)count + 6.0f;
        GrRect mr = {bounds.x, bounds.y + bounds.height + 2.0f, bounds.width, mh};
        Gr_DrawRectRounded(mr, 0.08f, Gc(15,23,42,245));
        Gr_DrawRectRoundedLines(mr, 0.08f, Gc(6,182,212,255));
        for (int i = 0; i < count; i++) {
            GrRect ir = {bounds.x + 2.0f, mr.y + 3.0f + i*ih, bounds.width - 4.0f, ih};
            int hovered = Gr_CheckPointInRect(ir, mx, my);
            if (hovered) { Gr_DrawRectRounded(ir, 0.12f, Gc(124,58,237,255)); if (Gr_MousePressed(0)) { *selected = i; clicked_item = i; *open = 0; } }
            else if (i == *selected) Gr_DrawRectRounded(ir, 0.12f, Gc(30,41,59,255));
            GrColor tc = (hovered || i == *selected) ? GR_WHITE : Gc(148,163,184,255);
            Gr_DrawText(items[i], (int)ir.x + 10, (int)(ir.y + (ih - (float)fs)*0.5f), fs, tc);
        }
    }
    return clicked_item;
}

int Gr_DrawCustomDropdownScrollable(
    GrRect bounds,
    const char *label,
    const char *items[],
    int count,
    int *selected,
    int *open,
    int *scroll,
    int max_visible
)
{
    float mx = (float)Gr_MouseX(), my = (float)Gr_MouseY();
    int visible_count;
    int max_scroll;

    if (scroll == NULL) {
        return Gr_DrawCustomDropdown(bounds, label, items, count, selected, open);
    }

    if (max_visible <= 0) max_visible = 8;
    visible_count = count < max_visible ? count : max_visible;
    max_scroll = count - visible_count;
    if (max_scroll < 0) max_scroll = 0;
    if (*scroll < 0) *scroll = 0;
    if (*scroll > max_scroll) *scroll = max_scroll;

    int was_open = *open;
    if (Gr_CheckPointInRect(bounds, mx, my) && Gr_MousePressed(0)) {
        *open = !(*open);
    }

    if (*open && !was_open) {
        if (*selected >= 0 && *selected < count) {
            *scroll = *selected - visible_count / 2;
            if (*scroll < 0) *scroll = 0;
            if (*scroll > max_scroll) *scroll = max_scroll;
        }
    }

    GrColor hbg = *open ? Gc(51,65,85,255) : Gc(30,41,59,255);
    GrColor border = *open ? Gc(6,182,212,255) : Gc(71,85,105,255);
    Gr_DrawRectRounded(bounds, 0.15f, hbg);
    Gr_DrawRectRoundedLines(bounds, 0.15f, border);

    const char *dt = (*selected >= 0 && *selected < count) ? items[*selected] : label;
    int fs = 15;
    Gr_ScissorBegin((int)bounds.x + 8, (int)bounds.y, (int)bounds.width - 40, (int)bounds.height);
    Gr_DrawText(dt, (int)bounds.x + 12, (int)(bounds.y + (bounds.height - (float)fs)*0.5f), fs, GR_WHITE);
    Gr_ScissorEnd();

    const char *arrow = *open ? "\xe2\x96\xb2" : "\xe2\x96\xbc";
    GrRect aw = Gr_MeasureText(arrow, fs);
    Gr_DrawText(arrow, bounds.x + bounds.width - aw.width - 12, bounds.y + (bounds.height - (float)fs)*0.5f, fs, Gc(148,163,184,255));

    int clicked_item = -1;
    if (*open && count > 0) {
        float ih = bounds.height - 4.0f;
        float mh = ih * (float)visible_count + 6.0f;
        GrRect mr = {bounds.x, bounds.y + bounds.height + 2.0f, bounds.width, mh};
        int wheel;

        if (Gr_CheckPointInRect(mr, mx, my)) {
            wheel = Gr_GetWheel();
            if (wheel > 0) *scroll -= 1;
            else if (wheel < 0) *scroll += 1;
            if (*scroll < 0) *scroll = 0;
            if (*scroll > max_scroll) *scroll = max_scroll;
        }

        Gr_DrawRectRounded(mr, 0.08f, Gc(15,23,42,245));
        Gr_DrawRectRoundedLines(mr, 0.08f, Gc(6,182,212,255));
        for (int row = 0; row < visible_count; row++) {
            int i = *scroll + row;
            GrRect ir = {bounds.x + 2.0f, mr.y + 3.0f + row*ih, bounds.width - 4.0f, ih};
            int hovered = Gr_CheckPointInRect(ir, mx, my);
            if (hovered) {
                Gr_DrawRectRounded(ir, 0.12f, Gc(124,58,237,255));
                if (Gr_MousePressed(0)) { *selected = i; clicked_item = i; *open = 0; }
            } else if (i == *selected) {
                Gr_DrawRectRounded(ir, 0.12f, Gc(30,41,59,255));
            }
            GrColor tc = (hovered || i == *selected) ? GR_WHITE : Gc(148,163,184,255);
            Gr_ScissorBegin((int)ir.x + 8, (int)ir.y, (int)ir.width - 16, (int)ir.height);
            Gr_DrawText(items[i], (int)ir.x + 10, (int)(ir.y + (ih - (float)fs)*0.5f), fs, tc);
            Gr_ScissorEnd();
        }

        if (count > visible_count) {
            float track_h = mr.height - 10.0f;
            float thumb_h = track_h * ((float)visible_count / (float)count);
            float thumb_y = mr.y + 5.0f;
            if (thumb_h < 14.0f) thumb_h = 14.0f;

            GrRect track_rect = {mr.x + mr.width - 12.0f, mr.y + 5.0f, 10.0f, track_h};
            if (Gr_CheckPointInRect(track_rect, mx, my) && Gr_MouseDown(0)) {
                float relative_y = my - track_rect.y - thumb_h * 0.5f;
                float ratio = relative_y / (track_h - thumb_h);
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
                *scroll = (int)(ratio * (float)max_scroll + 0.5f);
                if (*scroll < 0) *scroll = 0;
                if (*scroll > max_scroll) *scroll = max_scroll;
            }

            if (max_scroll > 0) {
                thumb_y += (track_h - thumb_h) * ((float)*scroll / (float)max_scroll);
            }
            Gr_DrawRect((GrRect){mr.x + mr.width - 6.0f, thumb_y, 3.0f, thumb_h}, Gc(100,116,139,220));
        }
    }

    return clicked_item;
}
