#include "model.h"
#include "workspace_audit.h"
#include "workspace_fact_bundle.h"
#include "workspace_json.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_dirent.h>

enum
{
    QUALITY_TEXT_SIZE                        = 1024,
    QUALITY_ENUM_CAPACITY                    = 128,
    QUALITY_VARIANT_CAPACITY                 = 64,
    QUALITY_ROOT_CHILD_COUNT                 = 22,
    QUALITY_PUBLIC_SURFACE_CHILD_COUNT       = 10,
    QUALITY_OUTCOME_SET_CHILD_COUNT          = 12,
    QUALITY_OUTCOME_EXCLUSION_CHILD_COUNT    = 10,
    QUALITY_AUDIT_RESPONSIBILITY_CHILD_COUNT = 12,
    QUALITY_PROCESS_TERMINATION_CHILD_COUNT  = 12,
    QUALITY_PLATFORM_EVIDENCE_CHILD_COUNT    = 10,
    QUALITY_PLATFORM_COUNT                   = 3
};

struct quality_enum_contract
{
    char usr[P101_WRAPPER_NAME_SIZE];
    char name[P101_WRAPPER_NAME_SIZE];
    char source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    bool exclusion;
};

static bool quality_text(struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, char *output, size_t output_size);
static bool quality_workspace_file(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *relative, char *absolute, size_t absolute_size);
static bool quality_oracle_exists(const struct p101_env *env, const struct p101_workspace_json *graph, size_t nodes, const char *oracle);
static bool quality_fact_path_matches(const struct p101_env *env, const char *absolute, const char *relative);
static bool quality_enum_fact(const struct p101_env *env, const struct p101_wrapper_model *model, const char *usr, const char *source, const char *name, size_t *fact_index);
static bool quality_variant_exists(const struct p101_env *env, const struct p101_wrapper_model *model, const char *parent_usr, const char *name);
static bool quality_classified(const struct p101_env *env, const struct quality_enum_contract *enums, size_t enum_count, const char *usr);
static bool quality_source_function(const struct p101_env *env, const struct p101_wrapper_model *model, const char *source, const char *usr);
static bool quality_source_has_role(const struct p101_env *env, const struct p101_wrapper_model *model, const char *caller_usr, const struct p101_workspace_json *contract, size_t roles);
static bool quality_string_array(const struct p101_workspace_json *document, size_t array);
static bool quality_prior_text_unique(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t array, size_t current, const char *key, const char *value);
static bool quality_contains_case_insensitive(const char *haystack, const char *needle);
static bool quality_documentation(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_workspace_json *contract, size_t documentation, struct p101_workspace_audit_result *result,
                                  const char *contract_path);
static void quality_add(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *section, const char *message);

