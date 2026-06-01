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

#define PS_MAX_FILES 512
#define PS_MAX_PATH_TEXT 512
#define PS_MAX_TEXT 65536
#define PS_MAX_LINE 1024
#define PS_MIN_WINDOW_W 980
#define PS_MIN_WINDOW_H 620
#define PS_MAX_TREE_DEPTH 24

#define IDC_WORKSPACE 1001
#define IDC_LOAD_DIR 1002
#define IDC_FILE_LIST 1003
#define IDC_EDITOR 1004
#define IDC_LINE_NUMBERS 1013
#define IDC_PROC_LIST 1005
#define IDC_PROC_NAME 1006
#define IDC_ARGS 1007
#define IDC_COMMAND 1008
#define IDC_CONSOLE 1009
#define IDC_STATUS 1010
#define IDC_RESULT 1011
#define IDC_PROC_LABEL 1015
#define IDC_ARGS_LABEL 1016
#define IDC_RESULT_LABEL 1017
#define IDC_CHECK 1101
#define IDC_RUN 1102
#define IDC_LIST 1103
#define IDC_SAVE 1104
#define IDC_BYTECODE 1105
#define IDC_IR 1106
#define IDC_EVIDENCE 1107
#define IDC_CGEN 1108
#define IDC_ASMGEN 1109
#define IDC_ASM8086 1110
#define IDC_CLEAR 1111
#define IDC_EXEC 1112
#define IDC_FORMAT 1113
#define IDC_COMPILE 1114

typedef struct PS_APP {
    HWND hwnd;
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
    HFONT font;
    HFONT ui_font;
    char workspace[PS_MAX_PATH_TEXT];
    char files[PS_MAX_FILES][PS_MAX_PATH_TEXT];
    int file_count;
    int selected_file;
    int loading_tree;
    HTREEITEM first_file_item;
} PS_APP;

static PS_APP g_app;

static void ps_update_line_numbers(void);
static void ps_check_project(void);
static void ps_run_proc(void);

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

    if (g_app.editor == 0) {
        return;
    }
    sel = (DWORD)SendMessageA(g_app.editor, EM_GETSEL, 0, 0);
    pos = LOWORD(sel);
    line = (int)SendMessageA(g_app.editor, EM_LINEFROMCHAR, pos, 0);
    line_start = (int)SendMessageA(g_app.editor, EM_LINEINDEX, line, 0);
    col = pos - line_start;
    snprintf(text, sizeof(text),
        "Ready  |  Ln %d, Col %d  |  PLK source via PlankaC API",
        line + 1, col + 1);
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

static void ps_select_default_proc(PLANKAC_CONTEXT *ctx)
{
    PLANKAC_PROC_INFO info;
    PLANKAC_PROC_INFO first;
    PLANKAC_PROC_INFO first_zero;
    int i;
    int n;
    int have_first;
    int have_zero;
    int first_index;
    int zero_index;

    memset(&first, 0, sizeof(first));
    memset(&first_zero, 0, sizeof(first_zero));
    have_first = 0;
    have_zero = 0;
    first_index = -1;
    zero_index = -1;
    n = plankac_context_proc_count(ctx);
    for (i = 0; i < n; ++i) {
        if (plankac_context_get_proc(ctx, i, &info)) {
            if (!have_first) {
                first = info;
                have_first = 1;
                first_index = i;
            }
            if (!have_zero && info.argc == 0) {
                first_zero = info;
                have_zero = 1;
                zero_index = i;
            }
        }
    }
    if (have_zero) {
        SendMessageA(g_app.proc_list, LB_SETCURSEL, zero_index, 0);
        ps_set_run_fields(&first_zero);
    } else if (have_first) {
        SendMessageA(g_app.proc_list, LB_SETCURSEL, first_index, 0);
        ps_set_run_fields(&first);
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
    fprintf(f, " %s", proc);
    if (args != 0 && args[0] != '\0') {
        fprintf(f, " %s", args);
    }
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
                ps_appendf("launched console: %s %s %s",
                    result.exe_path, run_proc, run_args);
                ps_set_status("Compiled and launched console exe");
            }
        } else {
            ps_appendf("run manually: %s %s %s",
                result.exe_path, run_proc, run_args);
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
    } else if (strcmp(verb, "compile") == 0) {
        ps_compile_active();
    } else {
        ps_append_console("commands: check, run <proc> [args], compile, format, save, bytecode, ir, evidence, cgen, asmgen, asm8086");
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
    bi.lpszTitle = "Select folder with .plk files";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    id = SHBrowseForFolderA(&bi);
    if (id != 0 && SHGetPathFromIDListA(id, path)) {
        SetWindowTextA(g_app.workspace_edit, path);
        ps_scan_workspace();
    }
}

