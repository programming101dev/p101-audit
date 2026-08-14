#include "model.h"
#include "workspace_analysis.h"
#include "workspace_audit.h"
#include "workspace_json.h"
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>

enum
{
    FAULT_MODE_COUNT             = 5,
    FAULT_PATH_COUNT             = 3,
    FAULT_MODE_MEMBER_COUNT      = 10,
    FAULT_MODE_FIELD_COUNT       = 12,
    FAULT_UNCERTAIN_FIELD_COUNT  = 16,
    FAULT_MECHANISM_MEMBER_COUNT = 8,
    FAULT_SHORT_MODE_INDEX       = 3,
    FAULT_UNCERTAIN_MODE_INDEX   = 4
};

static const char *const FAULT_MODES[FAULT_MODE_COUNT] = {"error", "eintr", "timeout", "short", "uncertain"};

static bool load_contract(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *relative, char *path, size_t path_size, struct p101_workspace_json *document);
static bool add_contract_finding(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *message);
static bool get_required(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, size_t *value, struct p101_workspace_audit_result *result, const char *path);
static bool array_contains(const struct p101_workspace_json *document, size_t array, const char *value);
static bool tokens_equal(const struct p101_env *env, const struct p101_workspace_json *left, size_t left_token, const struct p101_workspace_json *right, size_t right_token);
static bool admitted_contains(const struct p101_workspace_json *document, size_t admitted, const char *identity);
static bool manifest_contains(const struct p101_env *env, struct p101_error *err, const char *path, const struct p101_workspace_json *document, size_t identity);
static bool fact_identity_exists(const struct p101_wrapper_model *model, enum p101_c_analysis_kind kind, const struct p101_workspace_json *document, size_t identity);
static bool validate_modes(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *contract, const char *contract_path, struct p101_workspace_audit_result *result, size_t *short_array, size_t *uncertain_mode);
static bool validate_fact_mechanism(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *contract, const char *contract_path, const struct p101_wrapper_model *model, size_t admitted, size_t uncertain_mode,
                                    struct p101_workspace_audit_result *result);
static bool validate_derived_contracts(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *failure, const struct p101_workspace_json *lifecycle, const char *failure_path, const char *lifecycle_path, size_t admitted,
                                       const struct p101_workspace_json *contract, struct p101_workspace_audit_result *result);

