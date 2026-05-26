#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "plankac.h"

#define PS_MAX_FILES 128
#define PS_MAX_PATH_TEXT 512
#define PS_MAX_TEXT 65536
#define PS_MAX_LINE 1024

#define IDC_WORKSPACE 1001
#define IDC_LOAD_DIR 1002
#define IDC_FILE_LIST 1003
#define IDC_EDITOR 1004
#define IDC_PROC_LIST 1005
#define IDC_PROC_NAME 1006
#define IDC_ARGS 1007
#define IDC_COMMAND 1008
#define IDC_CONSOLE 1009
#define IDC_STATUS 1010
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

typedef struct PS_APP {
    HWND hwnd;
    HWND workspace_edit;
    HWND file_list;
    HWND editor;
    HWND proc_list;
    HWND proc_name;
    HWND args_edit;
    HWND command_edit;
    HWND console;
    HWND status;
    HFONT font;
    char workspace[PS_MAX_PATH_TEXT];
    char files[PS_MAX_FILES][PS_MAX_PATH_TEXT];
    int file_count;
    int selected_file;
} PS_APP;

static PS_APP g_app;

static void ps_set_status(const char *text)
{
    SetWindowTextA(g_app.status, text);
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

static void ps_load_selected_file(void)
{
    char text[PS_MAX_TEXT];

    if (g_app.selected_file < 0 || g_app.selected_file >= g_app.file_count) {
        SetWindowTextA(g_app.editor, "");
        return;
    }
    if (!ps_read_file(g_app.files[g_app.selected_file], text, sizeof(text))) {
        ps_appendf("cannot read %s", g_app.files[g_app.selected_file]);
        return;
    }
    SetWindowTextA(g_app.editor, text);
    ps_appendf("opened %s", g_app.files[g_app.selected_file]);
}

static void ps_scan_workspace(void)
{
    WIN32_FIND_DATAA data;
    HANDLE find;
    char pattern[PS_MAX_PATH_TEXT];
    int i;

    GetWindowTextA(g_app.workspace_edit, g_app.workspace,
        sizeof(g_app.workspace));
    SendMessageA(g_app.file_list, LB_RESETCONTENT, 0, 0);
    g_app.file_count = 0;
    g_app.selected_file = -1;
    ps_join_path(pattern, sizeof(pattern), g_app.workspace, "*.plk");
    find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        ps_set_status("No .plk files found");
        ps_appendf("no .plk files in %s", g_app.workspace);
        return;
    }
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                && ps_ends_with_plk(data.cFileName)
                && g_app.file_count < PS_MAX_FILES) {
            ps_join_path(g_app.files[g_app.file_count],
                sizeof(g_app.files[g_app.file_count]),
                g_app.workspace, data.cFileName);
            SendMessageA(g_app.file_list, LB_ADDSTRING, 0,
                (LPARAM)data.cFileName);
            ++g_app.file_count;
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    for (i = 0; i < g_app.file_count; ++i) {
        (void)i;
    }
    if (g_app.file_count > 0) {
        g_app.selected_file = 0;
        SendMessageA(g_app.file_list, LB_SETCURSEL, 0, 0);
        ps_load_selected_file();
        ps_set_status("Workspace loaded");
    }
    ps_appendf("workspace: %s (%d .plk file%s)", g_app.workspace,
        g_app.file_count, g_app.file_count == 1 ? "" : "s");
}

static int ps_context_load(PLANKAC_CONTEXT **ctx_out)
{
    PLANKAC_CONTEXT *ctx;
    const char *sources[PS_MAX_FILES + 1];
    char err[PS_MAX_LINE];
    int i;

    if (g_app.file_count <= 0) {
        ps_append_console("no .plk files loaded");
        return 0;
    }
    ctx = plankac_create();
    if (ctx == 0) {
        ps_append_console("cannot create PlankaC context");
        return 0;
    }
    for (i = 0; i < g_app.file_count; ++i) {
        sources[i] = g_app.files[i];
    }
    sources[g_app.file_count] = 0;
    err[0] = '\0';
    if (!plankac_context_load_sources(ctx, sources, err, sizeof(err))) {
        ps_appendf("load failed: %s", err);
        plankac_destroy(ctx);
        return 0;
    }
    *ctx_out = ctx;
    return 1;
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
}

