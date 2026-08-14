#include "cli.h"
#include "constants.h"
#include "paths.h"
#include "report.h"
#include "source_inputs.h"
#include "status.h"
#include "unity.h"
#include <p101_c/p101_stdio.h>
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
#include <p101_process/p101_sched.h>
#include <p101_process/p101_setjmp.h>
#include <p101_process/p101_signal.h>
#include <p101_process/p101_spawn.h>
#include <p101_process/p101_stdio.h>
#include <p101_process/p101_stdlib.h>
#include <p101_process/p101_unistd.h>
#include <p101_process/sys/p101_resource.h>
#include <p101_process/sys/p101_times.h>
#include <p101_process/sys/p101_wait.h>
#include <p101_tool_support/receipt.h>
#include <stdbool.h>
#include <stdlib.h>

static struct p101_error *error;
static struct p101_env   *env;

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
}

void tearDown(void)
{
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void test_status_classification(void)
{
    TEST_ASSERT_TRUE(p101_doctor_status_is_clean(0));
    TEST_ASSERT_TRUE(p101_doctor_status_has_findings(256));
    TEST_ASSERT_TRUE(p101_doctor_status_is_acceptable(0));
    TEST_ASSERT_TRUE(p101_doctor_status_is_acceptable(256));
    TEST_ASSERT_FALSE(p101_doctor_status_is_acceptable(512));
}

static void test_make_doctor_paths_uses_requested_directory(void)
{
    struct arguments    args;
    struct doctor_paths paths;

    p101_memset(env, &args, 0, sizeof(args));
    p101_memset(env, &paths, 0, sizeof(paths));
    args.doctor_dir = "/tmp/audit-doctor-test";

    p101_doctor_make_paths(env, error, &args, &paths);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test", paths.dir);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/audit-wrappers.stdout.txt", paths.wrapper_stdout);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/audit-wrappers.stderr.txt", paths.wrapper_stderr);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/audit-errors.stdout.txt", paths.error_contract_stdout);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/audit-errors.stderr.txt", paths.error_contract_stderr);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/audit-modules.stdout.txt", paths.module_stdout);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/audit-modules.stderr.txt", paths.module_stderr);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/audit-modules.md", paths.module_report);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/summary.md", paths.summary);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/receipt.txt", paths.receipt);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/tool-receipt.json", paths.tool_receipt);
    TEST_ASSERT_EQUAL_STRING("/tmp/audit-doctor-test/manifest.txt", paths.manifest);

    {
        char long_dir[PATH_LEN + 1U];

        p101_memset(env, long_dir, 'x', sizeof(long_dir) - 1U);
        long_dir[sizeof(long_dir) - 1U] = '\0';
        args.doctor_dir                 = long_dir;
        p101_doctor_make_paths(env, error, &args, &paths);
        TEST_ASSERT_TRUE(p101_error_has_error(error));
    }
}

static void test_parse_repeated_source_paths(void)
{
    struct arguments args;
    char            *argv[] = {"audit-doctor", "-s", "src", "-s", "include", "-x", "--", "./program", NULL};

    p101_doctor_arguments_init(env, &args);
    p101_doctor_parse_arguments(env, error, 8, argv, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_TRUE(args.skip_source_contracts);
    TEST_ASSERT_TRUE(args.source_paths_set);
    TEST_ASSERT_EQUAL_INT(2, args.source_count);
    TEST_ASSERT_EQUAL_STRING("src", args.source_paths[0]);
    TEST_ASSERT_EQUAL_STRING("include", args.source_paths[1]);
    TEST_ASSERT_EQUAL_INT(1, args.command_argc);
    TEST_ASSERT_EQUAL_STRING("./program", args.command_argv[0]);
    TEST_ASSERT_EQUAL_STRING(DEFAULT_WRAPPER_AUDIT, args.p101_wrapper_audit);
    TEST_ASSERT_EQUAL_STRING(DEFAULT_ERROR_CONTRACT, args.p101_error_contract);
}

static void expect_invalid(struct arguments *args)
{
    p101_doctor_check_arguments(env, error, args);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
}

static void test_argument_validation_and_conversion(void)
{
    struct arguments args;
    char            *command[] = {"/usr/bin/true", NULL};

    p101_doctor_arguments_init(env, &args);
    args.command_argv = command;
    args.command_argc = 1;
    p101_doctor_check_arguments(env, error, &args);
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    args.command_argv = NULL;
    expect_invalid(&args);
    args.command_argv = command;
    args.command_argc = MAX_TOOL_ARGS;
    expect_invalid(&args);
    args.command_argc = 1;
    args.doctor_dir   = "";
    expect_invalid(&args);
    args.doctor_dir   = NULL;
    args.source_count = 0;
    expect_invalid(&args);
    args.source_count    = 1;
    args.source_paths[0] = NULL;
    expect_invalid(&args);
    args.source_paths[0] = "";
    expect_invalid(&args);
    args.source_paths[0] = ".";
    args.compile_db_path = "";
    expect_invalid(&args);
    args.compile_db_path = "db";
    p101_doctor_check_arguments(env, error, &args);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    args.compile_db_path = NULL;

#define EXPECT_EMPTY_FIELD(field)                                                                                                                                                                                                                                  \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        args.field = "";                                                                                                                                                                                                                                           \
        expect_invalid(&args);                                                                                                                                                                                                                                     \
        args.field = "tool";                                                                                                                                                                                                                                       \
    } while(0)
    EXPECT_EMPTY_FIELD(p101_wrapper_audit);
    EXPECT_EMPTY_FIELD(p101_error_contract);
    EXPECT_EMPTY_FIELD(p101_module_map);
#undef EXPECT_EMPTY_FIELD

#define EXPECT_NULL_FIELD(field)                                                                                                                                                                                                                                   \
    do                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                              \
        args.field = NULL;                                                                                                                                                                                                                                         \
        expect_invalid(&args);                                                                                                                                                                                                                                     \
        args.field = "tool";                                                                                                                                                                                                                                       \
    } while(0)
    EXPECT_NULL_FIELD(p101_wrapper_audit);
    EXPECT_NULL_FIELD(p101_error_contract);
    EXPECT_NULL_FIELD(p101_module_map);
