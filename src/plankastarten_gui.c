#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "plankac.h"
#include "plankastarten_compile.h"
#include "plankastarten_gui_i18n.h"
#include "plankastarten_gui_ids.h"
#include "plankastarten_gui_menu.h"
#include "plankastarten_gui_popup.h"
#include "plankastarten_gui_theme.h"

#define PS_MAX_FILES 512
#define PS_MAX_PATH_TEXT 512
#define PS_MAX_TEXT 65536
#define PS_MAX_LINE 1024
#define PS_MIN_WINDOW_W 980
#define PS_MIN_WINDOW_H 620
#define PS_MAX_TREE_DEPTH 24

typedef struct PS_APP {
    HWND hwnd;
    HWND top_band;
    HWND title;
    HWND subtitle;
    HWND menu_bar;
    PST_MENU_HANDLES menus;
    PST_POPUP popup;
    HWND workspace_label;
    HWND tool_panel;
    HWND source_group;
    HWND backend_group;
    HWND project_label;
    HWND editor_label;
    HWND procs_label;
    HWND command_label;
    HWND output_label;
    HWND workspace_edit;
    HWND file_tree;
    HWND line_numbers;
    HWND editor;
    HWND proc_list;
    HWND proc_label;
    HWND proc_name;
    HWND args_label;
    HWND args_edit;
    HWND result_label;
    HWND result_edit;
    HWND command_edit;
    HWND console;
    HWND status;
    PST_THEME theme;
    char workspace[PS_MAX_PATH_TEXT];
    char files[PS_MAX_FILES][PS_MAX_PATH_TEXT];
    int file_count;
    int selected_file;
    int loading_tree;
    PST_LANG lang;
    HTREEITEM first_file_item;
} PS_APP;

static PS_APP g_app;

static void ps_update_line_numbers(void);
static void ps_check_project(void);
static void ps_run_proc(void);
static void ps_save_current(void);
static void ps_write_artifact(const char *kind);
static void ps_compile_active(void);
static void ps_select_folder(void);
static void ps_apply_language(void);

static void ps_set_min_track(MINMAXINFO *mmi)
{
    mmi->ptMinTrackSize.x = PS_MIN_WINDOW_W;
    mmi->ptMinTrackSize.y = PS_MIN_WINDOW_H;
}

static void ps_clamp_windowpos(WINDOWPOS *pos)
{
    if ((pos->flags & SWP_NOSIZE) != 0) {
        return;
    }
    if (pos->cx < PS_MIN_WINDOW_W) {
        pos->cx = PS_MIN_WINDOW_W;
    }
    if (pos->cy < PS_MIN_WINDOW_H) {
        pos->cy = PS_MIN_WINDOW_H;
    }
}

