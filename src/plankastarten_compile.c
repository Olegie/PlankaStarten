#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plankastarten_compile.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define PSC_COMMAND_MAX 8192

static void psc_set_error(char *err, unsigned err_size, const char *text)
{
    if (err != 0 && err_size > 0) {
        strncpy(err, text, err_size - 1);
        err[err_size - 1] = '\0';
    }
}

static int psc_file_exists(const char *path)
{
    DWORD attrs;

    attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES
        && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static void psc_join(char *out, unsigned out_size, const char *dir,
    const char *name)
{
    unsigned n;

    if (out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%s", dir != 0 ? dir : "");
    n = (unsigned)strlen(out);
    if (n > 0 && out[n - 1] != '\\' && out[n - 1] != '/') {
        strncat(out, "\\", out_size - strlen(out) - 1);
    }
    strncat(out, name, out_size - strlen(out) - 1);
}

static int psc_full_path(const char *path, char *out, unsigned out_size)
{
    DWORD n;

    n = GetFullPathNameA(path, out_size, out, 0);
    return n > 0 && n < out_size;
}

static void psc_parent_dir(char *path)
{
    char *slash;
    char *alt;

    slash = strrchr(path, '\\');
    alt = strrchr(path, '/');
    if (alt != 0 && (slash == 0 || alt > slash)) {
        slash = alt;
    }
    if (slash != 0) {
        *slash = '\0';
    }
}

static int psc_try_plankac_root(const char *candidate, char *out,
    unsigned out_size)
{
    char full[PSC_PATH_MAX];
    char probe[PSC_PATH_MAX];

    if (candidate == 0 || candidate[0] == '\0') {
        return 0;
    }
    if (!psc_full_path(candidate, full, sizeof(full))) {
        return 0;
    }
    psc_join(probe, sizeof(probe), full, "c\\include\\plankac.h");
    if (!psc_file_exists(probe)) {
        return 0;
    }
    strncpy(out, full, out_size - 1);
    out[out_size - 1] = '\0';
    return 1;
}

static int psc_get_module_dir(char *out, unsigned out_size)
{
    DWORD n;

    n = GetModuleFileNameA(0, out, out_size);
    if (n == 0 || n >= out_size) {
        return 0;
    }
    psc_parent_dir(out);
    return 1;
}

static int psc_find_plankac_root_above(const char *path, char *out,
    unsigned out_size)
{
    char dir[PSC_PATH_MAX];
    int i;

    if (path == 0 || path[0] == '\0') {
        return 0;
    }
    if (!psc_full_path(path, dir, sizeof(dir))) {
        return 0;
    }
    psc_parent_dir(dir);
    for (i = 0; i < 8 && dir[0] != '\0'; ++i) {
        if (psc_try_plankac_root(dir, out, out_size)) {
            return 1;
        }
        psc_parent_dir(dir);
    }
    return 0;
}

static int psc_get_plankac_root(const char *source_path, char *out,
    unsigned out_size)
{
    const char *env;
    char module_dir[PSC_PATH_MAX];
    char candidate[PSC_PATH_MAX];

    env = getenv("PLANKAC_ROOT");
    if (psc_try_plankac_root(env, out, out_size)) {
        return 1;
    }
    if (psc_find_plankac_root_above(source_path, out, out_size)) {
        return 1;
    }
    if (psc_try_plankac_root("..\\PlankaMath", out, out_size)) {
        return 1;
    }
    if (psc_try_plankac_root("..\\..\\PlankaMath", out, out_size)) {
        return 1;
    }
    if (psc_get_module_dir(module_dir, sizeof(module_dir))) {
        psc_join(candidate, sizeof(candidate), module_dir, "..\\PlankaMath");
        if (psc_try_plankac_root(candidate, out, out_size)) {
            return 1;
        }
        psc_join(candidate, sizeof(candidate), module_dir,
            "..\\..\\PlankaMath");
        if (psc_try_plankac_root(candidate, out, out_size)) {
            return 1;
        }
    }
    return 0;
}

static void psc_quote(char *out, unsigned out_size, const char *text)
{
    unsigned n;

    if (out_size == 0) {
        return;
    }
    n = 0;
    out[n++] = '"';
    while (text != 0 && *text != '\0' && n + 2 < out_size) {
        if (*text == '"') {
            out[n++] = '\\';
        }
        out[n++] = *text++;
    }
    if (n + 1 < out_size) {
        out[n++] = '"';
    }
    out[n] = '\0';
}

static void psc_c_escape(char *out, unsigned out_size, const char *text)
{
    unsigned n;

    if (out_size == 0) {
        return;
    }
    n = 0;
    while (text != 0 && *text != '\0' && n + 2 < out_size) {
        if (*text == '\\' || *text == '"') {
            out[n++] = '\\';
        }
        out[n++] = *text++;
    }
    out[n] = '\0';
}

static void psc_stem_from_path(const char *path, char *out,
    unsigned out_size)
{
    const char *base;
    const char *dot;
    unsigned i;
    unsigned n;

    base = strrchr(path, '\\');
    if (base == 0) {
        base = strrchr(path, '/');
    }
    base = base != 0 ? base + 1 : path;
    dot = strrchr(base, '.');
    if (dot == 0 || dot <= base) {
        dot = base + strlen(base);
    }
    n = 0;
    for (i = 0; base + i < dot && n + 1 < out_size; ++i) {
        char c;

        c = base[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '_') {
            out[n++] = c;
        } else {
            out[n++] = '_';
        }
    }
    if (n == 0 && out_size > 1) {
        out[n++] = 'p';
        out[n++] = 'l';
        out[n++] = 'k';
    }
    out[n] = '\0';
}

static char *psc_read_text(const char *path)
{
    FILE *fp;
    long size;
    char *text;

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return 0;
    }
    rewind(fp);
    text = (char *)malloc((size_t)size + 1);
    if (text == 0) {
        fclose(fp);
        return 0;
    }
    if (fread(text, 1, (size_t)size, fp) != (size_t)size) {
        free(text);
        fclose(fp);
        return 0;
    }
    text[size] = '\0';
    fclose(fp);
    return text;
}

static int psc_write_text(const char *path, const char *text)
{
    FILE *fp;

    fp = fopen(path, "wb");
    if (fp == 0) {
        return 0;
    }
    fwrite(text, 1, strlen(text), fp);
    fclose(fp);
    return 1;
}

static char *psc_replace_all(const char *text, const char *needle,
    const char *replacement)
{
    const char *p;
    const char *next;
    char *out;
    char *w;
    size_t needle_len;
    size_t replacement_len;
    size_t count;
    size_t new_len;

    needle_len = strlen(needle);
    replacement_len = strlen(replacement);
    if (needle_len == 0) {
        return 0;
    }
    count = 0;
    p = text;
    while ((next = strstr(p, needle)) != 0) {
        ++count;
        p = next + needle_len;
    }
    if (count == 0) {
        out = (char *)malloc(strlen(text) + 1);
        if (out != 0) {
            strcpy(out, text);
        }
        return out;
    }
    new_len = strlen(text) + count * (replacement_len - needle_len);
    out = (char *)malloc(new_len + 1);
    if (out == 0) {
        return 0;
    }
    p = text;
    w = out;
    while ((next = strstr(p, needle)) != 0) {
        size_t chunk;

        chunk = (size_t)(next - p);
        memcpy(w, p, chunk);
        w += chunk;
        memcpy(w, replacement, replacement_len);
        w += replacement_len;
        p = next + needle_len;
    }
    strcpy(w, p);
    return out;
}

static int psc_rewrite_all(char **text, const char *needle,
    const char *replacement)
{
    char *next;

    next = psc_replace_all(*text, needle, replacement);
    if (next == 0) {
        return 0;
    }
    free(*text);
    *text = next;
    return 1;
}

static int psc_rewrite_plankahost_c(const char *root,
    const char *in_path, const char *out_path)
{
    char *text;
    char root_c[PSC_COMMAND_MAX];
    char repl[PSC_COMMAND_MAX * 2];
    int ok;

    text = psc_read_text(in_path);
    if (text == 0) {
        return 0;
    }
    psc_c_escape(root_c, sizeof(root_c), root);
    ok = 1;
#define PSC_REWRITE(prefix, subdir) \
    snprintf(repl, sizeof(repl), "\"%s\\\\" subdir "\\\\", root_c); \
    ok = ok && psc_rewrite_all(&text, "\"" prefix, repl)
    PSC_REWRITE("../graphics/src/", "graphics\\\\src");
    PSC_REWRITE("graphics/src/", "graphics\\\\src");
    PSC_REWRITE("../src/", "src");
    PSC_REWRITE("src/", "src");
    PSC_REWRITE("../examples/", "examples");
    PSC_REWRITE("examples/", "examples");
    PSC_REWRITE("../tests/", "tests");
    PSC_REWRITE("tests/", "tests");
#undef PSC_REWRITE
    if (ok) {
        ok = psc_write_text(out_path, text);
    }
    free(text);
    return ok;
}

static int psc_rewrite_plankahost_window(const char *source_path,
    const char *in_path, const char *out_path)
{
    char *text;
    char escaped_source[PSC_PATH_MAX * 2];
    char replacement[PSC_PATH_MAX * 3];
    int ok;

    text = psc_read_text(in_path);
    if (text == 0) {
        return 0;
    }
    psc_c_escape(escaped_source, sizeof(escaped_source), source_path);
    snprintf(replacement, sizeof(replacement),
        "source[0] != '\\0' ? source : \"%s\"", escaped_source);
    ok = psc_rewrite_all(&text,
        "source[0] != '\\0' ? source : 0", replacement);
    if (ok) {
        ok = psc_write_text(out_path, text);
    }
    free(text);
    return ok;
}

static void psc_write_c_string(FILE *out, const char *text)
{
    fputc('"', out);
    while (text != 0 && *text != '\0') {
        if (*text == '\\' || *text == '"') {
            fputc('\\', out);
            fputc(*text, out);
        } else if (*text == '\n') {
            fputs("\\n", out);
        } else if (*text != '\r') {
            fputc(*text, out);
        }
        ++text;
    }
    fputs("\\n\"", out);
}

static int psc_write_console_host(PLANKAC_CONTEXT *ctx, const char *stem,
    const char *out_path, char *err, unsigned err_size)
{
    FILE *in;
    FILE *out;
    char bytecode_path[PSC_PATH_MAX];
    char bytecode_name[192];
    char line[1024];

    snprintf(bytecode_name, sizeof(bytecode_name),
        "%s_console_embed.pbc", stem != 0 ? stem : "plankastarten");
    psc_join(bytecode_path, sizeof(bytecode_path), "build", bytecode_name);
    if (!plankac_context_write_bytecode(ctx, bytecode_path, err, err_size)) {
        return 0;
    }
    in = fopen(bytecode_path, "rb");
    if (in == 0) {
        remove(bytecode_path);
        psc_set_error(err, err_size, "cannot read generated bytecode");
        return 0;
    }
    out = fopen(out_path, "wb");
    if (out == 0) {
        fclose(in);
        remove(bytecode_path);
        psc_set_error(err, err_size, "cannot write console host source");
        return 0;
    }

    fprintf(out, "/* Generated by PlankaStarten. */\n");
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "#include <string.h>\n");
    fprintf(out, "#include \"plankac.h\"\n\n");
    fprintf(out, "static const char *APP_NAME = \"");
    while (stem != 0 && *stem != '\0') {
        if (*stem == '\\' || *stem == '"') {
            fputc('\\', out);
        }
        fputc(*stem, out);
        ++stem;
    }
    fprintf(out, "\";\n\n");
    fprintf(out, "static const char *PLANKAC_EMBEDDED_BYTECODE[] = {\n");
    while (fgets(line, sizeof(line), in) != 0) {
        fputs("    ", out);
        psc_write_c_string(out, line);
        fputs(",\n", out);
    }
    fprintf(out, "    0\n};\n\n");
    fclose(in);
    remove(bytecode_path);

    fprintf(out,
"static int write_bytecode(const char *path)\n"
"{\n"
"    FILE *fp;\n"
"    int i;\n"
"    fp = fopen(path, \"w\");\n"
"    if (fp == 0) return 0;\n"
"    for (i = 0; PLANKAC_EMBEDDED_BYTECODE[i] != 0; ++i) {\n"
"        fputs(PLANKAC_EMBEDDED_BYTECODE[i], fp);\n"
"    }\n"
"    fclose(fp);\n"
"    return 1;\n"
"}\n\n"
"static int ends_with(const char *text, const char *suffix)\n"
"{\n"
"    size_t a;\n"
"    size_t b;\n"
"    if (text == 0 || suffix == 0) return 0;\n"
"    a = strlen(text);\n"
"    b = strlen(suffix);\n"
"    return a >= b && strcmp(text + a - b, suffix) == 0;\n"
"}\n\n"
"static int proc_score(const PLANKAC_PROC_INFO *info)\n"
"{\n"
"    int score;\n"
"    if (info == 0) return -1000000;\n"
"    if (strcmp(info->name, \"type_sheet\") == 0) return -1000000;\n"
"    score = info->number;\n"
"    if (strcmp(info->name, \"start\") == 0) score += 100000;\n"
"    else if (strcmp(info->name, \"main\") == 0) score += 95000;\n"
"    else if (strstr(info->name, \"_app\") != 0) score += 85000;\n"
"    else if (ends_with(info->name, \"_demo\")) score += 75000;\n"
"    else if (ends_with(info->name, \"_session\")) score += 65000;\n"
"    if (info->argc == 0) score += 500;\n"
"    return score;\n"
"}\n\n"
"static int choose_proc(PLANKAC_CONTEXT *ctx, PLANKAC_PROC_INFO *chosen)\n"
"{\n"
"    PLANKAC_PROC_INFO info;\n"
"    int best;\n"
"    int score;\n"
"    int i;\n"
"    int n;\n"
"    if (plankac_context_find_proc(ctx, \"start\", chosen)) return 1;\n"
"    best = -1000000;\n"
"    n = plankac_context_proc_count(ctx);\n"
"    for (i = 0; i < n; ++i) {\n"
"        if (plankac_context_get_proc(ctx, i, &info)) {\n"
"            score = proc_score(&info);\n"
"            if (score > best) {\n"
"                best = score;\n"
"                *chosen = info;\n"
"            }\n"
"        }\n"
"    }\n"
"    return best > -1000000;\n"
"}\n\n"
"static void print_result(const PLANKAC_RESULT *result)\n"
"{\n"
"    int i;\n"
"    char value[64];\n"
"    printf(\"\\nResult\\n\");\n"
"    printf(\"------\\n\");\n"
"    for (i = 0; i < result->count; ++i) {\n"
"        plankac_format(result->value[i], value, sizeof(value));\n"
"        printf(\"R%%d = %%s\\n\", i, value);\n"
"    }\n"
"}\n\n"
"static int read_args(const PLANKAC_PROC_INFO *info, double *args)\n"
"{\n"
"    char line[128];\n"
"    int i;\n"
"    if (info->argc <= 0) {\n"
"        printf(\"Press Enter to run %%s...\", info->name);\n"
"        fgets(line, sizeof(line), stdin);\n"
"        return 1;\n"
"    }\n"
"    printf(\"Input\\n\");\n"
"    printf(\"-----\\n\");\n"
"    for (i = 0; i < info->argc; ++i) {\n"
"        printf(\"V%%d: \", i);\n"
"        fflush(stdout);\n"
"        if (fgets(line, sizeof(line), stdin) == 0) return 0;\n"
"        args[i] = strtod(line, 0);\n"
"    }\n"
"    return 1;\n"
"}\n\n"
"static int run_proc(PLANKAC_CONTEXT *ctx, const PLANKAC_PROC_INFO *info,\n"
"    const double *args, int argc)\n"
"{\n"
"    PLANKAC_RESULT result;\n"
"    char err[256];\n"
"    err[0] = '\\0';\n"
"    if (!plankac_context_run(ctx, info->name, args, argc, &result,\n"
"            err, sizeof(err))) {\n"
"        printf(\"run failed: %%s\\n\", err);\n"
"        return 1;\n"
"    }\n"
"    print_result(&result);\n"
"    return 0;\n"
"}\n\n"
"int main(int argc, char **argv)\n"
"{\n"
"    PLANKAC_CONTEXT *ctx;\n"
"    PLANKAC_PROC_INFO info;\n"
"    double args[16];\n"
"    char err[256];\n"
"    const char *bytecode_path;\n"
"    int i;\n"
"    int interactive;\n"
"    bytecode_path = \"plankac_embedded_runner.pbc\";\n"
"    if (!write_bytecode(bytecode_path)) {\n"
"        printf(\"cannot write embedded bytecode\\n\");\n"
"        return 1;\n"
"    }\n"
"    ctx = plankac_create();\n"
"    if (ctx == 0) {\n"
"        remove(bytecode_path);\n"
"        printf(\"cannot create PlankaC context\\n\");\n"
"        return 1;\n"
"    }\n"
"    err[0] = '\\0';\n"
"    if (!plankac_context_load_bytecode(ctx, bytecode_path, err, sizeof(err))) {\n"
"        plankac_destroy(ctx);\n"
"        remove(bytecode_path);\n"
"        printf(\"bytecode load failed: %%s\\n\", err);\n"
"        return 1;\n"
"    }\n"
"    remove(bytecode_path);\n"
"    interactive = argc < 2 || strcmp(argv[1], \"/interactive\") == 0;\n"
"    printf(\"PlankaStarten application: %%s\\n\", APP_NAME);\n"
"    printf(\"Runtime: PlankaC embedded bytecode\\n\\n\");\n"
"    if (interactive) {\n"
"        if (argc >= 3 && strcmp(argv[1], \"/interactive\") == 0) {\n"
"            if (!plankac_context_find_proc(ctx, argv[2], &info)) {\n"
"                printf(\"unknown procedure: %%s\\n\", argv[2]);\n"
"                plankac_destroy(ctx);\n"
"                return 1;\n"
"            }\n"
"        } else if (!choose_proc(ctx, &info)) {\n"
"            printf(\"no runnable procedure found\\n\");\n"
"            plankac_destroy(ctx);\n"
"            return 1;\n"
"        }\n"
"        printf(\"Procedure: P%%d %%s (%%d -> %%d)\\n\\n\",\n"
"            info.number, info.name, info.argc, info.results);\n"
"        if (!read_args(&info, args)) {\n"
"            plankac_destroy(ctx);\n"
"            return 1;\n"
"        }\n"
"        i = run_proc(ctx, &info, args, info.argc);\n"
"        printf(\"\\nPress Enter to close...\");\n"
"        fflush(stdout);\n"
"        {\n"
"            char line[16];\n"
"            fgets(line, sizeof(line), stdin);\n"
"        }\n"
"        plankac_destroy(ctx);\n"
"        return i;\n"
"    }\n"
"    if (!plankac_context_find_proc(ctx, argv[1], &info)) {\n"
"        printf(\"unknown procedure: %%s\\n\", argv[1]);\n"
"        plankac_destroy(ctx);\n"
"        return 1;\n"
"    }\n"
"    if (argc - 2 != info.argc) {\n"
"        printf(\"%%s expects %%d argument(s), got %%d\\n\",\n"
"            info.name, info.argc, argc - 2);\n"
"        printf(\"run without arguments for interactive input\\n\");\n"
"        plankac_destroy(ctx);\n"
"        return 1;\n"
"    }\n"
"    for (i = 2; i < argc; ++i) {\n"
"        args[i - 2] = strtod(argv[i], 0);\n"
"    }\n"
"    i = run_proc(ctx, &info, args, argc - 2);\n"
"    plankac_destroy(ctx);\n"
"    return i;\n"
"}\n");
    fclose(out);
    return 1;
}

