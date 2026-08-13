#ifndef P101_AUDIT_WORKSPACE_JSON_H
#define P101_AUDIT_WORKSPACE_JSON_H

#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

enum p101_workspace_json_kind
{
    P101_WORKSPACE_JSON_OBJECT,
    P101_WORKSPACE_JSON_ARRAY,
    P101_WORKSPACE_JSON_STRING,
    P101_WORKSPACE_JSON_PRIMITIVE
};

struct p101_workspace_json_token
{
    enum p101_workspace_json_kind kind;
    size_t                        start;
    size_t                        end;
    size_t                        parent;
    size_t                        child_count;
};

struct p101_workspace_json
{
    char                             *text;
    size_t                            text_size;
    struct p101_workspace_json_token *tokens;
    size_t                            token_count;
    size_t                            token_capacity;
};

void p101_workspace_json_init(struct p101_workspace_json *document);
void p101_workspace_json_destroy(const struct p101_env *env, struct p101_workspace_json *document);
bool p101_workspace_json_load(const struct p101_env *env, struct p101_error *err, const char *path, struct p101_workspace_json *document);
bool p101_workspace_json_object_get(const struct p101_env *env, const struct p101_workspace_json *document, size_t object_index, const char *key, size_t *value_index);
bool p101_workspace_json_token_equals(const struct p101_env *env, const struct p101_workspace_json *document, size_t token_index, const char *value);
bool p101_workspace_json_token_copy(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t token_index, char *output, size_t output_size);
bool p101_workspace_json_token_size(const struct p101_env *env, const struct p101_workspace_json *document, size_t token_index, size_t *value);
bool p101_workspace_json_array_get(const struct p101_workspace_json *document, size_t array_index, size_t element_index, size_t *value_index);

#endif
