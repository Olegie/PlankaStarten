#ifndef PLANKASTARTEN_GUI_MENU_H
#define PLANKASTARTEN_GUI_MENU_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "plankastarten_gui_i18n.h"
#include "plankastarten_gui_popup.h"

typedef struct PST_MENU_HANDLES {
    HWND file;
    HWND edit;
    HWND search;
    HWND run;
    HWND build;
    HWND artifacts;
    HWND settings;
    HWND help;
} PST_MENU_HANDLES;

typedef struct PST_MENU_ACTIONS {
    void *userdata;
    void (*open_workspace)(void *userdata);
    void (*save)(void *userdata);
    void (*exit_app)(void *userdata);
    void (*select_all)(void *userdata);
    void (*copy)(void *userdata);
    void (*paste)(void *userdata);
    void (*format)(void *userdata);
    void (*find_procedure)(void *userdata);
    void (*focus_procedures)(void *userdata);
    void (*focus_editor)(void *userdata);
    void (*check)(void *userdata);
    void (*run_selected)(void *userdata);
    void (*compile_launch)(void *userdata);
    void (*artifact)(void *userdata, const char *kind);
    void (*open_build_folder)(void *userdata);
    void (*open_workspace_folder)(void *userdata);
    void (*set_language)(void *userdata, PST_LANG lang);
    void (*show_commands)(void *userdata);
    void (*show_about)(void *userdata);
} PST_MENU_ACTIONS;

void pst_menu_create_bar(HWND parent, HFONT font, PST_MENU_HANDLES *handles);
void pst_menu_layout(const PST_MENU_HANDLES *handles, int y, int h);
void pst_menu_apply_language(const PST_MENU_HANDLES *handles, PST_LANG lang);
int pst_menu_is_bar_id(int id);
void pst_menu_show(PST_POPUP *popup, HWND owner, HWND anchor, int id,
    PST_LANG lang, HFONT font);
int pst_menu_dispatch(int id, const PST_MENU_ACTIONS *actions);

#endif