static HWND ps_button(HWND parent, const char *text, int id)
{
    HWND hwnd;

    hwnd = CreateWindowExA(0, "BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 24, parent, (HMENU)(INT_PTR)id,
        GetModuleHandleA(0), 0);
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    return hwnd;
}

static HWND ps_edit(HWND parent, int id, DWORD extra_style)
{
    HWND hwnd;

    hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extra_style,
        0, 0, 100, 24, parent, (HMENU)(INT_PTR)id,
        GetModuleHandleA(0), 0);
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    return hwnd;
}

static HWND ps_label(HWND parent, const char *text, int id)
{
    HWND hwnd;

    hwnd = CreateWindowExA(0, "STATIC", text,
        WS_CHILD | WS_VISIBLE,
        0, 0, 100, 18, parent, (HMENU)(INT_PTR)id,
        GetModuleHandleA(0), 0);
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)g_app.ui_font, TRUE);
    return hwnd;
}

static void ps_layout(HWND hwnd)
{
    RECT rc;
    int w;
    int h;
    int top;
    int left_w;
    int right_w;
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
    int command_y;
    int work_h;
    int proc_h;

    GetClientRect(hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    top = 8;
    left_w = 220;
    right_w = 252;
    console_h = 150;
    mid_x = left_w + 16;
    gutter_w = 54;

    MoveWindow(g_app.status, 8, top, w - 16, 22, TRUE);
    top += 30;
    MoveWindow(g_app.workspace_edit, 8, top, left_w - 76, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_LOAD_DIR), left_w - 62, top, 62, 24, TRUE);
    top += 32;
    toolbar_y = top;
    button_y = toolbar_y + 3;
    bx = 8;
    MoveWindow(GetDlgItem(hwnd, IDC_SAVE), bx, button_y, 56, 24, TRUE);
    bx += 60;
    MoveWindow(GetDlgItem(hwnd, IDC_FORMAT), bx, button_y, 68, 24, TRUE);
    bx += 76;
    MoveWindow(GetDlgItem(hwnd, IDC_CHECK), bx, button_y, 62, 24, TRUE);
    bx += 66;
    MoveWindow(GetDlgItem(hwnd, IDC_RUN), bx, button_y, 54, 24, TRUE);
    bx += 58;
    MoveWindow(GetDlgItem(hwnd, IDC_COMPILE), bx, button_y, 76, 24, TRUE);
    bx += 80;
    MoveWindow(GetDlgItem(hwnd, IDC_LIST), bx, button_y, 54, 24, TRUE);

    bx += 84;
    MoveWindow(GetDlgItem(hwnd, IDC_BYTECODE), bx, button_y, 50, 24, TRUE);
    bx += 54;
    MoveWindow(GetDlgItem(hwnd, IDC_IR), bx, button_y, 42, 24, TRUE);
    bx += 46;
    MoveWindow(GetDlgItem(hwnd, IDC_EVIDENCE), bx, button_y, 58, 24, TRUE);
    bx += 64;
    MoveWindow(GetDlgItem(hwnd, IDC_CGEN), bx, button_y, 38, 24, TRUE);
    bx += 42;
    MoveWindow(GetDlgItem(hwnd, IDC_ASMGEN), bx, button_y, 54, 24, TRUE);
    bx += 58;
    MoveWindow(GetDlgItem(hwnd, IDC_ASM8086), bx, button_y, 54, 24, TRUE);
    bx += 62;
    MoveWindow(GetDlgItem(hwnd, IDC_CLEAR), bx, button_y, 60, 24, TRUE);

    top += 34;
    command_y = h - console_h - 32;
    work_h = command_y - top - 8;
    if (work_h < 160) {
        work_h = 160;
    }
    MoveWindow(g_app.file_tree, 8, top, left_w, work_h, TRUE);

    editor_x = mid_x;
    editor_y = top;
    editor_w = w - left_w - right_w - 32;
    editor_h = work_h;
    MoveWindow(g_app.line_numbers, editor_x, editor_y,
        gutter_w, editor_h, TRUE);
    MoveWindow(g_app.editor, editor_x + gutter_w - 1, editor_y,
        editor_w - gutter_w + 1, editor_h, TRUE);
    proc_h = work_h - 146;
    if (proc_h < 120) {
        proc_h = 120;
    }
    MoveWindow(g_app.proc_list, w - right_w - 8, editor_y,
        right_w, proc_h, TRUE);
    MoveWindow(g_app.proc_label, w - right_w - 8,
        editor_y + proc_h + 8, right_w, 18, TRUE);
    MoveWindow(g_app.proc_name, w - right_w - 8,
        editor_y + proc_h + 28, right_w, 24, TRUE);
    MoveWindow(g_app.args_label, w - right_w - 8,
        editor_y + proc_h + 58, right_w, 18, TRUE);
    MoveWindow(g_app.args_edit, w - right_w - 8,
        editor_y + proc_h + 78, right_w, 24, TRUE);
    MoveWindow(g_app.result_label, w - right_w - 8,
        editor_y + proc_h + 108, right_w, 18, TRUE);
    MoveWindow(g_app.result_edit, w - right_w - 8,
        editor_y + proc_h + 128, right_w, 26, TRUE);
    MoveWindow(g_app.command_edit, 8, command_y, w - 96, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXEC), w - 84, command_y, 76, 24, TRUE);
    MoveWindow(g_app.console, 8, h - console_h,
        w - 16, console_h - 8, TRUE);
}

