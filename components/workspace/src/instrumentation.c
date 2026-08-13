#include "instrumentation.h"
#include "output.h"
#include "workspace_audit.h"
#include "workspace_fact_bundle.h"
#include "workspace_json.h"
#include "workspace_sha256.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_record/record.h>

enum
{
    INSTRUMENTATION_API_CAPACITY  = 2048,
    INSTRUMENTATION_FIELD_COUNT   = 16,
    INSTRUMENTATION_LINE_CAPACITY = 16384
};

struct instrumentation_api
{
    char                                     library[P101_WRAPPER_NAME_SIZE];
    char                                     role[P101_WRAPPER_NAME_SIZE];
    char                                     name[P101_WRAPPER_NAME_SIZE];
    char                                     usr[P101_WRAPPER_NAME_SIZE];
    char                                     source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    size_t                                   function_fact;
    bool                                     has_env;
    bool                                     has_error;
    struct p101_instrumentation_capabilities capabilities;
};

struct instrumentation_api_slot
{
    const char *usr;
    size_t      api_index_plus_one;
};

static size_t instrumentation_split(char *line, char **fields, size_t capacity);
static bool   instrumentation_copy(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input);
static bool   instrumentation_load_manifest(const struct p101_env *env, struct p101_error *err, const char *workspace, const char *library, const char *role, struct instrumentation_api *apis, size_t *api_count);
static size_t instrumentation_hash(const char *text);
static bool   instrumentation_api_insert(const struct p101_env *env, struct instrumentation_api_slot *slots, size_t capacity, const struct instrumentation_api *apis, size_t api_index);
static size_t instrumentation_api_lookup(const struct p101_env *env, const struct instrumentation_api_slot *slots, size_t capacity, const char *usr, size_t missing);
static bool   instrumentation_assign_facts(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_instrumentation_capabilities *fact_capabilities, struct instrumentation_api *apis, size_t api_count);
static bool instrumentation_validate_required(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *contract, size_t required, const struct instrumentation_api *apis, size_t api_count, struct p101_workspace_audit_result *result,
                                              const char *contract_path);
static bool instrumentation_capability(const struct p101_env *env, const struct p101_instrumentation_capabilities *capabilities, const char *name, bool *known);
static void instrumentation_add(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *library, const char *name, const char *message);
static bool instrumentation_write_receipt(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_workspace_json *contract, size_t roles, const struct instrumentation_api *apis,
                                          size_t api_count, size_t required_count, const struct p101_workspace_audit_result *result);