static int psc_has_proc(PLANKAC_CONTEXT *ctx, const char *name)
{
    PLANKAC_PROC_INFO info;

    return plankac_context_find_proc(ctx, name, &info);
}

static int psc_is_gui_profile(PLANKAC_CONTEXT *ctx)
{
    return psc_has_proc(ctx, "app_canvas")
        || psc_has_proc(ctx, "gui_canvas")
        || psc_has_proc(ctx, "cube_canvas")
        || psc_has_proc(ctx, "app_kind")
        || psc_has_proc(ctx, "cube_project_vertex");
}

static int psc_run_command(const char *command)
{
    return system(command) == 0;
}

static int psc_compile_console(PLANKAC_CONTEXT *ctx, const char *root,
    const char *stem, PSC_COMPILE_RESULT *result,
    char *err, unsigned err_size)
{
    char include_dir[PSC_PATH_MAX];
    char lib_path[PSC_PATH_MAX];
    char q_c[PSC_PATH_MAX + 4];
    char q_exe[PSC_PATH_MAX + 4];
    char q_inc[PSC_PATH_MAX + 4];
    char q_lib[PSC_PATH_MAX + 4];
    char q_log[PSC_PATH_MAX + 4];
    char command[PSC_COMMAND_MAX];

    psc_join(include_dir, sizeof(include_dir), root, "c\\include");
    psc_join(lib_path, sizeof(lib_path), root, "build\\libplankac.a");
    if (!psc_file_exists(lib_path)) {
        psc_set_error(err, err_size, "PlankaC library was not built");
        return 0;
    }
    if (!psc_write_console_host(ctx, stem, result->c_path, err, err_size)) {
        return 0;
    }
    psc_quote(q_c, sizeof(q_c), result->c_path);
    psc_quote(q_exe, sizeof(q_exe), result->exe_path);
    psc_quote(q_inc, sizeof(q_inc), include_dir);
    psc_quote(q_lib, sizeof(q_lib), lib_path);
    psc_quote(q_log, sizeof(q_log), result->log_path);
    snprintf(command, sizeof(command),
        "gcc -Wall -Wextra -std=c99 -I%s %s %s -o %s "
        "-Wl,--subsystem,console:5.01 -static -static-libgcc -lm > %s 2>&1",
        q_inc, q_c, q_lib, q_exe, q_log);
    if (!psc_run_command(command)) {
        psc_set_error(err, err_size, "console native compile failed");
        return 0;
    }
    return 1;
}

