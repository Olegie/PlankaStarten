#ifndef PLANKASTARTEN_GUI_POPUP_H
#define PLANKASTARTEN_GUI_POPUP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct PST_POPUP_ITEM {
    int id;
    const char *text;
    int separator;
    int checked;
} PST_POPUP_ITEM;

typedef struct PST_POPUP {
    HWND hwnd;
    HWND owner;
    HFONT title_font;
    HFONT ui_font;
    HBRUSH bg_brush;
    HBRUSH orange_brush;
    HBRUSH dark_brush;
    HBRUSH white_brush;
    char last_text[256];
} PST_POPUP;

void pst_popup_init(PST_POPUP *popup);
void pst_popup_destroy(PST_POPUP *popup);
void pst_popup_close(PST_POPUP *popup);
void pst_popup_show_message(PST_POPUP *popup, HWND owner,
    const char *title, const char *body, HFONT title_font, HFONT ui_font);
void pst_popup_show_search(PST_POPUP *popup, HWND owner, const char *title,
    const char *label, const char *initial, const char *find_text,
    const char *list_text, HFONT title_font, HFONT ui_font);
int pst_popup_get_text(PST_POPUP *popup, char *out, unsigned out_size);
void pst_popup_show_menu(PST_POPUP *popup, HWND owner, HWND anchor,
    const PST_POPUP_ITEM *items, int count, HFONT ui_font);

#endif