bool p101_workspace_audit_run_fault_semantics(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    struct p101_workspace_json    contract;
    struct p101_workspace_json    failure;
    struct p101_workspace_json    lifecycle;
    char                          paths[FAULT_PATH_COUNT][P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                          source_paths[FAULT_PATH_COUNT][P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                          manifest_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                          analysis_arguments[P101_WORKSPACE_ANALYSIS_ARGUMENT_CAPACITY][P101_WORKSPACE_AUDIT_PATH_SIZE];
    struct p101_wrapper_arguments arguments;
    struct p101_wrapper_model     model;
    size_t                        schema;
    size_t                        short_array;
    size_t                        uncertain_mode;
    size_t                        index;
    size_t                        identity;
    bool                          loaded;
    bool                          found;
    bool                          valid;
    bool                          scanned;
    bool                          present;
    bool                          prepared;
    bool                          success;

    P101_TRACE_SCOPE(env);
    p101_workspace_json_init(&contract);
    p101_workspace_json_init(&failure);
    p101_workspace_json_init(&lifecycle);
    p101_wrapper_model_init(&model);
    p101_memset(env, &arguments, 0, sizeof(arguments));
    success = false;
    loaded  = load_contract(env, err, options, "contracts/wrapper-fault-semantics.json", paths[0], sizeof(paths[0]), &contract);
    if(loaded)
    {
        loaded = load_contract(env, err, options, "contracts/wrapper-failure-contract.json", paths[1], sizeof(paths[1]), &failure);
    }
    if(loaded)
    {
        loaded = load_contract(env, err, options, "contracts/wrapper-lifecycle-contract.json", paths[2], sizeof(paths[2]), &lifecycle);
    }
    if(!loaded)
    {
        goto done;
    }
    found = p101_workspace_json_object_get(env, &contract, 0U, "schema", &schema);
    valid = (found && p101_workspace_json_token_equals(env, &contract, schema, "p101-wrapper-fault-semantics-v3")) != 0;
    if(!valid)
    {
        add_contract_finding(env, err, result, paths[0], "unsupported fault-semantics schema");
        success = p101_error_has_no_error(err);
        goto done;
    }
    valid = validate_modes(env, err, &contract, paths[0], result, &short_array, &uncertain_mode);
    if(!valid)
    {
        goto done;
    }
    p101_workspace_audit_join(env, err, manifest_path, sizeof(manifest_path), options->workspace, "libraries/lib_io/api-manifest.tsv");
    for(index = 0U; index < contract.tokens[short_array].child_count; index++)
    {
        found = p101_workspace_json_array_get(&contract, short_array, index, &identity);
        if(found)
        {
            present = manifest_contains(env, err, manifest_path, &contract, identity);
            if(!present)
            {
                add_contract_finding(env, err, result, paths[0], "partial/uncertain wrapper identity is absent from lib_io manifest");
            }
        }
    }
    p101_workspace_audit_join(env, err, source_paths[0], sizeof(source_paths[0]), options->workspace, "libraries/lib_env/include/p101_env/env.h");
    p101_workspace_audit_join(env, err, source_paths[1], sizeof(source_paths[1]), options->workspace, "libraries/lib_io/src/unistd.c");
    p101_workspace_audit_join(env, err, source_paths[2], sizeof(source_paths[2]), options->workspace, "libraries/lib_io/test/test_behavior.c");
    for(index = 0U; index < FAULT_PATH_COUNT; index++)
    {
        arguments.paths[index] = source_paths[index];
    }
    arguments.path_count = FAULT_PATH_COUNT;
    arguments.keep_going = true;
    prepared             = p101_workspace_audit_prepare_analysis(env, err, options, &arguments, analysis_arguments, P101_WORKSPACE_ANALYSIS_ARGUMENT_CAPACITY);
    if(!prepared)
    {
        goto done;
    }
    scanned = p101_wrapper_model_scan(env, err, &model, &arguments);
    if(!scanned)
    {
        goto done;
    }
    validate_fact_mechanism(env, err, &contract, paths[0], &model, short_array, uncertain_mode, result);
    validate_derived_contracts(env, err, &failure, &lifecycle, paths[1], paths[2], short_array, &contract, result);
    result->checks += FAULT_MODE_COUNT + contract.tokens[short_array].child_count;
    success = p101_error_has_no_error(err);

done:
    p101_wrapper_model_destroy(env, &model);
    p101_workspace_json_destroy(env, &lifecycle);
    p101_workspace_json_destroy(env, &failure);
    p101_workspace_json_destroy(env, &contract);
    return success;
}

static bool load_contract(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *relative, char *path, size_t path_size, struct p101_workspace_json *document)
{
    bool joined;
    bool loaded;

    joined = p101_workspace_audit_join(env, err, path, path_size, options->scripts_root, relative);
    loaded = false;
    if(joined)
    {
        loaded = p101_workspace_json_load(env, err, path, document);
    }
    return loaded;
}

static bool add_contract_finding(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *message)
{
    bool added;

    added = p101_workspace_audit_add(env, err, result, path, message);
    return added;
}

static bool get_required(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, size_t *value, struct p101_workspace_audit_result *result, const char *path)
{
    bool found;

    found = p101_workspace_json_object_get(env, document, object, key, value);
    if(!found)
    {
        add_contract_finding(env, err, result, path, "fault-semantics contract is missing a required field");
    }
    return found;
}

static bool array_contains(const struct p101_workspace_json *document, size_t array, const char *value)
{
    size_t index;
    size_t token;
    bool   found;
    bool   equal;

    found = false;
    for(index = 0U; index < document->tokens[array].child_count && !found; index++)
    {
        p101_workspace_json_array_get(document, array, index, &token);
        equal = p101_workspace_json_token_equals(env, document, token, value);
        if(equal)
        {
            found = true;
        }
    }
    return found;
}

static bool tokens_equal(const struct p101_env *env, const struct p101_workspace_json *left, size_t left_token, const struct p101_workspace_json *right, size_t right_token)
{
    size_t left_size;
    size_t right_size;
    int    comparison;
    bool   equal;

    left_size  = left->tokens[left_token].end - left->tokens[left_token].start;
    right_size = right->tokens[right_token].end - right->tokens[right_token].start;
    equal      = false;
    if(left_size == right_size)
    {
        comparison = p101_strncmp(env, left->text + left->tokens[left_token].start, right->text + right->tokens[right_token].start, left_size);
        equal      = comparison == 0;
    }
    return equal;
}

static bool admitted_contains(const struct p101_workspace_json *document, size_t admitted, const char *identity)
{
    bool contains;

    contains = array_contains(document, admitted, identity);
    return contains;
}

static bool manifest_contains(const struct p101_env *env, struct p101_error *err, const char *path, const struct p101_workspace_json *document, size_t identity)
{
    char  *text;
    size_t text_size;
    size_t identity_size;
    size_t line_start;
    size_t field_start;
    size_t field_end;
    bool   loaded;
    bool   present;

    text      = NULL;
    text_size = 0U;
    present   = false;
    loaded    = p101_workspace_audit_read_file(env, err, path, &text, &text_size);
    if(!loaded)
    {
        goto done;
    }
    identity_size = document->tokens[identity].end - document->tokens[identity].start;
    line_start    = 0U;
    while(line_start < text_size && !present)
    {
        size_t line_end;
        int    comparison;

        line_end = line_start;
        while(line_end < text_size && text[line_end] != '\n' && text[line_end] != '\r')
        {
            line_end++;
        }
        field_start = line_start;
        while(field_start < line_end && text[field_start] != '\t')
        {
            field_start++;
        }
        if(field_start < line_end)
        {
            field_start++;
            field_end = field_start;
            while(field_end < line_end && text[field_end] != '\t')
            {
                field_end++;
            }
            comparison = -1;
            if(field_end - field_start == identity_size)
            {
                comparison = p101_strncmp(env, text + field_start, document->text + document->tokens[identity].start, identity_size);
            }
            if(comparison == 0)
            {
                present = true;
            }
        }
        line_start = line_end;
        while(line_start < text_size && (text[line_start] == '\n' || text[line_start] == '\r'))
        {
            line_start++;
        }
    }

done:
    p101_free(env, text);
    return present;
}

static bool fact_identity_exists(const struct p101_wrapper_model *model, enum p101_c_analysis_kind kind, const struct p101_workspace_json *document, size_t identity)
{
    size_t index;
    bool   equal;
    bool   found;

    found = false;
    for(index = 0U; index < model->fact_count && !found; index++)
    {
        if(model->facts[index].kind == kind)
        {
            equal = p101_workspace_json_token_equals(env, document, identity, model->facts[index].usr);
            if(equal)
            {
                found = true;
            }
        }
    }
    return found;
}

static bool validate_modes(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *contract, const char *contract_path, struct p101_workspace_audit_result *result, size_t *short_array, size_t *uncertain_mode)
{
    static const char *const kind_values[FAULT_MODE_COUNT]  = {"c:@EA@p101_env_fault_kind@P101_ENV_FAULT_ERROR",
                                                               "c:@EA@p101_env_fault_kind@P101_ENV_FAULT_ERROR",
                                                               "c:@EA@p101_env_fault_kind@P101_ENV_FAULT_ERROR",
                                                               "c:@EA@p101_env_fault_kind@P101_ENV_FAULT_SHORT",
                                                               "c:@EA@p101_env_fault_kind@P101_ENV_FAULT_UNCERTAIN"};
    static const char *const phases[FAULT_MODE_COUNT]       = {"before-call", "before-call", "before-call", "after-partial-progress", "after-dispatch"};
    static const char *const dispositions[FAULT_MODE_COUNT] = {"retry-safe", "retry-safe", "retry-safe", "progress-known", "outcome-uncertain"};
    size_t                   modes;
    size_t                   mode;
    size_t                   field;
    size_t                   index;
    bool                     found;
    bool                     equal;
    bool                     valid;

    *short_array    = SIZE_MAX;
    *uncertain_mode = SIZE_MAX;
    valid           = get_required(env, err, contract, 0U, "modes", &modes, result, contract_path);
    if(!valid || contract->tokens[modes].kind != P101_WORKSPACE_JSON_OBJECT || contract->tokens[modes].child_count != FAULT_MODE_MEMBER_COUNT)
    {
        add_contract_finding(env, err, result, contract_path, "fault mode inventory drifted");
        valid = false;
        goto done;
    }
    for(index = 0U; index < FAULT_MODE_COUNT; index++)
    {
        found = p101_workspace_json_object_get(env, contract, modes, FAULT_MODES[index], &mode);
        if(!found)
        {
            add_contract_finding(env, err, result, contract_path, "fault mode inventory drifted");
            valid = false;
            continue;
        }
        if(contract->tokens[mode].child_count != (index == FAULT_UNCERTAIN_MODE_INDEX ? FAULT_UNCERTAIN_FIELD_COUNT : FAULT_MODE_FIELD_COUNT))
        {
            add_contract_finding(env, err, result, contract_path, "fault mode semantic fields drifted");
        }
        found = get_required(env, err, contract, mode, "kind_usr", &field, result, contract_path);
        equal = false;
        if(found)
        {
            equal = p101_workspace_json_token_equals(env, contract, field, kind_values[index]);
        }
        if(found && !equal)
        {
            add_contract_finding(env, err, result, contract_path, "fault-kind identity drifted");
        }
        found = get_required(env, err, contract, mode, "phase", &field, result, contract_path);
        equal = false;
        if(found)
        {
            equal = p101_workspace_json_token_equals(env, contract, field, phases[index]);
        }
        if(found && !equal)
        {
            add_contract_finding(env, err, result, contract_path, "fault phase drifted");
        }
        found = get_required(env, err, contract, mode, "disposition", &field, result, contract_path);
        equal = false;
        if(found)
        {
            equal = p101_workspace_json_token_equals(env, contract, field, dispositions[index]);
        }
        if(found && !equal)
        {
            add_contract_finding(env, err, result, contract_path, "fault disposition drifted");
        }
        if(index == FAULT_SHORT_MODE_INDEX)
        {
            found = get_required(env, err, contract, mode, "supported_wrapper_usrs", short_array, result, contract_path);
            if(!found)
            {
                valid = false;
            }
        }
        if(index == FAULT_UNCERTAIN_MODE_INDEX)
        {
            *uncertain_mode = mode;
            found           = get_required(env, err, contract, mode, "supported_wrapper_usrs", &field, result, contract_path);
            if(!found || *short_array == SIZE_MAX)
            {
                valid = false;
            }
            else if(contract->tokens[field].child_count != contract->tokens[*short_array].child_count)
            {
                add_contract_finding(env, err, result, contract_path, "short and uncertain wrapper identities differ");
            }
            else
            {
                size_t item;
                size_t token;

                for(item = 0U; item < contract->tokens[*short_array].child_count; item++)
                {
                    size_t uncertain_token;

                    p101_workspace_json_array_get(contract, *short_array, item, &token);
                    equal = false;
                    if(p101_workspace_json_array_get(contract, field, item, &uncertain_token))
                    {
                        equal = tokens_equal(env, contract, token, contract, uncertain_token);
                    }
                    if(!equal)
                    {
                        add_contract_finding(env, err, result, contract_path, "short and uncertain wrapper identities differ");
                        break;
                    }
                }
            }
            found = get_required(env, err, contract, mode, "retry_rule", &field, result, contract_path);
            equal = false;
            if(found)
            {
                equal = p101_workspace_json_token_equals(env, contract, field, "automatic-retry-forbidden");
            }
            if(found && !equal)
            {
                add_contract_finding(env, err, result, contract_path, "uncertain outcomes authorize automatic retry");
            }
        }
    }

done:
    return valid;
}

static bool validate_fact_mechanism(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *contract, const char *contract_path, const struct p101_wrapper_model *model, size_t admitted, size_t uncertain_mode,
                                    struct p101_workspace_audit_result *result)
{
    size_t      mechanism;
    size_t      selector;
    size_t      recorder;
    size_t      role;
    size_t      evidence_wrapper;
    size_t      index;
    size_t      other;
    size_t      kind_usr;
    size_t      mode;
    bool        found;
    bool        selector_seen;
    bool        recorder_seen;
    bool        caller_admitted;
    bool        role_seen;
    bool        role_ambiguous;
    bool        evidence_call_seen;
    bool        equal;
    bool        valid;
    const char *role_caller;

    valid = get_required(env, err, contract, 0U, "mechanism", &mechanism, result, contract_path);
    if(!valid)
    {
        goto done;
    }
    if(contract->tokens[mechanism].child_count != FAULT_MECHANISM_MEMBER_COUNT)
    {
        add_contract_finding(env, err, result, contract_path, "fault mechanism identities drifted");
    }
    found = get_required(env, err, contract, mechanism, "action_selector_usr", &selector, result, contract_path);
    if(!found)
    {
        valid = false;
    }
    found = get_required(env, err, contract, mechanism, "action_recorder_usr", &recorder, result, contract_path);
    if(!found)
    {
        valid = false;
    }
    if(!valid)
    {
        goto done;
    }
    for(index = 0U; index < FAULT_MODE_COUNT; index++)
    {
        p101_workspace_json_object_get(env, contract, 0U, "modes", &mode);
        p101_workspace_json_object_get(env, contract, mode, FAULT_MODES[index], &mode);
        p101_workspace_json_object_get(env, contract, mode, "kind_usr", &kind_usr);
        found = fact_identity_exists(model, P101_C_ANALYSIS_ENUMERATOR, contract, kind_usr);
        if(!found)
        {
            add_contract_finding(env, err, result, contract_path, "fault-kind identity is absent from lib_env");
        }
    }
    for(index = 0U; index < contract->tokens[admitted].child_count; index++)
    {
        size_t identity;

        p101_workspace_json_array_get(contract, admitted, index, &identity);
        selector_seen = false;
        recorder_seen = false;
        for(other = 0U; other < model->fact_count; other++)
        {
            const struct p101_wrapper_fact *fact;

            fact = &model->facts[other];
            if(fact->kind != P101_C_ANALYSIS_CALL)
            {
                continue;
            }
            equal = p101_workspace_json_token_equals(env, contract, identity, fact->caller_usr);
            if(!equal)
            {
                continue;
            }
            equal = p101_workspace_json_token_equals(env, contract, selector, fact->usr);
            if(equal)
            {
                selector_seen = true;
            }
            equal = p101_workspace_json_token_equals(env, contract, recorder, fact->usr);
            if(equal)
            {
                recorder_seen = true;
            }
        }
        if(!selector_seen || !recorder_seen)
        {
            add_contract_finding(env, err, result, contract_path, "fault-action implementation identity drifted");
        }
    }
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_CALL)
        {
            continue;
        }
        selector_seen = p101_workspace_json_token_equals(env, contract, selector, fact->usr);
        recorder_seen = p101_workspace_json_token_equals(env, contract, recorder, fact->usr);
        if(selector_seen || recorder_seen)
        {
            caller_admitted = admitted_contains(contract, admitted, fact->caller_usr);
            if(!caller_admitted)
            {
                add_contract_finding(env, err, result, contract_path, "undeclared wrapper uses the after-dispatch fault mechanism");
            }
        }
    }
    found = get_required(env, err, contract, uncertain_mode, "evidence_role", &role, result, contract_path);
    if(!found)
    {
        valid = false;
    }
    found = get_required(env, err, contract, uncertain_mode, "evidence_wrapper_usr", &evidence_wrapper, result, contract_path);
    if(!found)
    {
        valid = false;
    }
    if(!valid)
    {
        goto done;
    }
    role_seen          = false;
    role_ambiguous     = false;
    evidence_call_seen = false;
    role_caller        = NULL;
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind == P101_C_ANALYSIS_NOTE)
        {
            const char *prefix;
            size_t      prefix_size;
            size_t      role_size;
            int         comparison;

            prefix      = "SEMANTIC_ROLE:";
            prefix_size = p101_strlen(env, prefix);
            role_size   = contract->tokens[role].end - contract->tokens[role].start;
            comparison  = p101_strncmp(env, fact->name, prefix, prefix_size);
            if(comparison == 0)
            {
                comparison = p101_strncmp(env, fact->name + prefix_size, contract->text + contract->tokens[role].start, role_size);
                if(comparison == 0 && fact->name[prefix_size + role_size] == '\0')
                {
                    if(role_seen)
                    {
                        comparison = p101_strcmp(env, role_caller, fact->caller_usr);
                        if(comparison != 0)
                        {
                            role_ambiguous = true;
                        }
                    }
                    else
                    {
                        role_caller = fact->caller_usr;
                    }
                    role_seen = true;
                    for(other = 0U; other < model->fact_count; other++)
                    {
                        int caller_comparison;

                        caller_comparison = p101_strcmp(env, model->facts[other].caller_usr, fact->caller_usr);
                        equal             = p101_workspace_json_token_equals(env, contract, evidence_wrapper, model->facts[other].usr);
                        if(model->facts[other].kind == P101_C_ANALYSIS_CALL && caller_comparison == 0 && equal)
                        {
                            evidence_call_seen = true;
                        }
                    }
                }
            }
        }
    }
    if(!role_seen || role_ambiguous || !evidence_call_seen)
    {
        add_contract_finding(env, err, result, contract_path, "uncertain-outcome semantic evidence drifted");
    }