static void ps_enforce_min_window(HWND hwnd)
{
    RECT rc;
    int ww;
    int wh;
    int nw;
    int nh;

    GetWindowRect(hwnd, &rc);
    ww = rc.right - rc.left;
    wh = rc.bottom - rc.top;
    nw = ww < PS_MIN_WINDOW_W ? PS_MIN_WINDOW_W : ww;
    nh = wh < PS_MIN_WINDOW_H ? PS_MIN_WINDOW_H : wh;
    if (nw != ww || nh != wh) {
        SetWindowPos(hwnd, 0, 0, 0, nw, nh,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

static void ps_set_status(const char *text)
{
    SetWindowTextA(g_app.status, text);
}

static void ps_update_cursor_status(void)
{
    DWORD sel;
    int pos;
    int line;
    int line_start;
    int col;
    char text[256];
    const char *ready;
    const char *line_word;
    const char *col_word;
    const char *source_word;

    if (g_app.editor == 0) {
        return;
    }
    sel = (DWORD)SendMessageA(g_app.editor, EM_GETSEL, 0, 0);
    pos = LOWORD(sel);
    line = (int)SendMessageA(g_app.editor, EM_LINEFROMCHAR, pos, 0);
    line_start = (int)SendMessageA(g_app.editor, EM_LINEINDEX, line, 0);
    col = pos - line_start;
    ready = g_app.lang == PST_LANG_DE ? "Bereit" : "Ready";
    line_word = g_app.lang == PST_LANG_DE ? "Zl" : "Ln";
    col_word = g_app.lang == PST_LANG_DE ? "Sp" : "Col";
    source_word = g_app.lang == PST_LANG_DE
        ? "PLK-Quelle via PlankaC-API"
        : "PLK source via PlankaC API";
    snprintf(text, sizeof(text),
        "%s  |  %s %d, %s %d  |  %s",
        ready, line_word, line + 1, col_word, col + 1, source_word);
    ps_set_status(text);
}

static void ps_sync_line_number_scroll(void)
{
    int editor_first;
    int gutter_first;

    if (g_app.editor == 0 || g_app.line_numbers == 0) {
        return;
    }
    editor_first = (int)SendMessageA(g_app.editor,
        EM_GETFIRSTVISIBLELINE, 0, 0);
    gutter_first = (int)SendMessageA(g_app.line_numbers,
        EM_GETFIRSTVISIBLELINE, 0, 0);
    if (editor_first != gutter_first) {
        SendMessageA(g_app.line_numbers, EM_LINESCROLL, 0,
            editor_first - gutter_first);
    }
}

static void ps_append_console(const char *text)
{
    int len;

    len = GetWindowTextLengthA(g_app.console);
    SendMessageA(g_app.console, EM_SETSEL, len, len);
    SendMessageA(g_app.console, EM_REPLACESEL, 0, (LPARAM)text);
    SendMessageA(g_app.console, EM_REPLACESEL, 0, (LPARAM)"\r\n");
}

static void ps_appendf(const char *fmt, ...)
{
    char text[PS_MAX_LINE];
    va_list args;

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    ps_append_console(text);
}

static void ps_set_result(const char *text)
{
    if (g_app.result_edit != 0) {
        SetWindowTextA(g_app.result_edit, text != 0 ? text : "");
    }
}

static void ps_join_path(char *out, unsigned out_size, const char *dir,
    const char *name)
{
    unsigned n;

    snprintf(out, out_size, "%s", dir);
    n = (unsigned)strlen(out);
    if (n > 0 && out[n - 1] != '\\' && out[n - 1] != '/') {
        strncat(out, "\\", out_size - strlen(out) - 1);
    }
    strncat(out, name, out_size - strlen(out) - 1);
}

static int ps_ends_with_plk(const char *name)
{
    unsigned n;

    n = (unsigned)strlen(name);
    if (n < 4) {
        return 0;
    }
    return _stricmp(name + n - 4, ".plk") == 0;
}

static int ps_read_file(const char *path, char *out, unsigned out_size)
{
    FILE *f;
    size_t n;

    f = fopen(path, "rb");
    if (f == 0) {
        return 0;
    }
    n = fread(out, 1, out_size - 1, f);
    fclose(f);
    out[n] = '\0';
    return 1;
}

static int ps_text_has_executable_source(const char *text)
{
    const char *p;

    p = text;
    while (p != 0 && *p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            ++p;
        }
        if (*p == '#') {
            while (*p != '\0' && *p != '\n') {
                ++p;
            }
            continue;
        }
        if (p[0] == 'P' && p[1] >= '0' && p[1] <= '9') {
            return 1;
        }
        if (strncmp(p, "PAGE", 4) == 0 || strncmp(p, "PROC", 4) == 0) {
            return 1;
        }
        while (*p != '\0' && *p != '\n') {
            ++p;
        }
    }
    return 0;
}

static int ps_file_has_executable_source(const char *path)
{
    char text[PS_MAX_TEXT];

    if (!ps_read_file(path, text, sizeof(text))) {
        return 0;
    }
    return ps_text_has_executable_source(text);
}

static char *ps_read_text_alloc(const char *path)
{
    FILE *f;
    long size;
    char *text;
    size_t n;

    f = fopen(path, "rb");
    if (f == 0) {
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return 0;
    }
    rewind(f);
    text = (char *)malloc((size_t)size + 1);
    if (text == 0) {
        fclose(f);
        return 0;
    }
    n = fread(text, 1, (size_t)size, f);
    fclose(f);
    text[n] = '\0';
    return text;
}

static void ps_append_file_tail(const char *title, const char *path)
{
    char *text;
    char *line;
    char *next;
    int printed;

    text = ps_read_text_alloc(path);
    if (text == 0 || text[0] == '\0') {
        free(text);
        return;
    }
    ps_appendf("%s:", title);
    printed = 0;
    line = text;
    while (line != 0 && *line != '\0' && printed < 40) {
        char *end;

        end = strchr(line, '\n');
        if (end != 0) {
            *end = '\0';
            next = end + 1;
        } else {
            next = 0;
        }
        while (*line == '\r') {
            ++line;
        }
        if (line[0] != '\0') {
            unsigned len;

            len = (unsigned)strlen(line);
            while (len > 0 && (line[len - 1] == '\r'
                    || line[len - 1] == '\n')) {
                line[--len] = '\0';
            }
            if (line[0] != '\0') {
                ps_appendf("  %s", line);
                ++printed;
            }
        }
        line = next;
    }
    free(text);
}

static void ps_to_windows_newlines(const char *in, char *out,
    unsigned out_size)
{
    unsigned n;

    n = 0;
    while (in != 0 && *in != '\0' && n + 2 < out_size) {
        if (*in == '\r') {
            out[n++] = *in++;
            if (*in == '\n' && n + 1 < out_size) {
                out[n++] = *in++;
            }
        } else if (*in == '\n') {
            out[n++] = '\r';
            out[n++] = '\n';
            ++in;
        } else {
            out[n++] = *in++;
        }
    }
    out[n] = '\0';
}

static void ps_to_file_newlines(char *text)
{
    char *read;
    char *write;

    read = text;
    write = text;
    while (*read != '\0') {
        if (read[0] == '\r' && read[1] == '\n') {
            *write++ = '\n';
            read += 2;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

static void ps_format_plk_in_editor(void)
{
    char *raw;
    char *normalized;
    char *formatted;
    char *line;
    char *next;
    int len;
    unsigned out_size;
    unsigned out_pos;

    len = GetWindowTextLengthA(g_app.editor);
    raw = (char *)malloc((size_t)len + 1);
    normalized = (char *)malloc((size_t)len + 1);
    formatted = (char *)malloc((size_t)len * 2 + 4096);
    if (raw == 0 || normalized == 0 || formatted == 0) {
        free(raw);
        free(normalized);
        free(formatted);
        ps_append_console("format failed: out of memory");
        return;
    }
    GetWindowTextA(g_app.editor, raw, len + 1);
    strcpy(normalized, raw);
    ps_to_file_newlines(normalized);

    out_size = (unsigned)len * 2 + 4096;
    out_pos = 0;
    formatted[0] = '\0';
    line = normalized;
    while (line != 0 && *line != '\0') {
        char *end;
        char saved;
        char *trim;
        int is_end;

        end = strchr(line, '\n');
        if (end != 0) {
            saved = *end;
            *end = '\0';
            next = end + 1;
        } else {
            saved = '\0';
            next = 0;
        }
        trim = line + strlen(line);
        while (trim > line && (trim[-1] == ' ' || trim[-1] == '\t'
                || trim[-1] == '\r')) {
            --trim;
        }
        *trim = '\0';
        is_end = strcmp(line, "END") == 0;
        if (out_pos + strlen(line) + 4 < out_size) {
            strcpy(formatted + out_pos, line);
            out_pos += (unsigned)strlen(line);
            formatted[out_pos++] = '\r';
            formatted[out_pos++] = '\n';
            formatted[out_pos] = '\0';
            if (is_end && next != 0 && next[0] != '\n'
                    && out_pos + 3 < out_size) {
                formatted[out_pos++] = '\r';
                formatted[out_pos++] = '\n';
                formatted[out_pos] = '\0';
            }
        }
        if (end != 0) {
            *end = saved;
        }
        line = next;
    }
    SetWindowTextA(g_app.editor, formatted);
    ps_update_line_numbers();
    ps_append_console("formatted current editor buffer");
    ps_set_status("Formatted");
    free(raw);
    free(normalized);
    free(formatted);
}

static int ps_write_file(const char *path, const char *text)
{
    FILE *f;

    f = fopen(path, "wb");
    if (f == 0) {
        return 0;
    }
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    return 1;
}

static int ps_editor_line_count(void)
{
    int count;

    count = (int)SendMessageA(g_app.editor, EM_GETLINECOUNT, 0, 0);
    return count > 0 ? count : 1;
}

static void ps_update_line_numbers(void)
{
    int lines;
    int i;
    unsigned pos;
    char *text;
    char number[32];

    if (g_app.line_numbers == 0 || g_app.editor == 0) {
        return;
    }
    lines = ps_editor_line_count();
    text = (char *)malloc((size_t)lines * 8 + 1);
    if (text == 0) {
        return;
    }
    pos = 0;
    text[0] = '\0';
    for (i = 1; i <= lines; ++i) {
        snprintf(number, sizeof(number), "%4d\r\n", i);
        if (pos + strlen(number) + 1 < (unsigned)lines * 8 + 1) {
            strcpy(text + pos, number);
            pos += (unsigned)strlen(number);
        }
    }
    SetWindowTextA(g_app.line_numbers, text);
    free(text);
    ps_sync_line_number_scroll();
    ps_update_cursor_status();
}

static void ps_load_selected_file(void)
{
    char text[PS_MAX_TEXT];
    char display[PS_MAX_TEXT * 2];

    if (g_app.selected_file < 0 || g_app.selected_file >= g_app.file_count) {
        SetWindowTextA(g_app.editor, "");
        return;
    }
    if (!ps_read_file(g_app.files[g_app.selected_file], text, sizeof(text))) {
        ps_appendf("cannot read %s", g_app.files[g_app.selected_file]);
        return;
    }
    ps_to_windows_newlines(text, display, sizeof(display));
    SetWindowTextA(g_app.editor, display);
    ps_update_line_numbers();
    ps_appendf("opened %s", g_app.files[g_app.selected_file]);
    ps_check_project();
}

static const char *ps_path_leaf(const char *path)
{
    const char *leaf;
    const char *p;

    leaf = path;
    for (p = path; p != 0 && *p != '\0'; ++p) {
        if (*p == '\\' || *p == '/') {
            leaf = p + 1;
        }
    }
    return *leaf == '\0' ? path : leaf;
}

static int ps_skip_tree_dir(const char *name)
{
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 1;
    }
    if (strcmp(name, ".git") == 0 || strcmp(name, "build") == 0) {
        return 1;
    }
    return 0;
}

static HTREEITEM ps_tree_insert(const char *label, HTREEITEM parent,
    LPARAM value)
{
    TVINSERTSTRUCTA item;

    memset(&item, 0, sizeof(item));
    item.hParent = parent;
    item.hInsertAfter = TVI_LAST;
    item.item.mask = TVIF_TEXT | TVIF_PARAM;
    item.item.pszText = (LPSTR)label;
    item.item.lParam = value;
    return TreeView_InsertItem(g_app.file_tree, &item);
}

static void ps_scan_tree_dir(const char *dir, HTREEITEM parent, int depth)
{
    WIN32_FIND_DATAA data;
    HANDLE find;
    char pattern[PS_MAX_PATH_TEXT];
    char child_path[PS_MAX_PATH_TEXT];
    HTREEITEM folder;
    HTREEITEM child;
    int file_index;

    if (depth > PS_MAX_TREE_DEPTH) {
        return;
    }

    ps_join_path(pattern, sizeof(pattern), dir, "*");
    find = FindFirstFileA(pattern, &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
                    && !ps_skip_tree_dir(data.cFileName)) {
                ps_join_path(child_path, sizeof(child_path), dir,
                    data.cFileName);
                folder = ps_tree_insert(data.cFileName, parent, 0);
                ps_scan_tree_dir(child_path, folder, depth + 1);
                child = TreeView_GetChild(g_app.file_tree, folder);
                if (child == 0) {
                    TreeView_DeleteItem(g_app.file_tree, folder);
                } else if (depth < 2) {
                    TreeView_Expand(g_app.file_tree, folder, TVE_EXPAND);
                }
            }
        } while (FindNextFileA(find, &data));
        FindClose(find);
    }

    find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                && ps_ends_with_plk(data.cFileName)
                && g_app.file_count < PS_MAX_FILES) {
            file_index = g_app.file_count;
            ps_join_path(g_app.files[file_index],
                sizeof(g_app.files[file_index]), dir, data.cFileName);
            child = ps_tree_insert(data.cFileName, parent,
                (LPARAM)(file_index + 1));
            if (g_app.first_file_item == 0) {
                g_app.first_file_item = child;
            }
            ++g_app.file_count;
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
}

static void ps_scan_workspace(void)
{
    const char *root_label;
    HTREEITEM root;

    GetWindowTextA(g_app.workspace_edit, g_app.workspace,
        sizeof(g_app.workspace));
    TreeView_DeleteAllItems(g_app.file_tree);
    g_app.file_count = 0;
    g_app.selected_file = -1;
    g_app.loading_tree = 0;
    g_app.first_file_item = 0;

    root_label = ps_path_leaf(g_app.workspace);
    root = ps_tree_insert(root_label, TVI_ROOT, 0);
    ps_scan_tree_dir(g_app.workspace, root, 0);
    TreeView_Expand(g_app.file_tree, root, TVE_EXPAND);

    if (g_app.file_count == 0) {
        ps_set_status("No .plk files found");
        ps_appendf("no .plk files in %s", g_app.workspace);
        return;
    }

    g_app.selected_file = 0;
    if (g_app.first_file_item != 0) {
        g_app.loading_tree = 1;
        TreeView_SelectItem(g_app.file_tree, g_app.first_file_item);
        g_app.loading_tree = 0;
    }
    ps_load_selected_file();
    ps_set_status("Workspace loaded");
    ps_appendf("workspace: %s (%d .plk file%s)", g_app.workspace,
        g_app.file_count, g_app.file_count == 1 ? "" : "s");
}

static void ps_parent_dir(const char *path, char *out, unsigned out_size)
{
    char *slash;
    char *alt;

    if (out_size == 0) {
        return;
    }
    strncpy(out, path != 0 ? path : "", out_size - 1);
    out[out_size - 1] = '\0';
    slash = strrchr(out, '\\');
    alt = strrchr(out, '/');
    if (alt != 0 && (slash == 0 || alt > slash)) {
        slash = alt;
    }
    if (slash != 0) {
        *slash = '\0';
    }
}

static int ps_path_equal(const char *a, const char *b)
{
    if (a == 0 || b == 0) {
        return 0;
    }
    return _stricmp(a, b) == 0;
}

static int ps_dir_exists(const char *path)
{
    DWORD attr;

    attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES
        && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static int ps_source_prefix_number(const char *path)
{
    const char *name;
    int value;
    int digits;

    name = ps_path_leaf(path);
    value = 0;
    digits = 0;
    while (*name >= '0' && *name <= '9') {
        value = value * 10 + (*name - '0');
        ++name;
        ++digits;
    }
    if (digits == 0 || *name != '_') {
        return -1;
    }
    return value;
}

static int ps_add_context_source(const char **sources, int *count,
    const char *path)
{
    int i;

    if (sources == 0 || count == 0 || path == 0 || path[0] == '\0') {
        return 0;
    }
    for (i = 0; i < *count; ++i) {
        if (ps_path_equal(sources[i], path)) {
            return 1;
        }
    }
    if (*count >= PS_MAX_FILES) {
        return 0;
    }
    sources[*count] = path;
    *count += 1;
    return 1;
}

static int ps_add_external_context_source(const char **sources, int *count,
    char storage[][PS_MAX_PATH_TEXT], int *storage_count, const char *path)
{
    int i;

    if (sources == 0 || count == 0 || storage == 0 || storage_count == 0
            || path == 0 || path[0] == '\0') {
        return 0;
    }
    for (i = 0; i < *count; ++i) {
        if (ps_path_equal(sources[i], path)) {
            return 1;
        }
    }
    if (*count >= PS_MAX_FILES || *storage_count >= PS_MAX_FILES) {
        return 0;
    }
    strncpy(storage[*storage_count], path, PS_MAX_PATH_TEXT - 1);
    storage[*storage_count][PS_MAX_PATH_TEXT - 1] = '\0';
    sources[*count] = storage[*storage_count];
    *count += 1;
    *storage_count += 1;
    return 1;
}

static void ps_add_numbered_sources_from_dir(const char **sources,
    int *count, const char *dir, int max_prefix)
{
    char file_dir[PS_MAX_PATH_TEXT];
    int prefix;
    int i;

    for (prefix = 0; prefix <= max_prefix && prefix < 90; ++prefix) {
        for (i = 0; i < g_app.file_count; ++i) {
            ps_parent_dir(g_app.files[i], file_dir, sizeof(file_dir));
            if (ps_path_equal(file_dir, dir)
                    && ps_source_prefix_number(g_app.files[i]) == prefix) {
                ps_add_context_source(sources, count, g_app.files[i]);
            }
        }
    }
}

static void ps_add_numbered_sources_from_disk(const char **sources,
    int *count, char storage[][PS_MAX_PATH_TEXT], int *storage_count,
    const char *dir, int max_prefix)
{
    WIN32_FIND_DATAA data;
    HANDLE find;
    char pattern[PS_MAX_PATH_TEXT];
    char path[PS_MAX_PATH_TEXT];
    int prefix;

    for (prefix = 0; prefix <= max_prefix && prefix < 90; ++prefix) {
        snprintf(pattern, sizeof(pattern), "%s\\%02d_*.plk", dir, prefix);
        find = FindFirstFileA(pattern, &data);
        if (find == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                ps_join_path(path, sizeof(path), dir, data.cFileName);
                ps_add_external_context_source(sources, count, storage,
                    storage_count, path);
            }
        } while (FindNextFileA(find, &data));
        FindClose(find);
    }
}

static int ps_project_root_for_examples(const char *active_dir,
    char *root, unsigned root_size)
{
    char src_dir[PS_MAX_PATH_TEXT];

    if (active_dir == 0 || root == 0 || root_size == 0) {
        return 0;
    }
    if (_stricmp(ps_path_leaf(active_dir), "examples") != 0) {
        return 0;
    }
    ps_parent_dir(active_dir, root, root_size);
    ps_join_path(src_dir, sizeof(src_dir), root, "src");
    return ps_dir_exists(src_dir);
}

static int ps_context_load_active(PLANKAC_CONTEXT **ctx_out)
{
    PLANKAC_CONTEXT *ctx;
    const char *sources[PS_MAX_FILES + 1];
    char external_sources[PS_MAX_FILES][PS_MAX_PATH_TEXT];
    char err[PS_MAX_LINE];
    char active_dir[PS_MAX_PATH_TEXT];
    char project_root[PS_MAX_PATH_TEXT];
    char project_src[PS_MAX_PATH_TEXT];
    int active_prefix;
    int count;
    int external_count;

    if (g_app.selected_file < 0 || g_app.selected_file >= g_app.file_count) {
        ps_append_console("no active .plk file");
        return 0;
    }
    if (!ps_file_has_executable_source(g_app.files[g_app.selected_file])) {
        ps_appendf("document only: %s", g_app.files[g_app.selected_file]);
        ps_append_console("select a source file with P... procedures or PAGE blocks to run");
        return 0;
    }
    ctx = plankac_create();
    if (ctx == 0) {
        ps_append_console("cannot create PlankaC context");
        return 0;
    }
    ps_parent_dir(g_app.files[g_app.selected_file], active_dir,
        sizeof(active_dir));
    active_prefix = ps_source_prefix_number(g_app.files[g_app.selected_file]);
    count = 0;
    external_count = 0;
    if (active_prefix >= 0 && active_prefix < 90) {
        ps_add_numbered_sources_from_dir(sources, &count, active_dir,
            active_prefix);
    } else if (ps_project_root_for_examples(active_dir, project_root,
            sizeof(project_root))) {
        ps_join_path(project_src, sizeof(project_src), project_root, "src");
        ps_add_numbered_sources_from_disk(sources, &count, external_sources,
            &external_count, project_src, 89);
    }
    if (!ps_add_context_source(sources, &count,
            g_app.files[g_app.selected_file])) {
        ps_append_console("too many source files for context");
        plankac_destroy(ctx);
        return 0;
    }
    sources[count] = 0;
    err[0] = '\0';
    if (!plankac_context_load_sources(ctx, sources, err, sizeof(err))) {
        ps_appendf("load failed: %s", err);
        plankac_destroy(ctx);
        return 0;
    }
    *ctx_out = ctx;
    return 1;
}

static void ps_default_args_for_count(int argc, char *out,
    unsigned out_size)
{
    int i;

    if (out_size == 0) {
        return;
    }
    out[0] = '\0';
    for (i = 0; i < argc; ++i) {
        char value[32];

        snprintf(value, sizeof(value), "%s%d", i == 0 ? "" : " ", i + 1);
        strncat(out, value, out_size - strlen(out) - 1);
    }
}

static void ps_set_run_fields(const PLANKAC_PROC_INFO *info)
{
    char args[256];
    char result[256];

    if (info == 0) {
        return;
    }
    SetWindowTextA(g_app.proc_name, info->name);
    ps_default_args_for_count(info->argc, args, sizeof(args));
    SetWindowTextA(g_app.args_edit, args);
    snprintf(result, sizeof(result), "Ready: P%d %s  (%d -> %d)",
        info->number, info->name, info->argc, info->results);
    ps_set_result(result);
}

static int ps_get_proc_info_by_name(PLANKAC_CONTEXT *ctx, const char *name,
    PLANKAC_PROC_INFO *info)
{
    char numbered[32];
    int i;
    int n;

    if (ctx == 0 || name == 0 || name[0] == '\0' || info == 0) {
        return 0;
    }
    if (plankac_context_find_proc(ctx, name, info)) {
        return 1;
    }
    n = plankac_context_proc_count(ctx);
    for (i = 0; i < n; ++i) {
        if (plankac_context_get_proc(ctx, i, info)) {
            snprintf(numbered, sizeof(numbered), "P%d", info->number);
            if (_stricmp(numbered, name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int ps_get_selected_proc_info(PLANKAC_CONTEXT *ctx,
    PLANKAC_PROC_INFO *info)
{
    char line[256];
    char name[128];
    int sel;

    GetWindowTextA(g_app.proc_name, name, sizeof(name));
    if (ps_get_proc_info_by_name(ctx, name, info)) {
        return 1;
    }
    sel = (int)SendMessageA(g_app.proc_list, LB_GETCURSEL, 0, 0);
    if (sel >= 0) {
        SendMessageA(g_app.proc_list, LB_GETTEXT, sel, (LPARAM)line);
        name[0] = '\0';
        sscanf(line, "P%*d %127s", name);
        if (ps_get_proc_info_by_name(ctx, name, info)) {
            return 1;
        }
    }
    return 0;
}

static int ps_name_ends_with(const char *text, const char *suffix)
{
    unsigned a;
    unsigned b;

    if (text == 0 || suffix == 0) {
        return 0;
    }
    a = (unsigned)strlen(text);
    b = (unsigned)strlen(suffix);
    return a >= b && _stricmp(text + a - b, suffix) == 0;
}

static int ps_proc_score(const PLANKAC_PROC_INFO *info)
{
    int score;

    if (info == 0) {
        return -1000000;
    }
    if (_stricmp(info->name, "type_sheet") == 0) {
        return -1000000;
    }
    score = info->number;
    if (_stricmp(info->name, "start") == 0) {
        score += 100000;
    } else if (_stricmp(info->name, "main") == 0) {
        score += 95000;
    } else if (strstr(info->name, "_app") != 0) {
        score += 85000;
    } else if (ps_name_ends_with(info->name, "_demo")) {
        score += 75000;
    } else if (ps_name_ends_with(info->name, "_session")) {
        score += 65000;
    }
    if (info->argc == 0) {
        score += 500;
    }
    return score;
}

static void ps_select_default_proc(PLANKAC_CONTEXT *ctx)
{
    PLANKAC_PROC_INFO info;
    PLANKAC_PROC_INFO best;
    int i;
    int n;
    int best_index;
    int best_score;
    int score;

    memset(&best, 0, sizeof(best));
    best_index = -1;
    best_score = -1000000;
    n = plankac_context_proc_count(ctx);
    for (i = 0; i < n; ++i) {
        if (plankac_context_get_proc(ctx, i, &info)) {
            score = ps_proc_score(&info);
            if (score > best_score) {
                best = info;
                best_index = i;
                best_score = score;
            }
        }
    }
    if (best_index >= 0) {
        SendMessageA(g_app.proc_list, LB_SETCURSEL, best_index, 0);
        ps_set_run_fields(&best);
    }
}

static void ps_update_proc_from_list(int run_after_select)
{
    PLANKAC_CONTEXT *ctx;
    PLANKAC_PROC_INFO info;
    char line[256];
    char name[128];
    int sel;

    sel = (int)SendMessageA(g_app.proc_list, LB_GETCURSEL, 0, 0);
    if (sel < 0) {
        return;
    }
    SendMessageA(g_app.proc_list, LB_GETTEXT, sel, (LPARAM)line);
    name[0] = '\0';
    sscanf(line, "P%*d %127s", name);
    if (name[0] == '\0') {
        return;
    }
    if (!ps_context_load_active(&ctx)) {
        ps_set_status("Load failed");
        return;
    }
    if (ps_get_proc_info_by_name(ctx, name, &info)) {
        ps_set_run_fields(&info);
        ps_set_status(run_after_select ? "Running selected procedure"
            : "Procedure selected");
    }
    plankac_destroy(ctx);
    if (run_after_select) {
        ps_run_proc();
    }
}

static void ps_fill_procedures(PLANKAC_CONTEXT *ctx)
{
    PLANKAC_PROC_INFO info;
    char line[256];
    int i;
    int n;

    SendMessageA(g_app.proc_list, LB_RESETCONTENT, 0, 0);
    n = plankac_context_proc_count(ctx);
    for (i = 0; i < n; ++i) {
        if (plankac_context_get_proc(ctx, i, &info)) {
            snprintf(line, sizeof(line), "P%d %s  (%d -> %d)",
                info.number, info.name, info.argc, info.results);
            SendMessageA(g_app.proc_list, LB_ADDSTRING, 0, (LPARAM)line);
        }
    }
    if (n > 0) {
        SendMessageA(g_app.proc_list, LB_SETCURSEL, 0, 0);
    }
}

static void ps_check_project(void)
{
    PLANKAC_CONTEXT *ctx;
    char summary[256];

    if (g_app.selected_file >= 0 && g_app.selected_file < g_app.file_count
            && !ps_file_has_executable_source(
                g_app.files[g_app.selected_file])) {
        SendMessageA(g_app.proc_list, LB_RESETCONTENT, 0, 0);
        SetWindowTextA(g_app.proc_name, "");
        SetWindowTextA(g_app.args_edit, "");
        ps_set_result("Document source: no runnable procedures");
        ps_set_status("Document source");
        return;
    }
    if (!ps_context_load_active(&ctx)) {
        ps_set_status("Load failed");
        return;
    }
    plankac_context_summary(ctx, summary, sizeof(summary));
    ps_fill_procedures(ctx);
    ps_select_default_proc(ctx);
    ps_append_console(summary);
    ps_update_cursor_status();
    plankac_destroy(ctx);
}

static int ps_parse_args(char *text, double *args)
{
    int count;
    char *tok;

    count = 0;
    tok = strtok(text, " \t,;");
    while (tok != 0 && count < PLANKAC_MAX_ARGS) {
        args[count++] = strtod(tok, 0);
        tok = strtok(0, " \t,;");
    }
    return count;
}

static void ps_run_proc(void)
{
    PLANKAC_CONTEXT *ctx;
    PLANKAC_PROC_INFO info;
    PLANKAC_RESULT result;
    double args[PLANKAC_MAX_ARGS];
    char proc[128];
    char arg_text[256];
    char default_args[256];
    char err[PS_MAX_LINE];
    char value[64];
    char line[PS_MAX_LINE + 128];
    int argc;
    int i;

    if (!ps_context_load_active(&ctx)) {
        ps_set_status("Load failed");
        return;
    }
    if (!ps_get_selected_proc_info(ctx, &info)) {
        ps_append_console("run failed: no procedure selected");
        ps_set_result("No procedure selected");
        ps_set_status("Run failed");
        plankac_destroy(ctx);
        return;
    }
    SetWindowTextA(g_app.proc_name, info.name);
    strncpy(proc, info.name, sizeof(proc) - 1);
    proc[sizeof(proc) - 1] = '\0';
    GetWindowTextA(g_app.args_edit, arg_text, sizeof(arg_text));
    if (arg_text[0] == '\0' && info.argc > 0) {
        ps_default_args_for_count(info.argc, default_args,
            sizeof(default_args));
        SetWindowTextA(g_app.args_edit, default_args);
        strncpy(arg_text, default_args, sizeof(arg_text) - 1);
        arg_text[sizeof(arg_text) - 1] = '\0';
    }
    argc = ps_parse_args(arg_text, args);
    if (argc != info.argc) {
        snprintf(line, sizeof(line),
            "run failed: %s expects %d argument(s), got %d",
            info.name, info.argc, argc);
        ps_append_console(line);
        ps_set_result(line);
        ps_set_status("Run failed");
        plankac_destroy(ctx);
        return;
    }
    err[0] = '\0';
    if (!plankac_context_run(ctx, proc, args, argc, &result,
            err, sizeof(err))) {
        snprintf(line, sizeof(line), "run failed: %s", err);
        ps_append_console(line);
        ps_set_result(line);
        ps_set_status("Run failed");
        plankac_destroy(ctx);
        return;
    }
    snprintf(line, sizeof(line), "%s ->", proc);
    for (i = 0; i < result.count; ++i) {
        plankac_format(result.value[i], value, sizeof(value));
        strncat(line, " R", sizeof(line) - strlen(line) - 1);
        {
            char index_text[16];

            snprintf(index_text, sizeof(index_text), "%d=", i);
            strncat(line, index_text, sizeof(line) - strlen(line) - 1);
        }
        strncat(line, value, sizeof(line) - strlen(line) - 1);
    }
    ps_set_result(line);
    ps_append_console(line);
    ps_set_status("Ran procedure");
    plankac_destroy(ctx);
}

static void ps_save_current(void)
{
    char *text;
    int len;

    if (g_app.selected_file < 0 || g_app.selected_file >= g_app.file_count) {
        ps_append_console("no selected file");
        return;
    }
    len = GetWindowTextLengthA(g_app.editor);
    text = (char *)malloc((size_t)len + 1);
    if (text == 0) {
        ps_append_console("out of memory");
        return;
    }
    GetWindowTextA(g_app.editor, text, len + 1);
    ps_to_file_newlines(text);
    if (!ps_write_file(g_app.files[g_app.selected_file], text)) {
        ps_appendf("save failed: %s", g_app.files[g_app.selected_file]);
        free(text);
        return;
    }
    free(text);
    ps_appendf("saved %s", g_app.files[g_app.selected_file]);
    ps_set_status("Saved");
    ps_update_line_numbers();
}

static void ps_make_build_path(char *out, unsigned out_size,
    const char *name)
{
    CreateDirectoryA("build", 0);
    ps_join_path(out, out_size, "build", name);
}

static void ps_write_artifact(const char *kind)
{
    PLANKAC_CONTEXT *ctx;
    char path[PS_MAX_PATH_TEXT];
    char err[PS_MAX_LINE];
    int ok;

    if (!ps_context_load_active(&ctx)) {
        ps_set_status("Load failed");
        return;
    }
    if (strcmp(kind, "bytecode") == 0) {
        ps_make_build_path(path, sizeof(path), "plankastarten.pbc");
        ok = plankac_context_write_bytecode(ctx, path, err, sizeof(err));
    } else if (strcmp(kind, "ir") == 0) {
        ps_make_build_path(path, sizeof(path), "plankastarten.ir");
        ok = plankac_context_write_ir(ctx, path, err, sizeof(err));
    } else if (strcmp(kind, "evidence") == 0) {
        ps_make_build_path(path, sizeof(path), "plankastarten.evidence.json");
        ok = plankac_context_write_evidence(ctx, path, err, sizeof(err));
    } else if (strcmp(kind, "cgen") == 0) {
        ps_make_build_path(path, sizeof(path), "plankastarten_generated.c");
        ok = plankac_context_write_c_backend(ctx, path, err, sizeof(err));
    } else if (strcmp(kind, "asmgen") == 0) {
        ps_make_build_path(path, sizeof(path), "plankastarten_runtime.S");
        ok = plankac_context_write_asm_runtime(ctx, path, err, sizeof(err));
    } else {
        ps_make_build_path(path, sizeof(path), "plankastarten_8086.asm");
        ok = plankac_context_write_asm8086_runtime(ctx, path, err,
            sizeof(err));
    }
    plankac_destroy(ctx);
    if (!ok) {
        ps_appendf("%s failed: %s", kind, err);
        ps_set_status("Artifact failed");
        return;
    }
    ps_appendf("%s written: %s", kind, path);
    ps_set_status("Artifact written");
}

static void ps_cmd_quote(FILE *f, const char *text)
{
    fputc('"', f);
    while (text != 0 && *text != '\0') {
        if (*text == '"') {
            fputc('\\', f);
        }
        fputc(*text, f);
        ++text;
    }
    fputc('"', f);
}

static int ps_choose_console_proc(PLANKAC_CONTEXT *ctx, char *proc,
    unsigned proc_size, char *args, unsigned args_size)
{
    PLANKAC_PROC_INFO info;
    char arg_text[256];
    int i;
    int n;

    proc[0] = '\0';
    args[0] = '\0';
    if (ps_get_selected_proc_info(ctx, &info)) {
        strncpy(proc, info.name, proc_size - 1);
        proc[proc_size - 1] = '\0';
        GetWindowTextA(g_app.args_edit, arg_text, sizeof(arg_text));
        if (arg_text[0] == '\0' && info.argc > 0) {
            ps_default_args_for_count(info.argc, args, args_size);
        } else {
            strncpy(args, arg_text, args_size - 1);
            args[args_size - 1] = '\0';
        }
        return 1;
    }
    if (plankac_context_find_proc(ctx, "start", &info)) {
        strncpy(proc, "start", proc_size - 1);
        proc[proc_size - 1] = '\0';
        ps_default_args_for_count(info.argc, args, args_size);
        return 1;
    }
    n = plankac_context_proc_count(ctx);
    for (i = 0; i < n; ++i) {
        if (plankac_context_get_proc(ctx, i, &info) && info.argc == 0) {
            strncpy(proc, info.name, proc_size - 1);
            proc[proc_size - 1] = '\0';
            return 1;
        }
    }
    if (n > 0 && plankac_context_get_proc(ctx, 0, &info)) {
        strncpy(proc, info.name, proc_size - 1);
        proc[proc_size - 1] = '\0';
        ps_default_args_for_count(info.argc, args, args_size);
        return 1;
    }
    return 0;
}

static int ps_write_console_launcher(const PSC_COMPILE_RESULT *result,
    const char *proc, const char *args, char *cmd_path, unsigned cmd_size)
{
    FILE *f;
    char cwd[PS_MAX_PATH_TEXT];
    char name[PS_MAX_PATH_TEXT];
    char *dot;

    if (result == 0 || result->exe_path[0] == '\0'
            || proc == 0 || proc[0] == '\0') {
        return 0;
    }
    (void)args;
    strncpy(name, ps_path_leaf(result->exe_path), sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    dot = strrchr(name, '.');
    if (dot != 0) {
        *dot = '\0';
    }
    strncat(name, "_run.cmd", sizeof(name) - strlen(name) - 1);
    ps_make_build_path(cmd_path, cmd_size, name);
    GetCurrentDirectoryA(sizeof(cwd), cwd);
    f = fopen(cmd_path, "wb");
    if (f == 0) {
        return 0;
    }
    fprintf(f, "@echo off\r\n");
    fprintf(f, "cd /d ");
    ps_cmd_quote(f, cwd);
    fprintf(f, "\r\n");
    fprintf(f, "echo PlankaStarten run: %s\r\n", proc);
    ps_cmd_quote(f, result->exe_path);
    fprintf(f, " /interactive %s", proc);
    fprintf(f, "\r\n");
    fprintf(f, "echo.\r\n");
    fprintf(f, "echo Exit code: %%ERRORLEVEL%%\r\n");
    fprintf(f, "pause\r\n");
    fclose(f);
    return 1;
}

static void ps_compile_active(void)
{
    PLANKAC_CONTEXT *ctx;
    PSC_COMPILE_RESULT result;
    char err[PS_MAX_LINE];
    char run_proc[128];
    char run_args[256];
    char launcher[PS_MAX_PATH_TEXT];
    HINSTANCE launched;

    if (g_app.selected_file < 0 || g_app.selected_file >= g_app.file_count) {
        ps_append_console("no selected file");
        ps_set_status("Compile failed");
        return;
    }
    ps_save_current();
    if (!ps_context_load_active(&ctx)) {
        ps_set_status("Compile failed");
        return;
    }
    if (!ps_choose_console_proc(ctx, run_proc, sizeof(run_proc),
            run_args, sizeof(run_args))) {
        strcpy(run_proc, "start");
        run_args[0] = '\0';
    }
    err[0] = '\0';
    if (!psc_compile_plk(ctx, g_app.files[g_app.selected_file],
            &result, err, sizeof(err))) {
        ps_appendf("compile failed: %s", err);
        ps_appendf("compile log: %s", result.log_path);
        ps_append_file_tail("compile output", result.log_path);
        ps_set_status("Compile failed");
        plankac_destroy(ctx);
        return;
    }
    plankac_destroy(ctx);
    if (result.kind == PSC_COMPILE_GUI) {
        ps_appendf("compiled GUI exe: %s", result.exe_path);
        ps_appendf("compile log: %s", result.log_path);
        launched = ShellExecuteA(g_app.hwnd, "open", result.exe_path,
            "", "", SW_SHOWNORMAL);
        if ((INT_PTR)launched <= 32) {
            ps_append_console("GUI exe was built but could not be launched");
            ps_set_status("Compiled GUI exe");
        } else {
            ps_set_status("Compiled and launched GUI exe");
        }
    } else {
        ps_appendf("compiled console exe: %s", result.exe_path);
        ps_appendf("compile log: %s", result.log_path);
        if (ps_write_console_launcher(&result, run_proc, run_args,
                launcher, sizeof(launcher))) {
            ps_appendf("launcher written: %s", launcher);
            launched = ShellExecuteA(g_app.hwnd, "open", launcher,
                "", "", SW_SHOWNORMAL);
            if ((INT_PTR)launched <= 32) {
                ps_appendf("run manually: %s", launcher);
                ps_set_status("Compiled console exe");
            } else {
                ps_appendf("launched console: %s /interactive %s",
                    result.exe_path, run_proc);
                ps_set_status("Compiled and launched console exe");
            }
        } else {
            ps_appendf("run manually: %s /interactive %s",
                result.exe_path, run_proc);
            ps_set_status("Compiled console exe");
        }
    }
}

static void ps_execute_command(void)
{
    char command[512];
    char *verb;
    char *arg;

    GetWindowTextA(g_app.command_edit, command, sizeof(command));
    ps_appendf("> %s", command);
    verb = strtok(command, " \t");
    if (verb == 0) {
        return;
    }
    if (strcmp(verb, "check") == 0) {
        ps_check_project();
    } else if (strcmp(verb, "run") == 0) {
        arg = strtok(0, "");
        if (arg != 0) {
            char proc[128];
            char rest[256];

            proc[0] = '\0';
            rest[0] = '\0';
            sscanf(arg, "%127s %255[^\n]", proc, rest);
            SetWindowTextA(g_app.proc_name, proc);
            SetWindowTextA(g_app.args_edit, rest);
        }
        ps_run_proc();
    } else if (strcmp(verb, "save") == 0) {
        ps_save_current();
    } else if (strcmp(verb, "format") == 0) {
        ps_format_plk_in_editor();
    } else if (strcmp(verb, "bytecode") == 0) {
        ps_write_artifact("bytecode");
    } else if (strcmp(verb, "ir") == 0) {
        ps_write_artifact("ir");
    } else if (strcmp(verb, "evidence") == 0) {
        ps_write_artifact("evidence");
    } else if (strcmp(verb, "cgen") == 0) {
        ps_write_artifact("cgen");
    } else if (strcmp(verb, "asmgen") == 0) {
        ps_write_artifact("asmgen");
    } else if (strcmp(verb, "asm8086") == 0) {
        ps_write_artifact("asm8086");
    } else if (strcmp(verb, "compile") == 0 || strcmp(verb, "app") == 0) {
        ps_compile_active();
    } else {
        ps_append_console("commands: check, run <proc> [args], app, compile, format, save, bytecode, ir, evidence, cgen, asmgen, asm8086");
    }
    SetWindowTextA(g_app.command_edit, "");
}

static void ps_select_folder(void)
{
    BROWSEINFOA bi;
    LPITEMIDLIST id;
    char path[PS_MAX_PATH_TEXT];

    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = g_app.hwnd;
    bi.lpszTitle = pst_text(g_app.lang, PST_T_SELECT_FOLDER);
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    id = SHBrowseForFolderA(&bi);
    if (id != 0 && SHGetPathFromIDListA(id, path)) {
        SetWindowTextA(g_app.workspace_edit, path);
        ps_scan_workspace();
    }
}

static HWND ps_static(HWND parent, const char *text, int id, DWORD style)
{
    HWND hwnd;

    hwnd = CreateWindowExA(0, "STATIC", text,
        WS_CHILD | WS_VISIBLE | style,
        0, 0, 100, 18, parent, (HMENU)(INT_PTR)id,
        GetModuleHandleA(0), 0);
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.theme.ui_font, TRUE);
    return hwnd;
}

static HWND ps_button(HWND parent, const char *text, int id)
{
    HWND hwnd;

    hwnd = CreateWindowExA(0, "BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 24, parent, (HMENU)(INT_PTR)id,
        GetModuleHandleA(0), 0);
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.theme.font, TRUE);
    return hwnd;
}

static HWND ps_edit(HWND parent, int id, DWORD extra_style)
{
    HWND hwnd;

    hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extra_style,
        0, 0, 100, 24, parent, (HMENU)(INT_PTR)id,
        GetModuleHandleA(0), 0);
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.theme.font, TRUE);
    return hwnd;
}

static HWND ps_label(HWND parent, const char *text, int id)
{
    return ps_static(parent, text, id, SS_LEFTNOWORDWRAP);
}

static void ps_set_button_text(int id, PST_TEXT text)
{
    SetDlgItemTextA(g_app.hwnd, id, pst_text(g_app.lang, text));
}

static void ps_apply_language(void)
{
    SetWindowTextA(g_app.subtitle, pst_text(g_app.lang, PST_T_SUBTITLE));
    SetWindowTextA(g_app.workspace_label, pst_text(g_app.lang, PST_T_WORKSPACE));
    SetWindowTextA(g_app.source_group, pst_text(g_app.lang, PST_T_SOURCE));
    SetWindowTextA(g_app.backend_group, pst_text(g_app.lang, PST_T_BACKENDS));
    SetWindowTextA(g_app.project_label, pst_text(g_app.lang, PST_T_PROJECT));
    SetWindowTextA(g_app.editor_label, pst_text(g_app.lang, PST_T_EDITOR));
    SetWindowTextA(g_app.procs_label, pst_text(g_app.lang, PST_T_PROCEDURES));
    SetWindowTextA(g_app.command_label, pst_text(g_app.lang, PST_T_COMMAND));
    SetWindowTextA(g_app.output_label, pst_text(g_app.lang, PST_T_OUTPUT));
    SetWindowTextA(g_app.proc_label, pst_text(g_app.lang, PST_T_PROCEDURE));
    SetWindowTextA(g_app.args_label, pst_text(g_app.lang, PST_T_ARGUMENTS));
    SetWindowTextA(g_app.result_label, pst_text(g_app.lang, PST_T_RESULT));
    ps_set_button_text(IDC_LOAD_DIR, PST_T_OPEN);
    ps_set_button_text(IDC_SAVE, PST_T_SAVE);
    ps_set_button_text(IDC_FORMAT, PST_T_FORMAT);
    ps_set_button_text(IDC_CHECK, PST_T_CHECK);
    ps_set_button_text(IDC_RUN, PST_T_RUN);
    ps_set_button_text(IDC_COMPILE, PST_T_COMPILE);
    ps_set_button_text(IDC_CLEAR, PST_T_CLEAR);
    ps_set_button_text(IDC_EXEC, PST_T_EXEC);
    pst_menu_apply_language(&g_app.menus, g_app.lang);
    ps_set_status(pst_text(g_app.lang, PST_T_STATUS_READY));
    if (g_app.hwnd != 0) {
        InvalidateRect(g_app.hwnd, 0, TRUE);
    }
}

static void ps_layout(HWND hwnd)
{
    RECT rc;
    int w;
    int h;
    int top;
    int left_w;
    int right_w;
    int gap;
    int mid_x;
    int console_h;
    int editor_x;
    int editor_y;
    int editor_w;
    int editor_h;
    int gutter_w;
    int toolbar_y;
    int button_y;
    int bx;
    int command_label_y;
    int command_y;
    int output_label_y;
    int console_y;
    int work_h;
    int proc_h;
    int result_h;
    int right_bottom;
    int right_x;

    GetClientRect(hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    top = 0;
    left_w = 226;
    right_w = 260;
    gap = 10;
    console_h = 138;
    mid_x = 8 + left_w + gap;
    gutter_w = 54;

    MoveWindow(g_app.top_band, 0, 0, w, 26, TRUE);
    MoveWindow(g_app.title, 14, 3, 210, 20, TRUE);
    MoveWindow(g_app.subtitle, 236, 5, w - 248, 18, TRUE);
    MoveWindow(g_app.menu_bar, 0, 26, w, 24, TRUE);
    pst_menu_layout(&g_app.menus, 30, 18);

    top = 56;
    MoveWindow(g_app.status, 8, top, w - 16, 22, TRUE);
    top += 30;
    MoveWindow(g_app.workspace_label, 12, top + 4, 74, 18, TRUE);
    MoveWindow(g_app.workspace_edit, 90, top, w - 180, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_LOAD_DIR), w - 82, top, 74, 24, TRUE);
    top += 32;
    toolbar_y = top;
    MoveWindow(g_app.tool_panel, 8, toolbar_y, w - 16, 40, TRUE);
    button_y = toolbar_y + 9;
    bx = 16;
    MoveWindow(g_app.source_group, bx, button_y, 58, 24, TRUE);
    bx += 64;
    MoveWindow(GetDlgItem(hwnd, IDC_SAVE), bx, button_y, 52, 24, TRUE);
    bx += 56;
    MoveWindow(GetDlgItem(hwnd, IDC_FORMAT), bx, button_y, 62, 24, TRUE);
    bx += 66;
    MoveWindow(GetDlgItem(hwnd, IDC_CHECK), bx, button_y, 56, 24, TRUE);
    bx += 60;
    MoveWindow(GetDlgItem(hwnd, IDC_RUN), bx, button_y, 48, 24, TRUE);
    bx += 52;
    MoveWindow(GetDlgItem(hwnd, IDC_COMPILE), bx, button_y, 70, 24, TRUE);
    bx += 74;

    MoveWindow(g_app.backend_group, bx, button_y, 72, 24, TRUE);
    bx += 78;
    MoveWindow(GetDlgItem(hwnd, IDC_BYTECODE), bx, button_y, 46, 24, TRUE);
    bx += 50;
    MoveWindow(GetDlgItem(hwnd, IDC_IR), bx, button_y, 36, 24, TRUE);
    bx += 40;
    MoveWindow(GetDlgItem(hwnd, IDC_EVIDENCE), bx, button_y, 52, 24, TRUE);
    bx += 56;
    MoveWindow(GetDlgItem(hwnd, IDC_CGEN), bx, button_y, 34, 24, TRUE);
    bx += 38;
    MoveWindow(GetDlgItem(hwnd, IDC_ASMGEN), bx, button_y, 48, 24, TRUE);
    bx += 52;
    MoveWindow(GetDlgItem(hwnd, IDC_ASM8086), bx, button_y, 48, 24, TRUE);
    bx += 52;
    MoveWindow(GetDlgItem(hwnd, IDC_CLEAR), bx, button_y, 54, 24, TRUE);

    top += 52;
    command_label_y = h - console_h - 62;
    command_y = command_label_y + 18;
    output_label_y = command_y + 30;
    console_y = output_label_y + 18;
    work_h = command_label_y - top - 8;
    if (work_h < 160) {
        work_h = 160;
    }
    MoveWindow(g_app.project_label, 8, top, left_w, 18, TRUE);
    MoveWindow(g_app.file_tree, 8, top + 20, left_w, work_h - 20, TRUE);

    editor_x = mid_x;
    editor_y = top + 20;
    editor_w = w - left_w - right_w - (gap * 3) - 16;
    editor_h = work_h - 20;
    MoveWindow(g_app.editor_label, editor_x, top, editor_w, 18, TRUE);
    MoveWindow(g_app.line_numbers, editor_x, editor_y,
        gutter_w, editor_h, TRUE);
    MoveWindow(g_app.editor, editor_x + gutter_w - 1, editor_y,
        editor_w - gutter_w + 1, editor_h, TRUE);
    right_x = w - right_w - 8;
    MoveWindow(g_app.procs_label, right_x, top, right_w, 18, TRUE);
    right_bottom = top + work_h;
    proc_h = work_h - 202;
    if (proc_h < 96) {
        proc_h = 96;
    }
    result_h = right_bottom - (editor_y + proc_h + 128);
    if (result_h < 30) {
        result_h = 30;
        proc_h = right_bottom - editor_y - 128 - result_h;
        if (proc_h < 80) {
            proc_h = 80;
        }
        result_h = right_bottom - (editor_y + proc_h + 128);
        if (result_h < 24) {
            result_h = 24;
        }
    }
    MoveWindow(g_app.proc_list, right_x, editor_y,
        right_w, proc_h, TRUE);
    MoveWindow(g_app.proc_label, right_x,
        editor_y + proc_h + 8, right_w, 18, TRUE);
    MoveWindow(g_app.proc_name, right_x,
        editor_y + proc_h + 28, right_w, 24, TRUE);
    MoveWindow(g_app.args_label, right_x,
        editor_y + proc_h + 58, right_w, 18, TRUE);
    MoveWindow(g_app.args_edit, right_x,
        editor_y + proc_h + 78, right_w, 24, TRUE);
    MoveWindow(g_app.result_label, right_x,
        editor_y + proc_h + 108, right_w, 18, TRUE);
    MoveWindow(g_app.result_edit, right_x,
        editor_y + proc_h + 128, right_w, result_h, TRUE);

    MoveWindow(g_app.command_label, 8, command_label_y, w - 16, 18, TRUE);
    MoveWindow(g_app.command_edit, 8, command_y, w - 96, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXEC), w - 84, command_y, 76, 24, TRUE);
    MoveWindow(g_app.output_label, 8, output_label_y, w - 16, 18, TRUE);
    MoveWindow(g_app.console, 8, console_y,
        w - 16, h - console_y - 8, TRUE);
}

static void ps_create_controls(HWND hwnd)
{
    pst_theme_init(&g_app.theme);
    pst_popup_init(&g_app.popup);

    g_app.top_band = ps_static(hwnd, "", IDC_TOP_BAND, SS_LEFT);
    g_app.title = ps_static(hwnd, "PlankaStarten", IDC_TITLE,
        SS_LEFTNOWORDWRAP);
    SendMessageA(g_app.title, WM_SETFONT, (WPARAM)g_app.theme.title_font, TRUE);
    g_app.subtitle = ps_static(hwnd,
        "", IDC_SUBTITLE, SS_LEFTNOWORDWRAP);
    SendMessageA(g_app.subtitle, WM_SETFONT, (WPARAM)g_app.theme.small_font, TRUE);
    g_app.menu_bar = ps_static(hwnd, "", IDC_MENU_BAR, SS_LEFTNOWORDWRAP);
    pst_menu_create_bar(hwnd, g_app.theme.small_font, &g_app.menus);
    g_app.tool_panel = ps_static(hwnd, "", IDC_TOOL_PANEL, SS_LEFT);
    g_app.workspace_label = ps_static(hwnd, "", IDC_WORKSPACE_LABEL,
        SS_LEFTNOWORDWRAP);
    g_app.source_group = ps_static(hwnd, "", IDC_SOURCE_GROUP,
        SS_LEFTNOWORDWRAP);
    g_app.backend_group = ps_static(hwnd, "", IDC_BACKEND_GROUP,
        SS_LEFTNOWORDWRAP);
    g_app.project_label = ps_static(hwnd, "", IDC_PROJECT_LABEL,
        SS_LEFTNOWORDWRAP);
    g_app.editor_label = ps_static(hwnd, "", IDC_EDITOR_LABEL,
        SS_LEFTNOWORDWRAP);
    g_app.procs_label = ps_static(hwnd, "", IDC_PROCS_LABEL,
        SS_LEFTNOWORDWRAP);
    g_app.command_label = ps_static(hwnd, "", IDC_COMMAND_LABEL,
        SS_LEFTNOWORDWRAP);
    g_app.output_label = ps_static(hwnd, "", IDC_OUTPUT_LABEL,
        SS_LEFTNOWORDWRAP);

    g_app.status = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC",
        "", WS_CHILD | WS_VISIBLE,
        0, 0, 100, 20, hwnd, (HMENU)(INT_PTR)IDC_STATUS,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.status, WM_SETFONT, (WPARAM)g_app.theme.ui_font, TRUE);
    g_app.workspace_edit = ps_edit(hwnd, IDC_WORKSPACE, 0);
    ps_button(hwnd, "", IDC_LOAD_DIR);
    g_app.file_tree = CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
            | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT
            | TVS_SHOWSELALWAYS,
        0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)IDC_FILE_LIST,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.file_tree, WM_SETFONT, (WPARAM)g_app.theme.font, TRUE);
    g_app.line_numbers = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "   1",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY
            | ES_RIGHT | ES_AUTOVSCROLL,
        0, 0, 48, 100, hwnd, (HMENU)(INT_PTR)IDC_LINE_NUMBERS,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.line_numbers, WM_SETFONT, (WPARAM)g_app.theme.font, TRUE);
    g_app.editor = ps_edit(hwnd, IDC_EDITOR,
        ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL);
    SendMessageA(g_app.editor, EM_SETMARGINS, EC_LEFTMARGIN, 6);
    g_app.proc_list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY,
        0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)IDC_PROC_LIST,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.proc_list, WM_SETFONT, (WPARAM)g_app.theme.font, TRUE);
    g_app.proc_label = ps_label(hwnd, "", IDC_PROC_LABEL);
    g_app.proc_name = ps_edit(hwnd, IDC_PROC_NAME, 0);
    g_app.args_label = ps_label(hwnd, "", IDC_ARGS_LABEL);
    g_app.args_edit = ps_edit(hwnd, IDC_ARGS, 0);
    g_app.result_label = ps_label(hwnd, "", IDC_RESULT_LABEL);
    g_app.result_edit = ps_edit(hwnd, IDC_RESULT, ES_READONLY);
    g_app.command_edit = ps_edit(hwnd, IDC_COMMAND, 0);
    g_app.console = ps_edit(hwnd, IDC_CONSOLE,
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL);

    ps_button(hwnd, "", IDC_SAVE);
    ps_button(hwnd, "", IDC_FORMAT);
    ps_button(hwnd, "", IDC_CHECK);
    ps_button(hwnd, "", IDC_RUN);
    ps_button(hwnd, "", IDC_COMPILE);
    ps_button(hwnd, "PBC", IDC_BYTECODE);
    ps_button(hwnd, "IR", IDC_IR);
    ps_button(hwnd, "Evid", IDC_EVIDENCE);
    ps_button(hwnd, "C", IDC_CGEN);
    ps_button(hwnd, "ASM", IDC_ASMGEN);
    ps_button(hwnd, "8086", IDC_ASM8086);
    ps_button(hwnd, "", IDC_CLEAR);
    ps_button(hwnd, "", IDC_EXEC);
    ps_apply_language();
}

