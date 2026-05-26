#ifndef PLANKASTARTEN_COMPILE_H
#define PLANKASTARTEN_COMPILE_H

#include "plankac.h"

#define PSC_COMPILE_CONSOLE 1
#define PSC_COMPILE_GUI 2
#define PSC_PATH_MAX 512

typedef struct PSC_COMPILE_RESULT {
    int kind;
    char source_path[PSC_PATH_MAX];
    char c_path[PSC_PATH_MAX];
    char exe_path[PSC_PATH_MAX];
    char log_path[PSC_PATH_MAX];
} PSC_COMPILE_RESULT;

int psc_compile_plk(PLANKAC_CONTEXT *ctx, const char *source_path,
    PSC_COMPILE_RESULT *result, char *err, unsigned err_size);

#endif
