#ifndef P101_DOCTOR_REPORT_H
#define P101_DOCTOR_REPORT_H

#include "arguments.h"
#include "paths.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

struct doctor_result
{
    int wrapper_status;
    int error_contract_status;
    int module_status;
};

void p101_doctor_write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result);
void p101_doctor_write_json_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result);
void p101_doctor_write_evidence_receipt_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
void p101_doctor_write_receipt_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result);

#endif    // P101_DOCTOR_REPORT_H
