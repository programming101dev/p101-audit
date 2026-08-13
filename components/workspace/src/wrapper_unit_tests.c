#include "model.h"
#include "workspace_audit.h"
#include "workspace_fact_bundle.h"
#include "workspace_json.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_record/record.h>

enum
{
    UNIT_API_CAPACITY  = 2048,
    UNIT_FIELD_COUNT   = 16,
    UNIT_LINE_CAPACITY = 16384
};

struct unit_api
{
    char library[P101_WRAPPER_NAME_SIZE];
    char name[P101_WRAPPER_NAME_SIZE];
    char usr[P101_WRAPPER_NAME_SIZE];
    char source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char test_source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char test_kind[64];
    bool tested;
};

static size_t unit_split(const struct p101_env *env, char *line, char **fields, size_t capacity);
static bool   unit_copy(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input);
static bool   unit_load_api_manifest(const struct p101_env *env, struct p101_error *err, const char *library, const char *path, struct unit_api *apis, size_t *api_count);
static bool   unit_load_test_manifest(const struct p101_env *env, struct p101_error *err, const char *library, const char *repo, const char *path, struct unit_api *apis, size_t api_count, struct p101_workspace_audit_result *result);
static bool   unit_has_call(const struct p101_env *env, const struct p101_wrapper_model *model, const char *path, const char *usr);
static void   unit_add(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *library, const char *name, const char *message);

bool p101_workspace_audit_run_wrapper_unit_tests(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    struct p101_workspace_json contract;
    struct p101_wrapper_model  model;
    struct unit_api           *apis;
    char                       contract_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       repo[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       library[P101_WRAPPER_NAME_SIZE];
    char                       role[P101_WRAPPER_NAME_SIZE];
    size_t                     roles;
    size_t                     key = 0U;
    size_t                     value;
    size_t                     child;
    size_t                     api_count;
    bool                       joined;
    bool                       loaded;
    bool                       valid;
    bool                       success;
    int                        comparison;

    p101_workspace_json_init(&contract);
    p101_wrapper_model_init(&model);
    apis      = NULL;
    api_count = 0U;
    success   = false;
    if(options->facts_path == NULL)
    {
        P101_ERROR_RAISE_USER(err, "wrapper unit-test policy requires --facts", EINVAL);
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
    valid = p101_workspace_json_object_get(env, &contract, 0U, "schema", &value);
    valid = valid && p101_workspace_json_token_equals(env, &contract, value, "p101-instrumentation-contract-v3");
    valid = p101_workspace_json_object_get(env, &contract, 0U, "library_roles", &roles) && valid;
    if(!valid || contract.tokens[roles].kind != P101_WORKSPACE_JSON_OBJECT)
    {
        unit_add(env, err, result, contract_path, "contract", "library_roles", "is invalid");
        success = p101_error_has_no_error(err);
        goto done;
    }
    apis = (struct unit_api *)p101_calloc(env, err, UNIT_API_CAPACITY, sizeof(*apis));
    if(apis == NULL)
    {
        goto done;
    }
    loaded = p101_workspace_fact_bundle_load(env, err, options->facts_path, &model);
    if(!loaded)
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
            value      = index;
            valid      = p101_workspace_json_token_copy(env, err, &contract, key, library, sizeof(library));
            valid      = p101_workspace_json_token_copy(env, err, &contract, value, role, sizeof(role)) && valid;
            comparison = valid ? p101_strcmp(env, role, "infrastructure") : 0;
            if(valid && comparison != 0)
            {
                int written;

                written = p101_snprintf(env, err, repo, sizeof(repo), "%s/libraries/%s", options->workspace, library);
                valid   = written >= 0 && (size_t)written < sizeof(repo);
                written = valid ? p101_snprintf(env, err, path, sizeof(path), "%s/api-manifest.tsv", repo) : -1;
                valid   = valid && written >= 0 && (size_t)written < sizeof(path);
                if(!valid || !unit_load_api_manifest(env, err, library, path, apis, &api_count))
                {
                    unit_add(env, err, result, path, library, "API manifest", "is missing or invalid");
                }
                written = p101_snprintf(env, err, path, sizeof(path), "%s/test/unit-test-manifest.tsv", repo);
                valid   = written >= 0 && (size_t)written < sizeof(path);
                if(!valid || !unit_load_test_manifest(env, err, library, repo, path, apis, api_count, result))
                {
                    unit_add(env, err, result, path, library, "unit-test manifest", "is missing or invalid");
                }
            }
        }
        child++;
    }
    for(size_t index = 0U; index < api_count; index++)
    {
        char absolute[P101_WORKSPACE_AUDIT_PATH_SIZE];
        int  written;

        if(!apis[index].tested)
        {
            unit_add(env, err, result, apis[index].source, apis[index].library, apis[index].name, "has no unit-test manifest row");
            continue;
        }
        written = p101_snprintf(env, err, absolute, sizeof(absolute), "%s/libraries/%s/%s", options->workspace, apis[index].library, apis[index].test_source);
        if(written < 0 || (size_t)written >= sizeof(absolute) || !unit_has_call(env, &model, absolute, apis[index].usr))
        {
            unit_add(env, err, result, absolute, apis[index].library, apis[index].name, "test source has no resolved call to its public declaration identity");
        }
        result->checks++;
    }
    success = p101_error_has_no_error(err);

done:
    p101_free(env, apis);
    p101_wrapper_model_destroy(env, &model);
    p101_workspace_json_destroy(env, &contract);
    return success;
}