static void ps_default_workspace(void)
{
    char cwd[PS_MAX_PATH_TEXT];
    char examples[PS_MAX_PATH_TEXT];
    DWORD attr;

    GetCurrentDirectoryA(sizeof(cwd), cwd);
    ps_join_path(examples, sizeof(examples), cwd, "examples");
    attr = GetFileAttributesA(examples);
    if (attr != INVALID_FILE_ATTRIBUTES
            && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        SetWindowTextA(g_app.workspace_edit, examples);
    } else {
        SetWindowTextA(g_app.workspace_edit, cwd);
    }
    ps_scan_workspace();
    SetWindowTextA(g_app.command_edit, "run");
}

static void ps_action_open_workspace(void *userdata)
{
    (void)userdata;
    ps_select_folder();
}

static void ps_action_save(void *userdata)
{
    (void)userdata;
    ps_save_current();
}

static void ps_action_exit(void *userdata)
{
    (void)userdata;
    PostMessageA(g_app.hwnd, WM_CLOSE, 0, 0);
}

static void ps_action_select_all(void *userdata)
{
    (void)userdata;
    SetFocus(g_app.editor);
    SendMessageA(g_app.editor, EM_SETSEL, 0, -1);
}

static void ps_action_copy(void *userdata)
{
    (void)userdata;
    SetFocus(g_app.editor);
    SendMessageA(g_app.editor, WM_COPY, 0, 0);
}

