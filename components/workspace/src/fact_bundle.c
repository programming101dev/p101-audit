#include "workspace_audit.h"
#include "workspace_fact_bundle.h"
#include <errno.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_record/record.h>

enum
{
    BUNDLE_KIND            = 0,
    BUNDLE_PATH            = 1,
    BUNDLE_NAME            = 2,
    BUNDLE_USR             = 3,
    BUNDLE_CALLER_USR      = 4,
    BUNDLE_RESOLVED        = 5,
    BUNDLE_LINE            = 6,
    BUNDLE_REPLACEMENT     = 7,
    BUNDLE_IS_DEFINITION   = 8,
    BUNDLE_TYPE            = 9,
    BUNDLE_CANONICAL_TYPE  = 10,
    BUNDLE_RETURN_TYPE     = 11,
    BUNDLE_CALLER          = 12,
    BUNDLE_COLUMN          = 13,
    BUNDLE_START           = 14,
    BUNDLE_END             = 15,
    BUNDLE_PARAMETER_INDEX = 16,
    BUNDLE_IS_HEADER       = 17,
    BUNDLE_IS_STATIC       = 18,
    BUNDLE_IS_PUBLIC       = 19,
    BUNDLE_IS_VARIADIC     = 20,
    BUNDLE_IS_LOCAL        = 21,
    BUNDLE_IS_INDIRECT     = 22,
    BUNDLE_NEEDS_ENV       = 23,
    BUNDLE_NEEDS_ERROR     = 24,
    BUNDLE_PARENT_USR      = 25,
    BUNDLE_FIELD_COUNT     = 26
};

static enum p101_c_analysis_kind analysis_kind(const struct p101_env *env, const char *text);
static bool                      copy_bundle_field(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input);
static bool                      parse_bundle_size(const char *text, size_t *value);
static bool                      parse_bundle_flag(const char *text, bool *value);

