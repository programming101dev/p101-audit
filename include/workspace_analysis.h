#ifndef P101_WORKSPACE_ANALYSIS_H
#define P101_WORKSPACE_ANALYSIS_H

#include "model.h"
#include "workspace_audit.h"

enum
{
    P101_WORKSPACE_ANALYSIS_ARGUMENT_CAPACITY = 64
};

bool p101_workspace_audit_prepare_analysis(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_wrapper_arguments *arguments, char storage[][P101_WORKSPACE_AUDIT_PATH_SIZE], size_t capacity);

#endif    // P101_WORKSPACE_ANALYSIS_H
