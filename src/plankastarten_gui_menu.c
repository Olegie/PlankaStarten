#include "plankastarten_gui_menu.h"
#include "plankastarten_gui_ids.h"

static HWND pst_menu_label(HWND parent, HFONT font, int id)
{
    HWND hwnd;

    hwnd = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_NOTIFY | SS_LEFTNOWORDWRAP,
        0, 0, 40, 18, parent, (HMENU)(INT_PTR)id,
        GetModuleHandleA(0), 0);
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    return hwnd;
}

static PST_POPUP_ITEM pst_item(int id, const char *en, const char *de,
    PST_LANG lang)
{
    PST_POPUP_ITEM item;

    item.id = id;
    item.text = lang == PST_LANG_DE ? de : en;
    item.separator = 0;
    item.checked = 0;
    return item;
}

static PST_POPUP_ITEM pst_separator(void)
{
    PST_POPUP_ITEM item;

    item.id = 0;
    item.text = "";
    item.separator = 1;
    item.checked = 0;
    return item;
}

void pst_menu_create_bar(HWND parent, HFONT font, PST_MENU_HANDLES *handles)
{
    if (handles == 0) {
        return;
    }
    handles->file = pst_menu_label(parent, font, IDC_MENU_FILE);
    handles->edit = pst_menu_label(parent, font, IDC_MENU_EDIT);
    handles->search = pst_menu_label(parent, font, IDC_MENU_SEARCH);
    handles->run = pst_menu_label(parent, font, IDC_MENU_RUN);
    handles->build = pst_menu_label(parent, font, IDC_MENU_BUILD);
    handles->artifacts = pst_menu_label(parent, font, IDC_MENU_ARTIFACTS);
    handles->settings = pst_menu_label(parent, font, IDC_MENU_SETTINGS);
    handles->help = pst_menu_label(parent, font, IDC_MENU_HELP);
}

void pst_menu_layout(const PST_MENU_HANDLES *handles, int y, int h)
{
    int x;

    if (handles == 0) {
        return;
    }
    x = 14;
    MoveWindow(handles->file, x, y, 42, h, TRUE);
    x += 44;
    MoveWindow(handles->edit, x, y, 70, h, TRUE);
    x += 72;
    MoveWindow(handles->search, x, y, 62, h, TRUE);
    x += 64;
    MoveWindow(handles->run, x, y, 82, h, TRUE);
    x += 84;
    MoveWindow(handles->build, x, y, 54, h, TRUE);
    x += 58;
    MoveWindow(handles->artifacts, x, y, 82, h, TRUE);
    x += 86;
    MoveWindow(handles->settings, x, y, 106, h, TRUE);
    x += 110;
    MoveWindow(handles->help, x, y, 58, h, TRUE);
}

void pst_menu_apply_language(const PST_MENU_HANDLES *handles, PST_LANG lang)
{
    if (handles == 0) {
        return;
    }
    SetWindowTextA(handles->file, lang == PST_LANG_DE ? "Datei" : "File");
    SetWindowTextA(handles->edit, lang == PST_LANG_DE ? "Bearbeiten" : "Edit");
    SetWindowTextA(handles->search, lang == PST_LANG_DE ? "Suche" : "Search");
    SetWindowTextA(handles->run, lang == PST_LANG_DE ? "Ausfuehren" : "Run");
    SetWindowTextA(handles->build, lang == PST_LANG_DE ? "Build" : "Build");
    SetWindowTextA(handles->artifacts,
        lang == PST_LANG_DE ? "Artefakte" : "Artifacts");
    SetWindowTextA(handles->settings,
        lang == PST_LANG_DE ? "Einstellungen" : "Settings");
    SetWindowTextA(handles->help, lang == PST_LANG_DE ? "Hilfe" : "Help");
}

int pst_menu_is_bar_id(int id)
{
    return id == IDC_MENU_FILE || id == IDC_MENU_EDIT
        || id == IDC_MENU_SEARCH || id == IDC_MENU_RUN
        || id == IDC_MENU_BUILD || id == IDC_MENU_ARTIFACTS
        || id == IDC_MENU_SETTINGS || id == IDC_MENU_HELP;
}

static void pst_show_file(PST_POPUP *popup, HWND owner, HWND anchor,
    PST_LANG lang, HFONT font)
{
    PST_POPUP_ITEM items[4];

    items[0] = pst_item(IDM_FILE_OPEN_WORKSPACE,
        "Open workspace...", "Arbeitsordner oeffnen...", lang);
    items[1] = pst_item(IDM_FILE_SAVE, "Save", "Speichern", lang);
    items[2] = pst_separator();
    items[3] = pst_item(IDM_FILE_EXIT, "Exit", "Beenden", lang);
    pst_popup_show_menu(popup, owner, anchor, items, 4, font);
}

