#ifndef P101_MODULE_MAP_STRINGS_H
#define P101_MODULE_MAP_STRINGS_H

#include <p101_env/env.h>
#include <stddef.h>

void        p101_module_map_copy_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source);
void        p101_module_map_append_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source);
void        p101_module_map_basename_no_suffix(const struct p101_env *env, char *destination, size_t destination_size, const char *path);
void        p101_module_map_normalize_module_name(const struct p101_env *env, char *destination, size_t destination_size, const char *module_name);
void        p101_module_map_include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const char *include_name);
char       *p101_module_map_trim_left(const struct p101_env *env, char *text);
void        p101_module_map_trim_right(const struct p101_env *env, char *text);
const char *p101_module_map_path_basename(const struct p101_env *env, const char *path);

#endif    // P101_MODULE_MAP_STRINGS_H