static void ps_create_controls(HWND hwnd)
{
    g_app.font = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    g_app.ui_font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "MS Sans Serif");
    g_app.status = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC",
        "PlankaStarten ready", WS_CHILD | WS_VISIBLE,
        0, 0, 100, 20, hwnd, (HMENU)(INT_PTR)IDC_STATUS,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.status, WM_SETFONT, (WPARAM)g_app.ui_font, TRUE);
    g_app.workspace_edit = ps_edit(hwnd, IDC_WORKSPACE, 0);
    ps_button(hwnd, "Open", IDC_LOAD_DIR);
    g_app.file_tree = CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
            | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT
            | TVS_SHOWSELALWAYS,
        0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)IDC_FILE_LIST,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.file_tree, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    g_app.line_numbers = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "   1",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY
            | ES_RIGHT | ES_AUTOVSCROLL,
        0, 0, 48, 100, hwnd, (HMENU)(INT_PTR)IDC_LINE_NUMBERS,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.line_numbers, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    g_app.editor = ps_edit(hwnd, IDC_EDITOR,
        ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL);
    SendMessageA(g_app.editor, EM_SETMARGINS, EC_LEFTMARGIN, 6);
    g_app.proc_list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY,
        0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)IDC_PROC_LIST,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.proc_list, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    g_app.proc_label = ps_label(hwnd, "Procedure", IDC_PROC_LABEL);
    g_app.proc_name = ps_edit(hwnd, IDC_PROC_NAME, 0);
    g_app.args_label = ps_label(hwnd, "Arguments", IDC_ARGS_LABEL);
    g_app.args_edit = ps_edit(hwnd, IDC_ARGS, 0);
    g_app.result_label = ps_label(hwnd, "Result", IDC_RESULT_LABEL);
    g_app.result_edit = ps_edit(hwnd, IDC_RESULT, ES_READONLY);
    g_app.command_edit = ps_edit(hwnd, IDC_COMMAND, 0);
    g_app.console = ps_edit(hwnd, IDC_CONSOLE,
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL);

    ps_button(hwnd, "Save", IDC_SAVE);
    ps_button(hwnd, "Format", IDC_FORMAT);
    ps_button(hwnd, "Check", IDC_CHECK);
    ps_button(hwnd, "Run", IDC_RUN);
    ps_button(hwnd, "Compile", IDC_COMPILE);
    ps_button(hwnd, "List", IDC_LIST);
    ps_button(hwnd, "PBC", IDC_BYTECODE);
    ps_button(hwnd, "IR", IDC_IR);
    ps_button(hwnd, "Evid", IDC_EVIDENCE);
    ps_button(hwnd, "C", IDC_CGEN);
    ps_button(hwnd, "ASM", IDC_ASMGEN);
    ps_button(hwnd, "8086", IDC_ASM8086);
    ps_button(hwnd, "Clear", IDC_CLEAR);
    ps_button(hwnd, "Exec", IDC_EXEC);
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

static LRESULT CALLBACK ps_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int id;
    int code;

    switch (msg) {
    case WM_CREATE:
        g_app.hwnd = hwnd;
        g_app.selected_file = -1;
        ps_create_controls(hwnd);
        ps_default_workspace();
        return 0;
    case WM_WINDOWPOSCHANGING:
        ps_clamp_windowpos((WINDOWPOS *)lp);
        return 0;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            ps_enforce_min_window(hwnd);
        }
        ps_layout(hwnd);
        return 0;
    case WM_GETMINMAXINFO:
        ps_set_min_track((MINMAXINFO *)lp);
        return 0;
    case WM_COMMAND:
        id = LOWORD(wp);
        code = HIWORD(wp);
        if (id == IDC_LOAD_DIR) {
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
        } else if (id == IDC_CHECK || id == IDC_LIST) {
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
        if (g_app.font != 0) {
            DeleteObject(g_app.font);
        }
        if (g_app.ui_font != 0) {
            DeleteObject(g_app.ui_font);
        }
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
