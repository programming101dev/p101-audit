#include "../include/report.h"
#include "../include/constants.h"
#include <errno.h>
#include <p101_c/p101_string.h>
#include <p101_tool_event/report.h>

static void report_check(const struct p101_env *env, struct p101_error *err, int status);

void p101_error_contract_report_begin(const struct p101_env *env, struct p101_error *err, struct contract_report *report, const struct arguments *args)
{
    const struct p101_tool_report_options options = {"audit-errors", "Clang AST facts for the selected C translation units.", "Unscanned code, third-party internals, and runtime-only behavior are outside this static contract report.", 0U, true};
    struct p101_tool_report_options       selected_options;
    int                                   status;

    P101_TRACE_SCOPE(env);
    p101_memset(env, report, 0, sizeof(*report));
    selected_options               = options;
    selected_options.outputs       = 0U;
    selected_options.human_summary = true;
    if(args->quiet)
    {
        selected_options.human_summary = false;
    }
    if(args->human || (!args->human && !args->json))
    {
        selected_options.outputs |= P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN;
    }
    if(args->json)
    {
        selected_options.outputs |= P101_TOOL_DIAGNOSTIC_OUTPUT_JSON;
    }
    status = p101_tool_report_begin(&report->tool_report, stdout, stderr, &selected_options);
    report_check(env, err, status);
}

void p101_error_contract_report_finding(const struct p101_env *env, struct p101_error *err, struct contract_report *report, enum p101_error_contract_rule rule_kind, const char *path, size_t line, const char *function_name, const char *message)
{
    static const p101_tool_finding rules[] =
        {P101_TOOL_FINDING_ERR_001, P101_TOOL_FINDING_ERR_002, P101_TOOL_FINDING_ERR_003, P101_TOOL_FINDING_ERR_004, P101_TOOL_FINDING_ERR_005, P101_TOOL_FINDING_ERR_006, P101_TOOL_FINDING_ERR_007, P101_TOOL_FINDING_ERR_008, P101_TOOL_FINDING_ERR_009};
    struct p101_tool_diagnostic diagnostic;
    int                         status;

    P101_TRACE_SCOPE(env);
    if(rule_kind < P101_ERROR_CONTRACT_RULE_ENV_REQUIRED || rule_kind >= P101_ERROR_CONTRACT_RULE_COUNT)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto done;
    }
    status = p101_tool_diagnostic_initialize(&diagnostic, rules[rule_kind], P101_TOOL_DIAGNOSTIC_ERROR, path, line, 0U, function_name, message);
    report_check(env, err, status);
    if(status != 0)
    {
        goto done;
    }
    status = p101_tool_report_emit(&report->tool_report, &diagnostic);
    report_check(env, err, status);
    if(status == 0)
    {
        report->findings++;
    }

done:
    return;
}

void p101_error_contract_report_end(const struct p101_env *env, struct p101_error *err, struct contract_report *report)
{
    const struct p101_tool_report_counter counters[] = {
        {"files_scanned", report->files_scanned}
    };
    p101_tool_outcome outcome;
    int               exit_status;
    int               status;

    P101_TRACE_SCOPE(env);
    outcome = P101_TOOL_OUTCOME_CLEAN;
    if(report->findings > 0U)
    {
        outcome = P101_TOOL_OUTCOME_FINDINGS;
    }
    exit_status = p101_tool_outcome_exit_status(outcome);
    status      = p101_tool_report_end(&report->tool_report, outcome, exit_status, counters, sizeof(counters) / sizeof(counters[0]));
    report_check(env, err, status);
}

static void report_check(const struct p101_env *env, struct p101_error *err, int status)
{
    P101_TRACE_SCOPE(env);
    if(status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}
