#include "../include/paths.h"
#include "../include/constants.h"
#include "../include/errors.h"
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
#include <stdio.h>

static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);

void p101_doctor_make_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct doctor_paths *paths)
{
    P101_TRACE_SCOPE(env);

    if(args->doctor_dir == NULL)
    {
        pid_t process_id;

        process_id = p101_getpid(env);
        p101_snprintf(env, err, paths->dir, sizeof(paths->dir), "%s-%ld", DEFAULT_DOCTOR_PREFIX, (long)process_id);
    }
    else
    {
        p101_strncpy(env, paths->dir, args->doctor_dir, sizeof(paths->dir) - 1U);
        paths->dir[sizeof(paths->dir) - 1U] = '\0';
    }

    join_path(env, err, paths->command, paths->dir, "command.txt");
    join_path(env, err, paths->wrapper_stdout, paths->dir, "audit-wrappers.stdout.txt");
    join_path(env, err, paths->wrapper_stderr, paths->dir, "audit-wrappers.stderr.txt");
    join_path(env, err, paths->facts, paths->dir, "source-facts.tsv");
    join_path(env, err, paths->input_manifest, paths->dir, "source-inputs.json");
    join_path(env, err, paths->error_contract_stdout, paths->dir, "audit-errors.stdout.txt");
    join_path(env, err, paths->error_contract_stderr, paths->dir, "audit-errors.stderr.txt");
    join_path(env, err, paths->module_stdout, paths->dir, "audit-modules.stdout.txt");
    join_path(env, err, paths->module_stderr, paths->dir, "audit-modules.stderr.txt");
    join_path(env, err, paths->module_report, paths->dir, "audit-modules.md");
    join_path(env, err, paths->module_json, paths->dir, "audit-modules.json");
    join_path(env, err, paths->summary, paths->dir, "summary.md");
    join_path(env, err, paths->json, paths->dir, "audit-doctor.json");
    join_path(env, err, paths->receipt, paths->dir, "receipt.txt");
    join_path(env, err, paths->tool_receipt, paths->dir, "tool-receipt.json");
    join_path(env, err, paths->manifest, paths->dir, "manifest.txt");
}

void p101_doctor_create_dir(const struct p101_env *env, struct p101_error *err, const char *path)
{
    P101_TRACE_SCOPE(env);
    p101_mkdir(env, err, path, DEFAULT_DIR_MODE);
}

void p101_doctor_write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *const command_argv)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream == NULL)
    {
        goto done;
    }

    for(size_t i = 0; command_argv[i] != NULL; i++)
    {
        p101_fprintf(env, err, stream, "%s%s", (i == 0U) ? "" : " ", command_argv[i]);
    }

    p101_fputc(env, err, '\n', stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_doctor_write_manifest_file(const struct p101_env *env, struct p101_error *err, const char *path, const struct arguments *args, const struct doctor_paths *paths)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "audit-doctor manifest\n", stream);
    p101_fprintf(env, err, stream, "doctor_dir=%s\n", paths->dir);
    p101_fputs(env, err, "source_paths=", stream);
    for(int i = 0; i < args->source_count; i++)
    {
        p101_fprintf(env, err, stream, "%s%s", (i == 0) ? "" : " ", args->source_paths[i]);
    }
    p101_fputc(env, err, '\n', stream);
    p101_fprintf(env, err, stream, "compile_database=%s\n", args->compile_db_path == NULL ? "auto" : args->compile_db_path);
    p101_fprintf(env, err, stream, "source_facts=%s\n", paths->facts);
    p101_fprintf(env, err, stream, "source_inputs=%s\n", paths->input_manifest);
    p101_fprintf(env, err, stream, "evidence_receipt=%s\n", paths->receipt);
    p101_fprintf(env, err, stream, "tool_receipt=%s\n", paths->tool_receipt);
    p101_fprintf(env, err, stream, "audit_wrappers=%s\n", args->p101_wrapper_audit);
    p101_fprintf(env, err, stream, "audit_errors=%s\n", args->p101_error_contract);
    p101_fprintf(env, err, stream, "audit_modules=%s\n", args->p101_module_map);
    p101_fputs(env, err, "command=", stream);

    for(size_t i = 0; args->command_argv[i] != NULL; i++)
    {
        p101_fprintf(env, err, stream, "%s%s", (i == 0U) ? "" : " ", args->command_argv[i]);
    }

    p101_fputc(env, err, '\n', stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name)
{
    int written;

    P101_TRACE_SCOPE(env);
    written = p101_snprintf(env, err, destination, PATH_LEN, "%s/%s", dir, name);

    if(written >= PATH_LEN)
    {
        P101_ERROR_RAISE_USER(err, "A doctor path is too long.", ERR_USAGE);
    }
}