bool p101_workspace_audit_run_instrumentation(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    struct p101_workspace_json                contract;
    struct p101_wrapper_model                 model;
    struct instrumentation_api               *apis;
    struct p101_instrumentation_capabilities *fact_capabilities;
    char                                      contract_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                                      library[P101_WRAPPER_NAME_SIZE];
    char                                      role[P101_WRAPPER_NAME_SIZE];
    size_t                                    schema;
    size_t                                    roles;
    size_t                                    required;
    size_t                                    key;
    size_t                                    child;
    size_t                                    api_count;
    size_t                                    required_count;
    bool                                      joined;
    bool                                      loaded;
    bool                                      valid;
    bool                                      success;
    int                                       comparison;

    p101_workspace_json_init(&contract);
    p101_wrapper_model_init(&model);
    apis              = NULL;
    fact_capabilities = NULL;
    key               = 0U;
    api_count         = 0U;
    required_count    = 0U;
    success           = false;
    if(options->facts_path == NULL)
    {
        P101_ERROR_RAISE_USER(err, "instrumentation policy requires --facts", EINVAL);
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, contract_path, sizeof(contract_path), options->scripts_root, "contracts/instrumentation-contract.json");
    if(!joined)
    {
        goto done;
    }
    loaded = p101_workspace_json_load(env, err, contract_path, &contract);
    if(!loaded)
    {
        goto done;
    }
    valid = p101_workspace_json_object_get(env, &contract, 0U, "schema", &schema);
    valid = valid && p101_workspace_json_token_equals(env, &contract, schema, "p101-instrumentation-contract-v3");
    valid = p101_workspace_json_object_get(env, &contract, 0U, "library_roles", &roles) && valid;
    valid = p101_workspace_json_object_get(env, &contract, 0U, "required", &required) && valid;
    if(!valid || contract.tokens[roles].kind != P101_WORKSPACE_JSON_OBJECT || contract.tokens[required].kind != P101_WORKSPACE_JSON_OBJECT)
    {
        instrumentation_add(env, err, result, contract_path, "contract", "schema", "is invalid");
        success = p101_error_has_no_error(err);
        goto done;
    }
    apis = (struct instrumentation_api *)p101_calloc(env, err, INSTRUMENTATION_API_CAPACITY, sizeof(*apis));
    if(apis == NULL)
    {
        goto done;
    }
    child = 0U;
    for(size_t index = roles + 1U; index < contract.token_count && contract.tokens[index].start < contract.tokens[roles].end; index++)
    {
        if(contract.tokens[index].parent != roles)
        {
            continue;
        }
        if((child % 2U) == 0U)
        {
            key = index;
        }
        else
        {
            valid = p101_workspace_json_token_copy(env, err, &contract, key, library, sizeof(library));
            valid = p101_workspace_json_token_copy(env, err, &contract, index, role, sizeof(role)) && valid;
            if(!valid)
            {
                goto done;
            }
            comparison = p101_strcmp(env, role, "infrastructure");
            if(comparison != 0)
            {
                comparison = p101_strcmp(env, role, "native-wrapper");
                valid      = comparison == 0;
                comparison = p101_strcmp(env, role, "traced-api");
                valid      = valid || comparison == 0;
                if(!valid)
                {
                    instrumentation_add(env, err, result, contract_path, library, role, "is not a recognized library role");
                }
                else if(!instrumentation_load_manifest(env, err, options->workspace, library, role, apis, &api_count))
                {
                    instrumentation_add(env, err, result, contract_path, library, "API manifest", "is missing or invalid");
                }
            }
        }
        child++;
    }
    loaded = p101_workspace_fact_bundle_load(env, err, options->facts_path, &model);
    if(!loaded)
    {
        goto done;
    }
    fact_capabilities = (struct p101_instrumentation_capabilities *)p101_calloc(env, err, model.fact_count, sizeof(*fact_capabilities));
    if(fact_capabilities == NULL && model.fact_count > 0U)
    {
        goto done;
    }
    loaded = p101_instrumentation_collect(env, err, &model, fact_capabilities);
    if(!loaded)
    {
        goto done;
    }
    loaded = instrumentation_assign_facts(env, err, &model, fact_capabilities, apis, api_count);
    if(!loaded)
    {
        goto done;
    }
    for(size_t index = 0U; index < api_count; index++)
    {
        if(apis[index].function_fact == model.fact_count)
        {
            instrumentation_add(env, err, result, apis[index].source, apis[index].library, apis[index].name, "has no public definition on this platform");
            continue;
        }
        if(model.facts[apis[index].function_fact].needs_env && (!apis[index].capabilities.trace_entry || !apis[index].capabilities.trace_exit))
        {
            instrumentation_add(env, err, result, apis[index].source, apis[index].library, apis[index].name, "lacks balanced entry/exit tracing");
        }
        comparison = p101_strcmp(env, apis[index].role, "native-wrapper");
        if(comparison == 0 && model.facts[apis[index].function_fact].needs_error && !apis[index].capabilities.fault)
        {
            instrumentation_add(env, err, result, apis[index].source, apis[index].library, apis[index].name, "has an error contract but no fault-injection point");
        }
        result->checks++;
    }
    required_count = contract.tokens[required].child_count / 2U;
    loaded         = instrumentation_validate_required(env, err, &contract, required, apis, api_count, result, contract_path);
    if(!loaded)
    {
        goto done;
    }
    loaded = instrumentation_write_receipt(env, err, options, &contract, roles, apis, api_count, required_count, result);
    if(!loaded)
    {
        goto done;
    }
    success = p101_error_has_no_error(err);

done:
    p101_free(env, fact_capabilities);
    p101_free(env, apis);
    p101_wrapper_model_destroy(env, &model);
    p101_workspace_json_destroy(env, &contract);
    return success;
}

static size_t instrumentation_split(char *line, char **fields, size_t capacity)
{
    char  *cursor;
    size_t count;

    cursor = line;
    count  = 0U;
    while(count < capacity && cursor != NULL)
    {
        fields[count] = p101_record_split(&cursor);
        if(fields[count] != NULL)
        {
            p101_record_unescape_field(fields[count]);
            count++;
        }
    }
    return count;
}

static bool instrumentation_copy(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input)
{
    size_t length;
    bool   copied;

    length = p101_strlen(env, input);
    copied = length < output_size;
    if(copied)
    {
        p101_memcpy(env, output, input, length + 1U);
    }
    else
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
    }
    return copied;
}

