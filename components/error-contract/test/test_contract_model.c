#include "arguments.h"
#include "constants.h"
#include "contract.h"
#include "contract_model.h"
#include "errors.h"
#include "report.h"
#include "test_hooks.h"
#include "unity.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/p101_fnmatch.h>
#include <p101_filesystem/p101_ftw.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_filesystem/p101_libgen.h>
#include <p101_filesystem/p101_stdio.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_filesystem/p101_unistd.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_filesystem/sys/p101_statvfs.h>

static struct p101_error     *error;
static struct p101_env       *env;
static struct contract_model *model;

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
    model = (struct contract_model *)p101_calloc(env, error, 1U, sizeof(*model));
    TEST_ASSERT_NOT_NULL(model);
}

void tearDown(void)
{
    p101_free(env, model);
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void apply_kind(struct contract_model *model, enum p101_c_fact_kind kind, const char *value, size_t line, bool first_semantic_flag, bool second_semantic_flag)
{
    struct p101_c_fact fact;

    p101_memset(env, &fact, 0, sizeof(fact));
    fact.kind       = kind;
    fact.path       = "file.c";
    fact.value      = (char *)value;
    fact.usr        = (char *)value;
    fact.caller     = "function";
    fact.caller_usr = "c:@F@function";
    fact.line       = line;
    if(kind == P101_C_FACT_KIND_FUNCTION)
    {
        fact.usr            = "c:@F@function";
        fact.is_static      = first_semantic_flag;
        fact.is_declaration = second_semantic_flag;
    }
    else if(kind == P101_C_FACT_KIND_CALL)
    {
        fact.has_env_parameter   = first_semantic_flag;
        fact.has_error_parameter = second_semantic_flag;
    }
    p101_error_contract_test_apply_fact(env, error, model, &fact);
}

static void test_model_fact_kinds_and_limits(void)
{
    const char *notes[] = {"ENV_CONTRACT", "ERROR_CONTRACT", "ENV_USE", "ERROR_USE", "TYPE_SEMANTIC_ROLE:p101:trace-scope", "ERROR_CHECK", "ERROR_OPTIONAL", "ERROR_DISCARD", "ERROR_PROPAGATED", "ERROR_UNCHECKED_CHAIN", "CALL_NOT_ISOLATED", "OTHER"};
    char        dst[8];
    char        long_line[READ_BUF_LEN];
    FILE       *stream;
    char        exact_line[READ_BUF_LEN];
    char        double_line[(READ_BUF_LEN * 2U) + 1U];

    apply_kind(model, P101_C_FACT_KIND_UNKNOWN, "", 0U, false, false);
    apply_kind(model, (enum p101_c_fact_kind)99, "", 0U, false, false);
    apply_kind(model, P101_C_FACT_KIND_FILE, "", 0U, false, false);
    apply_kind(model, P101_C_FACT_KIND_INCLUDE, "", 0U, false, false);
    apply_kind(model, P101_C_FACT_KIND_TYPE, "", 0U, false, false);
    apply_kind(model, P101_C_FACT_KIND_MACRO, "", 0U, false, false);
    apply_kind(model, P101_C_FACT_KIND_FUNCTION, "function", 10U, false, false);
    apply_kind(model, P101_C_FACT_KIND_FUNCTION, "declaration", 11U, false, true);
    apply_kind(model, P101_C_FACT_KIND_CALL, "ignored", 12U, false, false);
    apply_kind(model, P101_C_FACT_KIND_CALL, "error-only", 12U, false, true);
    apply_kind(model, P101_C_FACT_KIND_CALL, "call", 13U, true, true);
    apply_kind(model, P101_C_FACT_KIND_NOTE, "CALLEE_SEMANTIC_ROLE:p101:ownership:error:acquire", 14U, false, false);
    apply_kind(model, P101_C_FACT_KIND_NOTE, "CALLEE_SEMANTIC_ROLE:p101:ownership:error:acquire", 15U, false, false);
    apply_kind(model, P101_C_FACT_KIND_NOTE, "CALLEE_SEMANTIC_ROLE:p101:ownership:error:release", 16U, false, false);
    apply_kind(model, P101_C_FACT_KIND_NOTE, "CALLEE_SEMANTIC_ROLE:p101:ownership:env:acquire", 17U, false, false);
    apply_kind(model, P101_C_FACT_KIND_NOTE, "CALLEE_SEMANTIC_ROLE:p101:ownership:env:acquire", 18U, false, false);
    apply_kind(model, P101_C_FACT_KIND_NOTE, "CALLEE_SEMANTIC_ROLE:p101:ownership:env:release", 19U, false, false);
    for(size_t i = 0U; i < sizeof(notes) / sizeof(notes[0]); i++)
    {
        apply_kind(model, P101_C_FACT_KIND_NOTE, notes[i], 10U + i, false, false);
    }
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    {
        struct p101_c_fact mismatched_contract;

        p101_memset(env, &mismatched_contract, 0, sizeof(mismatched_contract));
        mismatched_contract.kind  = P101_C_FACT_KIND_NOTE;
        mismatched_contract.path  = "other.c";
        mismatched_contract.value = "ENV_CONTRACT";
        mismatched_contract.line  = 10U;
        p101_error_contract_test_apply_fact(env, error, model, &mismatched_contract);
    }

    model->function_count = MAX_FACT_FUNCTIONS;
    apply_kind(model, P101_C_FACT_KIND_FUNCTION, "overflow", 1U, false, false);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
    model->event_count = MAX_FACT_EVENTS;
    apply_kind(model, P101_C_FACT_KIND_CALL, "overflow", 1U, true, false);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
    model->ownership_file_count = MAX_FACT_FUNCTIONS;
    p101_strncpy(env, model->ownership_files[0].path, "occupied.c", sizeof(model->ownership_files[0].path));
    apply_kind(model, P101_C_FACT_KIND_NOTE, "CALLEE_SEMANTIC_ROLE:p101:ownership:error:acquire", 1U, false, false);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);

    p101_error_contract_test_copy_text(env, dst, 0U, "x");
    p101_error_contract_test_copy_text(env, dst, sizeof(dst), NULL);
    TEST_ASSERT_EQUAL_STRING("", dst);
    p101_error_contract_test_copy_text(env, dst, sizeof(dst), "long-text");
    TEST_ASSERT_EQUAL_STRING("long-te", dst);
    p101_error_contract_test_copy_text(NULL, dst, sizeof(dst), "plain");
    TEST_ASSERT_EQUAL_STRING("plain", dst);

    p101_memset(env, long_line, 'x', sizeof(long_line) - 1U);
    long_line[sizeof(long_line) - 1U] = '\0';
    stream                            = p101_tmpfile(env, error);
    p101_fputs(env, error, "tail\n", stream);
    p101_fseek(env, error, stream, 0L, SEEK_SET);
    TEST_ASSERT_FALSE(p101_error_contract_test_fact_line_complete(env, error, stream, long_line));
    p101_fclose(env, error, stream);

    p101_memset(env, exact_line, 'x', sizeof(exact_line) - 2U);
    exact_line[sizeof(exact_line) - 2U] = '\n';
    exact_line[sizeof(exact_line) - 1U] = '\0';
    stream                              = p101_tmpfile(env, error);
    TEST_ASSERT_TRUE(p101_error_contract_test_fact_line_complete(env, error, stream, exact_line));
    p101_fclose(env, error, stream);

    stream = p101_tmpfile(env, error);
    TEST_ASSERT_FALSE(p101_error_contract_test_fact_line_complete(env, error, stream, long_line));
    p101_fclose(env, error, stream);

    p101_memset(env, double_line, 'x', sizeof(double_line) - 1U);
    double_line[sizeof(double_line) - 1U] = '\0';
    stream                                = p101_tmpfile(env, error);
    p101_fputs(env, error, double_line, stream);
    p101_fputs(env, error, "\n", stream);
    p101_fseek(env, error, stream, 0L, SEEK_SET);
    TEST_ASSERT_FALSE(p101_error_contract_test_fact_line_complete(env, error, stream, long_line));
    p101_fclose(env, error, stream);
}

