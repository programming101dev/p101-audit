#ifndef P101_WORKSPACE_FACT_BUNDLE_H
#define P101_WORKSPACE_FACT_BUNDLE_H

#include "model.h"

bool p101_workspace_fact_bundle_load(const struct p101_env *env, struct p101_error *err, const char *path, struct p101_wrapper_model *model);

#endif