static void ps_action_paste(void *userdata)
{
    (void)userdata;
    SetFocus(g_app.editor);
    SendMessageA(g_app.editor, WM_PASTE, 0, 0);
}

static void ps_action_format(void *userdata)
{
    (void)userdata;
    ps_format_plk_in_editor();
}

static void ps_find_text_in_editor(const char *needle)
{
    char *text;
    char *hit;
    int len;

    if (needle == 0 || needle[0] == '\0') {
        SetFocus(g_app.proc_list);
        ps_set_status(g_app.lang == PST_LANG_DE
            ? "Prozedurliste fokussiert"
            : "Procedure list focused");
        return;
    }
    len = GetWindowTextLengthA(g_app.editor);
    text = (char *)malloc((size_t)len + 1);
    if (text == 0) {
        ps_set_status(g_app.lang == PST_LANG_DE ? "Suche fehlgeschlagen"
            : "Search failed");
        return;
    }
    GetWindowTextA(g_app.editor, text, len + 1);
    hit = strstr(text, needle);
    if (hit != 0) {
        int start;

        start = (int)(hit - text);
        SetFocus(g_app.editor);
        SendMessageA(g_app.editor, EM_SETSEL, start,
            start + (int)strlen(needle));
        SendMessageA(g_app.editor, EM_SCROLLCARET, 0, 0);
        ps_set_status(g_app.lang == PST_LANG_DE
            ? "Prozedur im Editor markiert"
            : "Procedure marked in editor");
    } else {
        ps_set_status(g_app.lang == PST_LANG_DE
            ? "Prozedur nicht im Editor gefunden"
            : "Procedure not found in editor");
    }
    free(text);
}

