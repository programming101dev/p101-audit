#ifndef P101_DOCTOR_PATHS_H
#define P101_DOCTOR_PATHS_H

#include "arguments.h"
#include <p101_env/env.h>
#include <p101_error/error.h>

struct doctor_paths
{
    char dir[PATH_LEN];
    char command[PATH_LEN];
    char wrapper_stdout[PATH_LEN];
    char wrapper_stderr[PATH_LEN];
    char facts[PATH_LEN];
    char input_manifest[PATH_LEN];
    char error_contract_stdout[PATH_LEN];
    char error_contract_stderr[PATH_LEN];
    char module_stdout[PATH_LEN];
    char module_stderr[PATH_LEN];
    char module_report[PATH_LEN];
    char module_json[PATH_LEN];
    char summary[PATH_LEN];
    char json[PATH_LEN];
    char receipt[PATH_LEN];
    char tool_receipt[PATH_LEN];
    char manifest[PATH_LEN];
};

void p101_doctor_make_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct doctor_paths *paths);
void p101_doctor_create_dir(const struct p101_env *env, struct p101_error *err, const char *path);
void p101_doctor_write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *command_argv);
void p101_doctor_write_manifest_file(const struct p101_env *env, struct p101_error *err, const char *path, const struct arguments *args, const struct doctor_paths *paths);

#endif    // P101_DOCTOR_PATHS_H