done:
    return valid;
}

static bool validate_derived_contracts(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *failure, const struct p101_workspace_json *lifecycle, const char *failure_path, const char *lifecycle_path, size_t admitted,
                                       const struct p101_workspace_json *contract, struct p101_workspace_audit_result *result)
{
    size_t semantics;
    size_t value;
    size_t wrappers;
    size_t scenarios;
    size_t index;
    size_t function_usr;
    size_t fault_modes;
    size_t observed;
    size_t matched;
    bool   found;
    bool   has_short;
    bool   present;
    bool   valid;

    matched = 0U;
    valid   = get_required(env, err, failure, 0U, "semantics", &semantics, result, failure_path);
    if(valid)
    {
        found   = p101_workspace_json_object_get(env, failure, semantics, "fault_boundary", &value);
        present = (found && p101_workspace_json_token_equals(env, failure, value, "after-entry-trace-before-native-work")) != 0;
        if(!present)
        {
            add_contract_finding(env, err, result, failure_path, "generated early-failure boundary drifted");
        }
    }
    found = p101_workspace_json_object_get(env, failure, 0U, "wrappers", &wrappers);
    if(found)
    {
        for(index = wrappers + 1U; index < failure->token_count && failure->tokens[index].start < failure->tokens[wrappers].end; index++)
        {
            if(failure->tokens[index].parent != wrappers || failure->tokens[index].kind != P101_WORKSPACE_JSON_OBJECT)
            {
                continue;
            }
            found     = p101_workspace_json_object_get(env, failure, index, "fault_modes", &fault_modes);
            has_short = false;
            if(found)
            {
                has_short = array_contains(failure, fault_modes, "short");
            }
            if(has_short)
            {
                found = p101_workspace_json_object_get(env, failure, index, "function_usr", &function_usr);
                if(found)
                {
                    size_t admitted_index;
                    size_t identity;
                    size_t identity_size;
                    size_t observed_size;
                    int    comparison;

                    for(admitted_index = 0U; admitted_index < contract->tokens[admitted].child_count; admitted_index++)
                    {
                        p101_workspace_json_array_get(contract, admitted, admitted_index, &identity);
                        identity_size = contract->tokens[identity].end - contract->tokens[identity].start;
                        observed_size = failure->tokens[function_usr].end - failure->tokens[function_usr].start;
                        comparison    = -1;
                        if(identity_size == observed_size)
                        {
                            comparison = p101_strncmp(env, contract->text + contract->tokens[identity].start, failure->text + failure->tokens[function_usr].start, identity_size);
                        }
                        if(comparison == 0)
                        {
                            matched++;
                        }
                    }
                }
            }
        }
    }
    if(matched != contract->tokens[admitted].child_count)
    {
        add_contract_finding(env, err, result, failure_path, "generated short-I/O wrapper identities drifted");
    }
    matched = 0U;
    found   = p101_workspace_json_object_get(env, lifecycle, 0U, "scenarios", &scenarios);
    if(found)
    {
        for(index = scenarios + 1U; index < lifecycle->token_count && lifecycle->tokens[index].start < lifecycle->tokens[scenarios].end; index++)
        {
            if(lifecycle->tokens[index].parent != scenarios || lifecycle->tokens[index].kind != P101_WORKSPACE_JSON_OBJECT)
            {
                continue;
            }
            found     = p101_workspace_json_object_get(env, lifecycle, index, "fault_modes", &fault_modes);
            has_short = false;
            if(found)
            {
                has_short = array_contains(lifecycle, fault_modes, "short");
            }
            if(has_short)
            {
                found = p101_workspace_json_object_get(env, lifecycle, index, "fault_usr", &observed);
                if(found)
                {
                    size_t admitted_index;
                    size_t identity;
                    size_t identity_size;
                    size_t observed_size;
                    int    comparison;

                    for(admitted_index = 0U; admitted_index < contract->tokens[admitted].child_count; admitted_index++)
                    {
                        p101_workspace_json_array_get(contract, admitted, admitted_index, &identity);
                        identity_size = contract->tokens[identity].end - contract->tokens[identity].start;
                        observed_size = lifecycle->tokens[observed].end - lifecycle->tokens[observed].start;
                        comparison    = -1;
                        if(identity_size == observed_size)
                        {
                            comparison = p101_strncmp(env, contract->text + contract->tokens[identity].start, lifecycle->text + lifecycle->tokens[observed].start, identity_size);
                        }
                        if(comparison == 0)
                        {
                            matched++;
                        }
                    }
                }
            }
        }
    }
    if(matched != contract->tokens[admitted].child_count)
    {
        add_contract_finding(env, err, result, lifecycle_path, "short-I/O lifecycle evidence identities drifted");
    }
    return valid;
}