static void test_analysis_reports_all_contract_findings(void)
{
    struct contract_report report;
    struct arguments       args;

    p101_memset(env, &args, 0, sizeof(args));
    p101_error_contract_report_begin(env, error, &report, &args);
    model->function_count    = 2U;
    model->functions[0].line = 10U;
    p101_strncpy(env, model->functions[0].path, "a.c", sizeof(model->functions[0].path));
    p101_strncpy(env, model->functions[0].name, "bad", sizeof(model->functions[0].name));
    model->functions[1].line = 100U;
    p101_strncpy(env, model->functions[1].path, "a.c", sizeof(model->functions[1].path));
    p101_strncpy(env, model->functions[1].name, "good", sizeof(model->functions[1].name));
    model->functions[1].has_env_contract   = true;
    model->functions[1].has_error_contract = true;

#define ADD_EVENT(index, event_kind, event_line)                                                                                                                                                                                                                   \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        model->events[index].kind = event_kind;                                                                                                                                                                                                                    \
        model->events[index].line = event_line;                                                                                                                                                                                                                    \
        p101_strncpy(env, model->events[index].path, "a.c", sizeof(model->events[index].path));                                                                                                                                                                    \
    } while(0)
    model->event_count = 10U;
    ADD_EVENT(0, CONTRACT_EVENT_TRACE_USE, 11U);
    ADD_EVENT(1, CONTRACT_EVENT_ERROR_CHECK, 12U);
    ADD_EVENT(2, CONTRACT_EVENT_ERROR_DISCARD, 13U);
    ADD_EVENT(3, CONTRACT_EVENT_ERROR_UNCHECKED_CHAIN, 14U);
    ADD_EVENT(4, CONTRACT_EVENT_ENV_USE, 101U);
    ADD_EVENT(5, CONTRACT_EVENT_ERROR_USE, 101U);
    ADD_EVENT(6, CONTRACT_EVENT_CALL, 102U);
    model->events[6].needs_env = model->events[6].needs_error = true;
    ADD_EVENT(7, CONTRACT_EVENT_ERROR_OPTIONAL, 103U);
    ADD_EVENT(8, CONTRACT_EVENT_ERROR_CHECK, 15U);
    ADD_EVENT(9, CONTRACT_EVENT_CALL_NOT_ISOLATED, 16U);
