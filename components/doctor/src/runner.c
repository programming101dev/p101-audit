#include "../include/runner.h"
#include "../include/constants.h"
#include "../include/paths.h"
#include "../include/report.h"
#include "../include/source_inputs.h"
#include "../include/status.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
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
#include <p101_util/tool_run.h>
#include <stdio.h>

static int run_p101_wrapper_audit(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int run_p101_error_contract(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int run_p101_module_map(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path);

int p101_doctor_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    int                  p101_expression_result_15;
    int                  p101_expression_result_16;
    int                  p101_expression_result_17;
    bool                 p101_call_result_18;
    bool                 p101_call_result_19;
    bool                 p101_call_result_20;
    int                  p101_expression_result_21;
    int                  p101_expression_result_22;
    int                  p101_expression_result_23;
    bool                 p101_call_result_24;
    bool                 p101_call_result_25;
    bool                 p101_call_result_26;
    bool                 p101_call_result_1;
    bool                 p101_call_result_2;
    bool                 p101_call_result_3;
    bool                 p101_call_result_4;
    bool                 p101_call_result_5;
    bool                 p101_call_result_6;
    bool                 p101_call_result_7;
    struct doctor_paths  paths;
    struct doctor_result result;
    int                  ret_val;

    P101_TRACE_SCOPE(env);
    p101_memset(env, &paths, 0, sizeof(paths));
    p101_memset(env, &result, 0, sizeof(result));
    ret_val = EXIT_TROUBLE;

    p101_doctor_make_paths(env, err, args, &paths);

    p101_call_result_1 = p101_error_has_error(err);
    if(p101_call_result_1)
    {
        goto done;
    }

    p101_doctor_create_dir(env, err, paths.dir);

    p101_call_result_2 = p101_error_has_error(err);
    if(p101_call_result_2)
    {
        goto done;
    }

    p101_doctor_write_command_file(env, err, paths.command, args->command_argv);
    p101_doctor_write_manifest_file(env, err, paths.manifest, args, &paths);

    p101_call_result_3 = p101_error_has_error(err);
    if(p101_call_result_3)
    {
        goto done;
    }

    if(!args->skip_source_contracts)
    {
        result.wrapper_status = run_p101_wrapper_audit(env, err, args, &paths);

        p101_call_result_4 = p101_error_has_error(err);
        if(p101_call_result_4)
        {
            goto done;
        }

        result.error_contract_status = run_p101_error_contract(env, err, args, &paths);

        p101_call_result_5 = p101_error_has_error(err);
        if(p101_call_result_5)
        {
            goto done;
        }
    }

    result.module_status = run_p101_module_map(env, err, args, &paths);

    p101_call_result_6 = p101_error_has_error(err);
    if(p101_call_result_6)
    {
        goto done;
    }

    p101_doctor_write_summary_file(env, err, args, &paths, &result);
    p101_doctor_write_json_file(env, err, args, &paths, &result);
    p101_doctor_write_evidence_receipt_file(env, err, args, &paths);
    p101_doctor_write_receipt_file(env, err, args, &paths, &result);

    p101_call_result_7 = p101_error_has_error(err);
    if(p101_call_result_7)
    {
        goto done;
    }

    p101_printf(env, err, "audit-doctor: wrote report to %s\n", paths.dir);

    p101_expression_result_16 = 0;
    if(!args->skip_source_contracts)
    {
        p101_call_result_18 = p101_doctor_status_is_acceptable(result.wrapper_status);
        if(!p101_call_result_18)
        {
            p101_expression_result_17 = 1;
        }
        else
        {
            p101_call_result_19 = p101_doctor_status_is_acceptable(result.error_contract_status);
            if(!p101_call_result_19)
            {
                p101_expression_result_17 = 1;
            }
            else
            {
                p101_expression_result_17 = 0;
            }
        }
        if(p101_expression_result_17)
        {
            p101_expression_result_16 = 1;
        }
    }
    if(p101_expression_result_16)
    {
        p101_expression_result_15 = 1;
    }
    else
    {
        p101_call_result_20 = p101_doctor_status_is_acceptable(result.module_status);
        if(!p101_call_result_20)
        {
            p101_expression_result_15 = 1;
        }
        else
        {
            p101_expression_result_15 = 0;
        }
    }
    if(p101_expression_result_15)
    {
        ret_val = EXIT_TROUBLE;
        goto done;
    }

    p101_expression_result_22 = 0;
    if(!args->skip_source_contracts)
    {
        p101_call_result_24 = p101_doctor_status_has_findings(result.wrapper_status);
        if(p101_call_result_24)
        {
            p101_expression_result_23 = 1;
        }
        else
        {
            p101_call_result_25 = p101_doctor_status_has_findings(result.error_contract_status);
            if(p101_call_result_25)
            {
                p101_expression_result_23 = 1;
            }
            else
            {
                p101_expression_result_23 = 0;
            }
        }
        if(p101_expression_result_23)
        {
            p101_expression_result_22 = 1;
        }
    }
    if(p101_expression_result_22)
    {
        p101_expression_result_21 = 1;
    }
    else
    {
        p101_call_result_26 = p101_doctor_status_has_findings(result.module_status);
        if(p101_call_result_26)
        {
            p101_expression_result_21 = 1;
        }
        else
        {
            p101_expression_result_21 = 0;
        }
    }
    if(p101_expression_result_21)
    {
        ret_val = EXIT_FINDINGS;
        goto done;
    }

    ret_val = EXIT_SUCCESS;

done:
    return ret_val;
}

static int run_p101_error_contract(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    char  *tool_argv[MAX_SOURCE_PATHS + STATIC_TOOL_RESERVE];
    char   contract_path[PATH_LEN];
    char   source_paths[MAX_SOURCE_PATHS][PATH_LEN];
    char   facts_option[] = "-i";
    char   facts_path[PATH_LEN];
    size_t index;
    int    ret_val;

    P101_TRACE_SCOPE(env);
    p101_doctor_copy_text(env, contract_path, args->p101_error_contract);
    p101_doctor_copy_text(env, facts_path, paths->facts);
    p101_doctor_copy_source_paths(env, args, source_paths);

    index              = 0;
    tool_argv[index++] = contract_path;
    tool_argv[index++] = facts_option;
    tool_argv[index++] = facts_path;
    index              = p101_doctor_append_source_paths(tool_argv, index, source_paths, args->source_count);
    tool_argv[index]   = NULL;

    ret_val = run_tool_capture(env, err, tool_argv, paths->error_contract_stdout, paths->error_contract_stderr);
    return ret_val;
}

static int run_p101_wrapper_audit(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    bool   p101_call_result_8;
    int    p101_call_result_9;
    bool   p101_call_result_10;
    char  *tool_argv[MAX_SOURCE_PATHS + STATIC_TOOL_RESERVE];
    char   audit_path[PATH_LEN];
    char   facts_path[PATH_LEN];
    char   input_manifest_path[PATH_LEN];
    char   compile_db_path[PATH_LEN];
    char   allow_file_path[] = DEFAULT_WRAPPER_ALLOW_FILE;
    char   source_paths[MAX_SOURCE_PATHS][PATH_LEN];
    char   facts_option[]          = "--facts-output";
    char   input_manifest_option[] = "--input-manifest";
    char   allow_file_option[]     = "--allow-file";
    char   portability_option[]    = "--check-portability-includes";
    char   compile_db_option[]     = "--compile-db";
    char   compile_only_option[]   = "--compile-db-only";
    size_t index;
    int    ret_val;

    P101_TRACE_SCOPE(env);
    p101_doctor_copy_text(env, audit_path, args->p101_wrapper_audit);
    p101_doctor_copy_text(env, facts_path, paths->facts);
    p101_doctor_copy_text(env, input_manifest_path, paths->input_manifest);
    p101_doctor_copy_source_paths(env, args, source_paths);

    index              = 0;
    tool_argv[index++] = audit_path;
    tool_argv[index++] = facts_option;
    tool_argv[index++] = facts_path;
    tool_argv[index++] = input_manifest_option;
    tool_argv[index++] = input_manifest_path;
    tool_argv[index++] = portability_option;
    p101_call_result_8 = p101_doctor_resolve_compile_database(env, err, args, compile_db_path);
    if(p101_call_result_8)
    {
        tool_argv[index++] = compile_db_option;
        tool_argv[index++] = compile_db_path;
        tool_argv[index++] = compile_only_option;
    }
    p101_call_result_9 = p101_access(env, P101_ERROR_OPTIONAL, allow_file_path, F_OK);
    if(p101_call_result_9 == 0)    // P101_ERROR_OPTIONAL rationale: absence means the project has no boundary ledger.
    {
        tool_argv[index++] = allow_file_option;
        tool_argv[index++] = allow_file_path;
    }
    p101_call_result_10 = p101_error_has_error(err);
    if(p101_call_result_10)
    {
        ret_val = EXIT_TROUBLE;
    }
    else
    {
        index            = p101_doctor_append_source_paths(tool_argv, index, source_paths, args->source_count);
        tool_argv[index] = NULL;
        ret_val          = run_tool_capture(env, err, tool_argv, paths->wrapper_stdout, paths->wrapper_stderr);
    }

    return ret_val;
}

static int run_p101_module_map(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    bool   p101_call_result_12;
    char  *tool_argv[MAX_SOURCE_PATHS + MODULE_MAP_RESERVE];
    char   module_path[PATH_LEN];
    char   output_path[PATH_LEN];
    char   json_path[PATH_LEN];
    char   source_paths[MAX_SOURCE_PATHS][PATH_LEN];
    char   output_option[]     = "-o";
    char   facts_option[]      = "-i";
    char   compile_db_option[] = "-C";
    char   json_option[]       = "-d:json";
    char   facts_path[PATH_LEN];
    char   compile_db_path[PATH_LEN];
    size_t arg_index;
    int    ret_val;

    P101_TRACE_SCOPE(env);
    p101_doctor_copy_text(env, module_path, args->p101_module_map);
    p101_doctor_copy_text(env, output_path, paths->module_report);
    p101_doctor_copy_text(env, json_path, paths->module_json);
    p101_doctor_copy_text(env, facts_path, paths->facts);
    compile_db_path[0] = '\0';
    p101_doctor_copy_source_paths(env, args, source_paths);

    tool_argv[0] = module_path;
    tool_argv[1] = output_option;
    tool_argv[2] = output_path;
    arg_index    = 3;

    if(!args->skip_source_contracts)
    {
        tool_argv[arg_index] = facts_option;
        arg_index++;
        tool_argv[arg_index] = facts_path;
        arg_index++;
    }
    else
    {
        bool database_resolved;

        database_resolved = p101_doctor_resolve_compile_database(env, err, args, compile_db_path);
        if(database_resolved)
        {
            tool_argv[arg_index++] = compile_db_option;
            tool_argv[arg_index++] = compile_db_path;
        }
    }
    p101_call_result_12 = p101_error_has_error(err);
    if(p101_call_result_12)
    {
        ret_val = EXIT_TROUBLE;
    }
    else
    {
        bool status_acceptable;

        arg_index            = p101_doctor_append_source_paths(tool_argv, arg_index, source_paths, args->source_count);
        tool_argv[arg_index] = NULL;

        ret_val           = run_tool_capture(env, err, tool_argv, paths->module_stdout, paths->module_stderr);
        status_acceptable = p101_doctor_status_is_acceptable(ret_val);
        if(status_acceptable)
        {
            int json_status;

            arg_index              = 1;
            tool_argv[arg_index++] = json_option;
            tool_argv[arg_index++] = output_option;
            tool_argv[arg_index++] = json_path;
            if(!args->skip_source_contracts)
            {
                tool_argv[arg_index++] = facts_option;
                tool_argv[arg_index++] = facts_path;
            }
            else
            {
                if(compile_db_path[0] != '\0')
                {
                    tool_argv[arg_index++] = compile_db_option;
                    tool_argv[arg_index++] = compile_db_path;
                }
            }
            arg_index            = p101_doctor_append_source_paths(tool_argv, arg_index, source_paths, args->source_count);
            tool_argv[arg_index] = NULL;
            json_status          = run_tool_capture(env, err, tool_argv, paths->module_stdout, paths->module_stderr);
            if(json_status != ret_val)
            {
                ret_val = EXIT_TROUBLE << WAIT_STATUS_SHIFT;
            }
        }
    }
    return ret_val;
}

static int run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path)
{
    int                          p101_call_result_14;
    struct p101_tool_run_options options;

    P101_TRACE_SCOPE(env);
    options.stdout_path         = stdout_path;
    options.stderr_path         = stderr_path;
    options.diagnostic_name     = "audit-doctor";
    options.output_mode         = REPORT_FILE_MODE;
    options.child_setup         = NULL;
    options.child_setup_context = NULL;
    p101_call_result_14         = p101_tool_run_capture(env, err, tool_argv, &options);
    return p101_call_result_14;
}
