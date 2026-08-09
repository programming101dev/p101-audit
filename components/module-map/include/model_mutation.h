#ifndef P101_MODULE_MAP_MODEL_MUTATION_H
#define P101_MODULE_MAP_MODEL_MUTATION_H

#include "model.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

void p101_module_map_add_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, bool is_header);
void p101_module_map_add_named_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, const char *module_name, bool is_header);
void p101_module_map_add_include(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *target, const char *resolved, size_t line, bool is_local);
void p101_module_map_add_function(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, const char *usr, size_t line, bool is_static, bool is_header_declaration);
void p101_module_map_add_macro(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line);
void p101_module_map_add_type(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line);
void p101_module_map_add_call(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, const char *usr, const char *caller_usr, size_t line);
void p101_module_map_add_note(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, const char *symbol, const char *symbol_usr, size_t line);

#endif    // P101_MODULE_MAP_MODEL_MUTATION_H