static void ps_action_find_procedure(void *userdata)
{
    char proc[128];

    (void)userdata;
    GetWindowTextA(g_app.proc_name, proc, sizeof(proc));
    pst_popup_show_search(&g_app.popup, g_app.hwnd,
        pst_text(g_app.lang, PST_T_SEARCH_TITLE),
        pst_text(g_app.lang, PST_T_SEARCH_LABEL),
        proc,
        pst_text(g_app.lang, PST_T_FIND),
        pst_text(g_app.lang, PST_T_PROC_LIST),
        g_app.theme.title_font, g_app.theme.ui_font);
}

static void ps_apply_popup_search(void)
{
    char query[256];

    if (!pst_popup_get_text(&g_app.popup, query, sizeof(query))) {
        GetWindowTextA(g_app.proc_name, query, sizeof(query));
    }
    ps_find_text_in_editor(query);
}

static void ps_action_focus_procedures(void *userdata)
{
    (void)userdata;
    SetFocus(g_app.proc_list);
    ps_set_status(g_app.lang == PST_LANG_DE
        ? "Prozedurliste fokussiert"
        : "Procedure list focused");
}

static void ps_action_focus_editor(void *userdata)
{
    (void)userdata;
    SetFocus(g_app.editor);
    ps_update_cursor_status();
}

