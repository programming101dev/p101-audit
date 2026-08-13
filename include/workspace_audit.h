#ifndef P101_AUDIT_WORKSPACE_AUDIT_H
#define P101_AUDIT_WORKSPACE_AUDIT_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

enum
{
    P101_WORKSPACE_AUDIT_PATH_SIZE    = 4096,
    P101_WORKSPACE_AUDIT_MESSAGE_SIZE = 2048
};

struct p101_workspace_audit_options
{
    const char  *workspace;
    const char  *scripts_root;
    const char  *policy;
    const char  *facts_path;
    const char  *receipt_path;
    const char  *execution_receipt_path;
    unsigned int outputs;
};

struct p101_workspace_audit_finding
{
    char path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char message[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
};

struct p101_workspace_audit_result
{
    struct p101_workspace_audit_finding *findings;
    size_t                               finding_count;
    size_t                               finding_capacity;
    size_t                               checks;
};

void p101_workspace_audit_result_init(struct p101_workspace_audit_result *result);
void p101_workspace_audit_result_destroy(const struct p101_env *env, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_add(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *message);
bool p101_workspace_audit_join(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *left, const char *right);
bool p101_workspace_audit_read_file(const struct p101_env *env, struct p101_error *err, const char *path, char **text, size_t *length);
bool p101_workspace_audit_file_exists(const struct p101_env *env, struct p101_error *err, const char *path);
bool p101_workspace_audit_run_functional_layout(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_run_native_parity(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_run_fault_semantics(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_run_test_inventory(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_run_source_responsibilities(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_run_boundaries(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_run_wrapper_unit_tests(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_run_instrumentation(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
bool p101_workspace_audit_run_quality_contract(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result);
void p101_workspace_audit_write(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_workspace_audit_result *result);

#endif