static int psc_compile_gui(const char *root, const char *source_path,
    const char *stem, PSC_COMPILE_RESULT *result,
    char *err, unsigned err_size)
{
    char host_window_in[PSC_PATH_MAX];
    char host_c_in[PSC_PATH_MAX];
    char host_window_out[PSC_PATH_MAX];
    char host_c_out[PSC_PATH_MAX];
    char include_c[PSC_PATH_MAX];
    char include_internal[PSC_PATH_MAX];
    char include_graphics[PSC_PATH_MAX];
    char lib_path[PSC_PATH_MAX];
    char q_window[PSC_PATH_MAX + 4];
    char q_host[PSC_PATH_MAX + 4];
    char q_exe[PSC_PATH_MAX + 4];
    char q_log[PSC_PATH_MAX + 4];
    char q_inc_c[PSC_PATH_MAX + 4];
    char q_inc_internal[PSC_PATH_MAX + 4];
    char q_inc_graphics[PSC_PATH_MAX + 4];
    char q_lib[PSC_PATH_MAX + 4];
    char command[PSC_COMMAND_MAX];

    (void)stem;
    psc_join(host_window_in, sizeof(host_window_in), root,
        "graphics\\c\\plankahost_window.c");
    psc_join(host_c_in, sizeof(host_c_in), root,
        "graphics\\c\\plankahost.c");
    psc_join(host_window_out, sizeof(host_window_out), "build",
        "plankastarten_gui_window.c");
    psc_join(host_c_out, sizeof(host_c_out), "build",
        "plankastarten_gui_host.c");
    psc_join(include_c, sizeof(include_c), root, "c\\include");
    psc_join(include_internal, sizeof(include_internal), root, "c\\internal");
    psc_join(include_graphics, sizeof(include_graphics), root, "graphics\\c");
    psc_join(lib_path, sizeof(lib_path), root, "build\\libplankac.a");
    if (!psc_file_exists(lib_path)) {
        psc_set_error(err, err_size, "PlankaC library was not built");
        return 0;
    }
    if (!psc_rewrite_plankahost_window(source_path,
            host_window_in, host_window_out)) {
        psc_set_error(err, err_size, "cannot prepare GUI host window source");
        return 0;
    }
    if (!psc_rewrite_plankahost_c(root, host_c_in, host_c_out)) {
        psc_set_error(err, err_size, "cannot prepare GUI host source");
        return 0;
    }
    strncpy(result->c_path, host_window_out, sizeof(result->c_path) - 1);
    result->c_path[sizeof(result->c_path) - 1] = '\0';
    psc_quote(q_window, sizeof(q_window), host_window_out);
    psc_quote(q_host, sizeof(q_host), host_c_out);
    psc_quote(q_exe, sizeof(q_exe), result->exe_path);
    psc_quote(q_log, sizeof(q_log), result->log_path);
    psc_quote(q_inc_c, sizeof(q_inc_c), include_c);
    psc_quote(q_inc_internal, sizeof(q_inc_internal), include_internal);
    psc_quote(q_inc_graphics, sizeof(q_inc_graphics), include_graphics);
    psc_quote(q_lib, sizeof(q_lib), lib_path);
    snprintf(command, sizeof(command),
        "gcc -Wall -Wextra -std=c89 -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 "
        "-I%s -I%s -I%s %s %s "
        "\"%s\\graphics\\c\\plankagui_scene.c\" "
        "\"%s\\graphics\\c\\plankagui_raster.c\" "
        "\"%s\\graphics\\c\\plankagui_font.c\" "
        "\"%s\\graphics\\c\\plankagui_render.c\" "
        "\"%s\\graphics\\c\\plankacube_scene.c\" "
        "\"%s\\graphics\\c\\plankacube_render.c\" "
        "%s -o %s -Wl,--subsystem,windows:5.01 "
        "-static -static-libgcc -lgdi32 -lm > %s 2>&1",
        q_inc_c, q_inc_internal, q_inc_graphics, q_window, q_host,
        root, root, root, root, root, root, q_lib, q_exe, q_log);
    if (!psc_run_command(command)) {
        psc_set_error(err, err_size, "GUI native compile failed");
        return 0;
    }
    return 1;
}