bool p101_workspace_fact_bundle_load(const struct p101_env *env, struct p101_error *err, const char *path, struct p101_wrapper_model *model)
{
    char                     *text;
    char                     *line;
    char                     *next_line;
    char                     *records;
    char                     *cursor;
    const char               *line_end;
    char                     *fields[BUNDLE_FIELD_COUNT];
    struct p101_wrapper_fact *fact;
    size_t                    length;
    size_t                    count;
    size_t                    field_index;
    int                       comparison;
    bool                      parsed;
    bool                      loaded;
    bool                      copied;

    text   = NULL;
    length = 0U;
    loaded = p101_workspace_audit_read_file(env, err, path, &text, &length);
    if(!loaded)
    {
        goto done;
    }
    line     = text;
    line_end = p101_strchr(env, line, '\n');
    if(line_end == NULL)
    {
        P101_ERROR_RAISE_USER(err, "invalid semantic fact bundle", EINVAL);
        loaded = false;
        goto done;
    }
    next_line  = line + (size_t)(line_end - line);
    *next_line = '\0';
    comparison = p101_strcmp(env, line, "P101SEMANTIC\t3");
    if(comparison != 0)
    {
        P101_ERROR_RAISE_USER(err, "invalid semantic fact bundle", EINVAL);
        loaded = false;
        goto done;
    }
    count   = 0U;
    records = next_line + 1;
    line    = records;
    while(*line != '\0')
    {
        count++;
        line_end = p101_strchr(env, line, '\n');
        if(line_end == NULL)
        {
            break;
        }
        next_line = line + (size_t)(line_end - line);
        line      = next_line + 1;
    }
    model->facts = (struct p101_wrapper_fact *)p101_calloc(env, err, count, sizeof(*model->facts));
    if(model->facts == NULL)
    {
        loaded = false;
        goto done;
    }
    model->fact_capacity = count;
    line                 = records;
    while(*line != '\0')
    {
        line_end = p101_strchr(env, line, '\n');
        if(line_end != NULL)
        {
            next_line  = line + (size_t)(line_end - line);
            *next_line = '\0';
        }
        else
        {
            next_line = NULL;
        }
        cursor = line;
        for(field_index = 0U; field_index < BUNDLE_FIELD_COUNT; field_index++)
        {
            fields[field_index] = p101_record_split(&cursor);
            if(fields[field_index] != NULL)
            {
                p101_record_unescape_field(fields[field_index]);
            }
        }
        if(fields[BUNDLE_FIELD_COUNT - 1U] == NULL || cursor != NULL)
        {
            P101_ERROR_RAISE_USER(err, "invalid semantic fact record", EINVAL);
            loaded = false;
            goto done;
        }
        fact       = &model->facts[model->fact_count];
        fact->kind = analysis_kind(env, fields[BUNDLE_KIND]);
        copied     = copy_bundle_field(env, err, fact->path, sizeof(fact->path), fields[BUNDLE_PATH]);
        copied     = ((copy_bundle_field(env, err, fact->name, sizeof(fact->name), fields[BUNDLE_NAME]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->usr, sizeof(fact->usr), fields[BUNDLE_USR]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->caller_usr, sizeof(fact->caller_usr), fields[BUNDLE_CALLER_USR]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->resolved, sizeof(fact->resolved), fields[BUNDLE_RESOLVED]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->replacement, sizeof(fact->replacement), fields[BUNDLE_REPLACEMENT]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->type, sizeof(fact->type), fields[BUNDLE_TYPE]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->canonical_type, sizeof(fact->canonical_type), fields[BUNDLE_CANONICAL_TYPE]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->return_type, sizeof(fact->return_type), fields[BUNDLE_RETURN_TYPE]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->caller, sizeof(fact->caller), fields[BUNDLE_CALLER]) && copied) != 0);
        copied     = ((copy_bundle_field(env, err, fact->parent_usr, sizeof(fact->parent_usr), fields[BUNDLE_PARENT_USR]) && copied) != 0);
        parsed     = parse_bundle_size(fields[BUNDLE_LINE], &fact->line);
        parsed     = ((parse_bundle_size(fields[BUNDLE_COLUMN], &fact->column) && parsed) != 0);
        parsed     = ((parse_bundle_size(fields[BUNDLE_START], &fact->start) && parsed) != 0);
        parsed     = ((parse_bundle_size(fields[BUNDLE_END], &fact->end) && parsed) != 0);
        parsed     = ((parse_bundle_size(fields[BUNDLE_PARAMETER_INDEX], &fact->parameter_index) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_IS_DEFINITION], &fact->is_definition) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_IS_HEADER], &fact->is_header) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_IS_STATIC], &fact->is_static) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_IS_PUBLIC], &fact->is_public) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_IS_VARIADIC], &fact->is_variadic) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_IS_LOCAL], &fact->is_local_include) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_IS_INDIRECT], &fact->is_indirect) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_NEEDS_ENV], &fact->needs_env) && parsed) != 0);
        parsed     = ((parse_bundle_flag(fields[BUNDLE_NEEDS_ERROR], &fact->needs_error) && parsed) != 0);
        if(!copied || !parsed)
        {
            P101_ERROR_RAISE_USER(err, "invalid semantic fact field", EINVAL);
            loaded = false;
            goto done;
        }
        model->fact_count++;
        if(next_line == NULL)
        {
            break;
        }
        line = next_line + 1;
    }
    loaded = true;

done:
    p101_free(env, text);
    return loaded;
}

static enum p101_c_analysis_kind analysis_kind(const struct p101_env *env, const char *text)
{
    static const char *const  names[] = {"FILE", "INCLUDE", "FUNCTION", "PARAMETER", "CALL", "TYPE", "ENUM", "ENUMERATOR", "MACRO", "NOTE", "MUTATION", "DIAGNOSTIC"};
    size_t                    index;
    enum p101_c_analysis_kind kind;

    kind = P101_C_ANALYSIS_DIAGNOSTIC;
    for(index = 0U; index < sizeof(names) / sizeof(names[0]); index++)
    {
        int comparison;

        comparison = p101_strcmp(env, text, names[index]);
        if(comparison == 0)
        {
            kind = (enum p101_c_analysis_kind)index;
            break;
        }
    }
    return kind;
}

static bool copy_bundle_field(const struct p101_env *env, struct p101_error *err, char *output, size_t output_size, const char *input)
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

static bool parse_bundle_size(const char *text, size_t *value)
{
    int  status;
    bool parsed;

    status = p101_record_parse_size(text, value);
    parsed = status != 0;
    return parsed;
}

static bool parse_bundle_flag(const char *text, bool *value)
{
    size_t parsed_value;
    bool   parsed;

    parsed = parse_bundle_size(text, &parsed_value);
    if(parsed && parsed_value <= 1U)
    {
        *value = parsed_value != 0U;
    }
    else
    {
        parsed = false;
    }
    return parsed;
}
