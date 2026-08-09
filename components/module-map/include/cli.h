#ifndef P101_MODULE_MAP_CLI_H
#define P101_MODULE_MAP_CLI_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_module_map_arguments_init(const struct p101_env *env, struct arguments *args);

void p101_module_map_arguments_deinit(const struct p101_env *env, struct arguments *args);
void p101_module_map_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
void p101_module_map_check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
void p101_module_map_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

#endif    // P101_MODULE_MAP_CLI_H
