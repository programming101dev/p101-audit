#include "workspace_analysis.h"
#include "workspace_audit.h"
#include <errno.h>
#include <limits.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_tool_support/diagnostic.h>

enum
{
    WORKSPACE_INITIAL_FINDING_CAPACITY = 16U,
    WORKSPACE_REPOSITORY_LINE_SIZE     = 8192U
};

static bool add_analysis_text(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_arguments *arguments, char storage[][P101_WORKSPACE_AUDIT_PATH_SIZE], size_t capacity, const char *text);
static bool add_analysis_include(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_arguments *arguments, char storage[][P101_WORKSPACE_AUDIT_PATH_SIZE], size_t capacity, const char *workspace, const char *destination,
                                 size_t destination_length);

void p101_workspace_audit_result_init(struct p101_workspace_audit_result *result)
{
    result->findings         = NULL;
    result->finding_count    = 0U;
    result->finding_capacity = 0U;
    result->checks           = 0U;
}

void p101_workspace_audit_result_destroy(const struct p101_env *env, struct p101_workspace_audit_result *result)
{
    P101_TRACE_SCOPE(env);
    p101_free(env, result->findings);
    p101_workspace_audit_result_init(result);
}

bool p101_workspace_audit_add(const struct p101_env *env, struct p101_error *err, struct p101_workspace_audit_result *result, const char *path, const char *message)
{
    struct p101_workspace_audit_finding *resized;
    struct p101_workspace_audit_finding *finding;
    size_t                               capacity;
    size_t                               path_length;
    size_t                               message_length;
    bool                                 added;

    P101_TRACE_SCOPE(env);
    added = false;
    if(result->finding_count == result->finding_capacity)
    {
        capacity = result->finding_capacity == 0U ? WORKSPACE_INITIAL_FINDING_CAPACITY : result->finding_capacity * 2U;
        if(capacity < result->finding_capacity || capacity > SIZE_MAX / sizeof(*result->findings))
        {
            P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
            goto done;
        }
        resized = (struct p101_workspace_audit_finding *)p101_realloc(env, err, result->findings, capacity * sizeof(*result->findings));
        if(resized == NULL)
        {
            goto done;
        }
        result->findings         = resized;
        result->finding_capacity = capacity;
    }
    path_length    = p101_strlen(env, path);
    message_length = p101_strlen(env, message);
    if(path_length >= sizeof(result->findings[0].path) || message_length >= sizeof(result->findings[0].message))
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    finding = &result->findings[result->finding_count];
    p101_memcpy(env, finding->path, path, path_length + 1U);
    p101_memcpy(env, finding->message, message, message_length + 1U);
    result->finding_count++;
    added = true;

done:
    return added;
}

bool p101_workspace_audit_join(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *left, const char *right)
{
    int  written;
    bool joined;

    P101_TRACE_SCOPE(env);
    written = p101_snprintf(env, err, output, output_size, "%s/%s", left, right);
    joined  = (written >= 0 && (size_t)written < output_size) != 0;
    if(!joined && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
    }
    return joined;
}

bool p101_workspace_audit_read_file(const struct p101_env *env, struct p101_error *err, const char *path, char **text, size_t *length)
{
    FILE  *stream;
    long   file_length;
    int    status;
    size_t amount;
    bool   read_ok;

    P101_TRACE_SCOPE(env);
    *text   = NULL;
    *length = 0U;
    read_ok = false;
    stream  = p101_fopen(env, err, path, "rb");
    if(stream == NULL)
    {
        goto done;
    }
    status = p101_fseek(env, err, stream, 0L, SEEK_END);
    if(status != 0)
    {
        goto close_stream;
    }
    file_length = p101_ftell(env, err, stream);
    if(file_length < 0 || (unsigned long)file_length > SIZE_MAX - 1U)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto close_stream;
    }
    status = p101_fseek(env, err, stream, 0L, SEEK_SET);
    if(status != 0)
    {
        goto close_stream;
    }
    *text = (char *)p101_malloc(env, err, (size_t)file_length + 1U);
    if(*text == NULL)
    {
        goto close_stream;
    }
    amount = p101_fread(env, err, *text, 1U, (size_t)file_length, stream);
    if(amount != (size_t)file_length)
    {
        if(p101_error_has_no_error(err))
        {
            P101_ERROR_RAISE_ERRNO(err, EIO);
        }
        goto close_stream;
    }
    (*text)[amount] = '\0';
    *length         = amount;
    read_ok         = true;

close_stream:
    status = p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    if(status != 0 && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
        read_ok = false;
    }

done:
    if(!read_ok)
    {
        p101_free(env, *text);
        *text   = NULL;
        *length = 0U;
    }
    return read_ok;
}

bool p101_workspace_audit_file_exists(const struct p101_env *env, struct p101_error *err, const char *path)
{
    struct stat status_buffer;
    int         status;
    bool        exists;

    P101_TRACE_SCOPE(env);
    status = p101_stat(env, err, path, &status_buffer);
    exists = status == 0;
    if(!exists && p101_error_is_errno(err, ENOENT))
    {
        p101_error_reset(err);
    }
    return exists;
}