int psc_compile_plk(PLANKAC_CONTEXT *ctx, const char *source_path,
    PSC_COMPILE_RESULT *result, char *err, unsigned err_size)
{
    char root[PSC_PATH_MAX];
    char stem[128];
    char name[192];

    if (ctx == 0 || source_path == 0 || result == 0) {
        psc_set_error(err, err_size, "bad compile request");
        return 0;
    }
    memset(result, 0, sizeof(*result));
    CreateDirectoryA("build", 0);
    if (!psc_full_path(source_path, result->source_path,
            sizeof(result->source_path))) {
        psc_set_error(err, err_size, "bad source path");
        return 0;
    }
    if (!psc_get_plankac_root(result->source_path, root, sizeof(root))) {
        psc_set_error(err, err_size, "cannot locate PlankaC root");
        return 0;
    }
    psc_stem_from_path(result->source_path, stem, sizeof(stem));
    result->kind = psc_is_gui_profile(ctx)
        ? PSC_COMPILE_GUI : PSC_COMPILE_CONSOLE;
    snprintf(name, sizeof(name), "%s_compile.log", stem);
    psc_join(result->log_path, sizeof(result->log_path), "build", name);
    if (result->kind == PSC_COMPILE_GUI) {
        snprintf(name, sizeof(name), "%s_gui.exe", stem);
        psc_join(result->exe_path, sizeof(result->exe_path), "build", name);
        return psc_compile_gui(root, result->source_path, stem,
            result, err, err_size);
    }
    snprintf(name, sizeof(name), "%s_generated.c", stem);
    psc_join(result->c_path, sizeof(result->c_path), "build", name);
    snprintf(name, sizeof(name), "%s_console.exe", stem);
    psc_join(result->exe_path, sizeof(result->exe_path), "build", name);
    return psc_compile_console(ctx, root, stem, result, err, err_size);
}

#else

int psc_compile_plk(PLANKAC_CONTEXT *ctx, const char *source_path,
    PSC_COMPILE_RESULT *result, char *err, unsigned err_size)
{
    (void)ctx;
    (void)source_path;
    if (result != 0) {
        memset(result, 0, sizeof(*result));
    }
    if (err != 0 && err_size > 0) {
        strncpy(err, "native exe compile is available in the Windows build",
            err_size - 1);
        err[err_size - 1] = '\0';
    }
    return 0;
}

#endif
