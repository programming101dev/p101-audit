#ifndef P101_ERROR_CONTRACT_TEST_HOOKS_H
#define P101_ERROR_CONTRACT_TEST_HOOKS_H

#include "contract_model.h"
#include "report.h"
#include <p101_c_facts/facts.h>
#include <stdio.h>

void p101_error_contract_test_apply_fact(const struct p101_env *, struct p101_error *, struct contract_model *, const struct p101_c_fact *);
bool p101_error_contract_test_fact_line_complete(const struct p101_env *, struct p101_error *, FILE *, char *);
void p101_error_contract_test_copy_text(const struct p101_env *, char *, size_t, const char *);
void p101_error_contract_test_analyze(const struct p101_env *, struct p101_error *, const struct contract_model *, struct contract_report *);

#endif