static bool instrumentation_load_manifest(const struct p101_env *env, struct p101_error *err, const char *workspace, const char *library, const char *role, struct instrumentation_api *apis, size_t *api_count)
{
    FILE  *stream;
    char   path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char   line[INSTRUMENTATION_LINE_CAPACITY];
    int    written;
    size_t function_column;
    size_t usr_column;
    size_t source_column;
    size_t column_count;
    bool   loaded;

    loaded  = false;
    written = p101_snprintf(env, err, path, sizeof(path), "%s/libraries/%s/api-manifest.tsv", workspace, library);
    if(written < 0 || (size_t)written >= sizeof(path))
    {
        goto done;
    }
    stream = p101_fopen(env, P101_ERROR_OPTIONAL, path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    if(p101_fgets(env, P101_ERROR_OPTIONAL, line, sizeof(line), stream) == NULL)
    {
        goto close_stream;
    }
    {
        char *columns[INSTRUMENTATION_FIELD_COUNT];

        function_column = INSTRUMENTATION_FIELD_COUNT;
        usr_column      = INSTRUMENTATION_FIELD_COUNT;
        source_column   = INSTRUMENTATION_FIELD_COUNT;
        column_count    = instrumentation_split(line, columns, INSTRUMENTATION_FIELD_COUNT);
        for(size_t index = 0U; index < column_count; index++)
        {
            int comparison;

            comparison = p101_strcmp(env, columns[index], "function");
            if(comparison == 0)
            {
                function_column = index;
            }
            comparison = p101_strcmp(env, columns[index], "function_usr");
            if(comparison == 0)
            {
                usr_column = index;
            }
            comparison = p101_strcmp(env, columns[index], "current_source");
            if(comparison == 0)
            {
                source_column = index;
            }
        }
        if(function_column == INSTRUMENTATION_FIELD_COUNT || usr_column == INSTRUMENTATION_FIELD_COUNT || source_column == INSTRUMENTATION_FIELD_COUNT)
        {
            goto close_stream;
        }
    }
    while(p101_fgets(env, P101_ERROR_OPTIONAL, line, sizeof(line), stream) != NULL)
    {
        char  *fields[INSTRUMENTATION_FIELD_COUNT];
        size_t count;
        bool   copied;

        count = instrumentation_split(line, fields, INSTRUMENTATION_FIELD_COUNT);
        if(count != column_count || *api_count == INSTRUMENTATION_API_CAPACITY)
        {
            goto close_stream;
        }
        copied = instrumentation_copy(env, err, apis[*api_count].library, sizeof(apis[*api_count].library), library);
        copied = instrumentation_copy(env, err, apis[*api_count].role, sizeof(apis[*api_count].role), role) && copied;
        copied = instrumentation_copy(env, err, apis[*api_count].name, sizeof(apis[*api_count].name), fields[function_column]) && copied;
        copied = instrumentation_copy(env, err, apis[*api_count].usr, sizeof(apis[*api_count].usr), fields[usr_column]) && copied;
        copied = instrumentation_copy(env, err, apis[*api_count].source, sizeof(apis[*api_count].source), fields[source_column]) && copied;
        if(!copied || apis[*api_count].usr[0] == '\0')
        {
            goto close_stream;
        }
        (*api_count)++;
    }
    loaded = true;

close_stream:
    p101_fclose(env, P101_ERROR_OPTIONAL, stream);

done:
    return loaded;
}

static size_t instrumentation_hash(const char *text)
{
    size_t        hash;
    unsigned char byte;

    hash = 5381U;
    while(*text != '\0')
    {
        byte = (unsigned char)*text;
        if(hash > (SIZE_MAX - byte) / 33U)
        {
            hash %= 104729U;
        }
        hash = hash * 33U + byte;
        text++;
    }
    return hash;
}

static bool instrumentation_api_insert(const struct p101_env *env, struct instrumentation_api_slot *slots, size_t capacity, const struct instrumentation_api *apis, size_t api_index)
{
    size_t slot;
    bool   inserted;
    int    comparison;

    slot     = instrumentation_hash(apis[api_index].usr) & (capacity - 1U);
    inserted = false;
    for(size_t probe = 0U; probe < capacity; probe++)
    {
        if(slots[slot].usr == NULL)
        {
            slots[slot].usr                = apis[api_index].usr;
            slots[slot].api_index_plus_one = api_index + 1U;
            inserted                       = true;
            break;
        }
        comparison = p101_strcmp(env, slots[slot].usr, apis[api_index].usr);
        if(comparison == 0)
        {
            break;
        }
        slot = (slot + 1U) & (capacity - 1U);
    }
    return inserted;
}

static size_t instrumentation_api_lookup(const struct p101_env *env, const struct instrumentation_api_slot *slots, size_t capacity, const char *usr, size_t missing)
{
    size_t slot;
    size_t found;
    int    comparison;

    found = missing;
    if(usr[0] == '\0')
    {
        goto done;
    }
    slot = instrumentation_hash(usr) & (capacity - 1U);
    for(size_t probe = 0U; probe < capacity; probe++)
    {
        if(slots[slot].usr == NULL)
        {
            break;
        }
        comparison = p101_strcmp(env, slots[slot].usr, usr);
        if(comparison == 0)
        {
            found = slots[slot].api_index_plus_one - 1U;
            break;
        }
        slot = (slot + 1U) & (capacity - 1U);
    }

done:
    return found;
}

static bool instrumentation_assign_facts(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_instrumentation_capabilities *fact_capabilities, struct instrumentation_api *apis, size_t api_count)
{
    struct instrumentation_api_slot *slots;
    const struct p101_wrapper_fact  *fact;
    size_t                           capacity;
    size_t                           api_index;
    bool                             inserted;
    bool                             assigned;
    int                              comparison;

    slots    = NULL;
    capacity = 16U;
    assigned = false;
    while(capacity < api_count * 2U)
    {
        capacity *= 2U;
    }
    slots = (struct instrumentation_api_slot *)p101_calloc(env, err, capacity, sizeof(*slots));
    if(slots == NULL)
    {
        goto done;
    }
    for(size_t index = 0U; index < api_count; index++)
    {
        apis[index].function_fact = model->fact_count;
        inserted                  = instrumentation_api_insert(env, slots, capacity, apis, index);
        if(!inserted)
        {
            P101_ERROR_RAISE_USER(err, "duplicate wrapper declaration identity", EINVAL);
            goto done;
        }
    }
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_FUNCTION || !fact->is_definition || !fact->is_public)
        {
            continue;
        }
        api_index = instrumentation_api_lookup(env, slots, capacity, fact->usr, api_count);
        if(api_index == api_count)
        {
            continue;
        }
        comparison = p101_strcmp(env, apis[api_index].source, fact->path);
        if(comparison != 0)
        {
            size_t fact_path_length;
            size_t source_length;
            size_t workspace_length;

            fact_path_length = p101_strlen(env, fact->path);
            source_length    = p101_strlen(env, apis[api_index].source);
            if(fact_path_length >= source_length)
            {
                workspace_length = fact_path_length - source_length;
                comparison       = p101_strcmp(env, fact->path + workspace_length, apis[api_index].source);
            }
        }
        if(comparison == 0)
        {
            apis[api_index].function_fact = index;
            apis[api_index].capabilities  = fact_capabilities[index];
            apis[api_index].has_env       = fact->needs_env || fact_capabilities[index].trace_entry || fact_capabilities[index].trace_exit;
            apis[api_index].has_error     = fact->needs_error;
        }
    }
    assigned = true;

