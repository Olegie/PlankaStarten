#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plankac.h"

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

static int ps_load_sources(PLANKAC_CONTEXT *ctx, int count, char **paths,
    char *err, unsigned err_size)
{
    const char *sources[128];
    int i;

    if (count <= 0 || count >= 128) {
        snprintf(err, err_size, "expected 1..127 source files");
        return 0;
    }
    for (i = 0; i < count; ++i) {
        sources[i] = paths[i];
    }
    sources[count] = 0;
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

static void ps_usage(void)
{
    printf("PlankaStarten API runner for .plk files\n");
    printf("usage:\n");
    printf("  plankastarten_cli check <file.plk> [more.plk...]\n");
    printf("  plankastarten_cli list <file.plk> [more.plk...]\n");
    printf("  plankastarten_cli run <file.plk> <procedure> [args...]\n");
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
        if (argc < 4) {
            ps_usage();
            return 1;
        }
        return ps_cmd_run(1, argv + 2, argv[3], argc - 4, argv + 4);
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
