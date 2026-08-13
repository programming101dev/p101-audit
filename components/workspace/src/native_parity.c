#include "model.h"
#include "workspace_analysis.h"
#include "workspace_audit.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_stdlib.h>

enum
{
    PARITY_FIELD_COUNT = 6,
    PARITY_LINE_SIZE   = 16384,
    PARITY_MAX_ROWS    = 512,
    PARITY_LIBRARY     = 0,
    PARITY_WRAPPER     = 1,
    PARITY_NATIVE      = 2,
    PARITY_SOURCE      = 3,
    PARITY_WRAPPER_USR = 4,
    PARITY_NATIVE_USR  = 5
};

struct parity_row
{
    char library[P101_WRAPPER_NAME_SIZE];
    char wrapper[P101_WRAPPER_NAME_SIZE];
    char native[P101_WRAPPER_NAME_SIZE];
    char source[P101_WRAPPER_PATH_SIZE];
    char wrapper_usr[P101_WRAPPER_NAME_SIZE];
    char native_usr[P101_WRAPPER_NAME_SIZE];
};

static size_t split_fields(char *line, char **fields, size_t capacity);
static bool   copy_text(const struct p101_env *env, struct p101_error *err, char *destination, size_t capacity, const char *source);
static bool   manifest_has_identity(const struct p101_env *env, struct p101_error *err, const char *path, const char *identity);
static bool   row_has_pair(const struct p101_env *env, const struct p101_wrapper_model *model, const struct parity_row *row);

