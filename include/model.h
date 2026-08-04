#ifndef P101_WRAPPER_AUDIT_MODEL_H
#define P101_WRAPPER_AUDIT_MODEL_H

#include <p101_c_facts/analysis.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

enum
{
    P101_WRAPPER_PATH_SIZE = 4096,
    P101_WRAPPER_NAME_SIZE = 256,
    P101_WRAPPER_MAX_PATHS = 128,
    P101_WRAPPER_MAX_NAMES = 256
};

struct p101_wrapper_arguments
{
    const char *paths[P101_WRAPPER_MAX_PATHS];
    size_t      path_count;
    const char *header_roots[P101_WRAPPER_MAX_PATHS];
    size_t      header_root_count;
    const char *extra_arguments[P101_WRAPPER_MAX_NAMES];
    size_t      extra_argument_count;
    const char *allowed[P101_WRAPPER_MAX_NAMES];
    char        allowed_storage[P101_WRAPPER_MAX_NAMES][P101_WRAPPER_NAME_SIZE];
    size_t      allowed_count;
    const char *allow_files[P101_WRAPPER_MAX_PATHS];
    size_t      allow_file_count;
    struct
    {
        char   path[P101_WRAPPER_PATH_SIZE];
        char   function[P101_WRAPPER_NAME_SIZE];
        char   callee[P101_WRAPPER_NAME_SIZE];
        size_t uses;
    } allow_rules[P101_WRAPPER_MAX_NAMES];
    size_t      allow_rule_count;
    const char *compile_database;
    char        compile_database_storage[P101_WRAPPER_PATH_SIZE];
    const char *facts_output;
    const char *input_manifest;
    const char *instrumentation_output;
    const char *mutation_output;
    bool        json;
    bool        strict_external;
    bool        compile_database_only;
    bool        active_headers_only;
    bool        keep_going;
    bool        show_inventory;
    bool        show_inventory_json;
    bool        emit_facts;
    bool        check_portability;
};

struct p101_wrapper_fact
{
    enum p101_c_analysis_kind kind;
    char                      path[P101_WRAPPER_PATH_SIZE];
    char                      name[P101_WRAPPER_NAME_SIZE];
    char                      type[P101_WRAPPER_NAME_SIZE];
    char                      caller[P101_WRAPPER_NAME_SIZE];
    char                      replacement[P101_WRAPPER_NAME_SIZE];
    size_t                    line;
    size_t                    column;
    size_t                    start;
    size_t                    end;
    enum p101_c_mutation_kind mutation;
    bool                      is_header;
    bool                      is_definition;
    bool                      is_static;
    bool                      is_public;
    bool                      is_local_include;
    bool                      is_indirect;
    bool                      needs_env;
    bool                      needs_error;
};

struct p101_wrapper_inventory
{
    char original[P101_WRAPPER_NAME_SIZE];
    char wrapper[P101_WRAPPER_NAME_SIZE];
};

enum p101_wrapper_finding_kind
{
    P101_WRAPPER_MISSED,
    P101_WRAPPER_EXTERNAL,
    P101_WRAPPER_INDIRECT,
    P101_WRAPPER_PORTABILITY
};

struct p101_wrapper_finding
{
    enum p101_wrapper_finding_kind kind;
    char                           path[P101_WRAPPER_PATH_SIZE];
    char                           name[P101_WRAPPER_NAME_SIZE];
    char                           caller[P101_WRAPPER_NAME_SIZE];
    char                           replacement[P101_WRAPPER_NAME_SIZE];
    size_t                         line;
    size_t                         column;
};

struct p101_wrapper_model
{
    struct p101_wrapper_fact      *facts;
    size_t                         fact_count;
    size_t                         fact_capacity;
    struct p101_wrapper_inventory *inventory;
    size_t                         inventory_count;
    size_t                         inventory_capacity;
    struct p101_wrapper_finding   *findings;
    size_t                         finding_count;
    size_t                         finding_capacity;
    size_t                         parse_failures;
};

void p101_wrapper_model_init(struct p101_wrapper_model *model);
void p101_wrapper_model_destroy(const struct p101_env *env, struct p101_wrapper_model *model);
bool p101_wrapper_analysis_observer(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context);
bool p101_wrapper_model_load_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments, const char *program_path);
bool p101_wrapper_model_scan(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments);
bool p101_wrapper_model_judge(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, struct p101_wrapper_arguments *arguments);

#endif