static void ps_check_project(void)
{
    PLANKAC_CONTEXT *ctx;
    char summary[256];

    if (!ps_context_load(&ctx)) {
        ps_set_status("Load failed");
        return;
    }
    plankac_context_summary(ctx, summary, sizeof(summary));
    ps_fill_procedures(ctx);
    ps_append_console(summary);
    ps_set_status("Ready");
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
    PLANKAC_RESULT result;
    double args[PLANKAC_MAX_ARGS];
    char proc[128];
    char arg_text[256];
    char err[PS_MAX_LINE];
    char value[64];
    char line[512];
    int argc;
    int i;

    if (!ps_context_load(&ctx)) {
        ps_set_status("Load failed");
        return;
    }
    GetWindowTextA(g_app.proc_name, proc, sizeof(proc));
    GetWindowTextA(g_app.args_edit, arg_text, sizeof(arg_text));
    argc = ps_parse_args(arg_text, args);
    err[0] = '\0';
    if (!plankac_context_run(ctx, proc, args, argc, &result,
            err, sizeof(err))) {
        ps_appendf("run failed: %s", err);
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
    if (!ps_write_file(g_app.files[g_app.selected_file], text)) {
        ps_appendf("save failed: %s", g_app.files[g_app.selected_file]);
        free(text);
        return;
    }
    free(text);
    ps_appendf("saved %s", g_app.files[g_app.selected_file]);
    ps_set_status("Saved");
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

    if (!ps_context_load(&ctx)) {
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
    } else {
        ps_append_console("commands: check, run <proc> [args], save, bytecode, ir, evidence, cgen, asmgen, asm8086");
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
    int y;
    int button_w;

    GetClientRect(hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    top = 8;
    left_w = 220;
    right_w = 240;
    console_h = 150;
    mid_x = left_w + 16;
    button_w = 74;

    MoveWindow(g_app.status, 8, top, w - 16, 22, TRUE);
    top += 30;
    MoveWindow(g_app.workspace_edit, 8, top, left_w - 76, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_LOAD_DIR), left_w - 62, top, 62, 24, TRUE);
    top += 32;
    MoveWindow(g_app.file_list, 8, top, left_w, h - top - console_h - 16, TRUE);

    y = 38;
    MoveWindow(GetDlgItem(hwnd, IDC_SAVE), mid_x, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_CHECK), mid_x + 78, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_RUN), mid_x + 156, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_LIST), mid_x + 234, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_BYTECODE), mid_x + 312, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_IR), mid_x + 390, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EVIDENCE), mid_x + 468, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_CGEN), mid_x + 546, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_ASMGEN), mid_x + 624, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_ASM8086), mid_x + 702, y, button_w, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_CLEAR), w - 88, y, 80, 24, TRUE);

    MoveWindow(g_app.editor, mid_x, 70,
        w - left_w - right_w - 32, h - console_h - 82, TRUE);
    MoveWindow(g_app.proc_list, w - right_w - 8, 70,
        right_w, (h - console_h - 130) / 2, TRUE);
    MoveWindow(g_app.proc_name, w - right_w - 8,
        78 + (h - console_h - 130) / 2, right_w, 24, TRUE);
    MoveWindow(g_app.args_edit, w - right_w - 8,
        108 + (h - console_h - 130) / 2, right_w, 24, TRUE);
    MoveWindow(g_app.command_edit, 8, h - console_h - 32,
        w - 96, 24, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_EXEC), w - 84, h - console_h - 32,
        76, 24, TRUE);
    MoveWindow(g_app.console, 8, h - console_h,
        w - 16, console_h - 8, TRUE);
}

static void ps_create_controls(HWND hwnd)
{
    g_app.font = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    g_app.status = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC",
        "PlankaStarten ready", WS_CHILD | WS_VISIBLE,
        0, 0, 100, 20, hwnd, (HMENU)(INT_PTR)IDC_STATUS,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.status, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    g_app.workspace_edit = ps_edit(hwnd, IDC_WORKSPACE, 0);
    ps_button(hwnd, "Open", IDC_LOAD_DIR);
    g_app.file_list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY,
        0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)IDC_FILE_LIST,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.file_list, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    g_app.editor = ps_edit(hwnd, IDC_EDITOR,
        ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL);
    g_app.proc_list = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY,
        0, 0, 100, 100, hwnd, (HMENU)(INT_PTR)IDC_PROC_LIST,
        GetModuleHandleA(0), 0);
    SendMessageA(g_app.proc_list, WM_SETFONT, (WPARAM)g_app.font, TRUE);
    g_app.proc_name = ps_edit(hwnd, IDC_PROC_NAME, 0);
    g_app.args_edit = ps_edit(hwnd, IDC_ARGS, 0);
    g_app.command_edit = ps_edit(hwnd, IDC_COMMAND, 0);
    g_app.console = ps_edit(hwnd, IDC_CONSOLE,
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL);

    ps_button(hwnd, "Save", IDC_SAVE);
    ps_button(hwnd, "Check", IDC_CHECK);
    ps_button(hwnd, "Run", IDC_RUN);
    ps_button(hwnd, "List", IDC_LIST);
    ps_button(hwnd, "PBC", IDC_BYTECODE);
    ps_button(hwnd, "IR", IDC_IR);
    ps_button(hwnd, "Evidence", IDC_EVIDENCE);
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
    SetWindowTextA(g_app.proc_name, "start");
    SetWindowTextA(g_app.args_edit, "");
    SetWindowTextA(g_app.command_edit, "run start");
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
    case WM_SIZE:
        ps_layout(hwnd);
        return 0;
    case WM_COMMAND:
        id = LOWORD(wp);
        code = HIWORD(wp);
        if (id == IDC_LOAD_DIR) {
            ps_select_folder();
        } else if (id == IDC_FILE_LIST && code == LBN_SELCHANGE) {
            g_app.selected_file = (int)SendMessageA(g_app.file_list,
                LB_GETCURSEL, 0, 0);
            ps_load_selected_file();
        } else if (id == IDC_PROC_LIST && code == LBN_SELCHANGE) {
            char line[256];
            char name[128];
            int sel;

            sel = (int)SendMessageA(g_app.proc_list, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                SendMessageA(g_app.proc_list, LB_GETTEXT, sel, (LPARAM)line);
                name[0] = '\0';
                sscanf(line, "P%*d %127s", name);
                SetWindowTextA(g_app.proc_name, name);
            }
        } else if (id == IDC_SAVE) {
            ps_save_current();
        } else if (id == IDC_CHECK || id == IDC_LIST) {
            ps_check_project();
        } else if (id == IDC_RUN) {
            ps_run_proc();
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
    case WM_DESTROY:
        if (g_app.font != 0) {
            DeleteObject(g_app.font);
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;

    (void)prev;
    (void)cmd;
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
        "PlankaStarten - PLK API Workbench",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
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
