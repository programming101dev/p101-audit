#include "model.h"
#include "workspace_audit.h"
#include "workspace_fact_bundle.h"
#include "workspace_json.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_stdlib.h>

enum
{
    BOUNDARY_TEXT_SIZE  = 1024,
    BOUNDARY_TEST_COUNT = 6
};

static const char *const BOUNDARY_TESTS[BOUNDARY_TEST_COUNT] = {"clean", "typed_refusal", "binding_swap", "identity_mismatch", "resource_limit", "stale_version"};

static bool boundary_text(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, char *output, size_t output_size);
static bool boundary_path(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *relative, char *absolute, size_t absolute_size);
static bool boundary_function_exists(const struct p101_env *env, const struct p101_wrapper_model *model, const char *path, const char *usr);
static bool boundary_test_wired(const struct p101_env *env, const struct p101_wrapper_model *model, const char *path, const char *role);
static bool boundary_shell_wired(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *path, const char *marker);
static bool boundary_execution_evidence(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_workspace_json *register_document, size_t boundaries, struct p101_workspace_audit_result *result,
                                        const char *contract_path);
static void boundary_finding(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *identifier, const char *message);

bool p101_workspace_audit_run_boundaries(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    static const char *const   REQUIRED_TEXT[] = {"owner_repo", "input", "output", "refusal", "evidence", "authority_owner", "mechanism_owner", "effects", "resource_budget"};
    struct p101_workspace_json document;
    struct p101_wrapper_model  model;
    char                       contract_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       identifier[BOUNDARY_TEXT_SIZE];
    char                       owner_source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       owner_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       owner_usr[BOUNDARY_TEXT_SIZE];
    char                       evidence_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       evidence_value[BOUNDARY_TEXT_SIZE];
    char                       text[BOUNDARY_TEXT_SIZE];
    size_t                     boundaries;
    size_t                     boundary;
    size_t                     tests;
    size_t                     evidence;
    size_t                     index;
    size_t                     test_index;
    bool                       joined;
    bool                       loaded;
    bool                       found;
    bool                       valid;
    bool                       path_ok;
    bool                       semantic;
    bool                       success;
    int                        comparison;

    p101_workspace_json_init(&document);
    p101_wrapper_model_init(&model);
    success = false;
    joined  = p101_workspace_audit_join(env, err, contract_path, sizeof(contract_path), options->scripts_root, "contracts/p101-boundaries.json");
    if(!joined)
    {
        goto done;
    }
    loaded = p101_workspace_json_load(env, err, contract_path, &document);
    if(!loaded)
    {
        goto done;
    }
    found = p101_workspace_json_object_get(env, &document, 0U, "schema", &index);
    valid = found && p101_workspace_json_token_equals(env, &document, index, "p101-boundary-register-v3");
    found = p101_workspace_json_object_get(env, &document, 0U, "does_not_prove", &index);
    valid = found && document.tokens[index].kind == P101_WORKSPACE_JSON_STRING && document.tokens[index].end > document.tokens[index].start && valid;
    found = p101_workspace_json_object_get(env, &document, 0U, "boundaries", &boundaries);
    valid = found && document.tokens[boundaries].kind == P101_WORKSPACE_JSON_ARRAY && document.tokens[boundaries].child_count > 0U && valid;
    if(!valid)
    {
        boundary_finding(env, err, result, contract_path, "boundary register", "has an invalid schema or no declared boundaries");
        success = p101_error_has_no_error(err);
        goto done;
    }
    if(options->facts_path == NULL)
    {
        P101_ERROR_RAISE_USER(err, "boundary policy requires --facts", EINVAL);
        goto done;
    }
    loaded = p101_workspace_fact_bundle_load(env, err, options->facts_path, &model);
    if(!loaded)
    {
        goto done;
    }
    for(index = 0U; index < document.tokens[boundaries].child_count; index++)
    {
        found = p101_workspace_json_array_get(&document, boundaries, index, &boundary);
        if(!found || document.tokens[boundary].kind != P101_WORKSPACE_JSON_OBJECT)
        {
            boundary_finding(env, err, result, contract_path, "boundary", "row is not an object");
            continue;
        }
        valid      = boundary_text(env, err, &document, boundary, "id", identifier, sizeof(identifier));
        comparison = valid ? p101_strncmp(env, identifier, "boundary:", sizeof("boundary:") - 1U) : -1;
        if(!valid || comparison != 0)
        {
            boundary_finding(env, err, result, contract_path, "boundary", "has no canonical identity");
            continue;
        }
        for(size_t field = 0U; field < sizeof(REQUIRED_TEXT) / sizeof(REQUIRED_TEXT[0]); field++)
        {
            valid = boundary_text(env, err, &document, boundary, REQUIRED_TEXT[field], text, sizeof(text));
            if(!valid)
            {
                boundary_finding(env, err, result, contract_path, identifier, "has a missing contract field");
            }
            result->checks++;
        }
        valid   = boundary_text(env, err, &document, boundary, "owner_source", owner_source, sizeof(owner_source));
        valid   = boundary_text(env, err, &document, boundary, "owner_usr", owner_usr, sizeof(owner_usr)) && valid;
        path_ok = valid && boundary_path(env, err, options, owner_source, owner_path, sizeof(owner_path));
        if(!path_ok || !boundary_function_exists(env, &model, owner_path, owner_usr))
        {
            boundary_finding(env, err, result, contract_path, identifier, "owner declaration identity is absent from its source");
        }
        found = p101_workspace_json_object_get(env, &document, boundary, "composition", &evidence);
        valid = found && document.tokens[evidence].kind == P101_WORKSPACE_JSON_ARRAY && document.tokens[evidence].child_count > 0U;
        found = p101_workspace_json_object_get(env, &document, boundary, "collaborators", &test_index);
        valid = found && document.tokens[test_index].kind == P101_WORKSPACE_JSON_ARRAY && document.tokens[test_index].child_count > 0U && valid;
        if(!valid)
        {
            boundary_finding(env, err, result, contract_path, identifier, "has no composition or collaborator contract");
        }
        found = p101_workspace_json_object_get(env, &document, boundary, "tests", &tests);
        if(!found || document.tokens[tests].kind != P101_WORKSPACE_JSON_OBJECT || document.tokens[tests].child_count != BOUNDARY_TEST_COUNT * 2U)
        {
            boundary_finding(env, err, result, contract_path, identifier, "does not declare the complete boundary test matrix");
            continue;
        }
        for(test_index = 0U; test_index < BOUNDARY_TEST_COUNT; test_index++)
        {
            found = p101_workspace_json_object_get(env, &document, tests, BOUNDARY_TESTS[test_index], &evidence);
            if(!found || document.tokens[evidence].kind != P101_WORKSPACE_JSON_OBJECT)
            {
                boundary_finding(env, err, result, contract_path, identifier, "has a missing boundary test case");
                continue;
            }
            found = p101_workspace_json_object_get(env, &document, evidence, "not_applicable", &boundary);
            if(found)
            {
                valid = test_index == BOUNDARY_TEST_COUNT - 1U && p101_workspace_json_token_equals(env, &document, boundary, "true");
                valid = boundary_text(env, err, &document, evidence, "reason", text, sizeof(text)) && valid;
                if(!valid)
                {
                    boundary_finding(env, err, result, contract_path, identifier, "has an invalid non-applicable test case");
                }
                continue;
            }
            valid    = boundary_text(env, err, &document, evidence, "path", evidence_path, sizeof(evidence_path));
            semantic = false;
            if(valid)
            {
                found    = p101_workspace_json_object_get(env, &document, evidence, "semantic_role", &boundary);
                semantic = found;
                if(found)
                {
                    valid = p101_workspace_json_token_copy(env, err, &document, boundary, evidence_value, sizeof(evidence_value));
                }
                else
                {
                    valid = boundary_text(env, err, &document, evidence, "marker", evidence_value, sizeof(evidence_value));
                }
            }
            path_ok = valid && boundary_path(env, err, options, evidence_path, owner_path, sizeof(owner_path));
            if(path_ok && semantic)
            {
                valid = boundary_test_wired(env, &model, owner_path, evidence_value);
            }
            else if(path_ok)
            {
                valid = boundary_shell_wired(env, err, options, evidence_path, evidence_value);
            }
            else
            {
                valid = false;
            }
            if(!valid)
            {
                boundary_finding(env, err, result, contract_path, identifier, "has missing or unwired executable boundary evidence");
            }
            result->checks++;
        }
    }
    if(options->execution_receipt_path != NULL)
    {
        loaded = boundary_execution_evidence(env, err, options, &document, boundaries, result, contract_path);
        if(!loaded)
        {
            goto done;
        }
    }
    success = p101_error_has_no_error(err);

done:
    p101_wrapper_model_destroy(env, &model);
    p101_workspace_json_destroy(env, &document);
    return success;
}

