#include "workspace_audit.h"
#include "workspace_json.h"
#include <errno.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>

enum
{
    JSON_INITIAL_TOKEN_CAPACITY = 256U
};

static const size_t JSON_NO_PARENT = SIZE_MAX;

static bool json_add_token(const struct p101_env *env, struct p101_error *err, struct p101_workspace_json *document, enum p101_workspace_json_kind kind, size_t start, size_t parent, size_t *token_index);
static bool json_parse(const struct p101_env *env, struct p101_error *err, struct p101_workspace_json *document);
static bool json_is_space(char character);
static bool json_is_delimiter(char character);

void p101_workspace_json_init(struct p101_workspace_json *document)
{
    document->text           = NULL;
    document->text_size      = 0U;
    document->tokens         = NULL;
    document->token_count    = 0U;
    document->token_capacity = 0U;
}

void p101_workspace_json_destroy(const struct p101_env *env, struct p101_workspace_json *document)
{
    P101_TRACE_SCOPE(env);
    p101_free(env, document->tokens);
    p101_free(env, document->text);
    p101_workspace_json_init(document);
}

bool p101_workspace_json_load(const struct p101_env *env, struct p101_error *err, const char *path, struct p101_workspace_json *document)
{
    bool loaded;
    bool parsed;

    P101_TRACE_SCOPE(env);
    p101_workspace_json_destroy(env, document);
    loaded = p101_workspace_audit_read_file(env, err, path, &document->text, &document->text_size);
    parsed = false;
    if(loaded)
    {
        parsed = json_parse(env, err, document);
    }
    if(!parsed)
    {
        p101_workspace_json_destroy(env, document);
    }
    return parsed;
}

bool p101_workspace_json_object_get(const struct p101_env *env, const struct p101_workspace_json *document, size_t object_index, const char *key, size_t *value_index)
{
    size_t index;
    size_t child;
    bool   key_matches;
    bool   found;

    found = false;
    if(object_index >= document->token_count || document->tokens[object_index].kind != P101_WORKSPACE_JSON_OBJECT)
    {
        goto done;
    }
    child = 0U;
    for(index = object_index + 1U; index < document->token_count && document->tokens[index].start < document->tokens[object_index].end; index++)
    {
        if(document->tokens[index].parent != object_index)
        {
            continue;
        }
        if((child % 2U) == 0U)
        {
            key_matches = p101_workspace_json_token_equals(env, document, index, key);
            if(key_matches && index + 1U < document->token_count && document->tokens[index + 1U].parent == object_index)
            {
                *value_index = index + 1U;
                found        = true;
                break;
            }
        }
        child++;
    }

done:
    return found;
}

bool p101_workspace_json_token_equals(const struct p101_env *env, const struct p101_workspace_json *document, size_t token_index, const char *value)
{
    size_t token_size;
    size_t value_size;
    int    comparison;
    bool   equal;

    equal = false;
    if(token_index >= document->token_count)
    {
        goto done;
    }
    token_size = document->tokens[token_index].end - document->tokens[token_index].start;
    value_size = p101_strlen(env, value);
    if(token_size == value_size)
    {
        comparison = p101_strncmp(env, document->text + document->tokens[token_index].start, value, token_size);
        equal      = comparison == 0;
    }

done:
    return equal;
}

bool p101_workspace_json_token_copy(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t token_index, char *output, size_t output_size)
{
    size_t length;
    bool   copied;

    copied = false;
    if(token_index >= document->token_count || document->tokens[token_index].kind != P101_WORKSPACE_JSON_STRING)
    {
        goto done;
    }
    length = document->tokens[token_index].end - document->tokens[token_index].start;
    if(length >= output_size)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    p101_memcpy(env, output, document->text + document->tokens[token_index].start, length);
    output[length] = '\0';
    copied         = true;

done:
    return copied;
}

bool p101_workspace_json_token_size(const struct p101_env *env, const struct p101_workspace_json *document, size_t token_index, size_t *value)
{
    char               text[64];
    char              *end;
    unsigned long long parsed;
    bool               copied;
    bool               valid;

    copied = false;
    if(token_index < document->token_count && document->tokens[token_index].kind == P101_WORKSPACE_JSON_PRIMITIVE)
    {
        size_t length;

        length = document->tokens[token_index].end - document->tokens[token_index].start;
        if(length < sizeof(text))
        {
            p101_memcpy(env, text, document->text + document->tokens[token_index].start, length);
            text[length] = '\0';
            copied       = true;
        }
    }
    valid = false;
    if(copied)
    {
        errno  = 0;
        parsed = p101_strtoull(env, P101_ERROR_OPTIONAL, text, &end, 10);
        valid  = errno == 0 && end != text && *end == '\0' && parsed <= SIZE_MAX;
        if(valid)
        {
            *value = (size_t)parsed;
        }
    }
    return valid;
}