#undef ADD_EVENT

    p101_error_contract_test_analyze(env, error, model, &report);
    TEST_ASSERT_EQUAL_size_t(5U, report.findings);

    args.json = true;
    p101_error_contract_report_begin(env, error, &report, &args);
    p101_error_contract_report_finding(env, error, &report, P101_ERROR_CONTRACT_RULE_ENV_REQUIRED, "p", 1U, NULL, "message");
    p101_error_contract_report_finding(env, error, &report, P101_ERROR_CONTRACT_RULE_ERROR_REQUIRED, "p", 2U, "", "message");
    p101_error_contract_report_end(env, error, &report);
    args.json  = false;
    args.quiet = true;
    p101_error_contract_report_begin(env, error, &report, &args);
    p101_error_contract_report_finding(env, error, &report, P101_ERROR_CONTRACT_RULE_ENV_REQUIRED, "p", 1U, NULL, "message");
    p101_error_contract_report_finding(env, error, &report, P101_ERROR_CONTRACT_RULE_ERROR_REQUIRED, "p", 1U, "", "message");
    p101_error_contract_report_end(env, error, &report);
}

static void test_ownership_imbalance_findings(void)
{
    struct contract_report report;
    struct arguments       args;

    p101_memset(env, &args, 0, sizeof(args));
    model->ownership_file_count                       = 1U;
    model->ownership_files[0].error_create_count      = 2U;
    model->ownership_files[0].error_destroy_count     = 1U;
    model->ownership_files[0].env_create_count        = 2U;
    model->ownership_files[0].env_destroy_count       = 1U;
    model->ownership_files[0].first_error_create_line = 10U;
    model->ownership_files[0].first_env_create_line   = 20U;
    p101_strncpy(env, model->ownership_files[0].path, "owner.c", sizeof(model->ownership_files[0].path));
    p101_error_contract_report_begin(env, error, &report, &args);
    p101_error_contract_test_analyze(env, error, model, &report);
    TEST_ASSERT_EQUAL_size_t(2U, report.findings);

    model->ownership_files[0].error_destroy_count = 2U;
    model->ownership_files[0].env_destroy_count   = 2U;
    p101_error_contract_report_begin(env, error, &report, &args);
    p101_error_contract_test_analyze(env, error, model, &report);
    TEST_ASSERT_EQUAL_size_t(0U, report.findings);
}