void p101_workspace_audit_write(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, const struct p101_workspace_audit_result *result)
{
    struct p101_tool_diagnostic diagnostic;
    FILE                       *human_stream;
    FILE                       *json_stream;
    size_t                      index;
    int                         status;

    P101_TRACE_SCOPE(env);
    human_stream = (options->outputs & P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN) != 0U ? stderr : NULL;
    json_stream  = (options->outputs & P101_TOOL_DIAGNOSTIC_OUTPUT_JSON) != 0U ? stdout : NULL;
    for(index = 0U; index < result->finding_count; index++)
    {
        diagnostic.id            = "P101-WORKSPACE";
        diagnostic.severity      = P101_TOOL_DIAGNOSTIC_ERROR;
        diagnostic.path          = result->findings[index].path;
        diagnostic.line          = 1U;
        diagnostic.column        = 1U;
        diagnostic.function_name = NULL;
        diagnostic.message       = result->findings[index].message;
        diagnostic.lesson_id     = NULL;
        diagnostic.lesson_path   = NULL;
        diagnostic.lesson_url    = NULL;
        status                   = p101_tool_diagnostic_write_outputs(human_stream, json_stream, &diagnostic);
        if(status != 0)
        {
            P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
            break;
        }
        if(json_stream != NULL)
        {
            p101_fputc(env, err, '\n', json_stream);
        }
    }
    if(human_stream != NULL)
    {
        p101_fprintf(env, err, stdout, "workspace audit: %zu checks, %zu findings\n", result->checks, result->finding_count);
    }
}

bool p101_workspace_audit_prepare_analysis(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_wrapper_arguments *arguments, char storage[][P101_WORKSPACE_AUDIT_PATH_SIZE], size_t capacity)
{
    char        repositories_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char        line[WORKSPACE_REPOSITORY_LINE_SIZE];
    FILE       *stream;
    char       *read_result;
    const char *first_separator;
    const char *second_separator;
    const char *destination;
    size_t      destination_length;
    int         close_status;
    bool        joined;
    bool        added;
    bool        prepared;

    P101_TRACE_SCOPE(env);
    prepared = false;
#ifdef __APPLE__
    added = add_analysis_text(env, err, arguments, storage, capacity, "-D_DARWIN_C_SOURCE");
#elif defined(__FreeBSD__)
    added = add_analysis_text(env, err, arguments, storage, capacity, "-D_BSD_SOURCE");
    if(added)
    {
        added = add_analysis_text(env, err, arguments, storage, capacity, "-D__BSD_VISIBLE");
    }
#elif defined(__linux__)
    added = add_analysis_text(env, err, arguments, storage, capacity, "-D_GNU_SOURCE");
#else
    added = true;
#endif
    if(!added)
    {
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, repositories_path, sizeof(repositories_path), options->scripts_root, "repos.txt");
    if(!joined)
    {
        goto done;
    }
    stream = p101_fopen(env, err, repositories_path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    while(true)
    {
        read_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(read_result == NULL)
        {
            break;
        }
        first_separator = p101_strchr(env, line, '|');
        if(first_separator == NULL)
        {
            continue;
        }
        destination      = first_separator + 1;
        second_separator = p101_strchr(env, destination, '|');
        if(second_separator == NULL)
        {
            continue;
        }
        destination_length = (size_t)(second_separator - destination);
        if(destination_length <= sizeof("../libraries/") - 1U)
        {
            continue;
        }
        if(p101_strncmp(env, destination, "../libraries/", sizeof("../libraries/") - 1U) != 0)
        {
            continue;
        }
        destination += sizeof("../") - 1U;
        destination_length -= sizeof("../") - 1U;
        added = add_analysis_include(env, err, arguments, storage, capacity, options->workspace, destination, destination_length);
        if(!added)
        {
            goto close_stream;
        }
    }
    prepared = p101_error_has_no_error(err);

close_stream:
    close_status = p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    if(close_status != 0 && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
        prepared = false;
    }

done:
    return prepared;
}

static bool add_analysis_text(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_arguments *arguments, char storage[][P101_WORKSPACE_AUDIT_PATH_SIZE], size_t capacity, const char *text)
{
    size_t index;
    size_t length;
    bool   added;

    added = false;
    index = arguments->extra_argument_count;
    if(index >= capacity || index >= P101_WRAPPER_MAX_NAMES)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    length = p101_strlen(env, text);
    if(length >= P101_WORKSPACE_AUDIT_PATH_SIZE)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    p101_memcpy(env, storage[index], text, length + 1U);
    arguments->extra_arguments[index] = storage[index];
    arguments->extra_argument_count++;
    added = true;

done:
    return added;
}

static bool add_analysis_include(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_arguments *arguments, char storage[][P101_WORKSPACE_AUDIT_PATH_SIZE], size_t capacity, const char *workspace, const char *destination,
                                 size_t destination_length)
{
    size_t index;
    int    written;
    bool   added;

    added = false;
    index = arguments->extra_argument_count;
    if(index >= capacity || index >= P101_WRAPPER_MAX_NAMES || destination_length > INT_MAX)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    written = p101_snprintf(env, err, storage[index], P101_WORKSPACE_AUDIT_PATH_SIZE, "-I%s/%.*s/include", workspace, (int)destination_length, destination);
    if(written < 0 || (size_t)written >= P101_WORKSPACE_AUDIT_PATH_SIZE)
    {
        if(p101_error_has_no_error(err))
        {
            P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        }
        goto done;
    }
    arguments->extra_arguments[index] = storage[index];
    arguments->extra_argument_count++;
    added = true;

done:
    return added;
}