done:
    p101_free(env, slots);
    return assigned;
}

static bool instrumentation_validate_required(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *contract, size_t required, const struct instrumentation_api *apis, size_t api_count, struct p101_workspace_audit_result *result,
                                              const char *contract_path)
{
    struct instrumentation_api_slot *slots;
    char                             usr[P101_WRAPPER_NAME_SIZE];
    char                             capability[P101_WRAPPER_NAME_SIZE];
    size_t                           capacity;
    size_t                           key;
    size_t                           child;
    size_t                           capabilities;
    size_t                           token;
    size_t                           api_index;
    bool                             inserted;
    bool                             valid;
    bool                             known;
    bool                             present;
    bool                             checked;
    int                              comparison;

    slots    = NULL;
    capacity = 16U;
    key      = 0U;
    checked  = false;
    while(capacity < api_count * 2U)
    {
        capacity *= 2U;
    }
    slots = (struct instrumentation_api_slot *)p101_calloc(env, err, capacity, sizeof(*slots));
    if(slots == NULL)
    {
        goto done;
    }
    for(size_t index = 0U; index < api_count; index++)
    {
        inserted = instrumentation_api_insert(env, slots, capacity, apis, index);
        if(!inserted)
        {
            P101_ERROR_RAISE_USER(err, "duplicate wrapper declaration identity", EINVAL);
            goto done;
        }
    }
    child = 0U;
    for(size_t index = required + 1U; index < contract->token_count && contract->tokens[index].start < contract->tokens[required].end; index++)
    {
        if(contract->tokens[index].parent != required)
        {
            continue;
        }
        if((child % 2U) == 0U)
        {
            key = index;
        }
        else
        {
            valid = p101_workspace_json_token_copy(env, err, contract, key, usr, sizeof(usr));
            valid = p101_workspace_json_object_get(env, contract, index, "capabilities", &capabilities) && valid;
            if(!valid || contract->tokens[capabilities].kind != P101_WORKSPACE_JSON_ARRAY)
            {
                instrumentation_add(env, err, result, contract_path, "contract", usr, "has invalid required capabilities");
                child++;
                continue;
            }
            api_index = instrumentation_api_lookup(env, slots, capacity, usr, api_count);
            if(api_index == api_count)
            {
                instrumentation_add(env, err, result, contract_path, "contract", usr, "required wrapper was not found");
                child++;
                continue;
            }
            comparison = p101_strcmp(env, apis[api_index].role, "native-wrapper");
            if(comparison != 0)
            {
                instrumentation_add(env, err, result, contract_path, apis[api_index].library, apis[api_index].name, "required capability belongs to a non-wrapper library");
            }
            for(size_t capability_index = 0U; capability_index < contract->tokens[capabilities].child_count; capability_index++)
            {
                valid = p101_workspace_json_array_get(contract, capabilities, capability_index, &token);
                valid = valid && p101_workspace_json_token_copy(env, err, contract, token, capability, sizeof(capability));
                if(!valid)
                {
                    goto done;
                }
                present = instrumentation_capability(env, &apis[api_index].capabilities, capability, &known);
                if(!known)
                {
                    instrumentation_add(env, err, result, contract_path, "contract", usr, "names an unknown instrumentation capability");
                }
                else if(!present)
                {
                    instrumentation_add(env, err, result, apis[api_index].source, apis[api_index].library, apis[api_index].name, "lacks a required instrumentation capability");
                }
                result->checks++;
            }
        }
        child++;
    }
    checked = true;

done:
    p101_free(env, slots);
    return checked;
}

