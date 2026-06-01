#ifndef PLANKASTARTEN_GUI_THEME_H
#define PLANKASTARTEN_GUI_THEME_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define PST_COLOR_BG RGB(232, 232, 228)
#define PST_COLOR_PANEL RGB(219, 219, 213)
#define PST_COLOR_PALE RGB(247, 244, 236)
#define PST_COLOR_ORANGE RGB(239, 158, 0)
#define PST_COLOR_DARK RGB(94, 58, 23)
#define PST_COLOR_TEXT RGB(20, 20, 20)
#define PST_COLOR_MUTED RGB(70, 70, 64)
#define PST_COLOR_GREEN RGB(0, 102, 28)

typedef struct PST_THEME {
    HFONT font;
    HFONT ui_font;
    HFONT title_font;
    HFONT small_font;
    HBRUSH bg_brush;
    HBRUSH orange_brush;
    HBRUSH dark_brush;
    HBRUSH panel_brush;
    HBRUSH pale_brush;
    HBRUSH white_brush;
} PST_THEME;

void pst_theme_init(PST_THEME *theme);
void pst_theme_destroy(PST_THEME *theme);
HBRUSH pst_theme_static_brush(PST_THEME *theme, HWND control, HDC dc);
HBRUSH pst_theme_edit_brush(PST_THEME *theme, HWND control, HDC dc);
HBRUSH pst_theme_list_brush(PST_THEME *theme, HWND control, HDC dc);

#endif