static void ps_action_check(void *userdata)
{
    (void)userdata;
    ps_check_project();
}

static void ps_action_run(void *userdata)
{
    (void)userdata;
    ps_run_proc();
}

static void ps_action_compile(void *userdata)
{
    (void)userdata;
    ps_compile_active();
}

static void ps_action_artifact(void *userdata, const char *kind)
{
    (void)userdata;
    ps_write_artifact(kind);
}

static void ps_open_shell_path(const char *path)
{
    HINSTANCE launched;

    launched = ShellExecuteA(g_app.hwnd, "open", path, "", "",
        SW_SHOWNORMAL);
    if ((INT_PTR)launched <= 32) {
        ps_appendf("open failed: %s", path);
    }
}

static void ps_action_open_build_folder(void *userdata)
{
    char path[PS_MAX_PATH_TEXT];

    (void)userdata;
    CreateDirectoryA("build", 0);
    GetFullPathNameA("build", sizeof(path), path, 0);
    ps_open_shell_path(path);
}

static void ps_action_open_workspace_folder(void *userdata)
{
    char path[PS_MAX_PATH_TEXT];

    (void)userdata;
    GetWindowTextA(g_app.workspace_edit, path, sizeof(path));
    if (path[0] != '\0') {
        ps_open_shell_path(path);
    }
}

