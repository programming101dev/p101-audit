#ifndef P101_MODULE_MAP_MODEL_NOTES_H
#define P101_MODULE_MAP_MODEL_NOTES_H

#include "model.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_module_map_note_error_use(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file);
void p101_module_map_note_error_check(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file);

#endif    // P101_MODULE_MAP_MODEL_NOTES_H