#undef EXPECT_NULL_FIELD

    args.skip_source_contracts = true;
    args.p101_wrapper_audit    = "";
    args.p101_error_contract   = "";
    p101_doctor_check_arguments(env, error, &args);
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    p101_doctor_convert_arguments(env, error, &args);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
}

static void test_source_inputs_status_and_reports(void)
{
    struct arguments     args;
    struct doctor_paths  paths;
    struct doctor_result result;
    char                 copied[PATH_LEN];
    char                 sources[MAX_SOURCE_PATHS][PATH_LEN];
    char                *tool_argv[MAX_SOURCE_PATHS + 2];
    char                *command[] = {"quote\" slash\\ line\nreturn\rtab\t\1\200", NULL};
    char                 dir[PATH_LEN];
    FILE                *stream;

    p101_doctor_arguments_init(env, &args);
    args.command_argv    = command;
    args.command_argc    = 1;
    args.source_count    = 2;
    args.source_paths[0] = "src";
    args.source_paths[1] = "include";
    p101_memset(env, &paths, 0, sizeof(paths));
    p101_memset(env, &result, 0, sizeof(result));
    p101_snprintf(env, error, dir, sizeof(dir), "/tmp/audit-doctor-unit-%ld", (long)p101_getpid(env));
    args.doctor_dir = dir;
    p101_doctor_make_paths(env, error, &args, &paths);
    p101_mkdir(env, error, paths.dir, 0700);
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    p101_doctor_copy_text(env, copied, "text");
    TEST_ASSERT_EQUAL_STRING("text", copied);
    p101_doctor_copy_source_paths(env, &args, sources);
    TEST_ASSERT_EQUAL_UINT64(2U, p101_doctor_append_source_paths(tool_argv, 0U, sources, 2));
    args.compile_db_path = "compile_commands.json";
    TEST_ASSERT_TRUE(p101_doctor_resolve_compile_database(env, error, &args, copied));
    args.compile_db_path = NULL;
    (void)p101_doctor_resolve_compile_database(env, error, &args, copied);
    p101_error_reset(error);

    stream = p101_tmpfile(env, error);
    TEST_ASSERT_NOT_NULL(stream);
    p101_doctor_print_status_markdown(env, error, stream, "clean", 0);
    p101_doctor_print_status_markdown(env, error, stream, "signal", SIGTERM);
    p101_doctor_print_status_markdown(env, error, stream, "other", 0x7f);
    p101_doctor_print_status_markdown(env, error, stream, "continued", 0x137f);
    p101_doctor_print_status_json(env, error, stream, "clean", 0);
    p101_doctor_print_status_json(env, error, stream, "signal", SIGTERM);
    p101_doctor_print_status_json(env, error, stream, "other", 0x7f);
    p101_doctor_print_status_json(env, error, stream, "continued", 0x137f);
    TEST_ASSERT_EQUAL_STRING("clean", p101_doctor_status_word(0));
    TEST_ASSERT_EQUAL_STRING("findings", p101_doctor_status_word(1 << 8));
    TEST_ASSERT_EQUAL_STRING("trouble", p101_doctor_status_word(2 << 8));
    p101_fclose(env, error, stream);

    result.wrapper_status        = 0;
    result.error_contract_status = 1 << 8;
    result.module_status         = SIGTERM;
    p101_doctor_write_summary_file(env, error, &args, &paths, &result);
    p101_doctor_write_json_file(env, error, &args, &paths, &result);
    p101_doctor_write_evidence_receipt_file(env, error, &args, &paths);
    p101_doctor_write_receipt_file(env, error, &args, &paths, &result);
    {
        struct p101_tool_run_receipt_validation validation;

        TEST_ASSERT_EQUAL_INT(0, p101_tool_run_receipt_validate_file(error, paths.tool_receipt, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, &validation));
        TEST_ASSERT_EQUAL_INT(P101_TOOL_RECEIPT_VALID, validation.status);
        TEST_ASSERT_EQUAL_INT(P101_TOOL_OUTCOME_TOOL_ERROR, validation.outcome);
    }

    args.skip_source_contracts = true;
    args.source_count          = 1;
    args.source_paths[0]       = NULL;
    p101_doctor_write_summary_file(env, error, &args, &paths, &result);
    p101_doctor_write_json_file(env, error, &args, &paths, &result);
    p101_doctor_write_evidence_receipt_file(env, error, &args, &paths);
    p101_doctor_write_receipt_file(env, error, &args, &paths, &result);
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    p101_unlink(env, error, paths.summary);
    p101_unlink(env, error, paths.json);
    p101_unlink(env, error, paths.receipt);
    p101_unlink(env, error, paths.tool_receipt);
    p101_rmdir(env, error, paths.dir);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_status_classification);
    RUN_TEST(test_make_doctor_paths_uses_requested_directory);
    RUN_TEST(test_parse_repeated_source_paths);
    RUN_TEST(test_argument_validation_and_conversion);
    RUN_TEST(test_source_inputs_status_and_reports);
    return UNITY_END();
}
