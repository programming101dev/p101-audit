#ifndef P101_AUDIT_API_H
#define P101_AUDIT_API_H

#include <p101_env/env.h>
#include <p101_error/error.h>

int p101_api_snapshot(const struct p101_env *env, struct p101_error *err, const char *workspace, const char *facts, const char *output);
int p101_api_compare(const struct p101_env *env, struct p101_error *err, const char *old_snapshot, const char *new_snapshot);

#endif
