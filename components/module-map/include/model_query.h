#ifndef P101_MODULE_MAP_MODEL_QUERY_H
#define P101_MODULE_MAP_MODEL_QUERY_H

#include "model.h"
#include <p101_env/env.h>
#include <stdbool.h>

bool p101_module_map_module_has_direct_include(const struct p101_env *env, const struct project_map *map, const char *from, const char *target);
bool p101_module_map_module_used_outside_module(const struct p101_env *env, const struct project_map *map, const char *module_name);
bool p101_module_map_function_has_header_declaration(const struct p101_env *env, const struct project_map *map, const struct function_record *function);
bool p101_module_map_function_used_outside_module(const struct p101_env *env, const struct project_map *map, const struct function_record *function);
bool p101_module_map_function_has_non_static_definition(const struct p101_env *env, const struct project_map *map, const struct function_record *function);
bool p101_module_map_symbol_used_outside_module(const struct p101_env *env, const struct project_map *map, const char *module_name, const char *symbol_name);
bool p101_module_map_include_target_exists(const struct p101_env *env, const struct project_map *map, const char *target);

#endif    // P101_MODULE_MAP_MODEL_QUERY_H
