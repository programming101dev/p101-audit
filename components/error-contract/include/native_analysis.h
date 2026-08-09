#ifndef P101_ERROR_CONTRACT_NATIVE_ANALYSIS_H
#define P101_ERROR_CONTRACT_NATIVE_ANALYSIS_H

#include "arguments.h"
#include "contract_types.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

void p101_error_contract_load_analysis(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct contract_model *model);
bool p101_error_contract_is_process_termination_call(const struct p101_env *env, const char *usr);

#endif