static size_t unit_split(const struct p101_env *env, char *line, char **fields, size_t capacity)
{
    char       *cursor;
    const char *found;
    char       *line_end;
    size_t      count;

    found = p101_strchr(env, line, '\n');
    if(found != NULL)
    {
        line_end  = line + (size_t)(found - line);
        *line_end = '\0';
    }
    found = p101_strchr(env, line, '\r');
    if(found != NULL)
    {
        line_end  = line + (size_t)(found - line);
        *line_end = '\0';
    }
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

static bool unit_copy(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input)
{
    size_t length;

    length = p101_strlen(env, input);
    if(length >= output_size)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        return false;
    }
    p101_memcpy(env, output, input, length + 1U);
    return true;
}

static bool unit_load_api_manifest(const struct p101_env *env, struct p101_error *err, const char *library, const char *path, struct unit_api *apis, size_t *api_count)
{
    FILE  *stream;
    char   line[UNIT_LINE_CAPACITY];
    size_t function_column;
    size_t usr_column;
    size_t source_column;
    size_t column_count;
    bool   loaded;

    loaded = false;
    stream = p101_fopen(env, P101_ERROR_OPTIONAL, path, "r");
    if(stream == NULL)
    {
        return false;
    }
    if(p101_fgets(env, P101_ERROR_OPTIONAL, line, sizeof(line), stream) == NULL)
    {
        goto done;
    }
    {
        char *columns[UNIT_FIELD_COUNT];

        function_column = UNIT_FIELD_COUNT;
        usr_column      = UNIT_FIELD_COUNT;
        source_column   = UNIT_FIELD_COUNT;
        column_count    = unit_split(env, line, columns, UNIT_FIELD_COUNT);
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
        if(function_column == UNIT_FIELD_COUNT || usr_column == UNIT_FIELD_COUNT || source_column == UNIT_FIELD_COUNT)
        {
            goto done;
        }
    }
    while(p101_fgets(env, P101_ERROR_OPTIONAL, line, sizeof(line), stream) != NULL)
    {
        char  *fields[UNIT_FIELD_COUNT];
        size_t count;
        bool   copied;

        count = unit_split(env, line, fields, UNIT_FIELD_COUNT);
        if(count != column_count || *api_count == UNIT_API_CAPACITY)
        {
            goto done;
        }
        copied = unit_copy(env, err, apis[*api_count].library, sizeof(apis[*api_count].library), library);
        copied = unit_copy(env, err, apis[*api_count].name, sizeof(apis[*api_count].name), fields[function_column]) && copied;
        copied = unit_copy(env, err, apis[*api_count].usr, sizeof(apis[*api_count].usr), fields[usr_column]) && copied;
        copied = unit_copy(env, err, apis[*api_count].source, sizeof(apis[*api_count].source), fields[source_column]) && copied;
        if(!copied || apis[*api_count].usr[0] == '\0')
        {
            goto done;
        }
        (*api_count)++;
    }
    loaded = true;

done:
    p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    return loaded;
}

