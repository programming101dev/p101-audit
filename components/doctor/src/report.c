#include "../include/report.h"
#include "../include/constants.h"
#include "../include/status.h"
#include <errno.h>
#include <inttypes.h>
#include <p101_c/p101_stdio.h>
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
#include <p101_record/record.h>
#include <p101_tool_event/receipt.h>
#include <stdio.h>

static void   write_grade_line(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);
static void   write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);
static bool   doctor_result_has_findings(const struct arguments *args, const struct doctor_result *result);
static bool   doctor_result_has_trouble(const struct arguments *args, const struct doctor_result *result);
static size_t doctor_result_completed_checks(const struct arguments *args, const struct doctor_result *result);
static void   write_artifact_fingerprint(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, const char *path);

static bool doctor_result_has_findings(const struct arguments *args, const struct doctor_result *result)
{
    int p101_expression_result_16;
    int p101_expression_result_17;
    p101_expression_result_17 = 0;
    if(!args->skip_source_contracts)
    {
        int  p101_expression_result_18;
        bool p101_call_result_19;

        p101_call_result_19 = p101_doctor_status_has_findings(result->wrapper_status);
        if(p101_call_result_19)
        {
            p101_expression_result_18 = 1;
        }
        else
        {
            bool p101_call_result_20;

            p101_call_result_20 = p101_doctor_status_has_findings(result->error_contract_status);
            if(p101_call_result_20)
            {
                p101_expression_result_18 = 1;
            }
            else
            {
                p101_expression_result_18 = 0;
            }
        }
        if(p101_expression_result_18)
        {
            p101_expression_result_17 = 1;
        }
    }
    if(p101_expression_result_17)
    {
        p101_expression_result_16 = 1;
    }
    else
    {
        bool p101_call_result_21;

        p101_call_result_21 = p101_doctor_status_has_findings(result->module_status);
        if(p101_call_result_21)
        {
            p101_expression_result_16 = 1;
        }
        else
        {
            p101_expression_result_16 = 0;
        }
    }
    return p101_expression_result_16 != 0;
}

static bool doctor_result_has_trouble(const struct arguments *args, const struct doctor_result *result)
{
    int p101_expression_result_22;
    int p101_expression_result_23;
    p101_expression_result_23 = 0;
    if(!args->skip_source_contracts)
    {
        int  p101_expression_result_24;
        bool p101_call_result_25;

        p101_call_result_25 = p101_doctor_status_is_acceptable(result->wrapper_status);
        if(!p101_call_result_25)
        {
            p101_expression_result_24 = 1;
        }
        else
        {
            bool p101_call_result_26;

            p101_call_result_26 = p101_doctor_status_is_acceptable(result->error_contract_status);
            if(!p101_call_result_26)
            {
                p101_expression_result_24 = 1;
            }
            else
            {
                p101_expression_result_24 = 0;
            }
        }
        if(p101_expression_result_24)
        {
            p101_expression_result_23 = 1;
        }
    }
    if(p101_expression_result_23)
    {
        p101_expression_result_22 = 1;
    }
    else
    {
        bool p101_call_result_27;

        p101_call_result_27 = p101_doctor_status_is_acceptable(result->module_status);
        if(!p101_call_result_27)
        {
            p101_expression_result_22 = 1;
        }
        else
        {
            p101_expression_result_22 = 0;
        }
    }
    return p101_expression_result_22 != 0;
}

static size_t doctor_result_completed_checks(const struct arguments *args, const struct doctor_result *result)
{
    bool   p101_call_result_1;
    size_t completed;

    completed          = 0U;
    p101_call_result_1 = p101_doctor_status_is_acceptable(result->module_status);
    if(p101_call_result_1)
    {
        completed++;
    }
    if(!args->skip_source_contracts)
    {
        bool p101_call_result_2;
        bool p101_call_result_3;

        p101_call_result_2 = p101_doctor_status_is_acceptable(result->wrapper_status);
        if(p101_call_result_2)
        {
            completed++;
        }
        p101_call_result_3 = p101_doctor_status_is_acceptable(result->error_contract_status);
        if(p101_call_result_3)
        {
            completed++;
        }
    }
    return completed;
}

