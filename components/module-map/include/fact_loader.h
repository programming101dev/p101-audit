#ifndef P101_MODULE_MAP_FACT_LOADER_H
#define P101_MODULE_MAP_FACT_LOADER_H

#include "arguments.h"
#include "model.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_module_map_load_clang_facts(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct arguments *args);

#endif    // P101_MODULE_MAP_FACT_LOADER_H