static void ps_action_set_language(void *userdata, PST_LANG lang)
{
    (void)userdata;
    g_app.lang = lang;
    ps_apply_language();
    ps_appendf("language: %s", pst_language_name(lang));
}

static void ps_action_show_commands(void *userdata)
{
    (void)userdata;
    pst_popup_show_message(&g_app.popup, g_app.hwnd,
        pst_text(g_app.lang, PST_T_COMMANDS_TITLE),
        pst_text(g_app.lang, PST_T_COMMANDS_BODY),
        g_app.theme.title_font, g_app.theme.ui_font);
}

static void ps_action_show_about(void *userdata)
{
    pst_popup_show_message(&g_app.popup, g_app.hwnd,
        pst_text(g_app.lang, PST_T_ABOUT_TITLE),
        pst_text(g_app.lang, PST_T_ABOUT_BODY),
        g_app.theme.title_font, g_app.theme.ui_font);
    (void)userdata;
}

static int ps_dispatch_menu_command(int id)
{
    PST_MENU_ACTIONS actions;

    memset(&actions, 0, sizeof(actions));
    actions.open_workspace = ps_action_open_workspace;
    actions.save = ps_action_save;
    actions.exit_app = ps_action_exit;
    actions.select_all = ps_action_select_all;
    actions.copy = ps_action_copy;
    actions.paste = ps_action_paste;
    actions.format = ps_action_format;
    actions.find_procedure = ps_action_find_procedure;
    actions.focus_procedures = ps_action_focus_procedures;
    actions.focus_editor = ps_action_focus_editor;
    actions.check = ps_action_check;
    actions.run_selected = ps_action_run;
    actions.compile_launch = ps_action_compile;
    actions.artifact = ps_action_artifact;
    actions.open_build_folder = ps_action_open_build_folder;
    actions.open_workspace_folder = ps_action_open_workspace_folder;
    actions.set_language = ps_action_set_language;
    actions.show_commands = ps_action_show_commands;
    actions.show_about = ps_action_show_about;
    return pst_menu_dispatch(id, &actions);
}

