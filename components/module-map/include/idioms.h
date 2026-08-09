#ifndef P101_MODULE_MAP_IDIOMS_H
#define P101_MODULE_MAP_IDIOMS_H

#include "model.h"
#include <p101_env/env.h>
#include <stdbool.h>
#include <stddef.h>

bool                       p101_module_map_idiom_public_function_exists(const struct p101_env *env, const struct project_map *map, const char *module_name, const char *function_name);
bool                       p101_module_map_idiom_public_function_anywhere(const struct p101_env *env, const struct project_map *map, const char *function_name);
bool                       p101_module_map_idiom_swap_suffix(const struct p101_env *env, const char *name, const char *suffix, const char *replacement, char *peer, size_t peer_size);
bool                       p101_module_map_idiom_module_shares_prefix(const struct p101_env *env, const struct project_map *map, const char *module_name, bool require_owner_prefix);
bool                       p101_module_map_idiom_wraps_platform_name(const struct p101_env *env, const struct project_map *map, const struct function_record *function);
const struct macro_record *p101_module_map_idiom_guard_macro(const struct p101_env *env, const struct project_map *map, const char *path);
bool                       p101_module_map_idiom_guard_suffix(const struct p101_env *env, const char *path, char *expected, size_t expected_size);
bool                       p101_module_map_idiom_name_ends_with(const struct p101_env *env, const char *name, const char *suffix);

#endif    // P101_MODULE_MAP_IDIOMS_H