static void write_artifact_fingerprint(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, const char *path)
{
    int                                p101_call_result_4;
    int                                p101_call_result_5;
    struct p101_tool_event_fingerprint fingerprint;

    p101_call_result_4 = p101_access(env, P101_ERROR_OPTIONAL, path, F_OK);
    if(p101_call_result_4 != 0)    // P101_ERROR_OPTIONAL rationale: missing artifacts are explicit receipt evidence.
    {
        p101_fprintf(env, err, stream, "artifact.%s=missing\tpath=%s\n", label, path);
        goto done;
    }
    p101_call_result_5 = p101_tool_event_fingerprint_file(err, path, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint);
    if(p101_call_result_5 == 0)
    {
        p101_fprintf(env, err, stream, "artifact.%s=present\tpath=%s\tbytes=%zu\trecords=%zu\tfnv1a64=%016" PRIx64 "\tfinal_newline=%d\n", label, path, fingerprint.bytes, fingerprint.records, fingerprint.fnv1a64, fingerprint.final_newline);
    }

done:
    return;
}

static void write_grade_line(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    const char *p101_call_result_7;
    const char *p101_call_result_8;
    p101_call_result_7 = p101_doctor_status_grade(status);
    p101_call_result_8 = p101_doctor_status_word(status);
    p101_fprintf(env, err, stream, "- %s: `%s` (%s)\n", label, p101_call_result_7, p101_call_result_8);
}

static void write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    int p101_call_result_9;
    P101_TRACE_SCOPE(env);
    p101_call_result_9 = p101_record_write_json_string(stream, text == NULL ? "" : text);
    if(p101_call_result_9 != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}

