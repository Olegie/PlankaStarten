#include "plankastarten_gui_theme.h"
#include "plankastarten_gui_ids.h"

#include <string.h>

static int pst_control_id(HWND control)
{
    return (int)(INT_PTR)GetDlgCtrlID(control);
}

void pst_theme_init(PST_THEME *theme)
{
    if (theme == 0) {
        return;
    }
    memset(theme, 0, sizeof(*theme));
    theme->font = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    theme->ui_font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
        FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Sans Serif");
    theme->title_font = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE,
        FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Sans Serif");
    theme->small_font = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
        FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Sans Serif");
    theme->bg_brush = CreateSolidBrush(PST_COLOR_BG);
    theme->orange_brush = CreateSolidBrush(PST_COLOR_ORANGE);
    theme->dark_brush = CreateSolidBrush(PST_COLOR_DARK);
    theme->panel_brush = CreateSolidBrush(PST_COLOR_PANEL);
    theme->pale_brush = CreateSolidBrush(PST_COLOR_PALE);
    theme->white_brush = CreateSolidBrush(RGB(255, 255, 255));
}

void pst_theme_destroy(PST_THEME *theme)
{
    if (theme == 0) {
        return;
    }
#define PST_DELETE_GDI(handle) \
    do { \
        if ((handle) != 0) { \
            DeleteObject(handle); \
            (handle) = 0; \
        } \
    } while (0)
    PST_DELETE_GDI(theme->font);
    PST_DELETE_GDI(theme->ui_font);
    PST_DELETE_GDI(theme->title_font);
    PST_DELETE_GDI(theme->small_font);
    PST_DELETE_GDI(theme->bg_brush);
    PST_DELETE_GDI(theme->orange_brush);
    PST_DELETE_GDI(theme->dark_brush);
    PST_DELETE_GDI(theme->panel_brush);
    PST_DELETE_GDI(theme->pale_brush);
    PST_DELETE_GDI(theme->white_brush);
#undef PST_DELETE_GDI
}

HBRUSH pst_theme_static_brush(PST_THEME *theme, HWND control, HDC dc)
{
    int id;

    if (theme == 0 || control == 0 || dc == 0) {
        return 0;
    }
    id = pst_control_id(control);
    SetBkMode(dc, OPAQUE);
    SetTextColor(dc, PST_COLOR_TEXT);
    SetBkColor(dc, PST_COLOR_BG);
    switch (id) {
    case IDC_TOP_BAND:
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, PST_COLOR_ORANGE);
        return theme->orange_brush;
    case IDC_TITLE:
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, PST_COLOR_ORANGE);
        return theme->orange_brush;
    case IDC_SUBTITLE:
        SetTextColor(dc, RGB(40, 30, 15));
        SetBkColor(dc, PST_COLOR_ORANGE);
        return theme->orange_brush;
    case IDC_MENU_BAR:
    case IDC_MENU_FILE:
    case IDC_MENU_EDIT:
    case IDC_MENU_SEARCH:
    case IDC_MENU_RUN:
    case IDC_MENU_BUILD:
    case IDC_MENU_ARTIFACTS:
    case IDC_MENU_SETTINGS:
    case IDC_MENU_HELP:
        SetTextColor(dc, RGB(255, 245, 220));
        SetBkColor(dc, PST_COLOR_DARK);
        return theme->dark_brush;
    case IDC_TOOL_PANEL:
        SetBkColor(dc, PST_COLOR_PANEL);
        return theme->panel_brush;
    case IDC_SOURCE_GROUP:
    case IDC_BACKEND_GROUP:
    case IDC_PROJECT_LABEL:
    case IDC_EDITOR_LABEL:
    case IDC_PROCS_LABEL:
    case IDC_COMMAND_LABEL:
    case IDC_OUTPUT_LABEL:
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkColor(dc, PST_COLOR_ORANGE);
        return theme->orange_brush;
    case IDC_PROC_LABEL:
    case IDC_ARGS_LABEL:
    case IDC_RESULT_LABEL:
    case IDC_WORKSPACE_LABEL:
        SetTextColor(dc, PST_COLOR_MUTED);
        SetBkColor(dc, PST_COLOR_BG);
        return theme->bg_brush;
    case IDC_STATUS:
        SetTextColor(dc, PST_COLOR_GREEN);
        SetBkColor(dc, PST_COLOR_PALE);
        return theme->pale_brush;
    default:
        SetBkColor(dc, PST_COLOR_BG);
        return theme->bg_brush;
    }
}

HBRUSH pst_theme_edit_brush(PST_THEME *theme, HWND control, HDC dc)
{
    int id;

    if (theme == 0 || dc == 0) {
        return 0;
    }
    id = pst_control_id(control);
    SetTextColor(dc, PST_COLOR_TEXT);
    if (id == IDC_LINE_NUMBERS || id == IDC_RESULT) {
        SetBkColor(dc, PST_COLOR_PALE);
        return theme->pale_brush;
    }
    SetBkColor(dc, RGB(255, 255, 255));
    return theme->white_brush;
}

HBRUSH pst_theme_list_brush(PST_THEME *theme, HWND control, HDC dc)
{
    (void)control;
    if (theme == 0 || dc == 0) {
        return 0;
    }
    SetTextColor(dc, PST_COLOR_TEXT);
    SetBkColor(dc, RGB(255, 255, 255));
    return theme->white_brush;
}
