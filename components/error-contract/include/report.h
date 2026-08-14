#ifndef P101_ERROR_CONTRACT_REPORT_H
#define P101_ERROR_CONTRACT_REPORT_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_tool_support/report.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct contract_report
{
    struct p101_tool_report tool_report;
    size_t                  findings;
    size_t                  files_scanned;
};

enum p101_error_contract_rule
{
    P101_ERROR_CONTRACT_RULE_ENV_REQUIRED = 0,
    P101_ERROR_CONTRACT_RULE_ERROR_REQUIRED,
    P101_ERROR_CONTRACT_RULE_ERROR_DISCARDED,
    P101_ERROR_CONTRACT_RULE_UNCHECKED_CHAIN,
    P101_ERROR_CONTRACT_RULE_ERROR_OWNERSHIP,
    P101_ERROR_CONTRACT_RULE_ENV_OWNERSHIP,
    P101_ERROR_CONTRACT_RULE_PROCESS_TERMINATION,
    P101_ERROR_CONTRACT_RULE_SINGLE_EXIT,
    P101_ERROR_CONTRACT_RULE_ISOLATED_CALL,
    P101_ERROR_CONTRACT_RULE_COUNT
};

void p101_error_contract_report_begin(const struct p101_env *env, struct p101_error *err, struct contract_report *report, const struct arguments *args);
void p101_error_contract_report_finding(const struct p101_env *env, struct p101_error *err, struct contract_report *report, enum p101_error_contract_rule rule_kind, const char *path, size_t line, const char *function_name, const char *message);
void p101_error_contract_report_end(const struct p101_env *env, struct p101_error *err, struct contract_report *report);

#endif    // P101_ERROR_CONTRACT_REPORT_H