static LRESULT CALLBACK ps_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int id;
    int code;

    switch (msg) {
    case WM_CREATE:
        g_app.hwnd = hwnd;
        g_app.selected_file = -1;
        g_app.lang = PST_LANG_EN;
        ps_create_controls(hwnd);
        ps_default_workspace();
        return 0;
    case WM_WINDOWPOSCHANGING:
        ps_clamp_windowpos((WINDOWPOS *)lp);
        return 0;
    case WM_ENTERSIZEMOVE:
    case WM_MOVE:
        pst_popup_close(&g_app.popup);
        break;
    case WM_SIZE:
        pst_popup_close(&g_app.popup);
        if (wp != SIZE_MINIMIZED) {
            ps_enforce_min_window(hwnd);
        }
        ps_layout(hwnd);
        return 0;
    case WM_GETMINMAXINFO:
        ps_set_min_track((MINMAXINFO *)lp);
        return 0;
    case WM_CTLCOLORSTATIC:
        return (LRESULT)pst_theme_static_brush(&g_app.theme,
            (HWND)lp, (HDC)wp);
    case WM_CTLCOLOREDIT:
        return (LRESULT)pst_theme_edit_brush(&g_app.theme,
            (HWND)lp, (HDC)wp);
    case WM_CTLCOLORLISTBOX:
        return (LRESULT)pst_theme_list_brush(&g_app.theme,
            (HWND)lp, (HDC)wp);
    case WM_COMMAND:
        id = LOWORD(wp);
        code = HIWORD(wp);
        if (pst_menu_is_bar_id(id)) {
            HWND anchor;

            anchor = (HWND)lp;
            if (anchor == 0) {
                anchor = GetDlgItem(hwnd, id);
            }
            pst_menu_show(&g_app.popup, hwnd, anchor, id, g_app.lang,
                g_app.theme.ui_font);
        } else if (id == IDM_POPUP_SEARCH_APPLY) {
            ps_apply_popup_search();
        } else if (id == IDM_POPUP_SEARCH_FOCUS_PROC) {
            ps_action_focus_procedures(0);
        } else if (ps_dispatch_menu_command(id)) {
            return 0;
        } else if (id == IDC_LOAD_DIR) {
            ps_select_folder();
        } else if (id == IDC_PROC_LIST && code == LBN_SELCHANGE) {
            ps_update_proc_from_list(0);
        } else if (id == IDC_PROC_LIST && code == LBN_DBLCLK) {
            ps_update_proc_from_list(1);
        } else if (id == IDC_EDITOR && code == EN_CHANGE) {
            ps_update_line_numbers();
        } else if (id == IDC_EDITOR && code == EN_VSCROLL) {
            ps_sync_line_number_scroll();
        } else if (id == IDC_EDITOR
                && (code == EN_UPDATE || code == EN_SETFOCUS)) {
            ps_update_cursor_status();
        } else if (id == IDC_SAVE) {
            ps_save_current();
        } else if (id == IDC_FORMAT) {
            ps_format_plk_in_editor();
        } else if (id == IDC_CHECK) {
            ps_check_project();
        } else if (id == IDC_RUN) {
            ps_run_proc();
        } else if (id == IDC_COMPILE) {
            ps_compile_active();
        } else if (id == IDC_BYTECODE) {
            ps_write_artifact("bytecode");
        } else if (id == IDC_IR) {
            ps_write_artifact("ir");
        } else if (id == IDC_EVIDENCE) {
            ps_write_artifact("evidence");
        } else if (id == IDC_CGEN) {
            ps_write_artifact("cgen");
        } else if (id == IDC_ASMGEN) {
            ps_write_artifact("asmgen");
        } else if (id == IDC_ASM8086) {
            ps_write_artifact("asm8086");
        } else if (id == IDC_CLEAR) {
            SetWindowTextA(g_app.console, "");
        } else if (id == IDC_EXEC) {
            ps_execute_command();
        }
        return 0;
    case WM_NOTIFY:
        if (((NMHDR *)lp)->idFrom == IDC_FILE_LIST
                && ((NMHDR *)lp)->code == TVN_SELCHANGEDA) {
            NMTREEVIEWA *tree;
            int file_index;

            if (g_app.loading_tree) {
                return 0;
            }
            tree = (NMTREEVIEWA *)lp;
            file_index = (int)tree->itemNew.lParam - 1;
            if (file_index >= 0 && file_index < g_app.file_count) {
                g_app.selected_file = file_index;
                ps_load_selected_file();
            } else {
                ps_set_status("Folder selected");
            }
            return 0;
        }
        break;
    case WM_DESTROY:
        pst_popup_destroy(&g_app.popup);
        pst_theme_destroy(&g_app.theme);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    INITCOMMONCONTROLSEX icc;
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;

    (void)prev;
    (void)cmd;
    memset(&icc, 0, sizeof(icc));
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icc);
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = ps_wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "PlankaStartenWindow";
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    if (!RegisterClassA(&wc)) {
        return 1;
    }
    hwnd = CreateWindowExA(0, wc.lpszClassName,
        "PlankaStarten",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS
            | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1120, 760,
        0, 0, inst, 0);
    if (hwnd == 0) {
        return 1;
    }
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    while (GetMessageA(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