static void test_local_contracts_and_optional_boundaries(void)
{
    struct contract_report report;
    struct arguments       args;

    p101_memset(env, &args, 0, sizeof(args));
    p101_error_contract_report_begin(env, error, &report, &args);
    model->function_count    = 2U;
    model->functions[0].line = 10U;
    p101_strncpy(env, model->functions[0].path, "a.c", sizeof(model->functions[0].path));
    p101_strncpy(env, model->functions[0].name, "local", sizeof(model->functions[0].name));
    model->functions[1].line = 50U;
    p101_strncpy(env, model->functions[1].path, "a.c", sizeof(model->functions[1].path));
    p101_strncpy(env, model->functions[1].name, "next", sizeof(model->functions[1].name));

    model->event_count = 9U;
#define LOCAL_EVENT(index, event_kind, event_path, event_line)                                                                                                                                                                                                     \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        model->events[index].kind = event_kind;                                                                                                                                                                                                                    \
        model->events[index].line = event_line;                                                                                                                                                                                                                    \
        p101_strncpy(env, model->events[index].path, event_path, sizeof(model->events[index].path));                                                                                                                                                               \
    } while(0)
    LOCAL_EVENT(0, CONTRACT_EVENT_ENV_USE, "a.c", 10U);
    LOCAL_EVENT(1, CONTRACT_EVENT_ERROR_USE, "a.c", 10U);
    LOCAL_EVENT(2, CONTRACT_EVENT_ERROR_OPTIONAL, "a.c", 12U);
    LOCAL_EVENT(3, CONTRACT_EVENT_CALL, "a.c", 13U);
    model->events[3].needs_env = model->events[3].needs_error = true;
    LOCAL_EVENT(4, CONTRACT_EVENT_ERROR_DISCARD, "a.c", 13U);
    LOCAL_EVENT(5, CONTRACT_EVENT_ERROR_UNCHECKED_CHAIN, "a.c", 13U);
    LOCAL_EVENT(6, CONTRACT_EVENT_CALL, "other.c", 13U);
    LOCAL_EVENT(7, CONTRACT_EVENT_CALL, "a.c", 60U);
    LOCAL_EVENT(8, CONTRACT_EVENT_ERROR_OPTIONAL, "a.c", 13U);
#undef LOCAL_EVENT
    p101_error_contract_test_analyze(env, error, model, &report);
    TEST_ASSERT_EQUAL_size_t(0U, report.findings);
}

static void test_function_boundaries_and_unmatched_optional_marker(void)
{
    struct contract_report report;
    struct arguments       args;

    p101_memset(env, &args, 0, sizeof(args));
    p101_error_contract_report_begin(env, error, &report, &args);
    model->function_count = 4U;
    p101_strncpy(env, model->functions[0].path, "a.c", sizeof(model->functions[0].path));
    p101_strncpy(env, model->functions[0].name, "first", sizeof(model->functions[0].name));
    model->functions[0].line = 10U;
    p101_strncpy(env, model->functions[1].path, "a.c", sizeof(model->functions[1].path));
    p101_strncpy(env, model->functions[1].name, "later", sizeof(model->functions[1].name));
    model->functions[1].line = 50U;
    p101_strncpy(env, model->functions[2].path, "a.c", sizeof(model->functions[2].path));
    p101_strncpy(env, model->functions[2].name, "latest", sizeof(model->functions[2].name));
    model->functions[2].line = 100U;
    p101_strncpy(env, model->functions[3].path, "other.c", sizeof(model->functions[3].path));
    p101_strncpy(env, model->functions[3].name, "other", sizeof(model->functions[3].name));
    model->functions[3].line = 20U;

    model->event_count           = 5U;
    model->events[0].kind        = CONTRACT_EVENT_ERROR_OPTIONAL;
    model->events[0].line        = 5U;
    model->events[1].kind        = CONTRACT_EVENT_ERROR_DISCARD;
    model->events[1].line        = 14U;
    model->events[2].kind        = CONTRACT_EVENT_ERROR_OPTIONAL;
    model->events[2].line        = 13U;
    model->events[3].kind        = CONTRACT_EVENT_ERROR_OPTIONAL;
    model->events[3].line        = 13U;
    model->events[4].kind        = CONTRACT_EVENT_CALL;
    model->events[4].line        = 13U;
    model->events[4].needs_error = true;
    p101_strncpy(env, model->events[0].path, "other.c", sizeof(model->events[0].path));
    p101_strncpy(env, model->events[1].path, "a.c", sizeof(model->events[1].path));
    p101_strncpy(env, model->events[2].path, "other.c", sizeof(model->events[2].path));
    p101_strncpy(env, model->events[3].path, "a.c", sizeof(model->events[3].path));
    p101_strncpy(env, model->events[4].path, "a.c", sizeof(model->events[4].path));

    p101_error_contract_test_analyze(env, error, model, &report);
    TEST_ASSERT_EQUAL_size_t(1U, report.findings);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_model_fact_kinds_and_limits);
    RUN_TEST(test_analysis_reports_all_contract_findings);
    RUN_TEST(test_ownership_imbalance_findings);
    RUN_TEST(test_local_contracts_and_optional_boundaries);
    RUN_TEST(test_function_boundaries_and_unmatched_optional_marker);
    return UNITY_END();
}