static bool unit_load_test_manifest(const struct p101_env *env, struct p101_error *err, const char *library, const char *repo, const char *path, struct unit_api *apis, size_t api_count, struct p101_workspace_audit_result *result)
{
    FILE *stream;
    char  line[UNIT_LINE_CAPACITY];
    bool  loaded;

    loaded = false;
    stream = p101_fopen(env, P101_ERROR_OPTIONAL, path, "r");
    if(stream == NULL)
    {
        return false;
    }
    if(p101_fgets(env, P101_ERROR_OPTIONAL, line, sizeof(line), stream) == NULL)
    {
        goto done;
    }
    while(p101_fgets(env, P101_ERROR_OPTIONAL, line, sizeof(line), stream) != NULL)
    {
        char  *fields[4];
        size_t count;
        size_t match;
        bool   found;
        bool   copied;
        int    comparison;

        count = unit_split(env, line, fields, 4U);
        if(count != 4U)
        {
            goto done;
        }
        found = false;
        match = api_count;
        for(size_t index = 0U; index < api_count && !found; index++)
        {
            int library_comparison;

            library_comparison = p101_strcmp(env, apis[index].library, library);
            comparison         = p101_strcmp(env, apis[index].usr, fields[1]);
            if(library_comparison == 0 && comparison == 0)
            {
                found = true;
                match = index;
            }
        }
        if(!found)
        {
            unit_add(env, err, result, path, library, fields[0], "test row has no public API");
            continue;
        }
        if(apis[match].tested)
        {
            unit_add(env, err, result, path, library, fields[0], "has duplicate unit-test rows");
            continue;
        }
        comparison = p101_strcmp(env, fields[2], "fault");
        if(comparison != 0)
        {
            comparison = p101_strcmp(env, fields[2], "behavior");
        }
        if(comparison != 0)
        {
            comparison = p101_strcmp(env, fields[2], "behavior-existing");
        }
        if(comparison != 0)
        {
            unit_add(env, err, result, path, library, fields[0], "has an unknown unit-test kind");
            continue;
        }
        copied = unit_copy(env, err, apis[match].test_kind, sizeof(apis[match].test_kind), fields[2]);
        copied = unit_copy(env, err, apis[match].test_source, sizeof(apis[match].test_source), fields[3]) && copied;
        if(copied)
        {
            char absolute[P101_WORKSPACE_AUDIT_PATH_SIZE];
            int  written;

            written = p101_snprintf(env, err, absolute, sizeof(absolute), "%s/%s", repo, fields[3]);
            if(written < 0 || (size_t)written >= sizeof(absolute) || !p101_workspace_audit_file_exists(env, err, absolute))
            {
                unit_add(env, err, result, path, library, fields[0], "names a missing test source");
            }
            apis[match].tested = true;
        }
    }
    loaded = true;

done:
    p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    return loaded;
}

static bool unit_has_call(const struct p101_env *env, const struct p101_wrapper_model *model, const char *path, const char *usr)
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
        found           = fact->kind == P101_C_ANALYSIS_CALL && path_comparison == 0 && usr_comparison == 0;
    }
    return found;
}

static void unit_add(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *library, const char *name, const char *message)
{
    char text[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    int  written;

    written = p101_snprintf(env, err, text, sizeof(text), "%s:%s: %s", library, name, message);
    if(written >= 0 && (size_t)written < sizeof(text))
    {
        p101_workspace_audit_add(env, err, result, path, text);
    }
}