bool p101_workspace_audit_run_quality_contract(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    static const char *const REQUIRED_TOP[] =
        {"does_not_prove", "public_surfaces", "tool_documentation", "typed_outcome_sets", "typed_outcome_exclusions", "audit_responsibilities", "boundaries", "process_termination", "platform_evidence", "implementation_oracles"};
    struct p101_workspace_json   contract;
    struct p101_workspace_json   graph;
    struct p101_workspace_json   boundary_register;
    struct p101_wrapper_model    model;
    struct quality_enum_contract enums[QUALITY_ENUM_CAPACITY];
    char                         contract_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                         graph_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                         boundary_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                         source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                         absolute[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                         text[QUALITY_TEXT_SIZE];
    char                         usr[P101_WRAPPER_NAME_SIZE];
    char                         name[P101_WRAPPER_NAME_SIZE];
    char                         oracle[P101_WRAPPER_NAME_SIZE];
    size_t                       nodes;
    size_t                       token;
    size_t                       array;
    size_t                       row;
    size_t                       evidence;
    size_t                       value;
    size_t                       fact_index;
    size_t                       enum_count;
    bool                         joined;
    bool                         loaded;
    bool                         found;
    bool                         valid;
    bool                         success;
    int                          comparison;

    p101_workspace_json_init(&contract);
    p101_workspace_json_init(&graph);
    p101_workspace_json_init(&boundary_register);
    p101_wrapper_model_init(&model);
    nodes      = 0U;
    token      = 0U;
    array      = 0U;
    row        = 0U;
    evidence   = 0U;
    value      = 0U;
    fact_index = 0U;
    enum_count = 0U;
    success    = false;
    if(options->facts_path == NULL)
    {
        P101_ERROR_RAISE_USER(err, "quality-contract policy requires --facts", EINVAL);
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, contract_path, sizeof(contract_path), options->scripts_root, "contracts/p101-quality-contract.json");
    joined = ((p101_workspace_audit_join(env, err, graph_path, sizeof(graph_path), options->scripts_root, "contracts/p101-check-graph.json") && joined) != 0);
    joined = ((p101_workspace_audit_join(env, err, boundary_path, sizeof(boundary_path), options->scripts_root, "contracts/p101-boundaries.json") && joined) != 0);
    if(!joined)
    {
        goto done;
    }
    loaded = p101_workspace_json_load(env, err, contract_path, &contract);
    loaded = ((p101_workspace_json_load(env, err, graph_path, &graph) && loaded) != 0);
    loaded = ((p101_workspace_json_load(env, err, boundary_path, &boundary_register) && loaded) != 0);
    loaded = ((p101_workspace_fact_bundle_load(env, err, options->facts_path, &model) && loaded) != 0);
    if(!loaded)
    {
        goto done;
    }
    found = p101_workspace_json_object_get(env, &contract, 0U, "schema", &token);
    valid = ((found && contract.tokens[0].child_count == QUALITY_ROOT_CHILD_COUNT && p101_workspace_json_token_equals(env, &contract, token, "p101-quality-contract-v3")) != 0);
    valid = ((quality_text(err, &contract, 0U, "does_not_prove", text, sizeof(text)) && valid) != 0);
    for(size_t index = 0U; index < sizeof(REQUIRED_TOP) / sizeof(REQUIRED_TOP[0]); index++)
    {
        found = p101_workspace_json_object_get(env, &contract, 0U, REQUIRED_TOP[index], &token);
        valid = ((found && valid) != 0);
        result->checks++;
    }
    found = p101_workspace_json_object_get(env, &graph, 0U, "nodes", &nodes);
    valid = ((found && graph.tokens[nodes].kind == P101_WORKSPACE_JSON_ARRAY && valid) != 0);
    if(!valid)
    {
        quality_add(env, err, result, contract_path, "schema", "quality catalog or check graph is invalid");
        success = p101_error_has_no_error(err);
        goto done;
    }
    found = p101_workspace_json_object_get(env, &contract, 0U, "tool_documentation", &token);
    if(found)
    {
        quality_documentation(env, err, options, &contract, token, result, contract_path);
    }

    found = p101_workspace_json_object_get(env, &contract, 0U, "public_surfaces", &array);
    if(found && contract.tokens[array].kind == P101_WORKSPACE_JSON_ARRAY)
    {
        for(size_t index = 0U; index < contract.tokens[array].child_count; index++)
        {
            found      = p101_workspace_json_array_get(&contract, array, index, &row);
            valid      = ((found && contract.tokens[row].kind == P101_WORKSPACE_JSON_OBJECT && contract.tokens[row].child_count == QUALITY_PUBLIC_SURFACE_CHILD_COUNT) != 0);
            valid      = ((quality_text(err, &contract, row, "id", text, sizeof(text)) && valid) != 0);
            comparison = (int)valid ? p101_strncmp(env, text, "public:", sizeof("public:") - 1U) : -1;
            valid      = ((comparison == 0 && quality_prior_text_unique(env, err, &contract, array, index, "id", text) && valid) != 0);
            valid      = ((quality_text(err, &contract, row, "owner", name, sizeof(name)) && valid) != 0);
            valid      = ((quality_text(err, &contract, row, "contract", source, sizeof(source)) && valid) != 0);
            valid      = ((valid && quality_workspace_file(env, err, options, source, absolute, sizeof(absolute))) != 0);
            valid      = ((quality_text(err, &contract, row, "checker", source, sizeof(source)) && valid) != 0);
            valid      = ((valid && quality_workspace_file(env, err, options, source, absolute, sizeof(absolute))) != 0);
            valid      = ((quality_text(err, &contract, row, "oracle", oracle, sizeof(oracle)) && valid) != 0);
            valid      = ((valid && quality_oracle_exists(env, &graph, nodes, oracle)) != 0);
            if(!valid)
            {
                quality_add(env, err, result, contract_path, "public surface", "has stale identity, file, or oracle evidence");
            }
            result->checks++;
        }
    }

    for(size_t classification = 0U; classification < 2U; classification++)
    {
        const char *key;

        key   = classification == 0U ? "typed_outcome_sets" : "typed_outcome_exclusions";
        found = p101_workspace_json_object_get(env, &contract, 0U, key, &array);
        if(!found || contract.tokens[array].kind != P101_WORKSPACE_JSON_ARRAY)
        {
            quality_add(env, err, result, contract_path, key, "is not an array");
            continue;
        }
        for(size_t index = 0U; index < contract.tokens[array].child_count; index++)
        {
            size_t variants;

            found = p101_workspace_json_array_get(&contract, array, index, &row);
            valid = ((found && contract.tokens[row].kind == P101_WORKSPACE_JSON_OBJECT) != 0);
            valid = ((contract.tokens[row].child_count == (classification == 0U ? QUALITY_OUTCOME_SET_CHILD_COUNT : QUALITY_OUTCOME_EXCLUSION_CHILD_COUNT) && valid) != 0);
            valid = ((quality_text(err, &contract, row, "source", source, sizeof(source)) && valid) != 0);
            valid = ((quality_text(err, &contract, row, "type", name, sizeof(name)) && valid) != 0);
            valid = ((quality_text(err, &contract, row, "type_usr", usr, sizeof(usr)) && valid) != 0);
            valid = ((quality_text(err, &contract, row, "owner", text, sizeof(text)) && valid) != 0);
            if(classification == 0U)
            {
                valid = ((quality_text(err, &contract, row, "oracle", oracle, sizeof(oracle)) && valid) != 0);
                valid = ((valid && quality_oracle_exists(env, &graph, nodes, oracle)) != 0);
            }
            valid = ((!quality_classified(env, enums, enum_count, usr) && valid) != 0);
            valid = ((valid && quality_workspace_file(env, err, options, source, absolute, sizeof(absolute))) != 0);
            valid = ((valid && quality_enum_fact(env, &model, usr, source, name, &fact_index)) != 0);
            if(classification != 0U)
            {
                valid = ((quality_text(err, &contract, row, "reason", text, sizeof(text)) && valid) != 0);
            }
            if(enum_count >= QUALITY_ENUM_CAPACITY)
            {
                P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
                goto done;
            }
            if(valid)
            {
                p101_snprintf(env, err, enums[enum_count].usr, sizeof(enums[enum_count].usr), "%s", usr);
                p101_snprintf(env, err, enums[enum_count].name, sizeof(enums[enum_count].name), "%s", name);
                p101_snprintf(env, err, enums[enum_count].source, sizeof(enums[enum_count].source), "%s", source);
                enums[enum_count].exclusion = classification != 0U;
                enum_count++;
            }
            if(classification == 0U)
            {
                found = p101_workspace_json_object_get(env, &contract, row, "variants", &variants);
                valid = ((found && contract.tokens[variants].kind == P101_WORKSPACE_JSON_ARRAY && contract.tokens[variants].child_count > 0U && valid) != 0);
                for(size_t variant_index = 0U; found && variant_index < contract.tokens[variants].child_count; variant_index++)
                {
                    found = p101_workspace_json_array_get(&contract, variants, variant_index, &value);
                    found = ((found && p101_workspace_json_token_copy(env, err, &contract, value, text, sizeof(text))) != 0);
                    found = ((found && quality_variant_exists(env, &model, usr, text)) != 0);
                    valid = ((found && valid) != 0);
                }
                for(size_t model_index = 0U; model_index < model.fact_count; model_index++)
                {
                    const struct p101_wrapper_fact *fact;
                    bool                            listed;

                    fact       = &model.facts[model_index];
                    comparison = p101_strcmp(env, fact->parent_usr, usr);
                    if(fact->kind != P101_C_ANALYSIS_ENUMERATOR || comparison != 0)
                    {
                        continue;
                    }
                    listed = false;
                    for(size_t variant_index = 0U; variant_index < contract.tokens[variants].child_count && !listed; variant_index++)
                    {
                        p101_workspace_json_array_get(&contract, variants, variant_index, &value);
                        listed = p101_workspace_json_token_equals(env, &contract, value, fact->name);
                    }
                    valid = ((listed && valid) != 0);
                }
            }
            if(!valid)
            {
                quality_add(env, err, result, contract_path, key, "enum identity, source, or variants drifted");
            }
            result->checks++;
        }
    }
    for(size_t index = 0U; index < model.fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;
        bool                            library_header;
        bool                            classified;
        const char                     *libraries;
        const char                     *include;

        fact = &model.facts[index];
        if(fact->kind != P101_C_ANALYSIS_ENUM)
        {
            continue;
        }
        libraries      = p101_strstr(env, fact->path, "/libraries/");
        include        = p101_strstr(env, fact->path, "/include/");
        library_header = ((libraries != NULL && include != NULL && include > libraries) != 0);
        if(!library_header)
        {
            continue;
        }
        classified = quality_classified(env, enums, enum_count, fact->usr);
        if(!classified)
        {
            quality_add(env, err, result, fact->path, fact->name, "public enum is not classified as an outcome or an explicit exclusion");
        }
        result->checks++;
    }

    found = p101_workspace_json_object_get(env, &contract, 0U, "audit_responsibilities", &array);
    if(found && contract.tokens[array].kind == P101_WORKSPACE_JSON_ARRAY)
    {
        for(size_t index = 0U; index < contract.tokens[array].child_count; index++)
        {
            char kind[P101_WRAPPER_NAME_SIZE];
            char mode[P101_WRAPPER_NAME_SIZE];
            char identifier[P101_WRAPPER_NAME_SIZE];
            char owner[P101_WRAPPER_NAME_SIZE];
            int  mode_comparison;

            found           = p101_workspace_json_array_get(&contract, array, index, &row);
            valid           = ((found && contract.tokens[row].kind == P101_WORKSPACE_JSON_OBJECT && contract.tokens[row].child_count == QUALITY_AUDIT_RESPONSIBILITY_CHILD_COUNT) != 0);
            valid           = ((quality_text(err, &contract, row, "id", identifier, sizeof(identifier)) && valid) != 0);
            comparison      = (int)valid ? p101_strncmp(env, identifier, "audit:", sizeof("audit:") - 1U) : -1;
            valid           = ((comparison == 0 && quality_prior_text_unique(env, err, &contract, array, index, "id", identifier) && valid) != 0);
            valid           = ((quality_text(err, &contract, row, "owner", owner, sizeof(owner)) && valid) != 0);
            valid           = ((quality_text(err, &contract, row, "mode", mode, sizeof(mode)) && valid) != 0);
            comparison      = p101_strcmp(env, mode, "local");
            mode_comparison = p101_strcmp(env, mode, "delegated");
            valid           = (((comparison == 0 || mode_comparison == 0) && valid) != 0);
            valid           = ((quality_text(err, &contract, row, "source", source, sizeof(source)) && valid) != 0);
            valid           = ((valid && quality_workspace_file(env, err, options, source, absolute, sizeof(absolute))) != 0);
            valid           = ((quality_text(err, &contract, row, "oracle", oracle, sizeof(oracle)) && valid) != 0);
            valid           = ((valid && quality_oracle_exists(env, &graph, nodes, oracle)) != 0);
            found           = p101_workspace_json_object_get(env, &contract, row, "evidence", &evidence);
            valid           = ((found && contract.tokens[evidence].kind == P101_WORKSPACE_JSON_OBJECT && contract.tokens[evidence].child_count == 4U && valid) != 0);
            valid           = ((quality_text(err, &contract, evidence, "kind", kind, sizeof(kind)) && valid) != 0);
            valid           = ((quality_text(err, &contract, evidence, "value", text, sizeof(text)) && valid) != 0);
            comparison      = p101_strcmp(env, kind, "function-usr");
            if(comparison == 0)
            {
                valid = ((valid && quality_source_function(env, &model, source, text)) != 0);
            }
            else
            {
                struct p101_workspace_json evidence_document;

                p101_workspace_json_init(&evidence_document);
                comparison = p101_strcmp(env, kind, "json-schema");
                loaded     = ((comparison == 0 && p101_workspace_json_load(env, P101_ERROR_OPTIONAL, absolute, &evidence_document)) != 0);
                found      = ((loaded && p101_workspace_json_object_get(env, &evidence_document, 0U, "schema", &value)) != 0);
                valid      = ((valid && found && p101_workspace_json_token_equals(env, &evidence_document, value, text)) != 0);
                p101_workspace_json_destroy(env, &evidence_document);
            }
            if(!valid)
            {
                quality_add(env, err, result, contract_path, "audit responsibility", "has stale semantic evidence or oracle ownership");
            }
            result->checks++;
        }
    }
    else
    {
        quality_add(env, err, result, contract_path, "audit responsibility", "catalog is missing or empty");
    }

    found  = p101_workspace_json_object_get(env, &contract, 0U, "boundaries", &array);
    loaded = p101_workspace_json_object_get(env, &boundary_register, 0U, "boundaries", &value);
    if(found && loaded && contract.tokens[array].child_count == boundary_register.tokens[value].child_count)
    {
        for(size_t index = 0U; index < contract.tokens[array].child_count; index++)
        {
            bool registered;

            p101_workspace_json_array_get(&contract, array, index, &row);
            valid      = ((contract.tokens[row].kind == P101_WORKSPACE_JSON_OBJECT && contract.tokens[row].child_count == 4U) != 0);
            valid      = ((quality_text(err, &contract, row, "id", text, sizeof(text)) && valid) != 0);
            valid      = ((quality_prior_text_unique(env, err, &contract, array, index, "id", text) && valid) != 0);
            valid      = ((quality_text(err, &contract, row, "oracle", oracle, sizeof(oracle)) && valid) != 0);
            valid      = ((valid && quality_oracle_exists(env, &graph, nodes, oracle)) != 0);
            registered = false;
            for(size_t boundary_index = 0U; boundary_index < boundary_register.tokens[value].child_count && !registered; boundary_index++)
            {
                p101_workspace_json_array_get(&boundary_register, value, boundary_index, &evidence);
                found      = p101_workspace_json_object_get(env, &boundary_register, evidence, "id", &token);
                registered = ((found && p101_workspace_json_token_equals(env, &boundary_register, token, text)) != 0);
            }
            valid = ((valid && registered) != 0);
            if(!valid)
            {
                quality_add(env, err, result, contract_path, "boundaries", "quality and boundary registers differ");
            }
            result->checks++;
        }
    }
    else
    {
        quality_add(env, err, result, contract_path, "boundaries", "quality and boundary registers have different sizes");
    }

    found      = p101_workspace_json_object_get(env, &contract, 0U, "process_termination", &row);
    valid      = ((found && contract.tokens[row].kind == P101_WORKSPACE_JSON_OBJECT && contract.tokens[row].child_count == QUALITY_PROCESS_TERMINATION_CHILD_COUNT) != 0);
    valid      = ((quality_text(err, &contract, row, "allowed_caller_usr", usr, sizeof(usr)) && valid) != 0);
    comparison = (int)valid ? p101_strcmp(env, usr, "c:@F@main") : -1;
    valid      = ((comparison == 0 && p101_workspace_json_object_get(env, &contract, row, "termination_usrs", &array) && valid) != 0);
    valid      = ((valid && quality_string_array(&contract, array)) != 0);
    valid      = p101_workspace_json_object_get(env, &contract, row, "excluded_semantic_roles", &evidence) && valid;
    valid      = ((quality_string_array(&contract, evidence) && valid) != 0);
    found      = p101_workspace_json_object_get(env, &contract, row, "source_roots", &token);
    valid      = ((found && quality_string_array(&contract, token) && valid) != 0);
    valid      = ((quality_text(err, &contract, row, "checker", source, sizeof(source)) && valid) != 0);
    valid      = ((valid && quality_workspace_file(env, err, options, source, absolute, sizeof(absolute))) != 0);
    valid      = ((quality_text(err, &contract, row, "oracle", oracle, sizeof(oracle)) && valid) != 0);
    valid      = ((valid && quality_oracle_exists(env, &graph, nodes, oracle)) != 0);
    if(valid)
    {
        for(size_t index = 0U; index < model.fact_count; index++)
        {
            const struct p101_wrapper_fact *fact;
            bool                            terminal;
            bool                            excluded;

            fact = &model.facts[index];
            if(fact->kind != P101_C_ANALYSIS_CALL || p101_strstr(env, fact->path, "/programs/") == NULL)
            {
                continue;
            }
            terminal = false;
            for(size_t termination_index = 0U; termination_index < contract.tokens[array].child_count && !terminal; termination_index++)
            {
                p101_workspace_json_array_get(&contract, array, termination_index, &token);
                terminal = p101_workspace_json_token_equals(env, &contract, token, fact->usr);
            }
            comparison = p101_strcmp(env, fact->caller_usr, "c:@F@main");
            if(!terminal || comparison == 0)
            {
                continue;
            }
            excluded = quality_source_has_role(env, &model, fact->caller_usr, &contract, evidence);
            if(!excluded)
            {
                quality_add(env, err, result, fact->path, fact->caller_usr, "calls process termination outside main");
            }
            result->checks++;
        }
    }
    else
    {
        quality_add(env, err, result, contract_path, "process termination", "has invalid semantic scope or non-main authority");
    }

    found = p101_workspace_json_object_get(env, &contract, 0U, "platform_evidence", &row);
    valid = ((found && contract.tokens[row].kind == P101_WORKSPACE_JSON_OBJECT && contract.tokens[row].child_count == QUALITY_PLATFORM_EVIDENCE_CHILD_COUNT) != 0);
    found = ((found && p101_workspace_json_object_get(env, &contract, row, "required", &array)) != 0);
    valid = ((found && contract.tokens[array].kind == P101_WORKSPACE_JSON_ARRAY && contract.tokens[array].child_count == QUALITY_PLATFORM_COUNT && valid) != 0);
    valid = ((valid && p101_workspace_json_object_get(env, &contract, row, "receipt_schema", &token)) != 0);
    valid = ((valid && p101_workspace_json_token_equals(env, &contract, token, "p101-check-graph-receipt-v2")) != 0);
    for(size_t platform_index = 0U; platform_index < QUALITY_PLATFORM_COUNT && valid; platform_index++)
    {
        static const char *const PLATFORMS[] = {"freebsd", "linux", "macos"};
        bool                     present;

        present = false;
        for(size_t index = 0U; index < contract.tokens[array].child_count && !present; index++)
        {
            p101_workspace_json_array_get(&contract, array, index, &token);
            present = p101_workspace_json_token_equals(env, &contract, token, PLATFORMS[platform_index]);
        }
        valid = ((present && valid) != 0);
    }
    valid = ((quality_text(err, &contract, row, "producer", source, sizeof(source)) && valid) != 0);
    valid = ((valid && quality_workspace_file(env, err, options, source, absolute, sizeof(absolute))) != 0);
    valid = ((quality_text(err, &contract, row, "merge_driver", source, sizeof(source)) && valid) != 0);
    valid = ((valid && quality_workspace_file(env, err, options, source, absolute, sizeof(absolute))) != 0);
    valid = ((quality_text(err, &contract, row, "oracle", oracle, sizeof(oracle)) && valid) != 0);
    valid = ((valid && quality_oracle_exists(env, &graph, nodes, oracle)) != 0);
    if(!valid)
    {
        quality_add(env, err, result, contract_path, "platform evidence", "must require FreeBSD, Linux, and macOS receipts");
    }
    result->checks++;

    found = p101_workspace_json_object_get(env, &contract, 0U, "implementation_oracles", &array);
    if(found && contract.tokens[array].kind == P101_WORKSPACE_JSON_ARRAY && contract.tokens[array].child_count > 0U)
    {
        for(size_t index = 0U; index < contract.tokens[array].child_count; index++)
        {
            p101_workspace_json_array_get(&contract, array, index, &row);
            valid = ((contract.tokens[row].kind == P101_WORKSPACE_JSON_OBJECT && contract.tokens[row].child_count == 4U) != 0);
            valid = ((quality_text(err, &contract, row, "kind", text, sizeof(text)) && valid) != 0);
            valid = ((quality_text(err, &contract, row, "oracle", oracle, sizeof(oracle)) && valid) != 0);
            valid = ((valid && quality_oracle_exists(env, &graph, nodes, oracle)) != 0);
            if(!valid)
            {
                quality_add(env, err, result, contract_path, "implementation oracle", "refers to an unknown graph node");
            }
            result->checks++;
        }
    }
    else
    {
        quality_add(env, err, result, contract_path, "implementation oracle", "catalog is missing or empty");
    }
    success = p101_error_has_no_error(err);

done:
    p101_wrapper_model_destroy(env, &model);
    p101_workspace_json_destroy(env, &boundary_register);
    p101_workspace_json_destroy(env, &graph);
    p101_workspace_json_destroy(env, &contract);
    return success;
}

static bool quality_text(struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, char *output, size_t output_size)
{
    size_t value;
    bool   found;
    bool   copied;

    found  = p101_workspace_json_object_get(env, document, object, key, &value);
    copied = ((found && p101_workspace_json_token_copy(env, err, document, value, output, output_size)) != 0);
    return (copied && output[0] != '\0') != 0;
}

static bool quality_workspace_file(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const char *relative, char *absolute, size_t absolute_size)
{
    bool joined;
    bool exists;

    joined = ((relative[0] != '/' && p101_strchr(env, relative, '\\') == NULL) != 0);
    joined = ((joined && p101_workspace_audit_join(env, err, absolute, absolute_size, options->workspace, relative)) != 0);
    exists = ((joined && p101_workspace_audit_file_exists(env, P101_ERROR_OPTIONAL, absolute)) != 0);
    return exists;
}

static bool quality_oracle_exists(const struct p101_env *env, const struct p101_workspace_json *graph, size_t nodes, const char *oracle)
{
    char   identifier[P101_WRAPPER_NAME_SIZE];
    size_t row;
    size_t token;
    bool   found;

    found = false;
    for(size_t index = 0U; index < graph->tokens[nodes].child_count && !found; index++)
    {
        p101_workspace_json_array_get(graph, nodes, index, &row);
        found = p101_workspace_json_object_get(env, graph, row, "id", &token);
        found = ((found && p101_workspace_json_token_copy(env, P101_ERROR_OPTIONAL, graph, token, identifier, sizeof(identifier))) != 0);
        found = ((found && p101_strcmp(env, identifier, oracle) == 0) != 0);
    }
    return found;
}

static bool quality_fact_path_matches(const struct p101_env *env, const char *absolute, const char *relative)
{
    size_t absolute_length;
    size_t relative_length;
    bool   matches;

    absolute_length = p101_strlen(env, absolute);
    relative_length = p101_strlen(env, relative);
    matches         = absolute_length >= relative_length;
    if(matches)
    {
        matches = p101_strcmp(env, absolute + absolute_length - relative_length, relative) == 0;
    }
    return matches;
}

static bool quality_enum_fact(const struct p101_env *env, const struct p101_wrapper_model *model, const char *usr, const char *source, const char *name, size_t *fact_index)
{
    bool found;

    *fact_index = model->fact_count;
    found       = false;
    for(size_t index = 0U; index < model->fact_count && !found; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact  = &model->facts[index];
        found = fact->kind == P101_C_ANALYSIS_ENUM;
        found = ((found && p101_strcmp(env, fact->usr, usr) == 0) != 0);
        found = ((found && p101_strcmp(env, fact->name, name) == 0) != 0);
        found = ((found && quality_fact_path_matches(env, fact->path, source)) != 0);
        if(found)
        {
            *fact_index = index;
        }
    }
    return found;
}

static bool quality_variant_exists(const struct p101_env *env, const struct p101_wrapper_model *model, const char *parent_usr, const char *name)
{
    bool found;

    found = false;
    for(size_t index = 0U; index < model->fact_count && !found; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact  = &model->facts[index];
        found = fact->kind == P101_C_ANALYSIS_ENUMERATOR;
        found = ((found && p101_strcmp(env, fact->parent_usr, parent_usr) == 0) != 0);
        found = ((found && p101_strcmp(env, fact->name, name) == 0) != 0);
    }
    return found;
}

static bool quality_classified(const struct p101_env *env, const struct quality_enum_contract *enums, size_t enum_count, const char *usr)
{
    bool found;

    found = false;
    for(size_t index = 0U; index < enum_count && !found; index++)
    {
        found = p101_strcmp(env, enums[index].usr, usr) == 0;
    }
    return found;
}

static bool quality_source_function(const struct p101_env *env, const struct p101_wrapper_model *model, const char *source, const char *usr)
{
    bool found;

    found = false;
    for(size_t index = 0U; index < model->fact_count && !found; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact  = &model->facts[index];
        found = fact->kind == P101_C_ANALYSIS_FUNCTION;
        found = ((found && p101_strcmp(env, fact->usr, usr) == 0) != 0);
        found = ((found && quality_fact_path_matches(env, fact->path, source)) != 0);
    }
    return found;
}

static bool quality_source_has_role(const struct p101_env *env, const struct p101_wrapper_model *model, const char *caller_usr, const struct p101_workspace_json *contract, size_t roles)
{
    char   expected[P101_WRAPPER_NAME_SIZE];
    char   role[P101_WRAPPER_NAME_SIZE];
    size_t token;
    bool   found;

    found = false;
    for(size_t role_index = 0U; role_index < contract->tokens[roles].child_count && !found; role_index++)
    {
        p101_workspace_json_array_get(contract, roles, role_index, &token);
        p101_workspace_json_token_copy(env, P101_ERROR_OPTIONAL, contract, token, role, sizeof(role));
        p101_snprintf(env, P101_ERROR_OPTIONAL, expected, sizeof(expected), "SEMANTIC_ROLE:%s", role);
        for(size_t index = 0U; index < model->fact_count && !found; index++)
        {
            const struct p101_wrapper_fact *fact;

            fact  = &model->facts[index];
            found = fact->kind == P101_C_ANALYSIS_NOTE;
            found = ((found && p101_strcmp(env, fact->caller_usr, caller_usr) == 0) != 0);
            found = ((found && p101_strcmp(env, fact->name, expected) == 0) != 0);
        }
    }
    return found;
}

static bool quality_string_array(const struct p101_workspace_json *document, size_t array)
{
    bool valid;

    valid = ((document->tokens[array].kind == P101_WORKSPACE_JSON_ARRAY && document->tokens[array].child_count > 0U) != 0);
    for(size_t index = 0U; valid && index < document->tokens[array].child_count; index++)
    {
        size_t value;

        valid = p101_workspace_json_array_get(document, array, index, &value);
        valid = ((valid && document->tokens[value].kind == P101_WORKSPACE_JSON_STRING) != 0);
        valid = ((valid && document->tokens[value].end > document->tokens[value].start) != 0);
    }
    return valid;
}

static bool quality_prior_text_unique(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t array, size_t current, const char *key, const char *value)
{
    char previous[QUALITY_TEXT_SIZE];
    bool unique;

    unique = true;
    for(size_t index = 0U; unique && index < current; index++)
    {
        size_t row;
        bool   loaded;
        int    comparison;

        loaded = p101_workspace_json_array_get(document, array, index, &row);
        loaded = ((loaded && quality_text(err, document, row, key, previous, sizeof(previous))) != 0);
        if(!loaded)
        {
            return false;
        }
        comparison = p101_strcmp(env, value, previous);
        unique     = comparison != 0;
    }
    return unique;
}

static bool quality_contains_case_insensitive(const char *haystack, const char *needle)
{
    const char *candidate;
    bool        found;

    found = false;
    for(candidate = haystack; *candidate != '\0' && !found; candidate++)
    {
        size_t index;

        index = 0U;
        while(needle[index] != '\0' && candidate[index] != '\0')
        {
            unsigned char left;
            unsigned char right;

            left  = (unsigned char)candidate[index];
            right = (unsigned char)needle[index];
            if(left >= (unsigned char)'A' && left <= (unsigned char)'Z')
            {
                left = (unsigned char)(left + ((unsigned char)'a' - (unsigned char)'A'));
            }
            if(right >= (unsigned char)'A' && right <= (unsigned char)'Z')
            {
                right = (unsigned char)(right + ((unsigned char)'a' - (unsigned char)'A'));
            }
            if(left != right)
            {
                break;
            }
            index++;
        }
        found = needle[index] == '\0';
    }
    return found;
}

static bool quality_documentation(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_workspace_json *contract, size_t documentation, struct p101_workspace_audit_result *result,
                                  const char *contract_path)
{
    DIR           *directory;
    struct dirent *entry;
    char           programs[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char           readme[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char          *contents;
    size_t         length;
    size_t         concepts;
    size_t concept;
    size_t patterns;
    size_t pattern;
    char   concept_id[P101_WRAPPER_NAME_SIZE];
    char   pattern_text[QUALITY_TEXT_SIZE];
    bool   joined;
    bool   found;
    bool   present;
    bool   valid;
    bool   success;
    int    comparison;
    int    close_status;

    contents = NULL;
    success  = false;
    joined   = p101_workspace_audit_join(env, err, programs, sizeof(programs), options->workspace, "programs");
    found    = p101_workspace_json_object_get(env, contract, documentation, "required_concepts", &concepts);
    if(!joined || !found || contract->tokens[concepts].kind != P101_WORKSPACE_JSON_ARRAY)
    {
        quality_add(env, err, result, contract_path, "tool documentation", "has an invalid root or concept catalog");
        success = p101_error_has_no_error(err);
        goto done;
    }
    directory = p101_opendir(env, err, programs);
    if(directory == NULL)
    {
        goto done;
    }
    while(true)
    {
        entry = p101_readdir(env, err, directory);
        if(entry == NULL)
        {
            break;
        }
        comparison = p101_strncmp(env, entry->d_name, "p101-", sizeof("p101-") - 1U);
        if(comparison != 0)
        {
            continue;
        }
        p101_snprintf(env, err, readme, sizeof(readme), "%s/%s/README.md", programs, entry->d_name);
        valid = p101_workspace_audit_read_file(env, P101_ERROR_OPTIONAL, readme, &contents, &length);
        if(!valid)
        {
            quality_add(env, err, result, readme, entry->d_name, "has no readable README.md");
            continue;
        }
        for(size_t concept_index = 0U; concept_index < contract->tokens[concepts].child_count; concept_index++)
        {
            p101_workspace_json_array_get(contract, concepts, concept_index, &concept);
            valid   = quality_text(err, contract, concept, "id", concept_id, sizeof(concept_id));
            found   = p101_workspace_json_object_get(env, contract, concept, "patterns", &patterns);
            present = false;
            for(size_t pattern_index = 0U; found && pattern_index < contract->tokens[patterns].child_count && !present; pattern_index++)
            {
                p101_workspace_json_array_get(contract, patterns, pattern_index, &pattern);
                p101_workspace_json_token_copy(env, err, contract, pattern, pattern_text, sizeof(pattern_text));
                present = quality_contains_case_insensitive(contents, pattern_text);
            }
            if(!valid || !present)
            {
                quality_add(env, err, result, readme, entry->d_name, "documentation lacks a required concept");
            }
            result->checks++;
        }
        p101_free(env, contents);
        contents = NULL;
    }
    success      = p101_error_has_no_error(err);
    close_status = p101_closedir(env, err, directory);
    if(close_status != 0)
    {
        success = false;
    }

done:
    p101_free(env, contents);
    return success;
}

static void quality_add(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *section, const char *message)
{
    char text[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    int  written;

    written = p101_snprintf(env, err, text, sizeof(text), "%s: %s", section, message);
    if(written >= 0 && (size_t)written < sizeof(text))
    {
        p101_workspace_audit_add(env, err, result, path, text);
    }
}
