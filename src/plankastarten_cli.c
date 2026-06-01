#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plankac.h"
#include "plankastarten_compile.h"

#define PS_MAX_CLI_SOURCES 128
#define PS_MAX_PATH_TEXT 512

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

static int ps_add_source(const char **sources, int *count,
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
    if (*count >= PS_MAX_CLI_SOURCES - 1
            || *storage_count >= PS_MAX_CLI_SOURCES - 1) {
        return 0;
    }
    strncpy(storage[*storage_count], path, PS_MAX_PATH_TEXT - 1);
    storage[*storage_count][PS_MAX_PATH_TEXT - 1] = '\0';
    sources[*count] = storage[*storage_count];
    *count += 1;
    *storage_count += 1;
    return 1;
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
                ps_add_source(sources, count, storage, storage_count, path);
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

static int ps_expand_sources(int count, char **paths, const char **sources,
    int *source_count, char storage[][PS_MAX_PATH_TEXT])
{
    char active_dir[PS_MAX_PATH_TEXT];
    char project_root[PS_MAX_PATH_TEXT];
    char project_src[PS_MAX_PATH_TEXT];
    int active_prefix;
    int storage_count;
    int i;

    if (count <= 0 || count >= PS_MAX_CLI_SOURCES) {
        return 0;
    }
    *source_count = 0;
    storage_count = 0;
    if (count != 1) {
        for (i = 0; i < count; ++i) {
            if (!ps_add_source(sources, source_count, storage,
                    &storage_count, paths[i])) {
                return 0;
            }
        }
        sources[*source_count] = 0;
        return 1;
    }
    ps_parent_dir(paths[0], active_dir, sizeof(active_dir));
    active_prefix = ps_source_prefix_number(paths[0]);
    if (active_prefix >= 0 && active_prefix < 90) {
        ps_add_numbered_sources_from_disk(sources, source_count, storage,
            &storage_count, active_dir, active_prefix);
    } else if (ps_project_root_for_examples(active_dir, project_root,
            sizeof(project_root))) {
        ps_join_path(project_src, sizeof(project_src), project_root, "src");
        ps_add_numbered_sources_from_disk(sources, source_count, storage,
            &storage_count, project_src, 89);
    }
    if (!ps_add_source(sources, source_count, storage, &storage_count,
            paths[0])) {
        return 0;
    }
    sources[*source_count] = 0;
    return 1;
}

static void ps_format_results(const PLANKAC_RESULT *result)
{
    int i;
    char value[64];

    for (i = 0; i < result->count; ++i) {
        if (i > 0) {
            printf(" ");
        }
        plankac_format(result->value[i], value, sizeof(value));
        printf("R%d=%s", i, value);
    }
    printf("\n");
}

static void ps_print_result_block(const PLANKAC_RESULT *result)
{
    int i;
    char value[64];

    printf("\nResult\n");
    printf("------\n");
    for (i = 0; i < result->count; ++i) {
        plankac_format(result->value[i], value, sizeof(value));
        printf("R%d = %s\n", i, value);
    }
}

static int ps_name_ends_with(const char *text, const char *suffix)
{
    size_t a;
    size_t b;

    if (text == 0 || suffix == 0) {
        return 0;
    }
    a = strlen(text);
    b = strlen(suffix);
    return a >= b && strcmp(text + a - b, suffix) == 0;
}

static int ps_proc_score(const PLANKAC_PROC_INFO *info)
{
    int score;

    if (info == 0) {
        return -1000000;
    }
    if (strcmp(info->name, "type_sheet") == 0) {
        return -1000000;
    }
    score = info->number;
    if (strcmp(info->name, "start") == 0) {
        score += 100000;
    } else if (strcmp(info->name, "main") == 0) {
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

static int ps_choose_proc(PLANKAC_CONTEXT *ctx, PLANKAC_PROC_INFO *chosen)
{
    PLANKAC_PROC_INFO info;
    int best;
    int score;
    int i;
    int n;

    if (plankac_context_find_proc(ctx, "start", chosen)) {
        return 1;
    }
    best = -1000000;
    n = plankac_context_proc_count(ctx);
    for (i = 0; i < n; ++i) {
        if (plankac_context_get_proc(ctx, i, &info)) {
            score = ps_proc_score(&info);
            if (score > best) {
                best = score;
                *chosen = info;
            }
        }
    }
    return best > -1000000;
}

static int ps_prompt_args(const PLANKAC_PROC_INFO *info, double *args)
{
    char line[128];
    int i;

    if (info->argc == 0) {
        printf("Press Enter to run %s...", info->name);
        fgets(line, sizeof(line), stdin);
        return 1;
    }
    printf("Input\n");
    printf("-----\n");
    for (i = 0; i < info->argc; ++i) {
        printf("V%d: ", i);
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == 0) {
            return 0;
        }
        args[i] = strtod(line, 0);
    }
    return 1;
}

static int ps_load_sources(PLANKAC_CONTEXT *ctx, int count, char **paths,
    char *err, unsigned err_size)
{
    const char *sources[PS_MAX_CLI_SOURCES];
    char storage[PS_MAX_CLI_SOURCES][PS_MAX_PATH_TEXT];
    int source_count;

    if (!ps_expand_sources(count, paths, sources, &source_count, storage)) {
        snprintf(err, err_size, "expected 1..%d source files",
            PS_MAX_CLI_SOURCES - 1);
        return 0;
    }
    return plankac_context_load_sources(ctx, sources, err, err_size);
}

static int ps_cmd_check(int count, char **paths)
{
    PLANKAC_CONTEXT *ctx;
    char err[512];
    char summary[256];
    int ok;

    ctx = plankac_create();
    if (ctx == 0) {
        printf("PlankaStarten: cannot create PlankaC context\n");
        return 1;
    }
    ok = ps_load_sources(ctx, count, paths, err, sizeof(err));
    if (!ok) {
        printf("load failed: %s\n", err);
        plankac_destroy(ctx);
        return 1;
    }
    if (!plankac_context_summary(ctx, summary, sizeof(summary))) {
        printf("summary failed\n");
        plankac_destroy(ctx);
        return 1;
    }
    printf("%s\n", summary);
    plankac_destroy(ctx);
    return 0;
}

static int ps_cmd_list(int count, char **paths)
{
    PLANKAC_CONTEXT *ctx;
    PLANKAC_PROC_INFO info;
    char err[512];
    int i;
    int n;

    ctx = plankac_create();
    if (ctx == 0) {
        printf("PlankaStarten: cannot create PlankaC context\n");
        return 1;
    }
    if (!ps_load_sources(ctx, count, paths, err, sizeof(err))) {
        printf("load failed: %s\n", err);
        plankac_destroy(ctx);
        return 1;
    }
    n = plankac_context_proc_count(ctx);
    for (i = 0; i < n; ++i) {
        if (plankac_context_get_proc(ctx, i, &info)) {
            printf("P%d %s argc=%d results=%d statements=%d\n",
                info.number, info.name, info.argc, info.results,
                info.statements);
        }
    }
    plankac_destroy(ctx);
    return 0;
}

static int ps_cmd_run(int source_count, char **sources, const char *proc,
    int argc, char **argv)
{
    PLANKAC_CONTEXT *ctx;
    PLANKAC_RESULT result;
    double args[PLANKAC_MAX_ARGS];
    char err[512];
    int i;
    int ok;

    if (argc > PLANKAC_MAX_ARGS) {
        printf("too many arguments\n");
        return 1;
    }
    for (i = 0; i < argc; ++i) {
        args[i] = strtod(argv[i], 0);
    }
    ctx = plankac_create();
    if (ctx == 0) {
        printf("PlankaStarten: cannot create PlankaC context\n");
        return 1;
    }
    if (!ps_load_sources(ctx, source_count, sources, err, sizeof(err))) {
        printf("load failed: %s\n", err);
        plankac_destroy(ctx);
        return 1;
    }
    ok = plankac_context_run(ctx, proc, args, argc, &result,
        err, sizeof(err));
    plankac_destroy(ctx);
    if (!ok) {
        printf("run failed: %s\n", err);
        return 1;
    }
    ps_format_results(&result);
    return 0;
}

static int ps_cmd_app(int source_count, char **sources, const char *proc)
{
    PLANKAC_CONTEXT *ctx;
    PLANKAC_PROC_INFO info;
    PLANKAC_RESULT result;
    double args[PLANKAC_MAX_ARGS];
    char err[512];

    ctx = plankac_create();
    if (ctx == 0) {
        printf("PlankaStarten: cannot create PlankaC context\n");
        return 1;
    }
    if (!ps_load_sources(ctx, source_count, sources, err, sizeof(err))) {
        printf("load failed: %s\n", err);
        plankac_destroy(ctx);
        return 1;
    }
    if (proc != 0 && proc[0] != '\0') {
        if (!plankac_context_find_proc(ctx, proc, &info)) {
            printf("unknown procedure: %s\n", proc);
            plankac_destroy(ctx);
            return 1;
        }
    } else if (!ps_choose_proc(ctx, &info)) {
        printf("no runnable procedure found\n");
        plankac_destroy(ctx);
        return 1;
    }
    printf("PlankaStarten application\n");
    printf("Runtime: PlankaC API\n\n");
    printf("Procedure: P%d %s (%d -> %d)\n\n",
        info.number, info.name, info.argc, info.results);
    if (!ps_prompt_args(&info, args)) {
        plankac_destroy(ctx);
        return 1;
    }
    err[0] = '\0';
    if (!plankac_context_run(ctx, info.name, args, info.argc, &result,
            err, sizeof(err))) {
        printf("run failed: %s\n", err);
        plankac_destroy(ctx);
        return 1;
    }
    ps_print_result_block(&result);
    plankac_destroy(ctx);
    return 0;
}

static int ps_cmd_write(int count, char **paths, const char *kind,
    const char *out_path)
{
    PLANKAC_CONTEXT *ctx;
    char err[512];
    int ok;

    ctx = plankac_create();
    if (ctx == 0) {
        printf("PlankaStarten: cannot create PlankaC context\n");
        return 1;
    }
    if (!ps_load_sources(ctx, count, paths, err, sizeof(err))) {
        printf("load failed: %s\n", err);
        plankac_destroy(ctx);
        return 1;
    }
    ok = 0;
    if (strcmp(kind, "bytecode") == 0) {
        ok = plankac_context_write_bytecode(ctx, out_path, err, sizeof(err));
    } else if (strcmp(kind, "ir") == 0) {
        ok = plankac_context_write_ir(ctx, out_path, err, sizeof(err));
    } else if (strcmp(kind, "evidence") == 0) {
        ok = plankac_context_write_evidence(ctx, out_path, err, sizeof(err));
    } else if (strcmp(kind, "cgen") == 0) {
        ok = plankac_context_write_c_backend(ctx, out_path, err, sizeof(err));
    } else if (strcmp(kind, "asmgen") == 0) {
        ok = plankac_context_write_asm_runtime(ctx, out_path, err,
            sizeof(err));
    } else if (strcmp(kind, "asm8086") == 0) {
        ok = plankac_context_write_asm8086_runtime(ctx, out_path, err,
            sizeof(err));
    } else if (strcmp(kind, "lowering") == 0) {
        ok = plankac_context_write_lowering_report(ctx, out_path, err,
            sizeof(err));
    }
    plankac_destroy(ctx);
    if (!ok) {
        printf("%s failed: %s\n", kind, err);
        return 1;
    }
    printf("%s written: %s\n", kind, out_path);
    return 0;
}

static int ps_cmd_compile(const char *path)
{
    PLANKAC_CONTEXT *ctx;
    PSC_COMPILE_RESULT result;
    char err[512];
    char *sources[1];

    ctx = plankac_create();
    if (ctx == 0) {
        printf("PlankaStarten: cannot create PlankaC context\n");
        return 1;
    }
    sources[0] = (char *)path;
    if (!ps_load_sources(ctx, 1, sources, err, sizeof(err))) {
        printf("load failed: %s\n", err);
        plankac_destroy(ctx);
        return 1;
    }
    if (!psc_compile_plk(ctx, path, &result, err, sizeof(err))) {
        printf("compile failed: %s\n", err);
        printf("compile log: %s\n", result.log_path);
        plankac_destroy(ctx);
        return 1;
    }
    printf("%s exe: %s\n",
        result.kind == PSC_COMPILE_GUI ? "GUI" : "console",
        result.exe_path);
    printf("compile log: %s\n", result.log_path);
    plankac_destroy(ctx);
    return 0;
}

static void ps_usage(void)
{
    printf("PlankaStarten API runner for .plk files\n");
    printf("usage:\n");
    printf("  plankastarten_cli check <file.plk> [more.plk...]\n");
    printf("  plankastarten_cli list <file.plk> [more.plk...]\n");
    printf("  plankastarten_cli run <file.plk> <procedure> [args...]\n");
    printf("  plankastarten_cli run <file.plk> [more.plk...] -- <procedure> [args...]\n");
    printf("  plankastarten_cli app <file.plk> [procedure]\n");
    printf("  plankastarten_cli compile <file.plk>\n");
    printf("  plankastarten_cli bytecode <file.plk> <out.pbc>\n");
    printf("  plankastarten_cli ir <file.plk> <out.ir>\n");
    printf("  plankastarten_cli evidence <file.plk> <out.json>\n");
    printf("  plankastarten_cli cgen <file.plk> <out.c>\n");
    printf("  plankastarten_cli asmgen <file.plk> <out.S>\n");
    printf("  plankastarten_cli asm8086 <file.plk> <out.asm>\n");
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        ps_usage();
        return 1;
    }
    if (strcmp(argv[1], "check") == 0) {
        return ps_cmd_check(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "list") == 0) {
        return ps_cmd_list(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "run") == 0) {
        int sep;

        if (argc < 4) {
            ps_usage();
            return 1;
        }
        for (sep = 2; sep < argc; ++sep) {
            if (strcmp(argv[sep], "--") == 0) {
                if (sep == 2 || sep + 1 >= argc) {
                    ps_usage();
                    return 1;
                }
                return ps_cmd_run(sep - 2, argv + 2, argv[sep + 1],
                    argc - sep - 2, argv + sep + 2);
            }
        }
        return ps_cmd_run(1, argv + 2, argv[3], argc - 4, argv + 4);
    }
    if (strcmp(argv[1], "compile") == 0) {
        return ps_cmd_compile(argv[2]);
    }
    if (strcmp(argv[1], "app") == 0) {
        return ps_cmd_app(1, argv + 2, argc >= 4 ? argv[3] : 0);
    }
    if (strcmp(argv[1], "bytecode") == 0 || strcmp(argv[1], "ir") == 0
            || strcmp(argv[1], "evidence") == 0
            || strcmp(argv[1], "cgen") == 0
            || strcmp(argv[1], "asmgen") == 0
            || strcmp(argv[1], "asm8086") == 0
            || strcmp(argv[1], "lowering") == 0) {
        if (argc < 4) {
            ps_usage();
            return 1;
        }
        return ps_cmd_write(1, argv + 2, argv[1], argv[3]);
    }
    ps_usage();
    return 1;
}