static bool boundary_execution_evidence(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_workspace_json *register_document, size_t boundaries, struct p101_workspace_audit_result *result,
                                        const char *contract_path)
{
    struct p101_workspace_json receipt;
    char                       owner[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       owner_name[P101_WRAPPER_NAME_SIZE];
    char                       repository[P101_WRAPPER_NAME_SIZE];
    char                       unit[P101_WRAPPER_NAME_SIZE];
    size_t                     schema;
    size_t                     passed;
    size_t                     repositories;
    size_t                     boundary;
    size_t                     record;
    const char                *base;
    const char                *slash;
    bool                       loaded;
    bool                       valid;
    bool                       executed;
    bool                       success;
    int                        comparison;

    p101_workspace_json_init(&receipt);
    success = false;
    loaded  = p101_workspace_json_load(env, err, options->execution_receipt_path, &receipt);
    if(!loaded)
    {
        goto done;
    }
    valid = p101_workspace_json_object_get(env, &receipt, 0U, "schema", &schema);
    valid = valid && p101_workspace_json_token_equals(env, &receipt, schema, "p101-repository-test-receipt-v1");
    valid = p101_workspace_json_object_get(env, &receipt, 0U, "passed", &passed) && valid;
    valid = valid && p101_workspace_json_token_equals(env, &receipt, passed, "true");
    valid = p101_workspace_json_object_get(env, &receipt, 0U, "repositories", &repositories) && valid;
    valid = valid && receipt.tokens[repositories].kind == P101_WORKSPACE_JSON_ARRAY;
    if(!valid)
    {
        boundary_finding(env, err, result, options->execution_receipt_path, "execution receipt", "is not a clean repository-test receipt");
        success = p101_error_has_no_error(err);
        goto done;
    }
    for(size_t index = 0U; index < register_document->tokens[boundaries].child_count; index++)
    {
        p101_workspace_json_array_get(register_document, boundaries, index, &boundary);
        valid = boundary_text(env, err, register_document, boundary, "owner_repo", owner, sizeof(owner));
        base  = owner;
        slash = p101_strrchr(env, owner, '/');
        if(slash != NULL)
        {
            base = slash + 1;
        }
        p101_snprintf(env, err, owner_name, sizeof(owner_name), "%s", base);
        executed = false;
        for(size_t receipt_index = 0U; valid && receipt_index < receipt.tokens[repositories].child_count && !executed; receipt_index++)
        {
            p101_workspace_json_array_get(&receipt, repositories, receipt_index, &record);
            valid      = boundary_text(env, err, &receipt, record, "repository", repository, sizeof(repository));
            valid      = boundary_text(env, err, &receipt, record, "unit", unit, sizeof(unit)) && valid;
            comparison = p101_strcmp(env, repository, owner_name);
            if(comparison == 0)
            {
                comparison = p101_strcmp(env, unit, "PASS");
                executed   = comparison == 0;
                comparison = p101_strcmp(env, unit, "REUSED");
                executed   = executed || comparison == 0;
            }
        }
        if(!valid || !executed)
        {
            boundary_finding(env, err, result, contract_path, owner_name, "owner test suite did not pass in this campaign");
        }
        result->checks++;
    }
    success = p101_error_has_no_error(err);

done:
    p101_workspace_json_destroy(env, &receipt);
    return success;
}

static bool boundary_text(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, char *output, size_t output_size)
{
    size_t value;
    bool   found;
    bool   copied;

    found  = p101_workspace_json_object_get(env, document, object, key, &value);
    copied = found && p101_workspace_json_token_copy(env, err, document, value, output, output_size);
    return copied && output[0] != '\0';
}

static bool boundary_path(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *relative, char *absolute, size_t absolute_size)
{
    char  joined[P101_WORKSPACE_AUDIT_PATH_SIZE];
    bool  joined_ok;
    char *resolved;

    joined_ok = relative[0] != '/' && p101_workspace_audit_join(env, err, joined, sizeof(joined), options->workspace, relative);
    if(!joined_ok)
    {
        return false;
    }
    resolved = p101_realpath(env, P101_ERROR_OPTIONAL, joined, absolute);
    return resolved != NULL && p101_strlen(env, absolute) < absolute_size;
}

static bool boundary_function_exists(const struct p101_env *env, const struct p101_wrapper_model *model, const char *path, const char *usr)
{
    bool found;

    found = false;
    for(size_t index = 0U; index < model->fact_count && !found; index++)
    {
        const struct p101_wrapper_fact *fact;
        int                             path_comparison;
        int                             usr_comparison;

        fact            = &model->facts[index];
        path_comparison = p101_strcmp(env, fact->path, path);
        usr_comparison  = p101_strcmp(env, fact->usr, usr);
        found           = fact->kind == P101_C_ANALYSIS_FUNCTION && path_comparison == 0 && usr_comparison == 0;
    }
    return found;
}

static bool boundary_test_wired(const struct p101_env *env, const struct p101_wrapper_model *model, const char *path, const char *role)
{
    char        expected[P101_WRAPPER_NAME_SIZE];
    const char *caller_usr;
    bool        found_role;
    bool        found_call;
    int         written;

    written = p101_snprintf(env, P101_ERROR_OPTIONAL, expected, sizeof(expected), "SEMANTIC_ROLE:%s", role);
    if(written < 0 || (size_t)written >= sizeof(expected))
    {
        return false;
    }
    caller_usr = NULL;
    found_role = false;
    for(size_t index = 0U; index < model->fact_count && !found_role; index++)
    {
        const struct p101_wrapper_fact *fact;
        int                             path_comparison;
        int                             role_comparison;

        fact            = &model->facts[index];
        path_comparison = p101_strcmp(env, fact->path, path);
        role_comparison = p101_strcmp(env, fact->name, expected);
        if(fact->kind == P101_C_ANALYSIS_NOTE && path_comparison == 0 && role_comparison == 0 && fact->caller_usr[0] != '\0')
        {
            caller_usr = fact->caller_usr;
            found_role = true;
        }
    }
    found_call = false;
    for(size_t index = 0U; index < model->fact_count && found_role && !found_call; index++)
    {
        const struct p101_wrapper_fact *fact;
        int                             path_comparison;
        int                             caller_comparison;

        fact              = &model->facts[index];
        path_comparison   = p101_strcmp(env, fact->path, path);
        caller_comparison = p101_strcmp(env, fact->caller_usr, caller_usr);
        found_call        = fact->kind == P101_C_ANALYSIS_CALL && path_comparison == 0 && caller_comparison == 0;
    }
    return found_role && found_call;
}

static bool boundary_shell_wired(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *path, const char *marker)
{
    char        absolute[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char        cmake_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char       *source;
    char       *cmake;
    size_t      source_length;
    size_t      cmake_length;
    const char *slash;
    bool        path_ok;
    bool        loaded;
    bool        wired;
    int         written;

    source  = NULL;
    cmake   = NULL;
    path_ok = boundary_path(env, err, options, path, absolute, sizeof(absolute));
    if(!path_ok)
    {
        return false;
    }
    loaded = p101_workspace_audit_read_file(env, err, absolute, &source, &source_length);
    if(!loaded || p101_strstr(env, source, marker) == NULL)
    {
        p101_free(env, source);
        return false;
    }
    slash = p101_strrchr(env, absolute, '/');
    if(slash == NULL)
    {
        p101_free(env, source);
        return false;
    }
    written = p101_snprintf(env, err, cmake_path, sizeof(cmake_path), "%.*s/CMakeLists.txt", (int)(slash - absolute), absolute);
    loaded  = written >= 0 && (size_t)written < sizeof(cmake_path) && p101_workspace_audit_read_file(env, err, cmake_path, &cmake, &cmake_length);
    wired   = loaded && p101_strstr(env, cmake, slash + 1) != NULL;
    p101_free(env, cmake);
    p101_free(env, source);
    return wired;
}

static void boundary_finding(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *identifier, const char *message)
{
    char text[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    int  written;

    written = p101_snprintf(env, err, text, sizeof(text), "%s %s", identifier, message);
    if(written >= 0 && (size_t)written < sizeof(text))
    {
        p101_workspace_audit_add(env, err, result, path, text);
    }
}
