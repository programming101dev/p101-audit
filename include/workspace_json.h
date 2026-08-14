#ifndef P101_AUDIT_WORKSPACE_JSON_H
#define P101_AUDIT_WORKSPACE_JSON_H

#include <p101_env/env.h>
#include <p101_json/json.h>

/*
 * The workspace audit retains this narrow adapter so its file-loading policy
 * stays local. Tokenization and JSON navigation are owned by p101_record.
 */
#define p101_workspace_json p101_json
#define p101_workspace_json_token p101_json_token
#define p101_workspace_json_kind p101_json_kind
#define P101_WORKSPACE_JSON_OBJECT P101_JSON_OBJECT
#define P101_WORKSPACE_JSON_ARRAY P101_JSON_ARRAY
#define P101_WORKSPACE_JSON_STRING P101_JSON_STRING
#define P101_WORKSPACE_JSON_PRIMITIVE P101_JSON_PRIMITIVE

#define p101_workspace_json_init(document) p101_json_init((document))
#define p101_workspace_json_destroy(env, document) p101_json_destroy((document))
#define p101_workspace_json_object_get(env, document, object_index, key, value_index) p101_json_object_get((document), (object_index), (key), (value_index))
#define p101_workspace_json_token_equals(env, document, token_index, value) p101_json_token_equals((document), (token_index), (value))
#define p101_workspace_json_token_copy(env, err, document, token_index, output, output_size) p101_json_token_copy((err), (document), (token_index), (output), (output_size))
#define p101_workspace_json_token_size(env, document, token_index, value) p101_json_token_size((document), (token_index), (value))
#define p101_workspace_json_array_get(document, array_index, element_index, value_index) p101_json_array_get((document), (array_index), (element_index), (value_index))

bool p101_workspace_json_load(const struct p101_env *env, struct p101_error *err, const char *path, struct p101_json *document);

#endif