bool p101_workspace_audit_run_native_parity(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    char                          contract_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                          relative_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                          manifest_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                          resolved_source[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                          line[PARITY_LINE_SIZE];
    char                         *fields[PARITY_FIELD_COUNT];
    FILE                         *stream;
    struct parity_row             rows[PARITY_MAX_ROWS];
    size_t                        row_count;
    size_t                        field_count;
    size_t                        index;
    size_t                        other;
    bool                          joined;
    bool                          copied;
    bool                          present;
    bool                          duplicate;
    bool                          loaded;
    bool                          scanned;
    bool                          success;
    struct p101_wrapper_arguments arguments;
    struct p101_wrapper_model     model;
    char                          message[P101_WORKSPACE_AUDIT_MESSAGE_SIZE];
    char                          analysis_arguments[P101_WORKSPACE_ANALYSIS_ARGUMENT_CAPACITY][P101_WORKSPACE_AUDIT_PATH_SIZE];
    int                           written;
    int                           close_status;
    bool                          prepared;

    P101_TRACE_SCOPE(env);
    row_count = 0U;
    success   = false;
    p101_wrapper_model_init(&model);
    p101_memset(env, &arguments, 0, sizeof(arguments));
    joined = p101_workspace_audit_join(env, err, contract_path, sizeof(contract_path), options->scripts_root, "contracts/native-wrapper-parity.tsv");
    if(!joined)
    {
        goto done;
    }
    stream = p101_fopen(env, err, contract_path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    loaded = p101_fgets(env, err, line, sizeof(line), stream) != NULL;
    if(!loaded)
    {
        p101_workspace_audit_add(env, err, result, contract_path, "native parity contract contains no header");
        goto close_stream;
    }
    while(row_count < PARITY_MAX_ROWS)
    {
        char *read_result;

        read_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(read_result == NULL)
        {
            break;
        }
        field_count = split_fields(line, fields, PARITY_FIELD_COUNT);
        if(field_count != PARITY_FIELD_COUNT)
        {
            p101_workspace_audit_add(env, err, result, contract_path, "native parity contract has an incomplete row");
            continue;
        }
        present = true;
        for(index = 0U; index < PARITY_FIELD_COUNT; index++)
        {
            if(fields[index][0] == '\0')
            {
                present = false;
            }
        }
        if(!present)
        {
            p101_workspace_audit_add(env, err, result, contract_path, "native parity contract has an incomplete row");
            continue;
        }
        written = p101_strcmp(env, fields[PARITY_WRAPPER_USR], fields[PARITY_NATIVE_USR]);
        if(written == 0)
        {
            p101_workspace_audit_add(env, err, result, contract_path, "native parity wrapper and native identities match");
            continue;
        }
        copied = copy_text(env, err, rows[row_count].library, sizeof(rows[row_count].library), fields[PARITY_LIBRARY]);
        if(copied)
        {
            copied = copy_text(env, err, rows[row_count].wrapper, sizeof(rows[row_count].wrapper), fields[PARITY_WRAPPER]);
        }
        if(copied)
        {
            copied = copy_text(env, err, rows[row_count].native, sizeof(rows[row_count].native), fields[PARITY_NATIVE]);
        }
        written = p101_snprintf(env, err, relative_path, sizeof(relative_path), "libraries/%s/%s", fields[PARITY_LIBRARY], fields[PARITY_SOURCE]);
        if(written < 0 || (size_t)written >= sizeof(relative_path))
        {
            copied = false;
        }
        if(copied)
        {
            copied = p101_workspace_audit_join(env, err, rows[row_count].source, sizeof(rows[row_count].source), options->workspace, relative_path);
        }
        if(copied)
        {
            char *resolved;

            resolved = p101_realpath(env, err, rows[row_count].source, resolved_source);
            copied   = resolved != NULL;
        }
        if(copied)
        {
            copied = copy_text(env, err, rows[row_count].source, sizeof(rows[row_count].source), resolved_source);
        }
        if(copied)
        {
            copied = copy_text(env, err, rows[row_count].wrapper_usr, sizeof(rows[row_count].wrapper_usr), fields[PARITY_WRAPPER_USR]);
        }
        if(copied)
        {
            copied = copy_text(env, err, rows[row_count].native_usr, sizeof(rows[row_count].native_usr), fields[PARITY_NATIVE_USR]);
        }
        if(!copied)
        {
            goto close_stream;
        }
        duplicate = false;
        for(other = 0U; other < row_count; other++)
        {
            int library_comparison;
            int identity_comparison;

            library_comparison  = p101_strcmp(env, rows[other].library, rows[row_count].library);
            identity_comparison = p101_strcmp(env, rows[other].wrapper_usr, rows[row_count].wrapper_usr);
            if(library_comparison == 0 && identity_comparison == 0)
            {
                duplicate = true;
            }
        }
        if(duplicate)
        {
            p101_workspace_audit_add(env, err, result, contract_path, "native parity contract duplicates a wrapper identity");
            continue;
        }
        present = p101_workspace_audit_file_exists(env, err, rows[row_count].source);
        if(!present)
        {
            written = p101_snprintf(env, err, message, sizeof(message), "%s:%s: missing %s", fields[PARITY_LIBRARY], fields[PARITY_WRAPPER], fields[PARITY_SOURCE]);
            if(written >= 0 && (size_t)written < sizeof(message))
            {
                p101_workspace_audit_add(env, err, result, rows[row_count].source, message);
            }
            continue;
        }
        written = p101_snprintf(env, err, relative_path, sizeof(relative_path), "libraries/%s/test/unit-test-manifest.tsv", fields[PARITY_LIBRARY]);
        if(written < 0 || (size_t)written >= sizeof(relative_path))
        {
            goto close_stream;
        }
        joined = p101_workspace_audit_join(env, err, manifest_path, sizeof(manifest_path), options->workspace, relative_path);
        if(!joined)
        {
            goto close_stream;
        }
        present = manifest_has_identity(env, err, manifest_path, rows[row_count].wrapper_usr);
        if(!present && p101_error_has_no_error(err))
        {
            written = p101_snprintf(env, err, message, sizeof(message), "%s:%s: identity %s is absent from unit-test manifest", rows[row_count].library, rows[row_count].wrapper, rows[row_count].wrapper_usr);
            if(written >= 0 && (size_t)written < sizeof(message))
            {
                p101_workspace_audit_add(env, err, result, manifest_path, message);
            }
        }
        arguments.paths[arguments.path_count] = rows[row_count].source;
        arguments.path_count++;
        row_count++;
    }
    if(row_count == PARITY_MAX_ROWS)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
    }

close_stream:
    close_status = p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    if(close_status != 0 && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
    if(p101_error_has_error(err))
    {
        goto done;
    }
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
    for(index = 0U; index < row_count; index++)
    {
        present = row_has_pair(env, &model, &rows[index]);
        if(!present)
        {
            written = p101_snprintf(env, err, message, sizeof(message), "%s:%s: %s has no resolved test function that invokes both %s and %s", rows[index].library, rows[index].wrapper, rows[index].source, rows[index].wrapper_usr, rows[index].native_usr);
            if(written >= 0 && (size_t)written < sizeof(message))
            {
                p101_workspace_audit_add(env, err, result, rows[index].source, message);
            }
        }
        result->checks++;
    }
    success = p101_error_has_no_error(err);

done:
    p101_wrapper_model_destroy(env, &model);
    return success;
}

static size_t split_fields(char *line, char **fields, size_t capacity)
{
    size_t count;
    char  *cursor;

    count  = 0U;
    cursor = line;
    while(count < capacity)
    {
        fields[count] = cursor;
        count++;
        while(*cursor != '\0' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r')
        {
            cursor++;
        }
        if(*cursor != '\t')
        {
            *cursor = '\0';
            break;
        }
        *cursor = '\0';
        cursor++;
    }
    return count;
}

static bool copy_text(const struct p101_env *env, struct p101_error *err, char *destination, size_t capacity, const char *source)
{
    size_t length;
    bool   copied;

    length = p101_strlen(env, source);
    copied = length < capacity;
    if(copied)
    {
        p101_memcpy(env, destination, source, length + 1U);
    }
    else
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
    }
    return copied;
}

static bool manifest_has_identity(const struct p101_env *env, struct p101_error *err, const char *path, const char *identity)
{
    FILE *stream;
    char  line[PARITY_LINE_SIZE];
    bool  present;
    int   close_status;

    present = false;
    stream  = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    while(!present)
    {
        char  *read_result;
        char  *fields[4];
        size_t field_count;
        int    comparison;

        read_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(read_result == NULL)
        {
            break;
        }
        field_count = split_fields(line, fields, sizeof(fields) / sizeof(fields[0]));
        if(field_count >= 2U)
        {
            comparison = p101_strcmp(env, fields[1], identity);
            present    = comparison == 0;
        }
    }
    close_status = p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    if(close_status != 0 && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
        present = false;
    }

done:
    return present;
}

static bool row_has_pair(const struct p101_env *env, const struct p101_wrapper_model *model, const struct parity_row *row)
{
    size_t caller_index;
    size_t fact_index;
    bool   wrapper_seen;
    bool   native_seen;
    bool   matched;
    int    comparison;

    matched = false;
    for(caller_index = 0U; caller_index < model->fact_count && !matched; caller_index++)
    {
        const struct p101_wrapper_fact *caller_fact;

        caller_fact = &model->facts[caller_index];
        if(caller_fact->caller_usr[0] == '\0')
        {
            continue;
        }
        comparison = p101_strcmp(env, caller_fact->path, row->source);
        if(comparison != 0)
        {
            continue;
        }
        wrapper_seen = false;
        native_seen  = false;
        for(fact_index = 0U; fact_index < model->fact_count; fact_index++)
        {
            const struct p101_wrapper_fact *fact;
            int                             caller_comparison;

            fact              = &model->facts[fact_index];
            caller_comparison = p101_strcmp(env, fact->caller_usr, caller_fact->caller_usr);
            if(caller_comparison != 0)
            {
                continue;
            }
            comparison = p101_strcmp(env, fact->usr, row->wrapper_usr);
            if(fact->kind == P101_C_ANALYSIS_CALL && comparison == 0)
            {
                wrapper_seen = true;
            }
            comparison = p101_strcmp(env, fact->usr, row->native_usr);
            if(fact->kind == P101_C_ANALYSIS_CALL && comparison == 0)
            {
                native_seen = true;
            }
            comparison = p101_strcmp(env, fact->replacement, row->native);
            if(fact->kind == P101_C_ANALYSIS_MACRO && !fact->is_definition && comparison == 0)
            {
                native_seen = true;
            }
            comparison = p101_strcmp(env, fact->name, row->native);
            if(fact->kind == P101_C_ANALYSIS_MACRO && !fact->is_definition && comparison == 0)
            {
                native_seen = true;
            }
        }
        matched = (wrapper_seen && native_seen) != 0;
    }
    return matched;
}
