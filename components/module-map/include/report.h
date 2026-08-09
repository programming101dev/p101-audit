#ifndef P101_MODULE_MAP_REPORT_H
#define P101_MODULE_MAP_REPORT_H

#include "arguments.h"
#include "model.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdio.h>

bool p101_module_map_write_report(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map);
void p101_module_map_write_modules(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map);
void p101_module_map_write_include_graph(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map);
bool p101_module_map_write_findings(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map);
void p101_module_map_write_functions_for_module(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map, const char *module_name);

#endif    // P101_MODULE_MAP_REPORT_H
