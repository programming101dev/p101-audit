#ifndef P101_DOCTOR_RUNNER_H
#define P101_DOCTOR_RUNNER_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

int p101_doctor_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args);

#endif    // P101_DOCTOR_RUNNER_H
