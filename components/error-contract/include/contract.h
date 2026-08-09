#ifndef P101_ERROR_CONTRACT_CONTRACT_H
#define P101_ERROR_CONTRACT_CONTRACT_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

int p101_error_contract_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args);

#endif    // P101_ERROR_CONTRACT_CONTRACT_H