static void pst_show_edit(PST_POPUP *popup, HWND owner, HWND anchor,
    PST_LANG lang, HFONT font)
{
    PST_POPUP_ITEM items[5];

    items[0] = pst_item(IDM_EDIT_SELECT_ALL,
        "Select all", "Alles markieren", lang);
    items[1] = pst_item(IDM_EDIT_COPY, "Copy", "Kopieren", lang);
    items[2] = pst_item(IDM_EDIT_PASTE, "Paste", "Einfuegen", lang);
    items[3] = pst_separator();
    items[4] = pst_item(IDM_EDIT_FORMAT, "Format PLK",
        "PLK formatieren", lang);
    pst_popup_show_menu(popup, owner, anchor, items, 5, font);
}

static void pst_show_search(PST_POPUP *popup, HWND owner, HWND anchor,
    PST_LANG lang, HFONT font)
{
    PST_POPUP_ITEM items[3];

    items[0] = pst_item(IDM_SEARCH_FIND_PROC,
        "Find selected procedure", "Gewaehlte Prozedur suchen", lang);
    items[1] = pst_item(IDM_SEARCH_FOCUS_PROC,
        "Focus procedure list", "Prozedurliste fokussieren", lang);
    items[2] = pst_item(IDM_SEARCH_FOCUS_EDITOR,
        "Focus editor", "Editor fokussieren", lang);
    pst_popup_show_menu(popup, owner, anchor, items, 3, font);
}

static void pst_show_run(PST_POPUP *popup, HWND owner, HWND anchor,
    PST_LANG lang, HFONT font)
{
    PST_POPUP_ITEM items[3];

    items[0] = pst_item(IDM_RUN_CHECK, "Check", "Pruefen", lang);
    items[1] = pst_item(IDM_RUN_SELECTED,
        "Run selected", "Auswahl starten", lang);
    items[2] = pst_item(IDM_RUN_COMPILE_LAUNCH,
        "Compile and launch", "Bauen und starten", lang);
    pst_popup_show_menu(popup, owner, anchor, items, 3, font);
}

static void pst_show_build(PST_POPUP *popup, HWND owner, HWND anchor,
    PST_LANG lang, HFONT font)
{
    PST_POPUP_ITEM items[8];

    items[0] = pst_item(IDM_BUILD_COMPILE, "Compile", "Bauen", lang);
    items[1] = pst_separator();
    items[2] = pst_item(IDM_BUILD_BYTECODE,
        "Write bytecode", "Bytecode schreiben", lang);
    items[3] = pst_item(IDM_BUILD_IR, "Write IR", "IR schreiben", lang);
    items[4] = pst_item(IDM_BUILD_EVIDENCE,
        "Write evidence", "Evidence schreiben", lang);
    items[5] = pst_item(IDM_BUILD_C, "Write C", "C schreiben", lang);
    items[6] = pst_item(IDM_BUILD_ASM, "Write ASM", "ASM schreiben", lang);
    items[7] = pst_item(IDM_BUILD_8086,
        "Write 8086 ASM", "8086-ASM schreiben", lang);
    pst_popup_show_menu(popup, owner, anchor, items, 8, font);
}

static void pst_show_artifacts(PST_POPUP *popup, HWND owner, HWND anchor,
    PST_LANG lang, HFONT font)
{
    PST_POPUP_ITEM items[2];

    items[0] = pst_item(IDM_ARTIFACTS_BUILD_FOLDER,
        "Open build folder", "Build-Ordner oeffnen", lang);
    items[1] = pst_item(IDM_ARTIFACTS_WORKSPACE_FOLDER,
        "Open workspace folder", "Arbeitsordner oeffnen", lang);
    pst_popup_show_menu(popup, owner, anchor, items, 2, font);
}

static void pst_show_settings(PST_POPUP *popup, HWND owner, HWND anchor,
    PST_LANG lang, HFONT font)
{
    PST_POPUP_ITEM items[2];

    items[0] = pst_item(IDM_SETTINGS_LANG_EN, "English", "Englisch", lang);
    items[1] = pst_item(IDM_SETTINGS_LANG_DE, "German", "Deutsch", lang);
    items[0].checked = lang == PST_LANG_EN;
    items[1].checked = lang == PST_LANG_DE;
    pst_popup_show_menu(popup, owner, anchor, items, 2, font);
}

static void pst_show_help(PST_POPUP *popup, HWND owner, HWND anchor,
    PST_LANG lang, HFONT font)
{
    PST_POPUP_ITEM items[2];

    items[0] = pst_item(IDM_HELP_COMMANDS,
        "Command list", "Befehlsliste", lang);
    items[1] = pst_item(IDM_HELP_ABOUT, "About", "Ueber", lang);
    pst_popup_show_menu(popup, owner, anchor, items, 2, font);
}

