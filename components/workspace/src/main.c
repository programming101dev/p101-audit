#include "workspace_audit.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_tool_support/diagnostic.h>

enum
{
    EXIT_FINDINGS = 1,
    EXIT_TROUBLE  = 2
};

static void usage(const struct p101_env *env, struct p101_error *err, const char *program);
static bool parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char **argv, struct p101_workspace_audit_options *options, bool *help);

int main(int argc, char **argv)
{
    struct p101_error                  *err;
    struct p101_env                    *env;
    struct p101_workspace_audit_options options;
    struct p101_workspace_audit_result  result;
    bool                                help;
    bool                                parsed;
    bool                                ran;
    bool                                has_error;
    int                                 comparison;
    int                                 status;
    const char                         *message;
    char                                workspace[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                                scripts_root[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                               *resolved;

    err = p101_error_create(false);
    env = p101_env_create(err, NULL);
    p101_workspace_audit_result_init(&result);
    status = EXIT_TROUBLE;
    parsed = parse_arguments(env, err, argc, argv, &options, &help);
    if(!parsed)
    {
        usage(env, P101_ERROR_OPTIONAL, argv[0]);
        if(help)
        {
            status = EXIT_SUCCESS;
        }
        goto done;
    }
    resolved = p101_realpath(env, err, options.workspace, workspace);
    if(resolved == NULL)
    {
        goto done;
    }
    options.workspace = workspace;
    resolved          = p101_realpath(env, err, options.scripts_root, scripts_root);
    if(resolved == NULL)
    {
        goto done;
    }
    options.scripts_root = scripts_root;
    comparison           = p101_strcmp(env, options.policy, "wrapper-fault-semantics");
    if(comparison == 0)
    {
        ran = p101_workspace_audit_run_fault_semantics(env, err, &options, &result);
    }
    else
    {
        comparison = p101_strcmp(env, options.policy, "native-wrapper-parity");
        if(comparison == 0)
        {
            ran = p101_workspace_audit_run_native_parity(env, err, &options, &result);
        }
        else
        {
            comparison = p101_strcmp(env, options.policy, "functional-library-split");
            if(comparison == 0)
            {
                ran = p101_workspace_audit_run_functional_layout(env, err, &options, &result);
            }
            else
            {
                comparison = p101_strcmp(env, options.policy, "test-inventory");
                if(comparison == 0)
                {
                    ran = p101_workspace_audit_run_test_inventory(env, err, &options, &result);
                }
                else
                {
                    comparison = p101_strcmp(env, options.policy, "source-responsibilities");
                    if(comparison == 0)
                    {
                        ran = p101_workspace_audit_run_source_responsibilities(env, err, &options, &result);
                    }
                    else
                    {
                        comparison = p101_strcmp(env, options.policy, "boundaries");
                        if(comparison == 0)
                        {
                            ran = p101_workspace_audit_run_boundaries(env, err, &options, &result);
                        }
                        else
                        {
                            comparison = p101_strcmp(env, options.policy, "wrapper-unit-tests");
                            if(comparison == 0)
                            {
                                ran = p101_workspace_audit_run_wrapper_unit_tests(env, err, &options, &result);
                            }
                            else
                            {
                                comparison = p101_strcmp(env, options.policy, "instrumentation");
                                if(comparison == 0)
                                {
                                    ran = p101_workspace_audit_run_instrumentation(env, err, &options, &result);
                                }
                                else
                                {
                                    comparison = p101_strcmp(env, options.policy, "quality-contract");
                                    if(comparison == 0)
                                    {
                                        ran = p101_workspace_audit_run_quality_contract(env, err, &options, &result);
                                    }
                                    else
                                    {
                                        P101_ERROR_RAISE_USER(err, "unknown workspace audit policy", EINVAL);
                                        ran = false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if(!ran)
    {
        goto done;
    }
    p101_workspace_audit_write(env, err, &options, &result);
    status = result.finding_count == 0U ? EXIT_SUCCESS : EXIT_FINDINGS;

done:
    has_error = p101_error_has_error(err);
    if(has_error)
    {
        message = p101_error_get_message(err);
        p101_fprintf(env, P101_ERROR_OPTIONAL, stderr, "%s:1:1: error: %s [P101-WORKSPACE-TROUBLE]\n", argv[0], message);
        status = EXIT_TROUBLE;
    }
    p101_workspace_audit_result_destroy(env, &result);
    p101_env_destroy(env);
    p101_error_destroy(err);
    return status;
}

static void usage(const struct p101_env *env, struct p101_error *err, const char *program)
{
    p101_fprintf(env,
                 err,
                 stderr,
                 "Usage: %s --policy POLICY [--workspace PATH] [--scripts-root PATH] [--facts PATH] [--receipt PATH] [--execution-receipt PATH] [-d:human|json|human,json]\n"
                 "Policies: boundaries, functional-library-split, instrumentation, native-wrapper-parity, quality-contract, source-responsibilities, test-inventory, wrapper-fault-semantics, wrapper-unit-tests\n",
                 program);
}

static bool parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char **argv, struct p101_workspace_audit_options *options, bool *help)
{
    int          index;
    int          comparison;
    int          parse_status;
    unsigned int outputs;
    bool         parsed;

    p101_memset(env, options, 0, sizeof(*options));
    options->workspace    = "..";
    options->scripts_root = ".";
    options->outputs      = P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN;
    *help                 = false;
    parsed                = true;
    for(index = 1; index < argc && parsed; index++)
    {
        comparison = p101_strcmp(env, argv[index], "-h");
        if(comparison == 0)
        {
            *help  = true;
            parsed = false;
            continue;
        }
        comparison = p101_strcmp(env, argv[index], "--help");
        if(comparison == 0)
        {
            *help  = true;
            parsed = false;
            continue;
        }
        comparison = p101_strcmp(env, argv[index], "--policy");
        if(comparison == 0 && index + 1 < argc)
        {
            index++;
            options->policy = argv[index];
            continue;
        }
        comparison = p101_strcmp(env, argv[index], "--workspace");
        if(comparison == 0 && index + 1 < argc)
        {
            index++;
            options->workspace = argv[index];
            continue;
        }
        comparison = p101_strcmp(env, argv[index], "--scripts-root");
        if(comparison == 0 && index + 1 < argc)
        {
            index++;
            options->scripts_root = argv[index];
            continue;
        }
        comparison = p101_strcmp(env, argv[index], "--facts");
        if(comparison == 0 && index + 1 < argc)
        {
            index++;
            options->facts_path = argv[index];
            continue;
        }
        comparison = p101_strcmp(env, argv[index], "--receipt");
        if(comparison == 0 && index + 1 < argc)
        {
            index++;
            options->receipt_path = argv[index];
            continue;
        }
        comparison = p101_strcmp(env, argv[index], "--execution-receipt");
        if(comparison == 0 && index + 1 < argc)
        {
            index++;
            options->execution_receipt_path = argv[index];
            continue;
        }
        comparison = p101_strncmp(env, argv[index], "-d:", 3U);
        if(comparison == 0)
        {
            outputs      = 0U;
            parse_status = p101_tool_diagnostic_parse_outputs(argv[index] + 3, &outputs);
            if(parse_status != 0)
            {
                P101_ERROR_RAISE_USER(err, "invalid diagnostic output selection", EINVAL);
                parsed = false;
            }
            else
            {
                options->outputs = outputs;
            }
            continue;
        }
        P101_ERROR_RAISE_USER(err, "unknown workspace audit option", EINVAL);
        parsed = false;
    }
    if(parsed && options->policy == NULL)
    {
        P101_ERROR_RAISE_USER(err, "--policy is required", EINVAL);
        parsed = false;
    }
    return parsed;
}
