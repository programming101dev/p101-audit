#ifndef P101_DOCTOR_SOURCE_INPUTS_H
#define P101_DOCTOR_SOURCE_INPUTS_H

#include "arguments.h"
#include "constants.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

void   p101_doctor_copy_text(const struct p101_env *env, char dest[PATH_LEN], const char *src);
void   p101_doctor_copy_source_paths(const struct p101_env *env, const struct arguments *args, char source_paths[MAX_SOURCE_PATHS][PATH_LEN]);
size_t p101_doctor_append_source_paths(char *tool_argv[], size_t index, char source_paths[MAX_SOURCE_PATHS][PATH_LEN], int source_count);
bool   p101_doctor_resolve_compile_database(const struct p101_env *env, struct p101_error *err, const struct arguments *args, char path[PATH_LEN]);

#endif    // P101_DOCTOR_SOURCE_INPUTS_H
