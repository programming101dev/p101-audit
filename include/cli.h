#ifndef P101_WRAPPER_AUDIT_CLI_H
#define P101_WRAPPER_AUDIT_CLI_H

#include "model.h"

bool p101_wrapper_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct p101_wrapper_arguments *arguments, bool facts_only);
void p101_wrapper_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, bool facts_only);

#endif