void p101_doctor_write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, paths->summary, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "# audit-doctor\n\n", stream);
    p101_fprintf(env, err, stream, "Command: `%s`\n\n", args->command_argv[0]);
    if(args->skip_source_contracts)
    {
        p101_fputs(env, err, "Source contracts: `skipped`\n\n", stream);
    }
    else
    {
        p101_fputs(env, err, "Source paths:", stream);

        for(int i = 0; i < args->source_count; i++)
        {
            p101_fprintf(env, err, stream, " `%s`", args->source_paths[i]);
        }

        p101_fputs(env, err, "\n\n", stream);
    }
    p101_fputs(env, err, "## Quick grade\n\n", stream);
    if(!args->skip_source_contracts)
    {
        write_grade_line(env, err, stream, "Wrapper usage", result->wrapper_status);
        write_grade_line(env, err, stream, "Error contracts", result->error_contract_status);
    }
    write_grade_line(env, err, stream, "Module shape", result->module_status);
    p101_fputs(env, err, "\n", stream);

    p101_fputs(env, err, "## Results\n\n", stream);
    p101_fputs(env, err, "| Step | Status |\n", stream);
    p101_fputs(env, err, "| --- | --- |\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_doctor_print_status_markdown(env, err, stream, "audit-wrappers", result->wrapper_status);
        p101_doctor_print_status_markdown(env, err, stream, "audit-errors", result->error_contract_status);
    }
    p101_doctor_print_status_markdown(env, err, stream, "audit-modules", result->module_status);

    p101_fputs(env, err, "\n## Artifacts\n\n", stream);
    p101_fputs(env, err, "- Command: [command.txt](./command.txt)\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_fputs(env, err, "- Wrapper audit stdout: [wrapper-audit.stdout.txt](./wrapper-audit.stdout.txt)\n", stream);
        p101_fputs(env, err, "- Wrapper audit stderr: [wrapper-audit.stderr.txt](./wrapper-audit.stderr.txt)\n", stream);
        p101_fputs(env, err, "- Shared source facts: [source-facts.tsv](./source-facts.tsv)\n", stream);
        p101_fputs(env, err, "- Admitted source inputs: [source-inputs.json](./source-inputs.json)\n", stream);
        p101_fputs(env, err, "- Error contract stdout: [error-contract.stdout.txt](./error-contract.stdout.txt)\n", stream);
        p101_fputs(env, err, "- Error contract stderr: [error-contract.stderr.txt](./error-contract.stderr.txt)\n", stream);
    }
    p101_fputs(env, err, "- Module map report: [module-map.md](./module-map.md)\n", stream);
    p101_fputs(env, err, "- Module map JSON findings: [module-map.json](./module-map.json)\n", stream);
    p101_fputs(env, err, "- Module map stdout: [module-map.stdout.txt](./module-map.stdout.txt)\n", stream);
    p101_fputs(env, err, "- Module map stderr: [module-map.stderr.txt](./module-map.stderr.txt)\n", stream);
    p101_fputs(env, err, "- Machine-readable doctor index: [doctor.json](./doctor.json)\n", stream);
    p101_fputs(env, err, "- Bounded artifact fingerprints: [receipt.txt](./receipt.txt)\n", stream);
    p101_fputs(env, err, "- Verifiable run receipt: [tool-receipt.json](./tool-receipt.json)\n", stream);
    p101_fputs(env, err, "- Run manifest: [manifest.txt](./manifest.txt)\n", stream);

    p101_fputs(env, err, "\n## How to read this\n\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_fputs(env, err, "`audit-wrappers` is the static boundary story: it reports calls that bypass available p101 wrappers.\n\n", stream);
        p101_fputs(env, err, "`audit-errors` is the static error-handling story: it reports p101 calls used before a visible env/error contract.\n\n", stream);
    }
    p101_fputs(env, err, "`audit-modules` is the static design story: it reports module shape, public API surface, include relationships, and likely split/static-scope opportunities.\n\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_doctor_write_evidence_receipt_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, paths->receipt, "w");
    if(stream == NULL)
    {
        goto done;
    }
    /*
     * Each artifact line is a tab-delimited sequence of key=value groups so a
     * reader can split the fields unambiguously even when a caller-supplied
     * output path contains spaces.
     */
    p101_fputs(env, err, "schema=audit-doctor-evidence-receipt-v1\n", stream);
    write_artifact_fingerprint(env, err, stream, "command", paths->command);
    write_artifact_fingerprint(env, err, stream, "manifest", paths->manifest);
    if(!args->skip_source_contracts)
    {
        write_artifact_fingerprint(env, err, stream, "wrapper-stdout", paths->wrapper_stdout);
        write_artifact_fingerprint(env, err, stream, "wrapper-stderr", paths->wrapper_stderr);
        write_artifact_fingerprint(env, err, stream, "source-facts", paths->facts);
        write_artifact_fingerprint(env, err, stream, "source-inputs", paths->input_manifest);
        write_artifact_fingerprint(env, err, stream, "error-contract-stdout", paths->error_contract_stdout);
        write_artifact_fingerprint(env, err, stream, "error-contract-stderr", paths->error_contract_stderr);
    }
    write_artifact_fingerprint(env, err, stream, "module-map-stdout", paths->module_stdout);
    write_artifact_fingerprint(env, err, stream, "module-map-stderr", paths->module_stderr);
    write_artifact_fingerprint(env, err, stream, "module-map-report", paths->module_report);
    write_artifact_fingerprint(env, err, stream, "module-map-json", paths->module_json);
    write_artifact_fingerprint(env, err, stream, "summary", paths->summary);
    write_artifact_fingerprint(env, err, stream, "doctor-json", paths->json);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_doctor_write_receipt_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
{
    bool                               p101_call_result_15;
    int                                p101_call_result_10;
    size_t                             p101_call_result_11;
    bool                               p101_call_result_12;
    int                                p101_call_result_13;
    struct p101_tool_event_fingerprint fingerprint;
    struct p101_tool_run_receipt       receipt;
    FILE                              *stream;

    P101_TRACE_SCOPE(env);
    stream              = NULL;
    p101_call_result_10 = p101_tool_event_fingerprint_file(err, paths->receipt, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint);
    if(p101_call_result_10 != 0)
    {
        goto done;
    }
    p101_call_result_11 = doctor_result_completed_checks(args, result);
    receipt             = (struct p101_tool_run_receipt){
        .tool_name        = "audit-doctor",
        .tool_version     = "1.0.0",
        .input_schema     = "audit-doctor-evidence-receipt-v1",
        .input_identity   = paths->receipt,
        .policy_schema    = "audit-doctor-policy-v1",
        .policy_identity  = "full-source-contracts",
        .run_identity     = paths->dir,
        .failed_stage     = "",
        .first_diagnostic = "",
        .checks_attempted = 3U,
        .checks_completed = p101_call_result_11,
        .does_not_prove   = "runtime correctness, complete wrapper instrumentation, third-party code, or unsupported compiler behavior",
    };
    if(args->skip_source_contracts)
    {
        receipt.policy_identity  = "module-map-only";
        receipt.checks_attempted = 1U;
    }
    p101_call_result_12 = doctor_result_has_trouble(args, result);
    if(p101_call_result_12)
    {
        receipt.outcome          = P101_TOOL_OUTCOME_TOOL_ERROR;
        receipt.failure_reason   = P101_TOOL_FAILURE_TOOL_ERROR;
        receipt.failed_stage     = "subtool";
        receipt.first_diagnostic = "at least one doctor subtool did not produce an acceptable result";
    }
    else
    {
        p101_call_result_15 = doctor_result_has_findings(args, result);
        if(p101_call_result_15)
        {
            receipt.outcome          = P101_TOOL_OUTCOME_FINDINGS;
            receipt.failure_reason   = P101_TOOL_FAILURE_FINDINGS_PRESENT;
            receipt.failed_stage     = "source-contract";
            receipt.first_diagnostic = "at least one doctor subtool reported findings";
        }
        else
        {
            receipt.outcome        = P101_TOOL_OUTCOME_CLEAN;
            receipt.failure_reason = P101_TOOL_FAILURE_NONE;
        }
    }
    stream = p101_fopen(env, err, paths->tool_receipt, "w");
    if(stream != NULL)
    {
        p101_call_result_13 = p101_tool_run_receipt_write_json(err, stream, &receipt, &fingerprint);
        (void)p101_call_result_13;
    }

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_doctor_write_json_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, paths->json, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "{\n  \"schema\": \"audit-doctor-v3\",\n", stream);
    p101_fputs(env, err, "  \"command\": ", stream);
    write_json_string(env, err, stream, args->command_argv[0]);
    p101_fputs(env, err, ",\n", stream);
    if(args->skip_source_contracts)
    {
        p101_fputs(env, err, "  \"source_contracts\": false,\n", stream);
    }
    else
    {
        p101_fputs(env, err, "  \"source_contracts\": true,\n", stream);
    }
    p101_fputs(env, err, "  \"source_paths\": [", stream);
    for(int i = 0; i < args->source_count; i++)
    {
        if(i > 0)
        {
            p101_fputs(env, err, ", ", stream);
        }
        write_json_string(env, err, stream, args->source_paths[i]);
    }
    p101_fputs(env, err, "],\n", stream);
    p101_fputs(env, err, "  \"doctor_dir\": ", stream);
    write_json_string(env, err, stream, paths->dir);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"manifest\": ", stream);
    write_json_string(env, err, stream, paths->manifest);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"source_facts\": ", stream);
    write_json_string(env, err, stream, paths->facts);
    p101_fputs(env, err, ",\n  \"source_inputs\": ", stream);
    write_json_string(env, err, stream, paths->input_manifest);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"module_report\": ", stream);
    write_json_string(env, err, stream, paths->module_report);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"module_findings\": ", stream);
    write_json_string(env, err, stream, paths->module_json);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"statuses\": {\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_doctor_print_status_json(env, err, stream, "audit_wrappers", result->wrapper_status);
        p101_fputs(env, err, ",\n", stream);
        p101_doctor_print_status_json(env, err, stream, "audit_errors", result->error_contract_status);
        p101_fputs(env, err, ",\n", stream);
    }
    p101_doctor_print_status_json(env, err, stream, "audit_modules", result->module_status);
    p101_fputs(env, err, "\n  }\n", stream);
    p101_fputs(env, err, "}\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}
