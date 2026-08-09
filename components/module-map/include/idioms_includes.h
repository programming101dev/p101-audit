#ifndef P101_MODULE_MAP_IDIOMS_INCLUDES_H
#define P101_MODULE_MAP_IDIOMS_INCLUDES_H

#include "model.h"
#include <p101_env/env.h>
#include <stdbool.h>

const struct include_record *p101_module_map_idiom_first_local_include(const struct p101_env *env, const struct project_map *map, const char *path);
bool                         p101_module_map_idiom_target_matches_module(const struct p101_env *env, const char *target, const char *module_name);
bool                         p101_module_map_idiom_source_includes_own(const struct p101_env *env, const struct project_map *map, const char *path, const char *module_name);
bool                         p101_module_map_idiom_module_has_header(const struct p101_env *env, const struct project_map *map, const char *module_name);

#endif    // P101_MODULE_MAP_IDIOMS_INCLUDES_H