static bool instrumentation_capability(const struct p101_env *env, const struct p101_instrumentation_capabilities *capabilities, const char *name, bool *known)
{
    bool value;
    int  comparison;

    *known     = true;
    value      = false;
    comparison = p101_strcmp(env, name, "fault");
    if(comparison == 0)
    {
        value = capabilities->fault;
    }
    else
    {
        comparison = p101_strcmp(env, name, "fd");
        if(comparison == 0)
        {
            value = capabilities->fd;
        }
        else
        {
            comparison = p101_strcmp(env, name, "allocation");
            if(comparison == 0)
            {
                value = capabilities->allocation;
            }
            else
            {
                comparison = p101_strcmp(env, name, "resource");
                if(comparison == 0)
                {
                    value = capabilities->resource;
                }
                else
                {
                    *known = false;
                }
            }
        }
    }
    return value;
}

static void instrumentation_add(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *library, const char *name, const char *message)
{
    char text[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    int  written;

    written = p101_snprintf(env, err, text, sizeof(text), "%s: %s: %s", library, name, message);
    if(written >= 0 && (size_t)written < sizeof(text))
    {
        p101_workspace_audit_add(env, err, result, path, text);
    }
}

static bool instrumentation_write_receipt(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_workspace_json *contract, size_t roles, const struct instrumentation_api *apis,
                                          size_t api_count, size_t required_count, const struct p101_workspace_audit_result *result)
{
    FILE       *stream;
    const char *platform;
    const char *machine;
    const char *separator;
    char        contract_sha256[65];
    char        library[P101_WRAPPER_NAME_SIZE];
    size_t      child;
    size_t      key;
    bool        written;

    written = true;
    if(options->receipt_path == NULL)
    {
        goto done;
    }
#ifdef __APPLE__
    platform = "Darwin";
#elif defined(__FreeBSD__)
    platform = "FreeBSD";
#elif defined(__linux__)
    platform = "Linux";
#else
    platform = "Unknown";
#endif
#if defined(__aarch64__) || defined(__arm64__)
    machine = "arm64";
#elif defined(__x86_64__)
    machine = "x86_64";
#else
    machine = "unknown";
#endif
    stream = p101_fopen(env, err, options->receipt_path, "w");
    if(stream == NULL)
    {
        written = false;
        goto done;
    }
    p101_fputs(env, err, "{\"schema\":\"p101-instrumentation-platform-receipt-v1\",\"platform\":", stream);
    p101_wrapper_output_json_string(env, err, stream, platform);
    p101_fputs(env, err, ",\"machine\":", stream);
    p101_wrapper_output_json_string(env, err, stream, machine);
    p101_workspace_sha256((const unsigned char *)contract->text, contract->text_size, contract_sha256);
    p101_fputs(env, err, ",\"contract_sha256\":", stream);
    p101_wrapper_output_json_string(env, err, stream, contract_sha256);
    p101_fputs(env, err, ",\"classified_libraries\":[", stream);
    separator = "";
    child     = 0U;
    key       = 0U;
    for(size_t index = roles + 1U; index < contract->token_count && contract->tokens[index].start < contract->tokens[roles].end; index++)
    {
        if(contract->tokens[index].parent != roles)
        {
            continue;
        }
        if((child % 2U) == 0U)
        {
            key = index;
        }
        else
        {
            p101_workspace_json_token_copy(env, err, contract, key, library, sizeof(library));
            p101_fputs(env, err, separator, stream);
            p101_wrapper_output_json_string(env, err, stream, library);
            separator = ",";
        }
        child++;
    }
    p101_fputs(env, err, "],\"functions\":[", stream);
    separator = "";
    for(size_t index = 0U; index < api_count; index++)
    {
        p101_fputs(env, err, separator, stream);
        p101_fputc(env, err, '"', stream);
        p101_fprintf(env, err, stream, "%s:%s:0:%s", apis[index].library, apis[index].source, apis[index].usr);
        p101_fputc(env, err, '"', stream);
        separator = ",";
    }
    p101_fputs(env, err, "],\"function_capabilities\":[", stream);
    separator = "";
    for(size_t index = 0U; index < api_count; index++)
    {
        const char *has_env;
        const char *has_error;
        const char *trace_entry;
        const char *trace_exit;
        const char *fault;
        const char *fd;
        const char *allocation;
        const char *resource;

        has_env     = p101_wrapper_output_json_bool_text(apis[index].has_env);
        has_error   = p101_wrapper_output_json_bool_text(apis[index].has_error);
        trace_entry = p101_wrapper_output_json_bool_text(apis[index].capabilities.trace_entry);
        trace_exit  = p101_wrapper_output_json_bool_text(apis[index].capabilities.trace_exit);
        fault       = p101_wrapper_output_json_bool_text(apis[index].capabilities.fault);
        fd          = p101_wrapper_output_json_bool_text(apis[index].capabilities.fd);
        allocation  = p101_wrapper_output_json_bool_text(apis[index].capabilities.allocation);
        resource    = p101_wrapper_output_json_bool_text(apis[index].capabilities.resource);
        p101_fputs(env, err, separator, stream);
        p101_fputs(env, err, "{\"library\":", stream);
        p101_wrapper_output_json_string(env, err, stream, apis[index].library);
        p101_fputs(env, err, ",\"function\":", stream);
        p101_wrapper_output_json_string(env, err, stream, apis[index].name);
        p101_fputs(env, err, ",\"usr\":", stream);
        p101_wrapper_output_json_string(env, err, stream, apis[index].usr);
        p101_fprintf(env, err, stream, ",\"has_env\":%s,\"has_error\":%s,\"trace_entry\":%s,\"trace_exit\":%s,\"fault\":%s,\"fd\":%s,\"allocation\":%s,\"resource\":%s}", has_env, has_error, trace_entry, trace_exit, fault, fd, allocation, resource);
        separator = ",";
    }
    p101_fprintf(env, err, stream, "],\"explicit_capability_contracts\":%zu,\"failures\":[", required_count);
    separator = "";
    for(size_t index = 0U; index < result->finding_count; index++)
    {
        p101_fputs(env, err, separator, stream);
        p101_wrapper_output_json_string(env, err, stream, result->findings[index].message);
        separator = ",";
    }
    p101_fputs(env, err, "],\"passed\":", stream);
    p101_fputs(env, err, result->finding_count == 0U ? "true" : "false", stream);
    p101_fputs(env, err, "}\n", stream);
    p101_fclose(env, err, stream);
    written = p101_error_has_no_error(err);

done:
    return written;
}
