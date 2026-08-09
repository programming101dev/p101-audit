#ifndef P101_WRAPPER_AUDIT_JUDGE_SUPPORT_H
#define P101_WRAPPER_AUDIT_JUDGE_SUPPORT_H

#include "model.h"

bool p101_wrapper_is_wrapper_implementation(const struct p101_env *env, const struct p101_wrapper_fact *call, const struct p101_wrapper_inventory *wrapper);
bool p101_wrapper_is_local(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call);
bool p101_wrapper_is_errno_macro_lowering(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call);
bool p101_wrapper_path_has_directory_component(const struct p101_env *env, const char *path, const char *component);
bool p101_wrapper_path_has_trailing_components(const struct p101_env *env, const char *path, const char *suffix);

#endif