bool p101_workspace_json_array_get(const struct p101_workspace_json *document, size_t array_index, size_t element_index, size_t *value_index)
{
    size_t index;
    size_t child;
    bool   found;

    found = false;
    if(array_index >= document->token_count || document->tokens[array_index].kind != P101_WORKSPACE_JSON_ARRAY)
    {
        goto done;
    }
    child = 0U;
    for(index = array_index + 1U; index < document->token_count && document->tokens[index].start < document->tokens[array_index].end; index++)
    {
        if(document->tokens[index].parent == array_index)
        {
            if(child == element_index)
            {
                *value_index = index;
                found        = true;
                break;
            }
            child++;
        }
    }

done:
    return found;
}

static bool json_add_token(const struct p101_env *env, struct p101_error *err, struct p101_workspace_json *document, enum p101_workspace_json_kind kind, size_t start, size_t parent, size_t *token_index)
{
    struct p101_workspace_json_token *tokens;
    size_t                            capacity;
    bool                              added;

    added = false;
    if(document->token_count == document->token_capacity)
    {
        capacity = document->token_capacity == 0U ? JSON_INITIAL_TOKEN_CAPACITY : document->token_capacity * 2U;
        if(capacity < document->token_capacity || capacity > SIZE_MAX / sizeof(*document->tokens))
        {
            P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
            goto done;
        }
        tokens = (struct p101_workspace_json_token *)p101_realloc(env, err, document->tokens, capacity * sizeof(*document->tokens));
        if(tokens == NULL)
        {
            goto done;
        }
        document->tokens         = tokens;
        document->token_capacity = capacity;
    }
    *token_index                               = document->token_count;
    document->tokens[*token_index].kind        = kind;
    document->tokens[*token_index].start       = start;
    document->tokens[*token_index].end         = start;
    document->tokens[*token_index].parent      = parent;
    document->tokens[*token_index].child_count = 0U;
    document->token_count++;
    if(parent != JSON_NO_PARENT)
    {
        document->tokens[parent].child_count++;
    }
    added = true;

done:
    return added;
}

static bool json_parse(const struct p101_env *env, struct p101_error *err, struct p101_workspace_json *document)
{
    size_t index;
    size_t parent;
    size_t token_index;
    size_t cursor;
    char   character;
    bool   escaped;
    bool   added;
    bool   parsed;

    parent = JSON_NO_PARENT;
    parsed = true;
    cursor = 0U;
    while(cursor < document->text_size && parsed)
    {
        character = document->text[cursor];
        if(json_is_space(character))
        {
            cursor++;
            continue;
        }
        if(character == '{' || character == '[')
        {
            added = json_add_token(env, err, document, character == '{' ? P101_WORKSPACE_JSON_OBJECT : P101_WORKSPACE_JSON_ARRAY, cursor, parent, &token_index);
            if(!added)
            {
                parsed = false;
            }
            else
            {
                parent = token_index;
                cursor++;
            }
            continue;
        }
        if(character == '}' || character == ']')
        {
            if(parent == JSON_NO_PARENT)
            {
                parsed = false;
                break;
            }
            if((character == '}' && document->tokens[parent].kind != P101_WORKSPACE_JSON_OBJECT) || (character == ']' && document->tokens[parent].kind != P101_WORKSPACE_JSON_ARRAY))
            {
                parsed = false;
                break;
            }
            document->tokens[parent].end = cursor + 1U;
            parent                       = document->tokens[parent].parent;
            cursor++;
            continue;
        }
        if(character == '"')
        {
            cursor++;
            added = json_add_token(env, err, document, P101_WORKSPACE_JSON_STRING, cursor, parent, &token_index);
            if(!added)
            {
                parsed = false;
                continue;
            }
            escaped = false;
            while(cursor < document->text_size)
            {
                character = document->text[cursor];
                if(!escaped && character == '"')
                {
                    break;
                }
                if(!escaped && character == '\\')
                {
                    escaped = true;
                }
                else
                {
                    escaped = false;
                }
                cursor++;
            }
            if(cursor >= document->text_size)
            {
                parsed = false;
            }
            else
            {
                document->tokens[token_index].end = cursor;
                cursor++;
            }
            continue;
        }
        if(character == ':' || character == ',')
        {
            cursor++;
            continue;
        }
        index = cursor;
        while(cursor < document->text_size && !json_is_delimiter(document->text[cursor]))
        {
            cursor++;
        }
        if(index == cursor)
        {
            parsed = false;
            continue;
        }
        added = json_add_token(env, err, document, P101_WORKSPACE_JSON_PRIMITIVE, index, parent, &token_index);
        if(!added)
        {
            parsed = false;
        }
        else
        {
            document->tokens[token_index].end = cursor;
        }
    }
    if(parsed && (parent != JSON_NO_PARENT || document->token_count == 0U))
    {
        parsed = false;
    }
    if(!parsed && p101_error_has_no_error(err))
    {
        P101_ERROR_RAISE_USER(err, "invalid JSON contract", EINVAL);
    }
    return parsed;
}

static bool json_is_space(char character)
{
    bool is_space;

    is_space = (character == ' ' || character == '\t' || character == '\n' || character == '\r') != 0;
    return is_space;
}

static bool json_is_delimiter(char character)
{
    bool is_delimiter;

    is_delimiter = json_is_space(character);
    if(!is_delimiter)
    {
        is_delimiter = (character == ',' || character == ']' || character == '}') != 0;
    }
    return is_delimiter;
}