void pst_menu_show(PST_POPUP *popup, HWND owner, HWND anchor, int id,
    PST_LANG lang, HFONT font)
{
    switch (id) {
    case IDC_MENU_FILE:
        pst_show_file(popup, owner, anchor, lang, font);
        break;
    case IDC_MENU_EDIT:
        pst_show_edit(popup, owner, anchor, lang, font);
        break;
    case IDC_MENU_SEARCH:
        pst_show_search(popup, owner, anchor, lang, font);
        break;
    case IDC_MENU_RUN:
        pst_show_run(popup, owner, anchor, lang, font);
        break;
    case IDC_MENU_BUILD:
        pst_show_build(popup, owner, anchor, lang, font);
        break;
    case IDC_MENU_ARTIFACTS:
        pst_show_artifacts(popup, owner, anchor, lang, font);
        break;
    case IDC_MENU_SETTINGS:
        pst_show_settings(popup, owner, anchor, lang, font);
        break;
    case IDC_MENU_HELP:
        pst_show_help(popup, owner, anchor, lang, font);
        break;
    default:
        break;
    }
}

int pst_menu_dispatch(int id, const PST_MENU_ACTIONS *actions)
{
    if (actions == 0) {
        return 0;
    }
    switch (id) {
    case IDM_FILE_OPEN_WORKSPACE:
        if (actions->open_workspace != 0) actions->open_workspace(actions->userdata);
        return 1;
    case IDM_FILE_SAVE:
        if (actions->save != 0) actions->save(actions->userdata);
        return 1;
    case IDM_FILE_EXIT:
        if (actions->exit_app != 0) actions->exit_app(actions->userdata);
        return 1;
    case IDM_EDIT_SELECT_ALL:
        if (actions->select_all != 0) actions->select_all(actions->userdata);
        return 1;
    case IDM_EDIT_COPY:
        if (actions->copy != 0) actions->copy(actions->userdata);
        return 1;
    case IDM_EDIT_PASTE:
        if (actions->paste != 0) actions->paste(actions->userdata);
        return 1;
    case IDM_EDIT_FORMAT:
        if (actions->format != 0) actions->format(actions->userdata);
        return 1;
    case IDM_SEARCH_FIND_PROC:
        if (actions->find_procedure != 0) actions->find_procedure(actions->userdata);
        return 1;
    case IDM_SEARCH_FOCUS_PROC:
        if (actions->focus_procedures != 0) actions->focus_procedures(actions->userdata);
        return 1;
    case IDM_SEARCH_FOCUS_EDITOR:
        if (actions->focus_editor != 0) actions->focus_editor(actions->userdata);
        return 1;
    case IDM_RUN_CHECK:
        if (actions->check != 0) actions->check(actions->userdata);
        return 1;
    case IDM_RUN_SELECTED:
        if (actions->run_selected != 0) actions->run_selected(actions->userdata);
        return 1;
    case IDM_RUN_COMPILE_LAUNCH:
    case IDM_BUILD_COMPILE:
        if (actions->compile_launch != 0) actions->compile_launch(actions->userdata);
        return 1;
    case IDM_BUILD_BYTECODE:
        if (actions->artifact != 0) actions->artifact(actions->userdata, "bytecode");
        return 1;
    case IDM_BUILD_IR:
        if (actions->artifact != 0) actions->artifact(actions->userdata, "ir");
        return 1;
    case IDM_BUILD_EVIDENCE:
        if (actions->artifact != 0) actions->artifact(actions->userdata, "evidence");
        return 1;
    case IDM_BUILD_C:
        if (actions->artifact != 0) actions->artifact(actions->userdata, "cgen");
        return 1;
    case IDM_BUILD_ASM:
        if (actions->artifact != 0) actions->artifact(actions->userdata, "asmgen");
        return 1;
    case IDM_BUILD_8086:
        if (actions->artifact != 0) actions->artifact(actions->userdata, "asm8086");
        return 1;
    case IDM_ARTIFACTS_BUILD_FOLDER:
        if (actions->open_build_folder != 0) actions->open_build_folder(actions->userdata);
        return 1;
    case IDM_ARTIFACTS_WORKSPACE_FOLDER:
        if (actions->open_workspace_folder != 0) actions->open_workspace_folder(actions->userdata);
        return 1;
    case IDM_SETTINGS_LANG_EN:
        if (actions->set_language != 0) actions->set_language(actions->userdata, PST_LANG_EN);
        return 1;
    case IDM_SETTINGS_LANG_DE:
        if (actions->set_language != 0) actions->set_language(actions->userdata, PST_LANG_DE);
        return 1;
    case IDM_HELP_COMMANDS:
        if (actions->show_commands != 0) actions->show_commands(actions->userdata);
        return 1;
    case IDM_HELP_ABOUT:
        if (actions->show_about != 0) actions->show_about(actions->userdata);
        return 1;
    default:
        return 0;
    }
}
