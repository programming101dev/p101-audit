#include "workspace_audit.h"
#include "workspace_json.h"
#include <p101_c/p101_stdlib.h>

bool p101_workspace_json_load(const struct p101_env *env, struct p101_error *err, const char *path, struct p101_json *document)
{
    char  *text;
    size_t text_size;
    bool   loaded;
    bool   parsed;

    P101_TRACE_SCOPE(env);
    text      = NULL;
    text_size = 0U;
    loaded    = p101_workspace_audit_read_file(env, err, path, &text, &text_size);
    parsed    = false;
    if(loaded)
    {
        parsed = p101_json_parse(err, text, text_size, document);
    }
    p101_free(env, text);
    if(!parsed)
    {
        p101_json_destroy(document);
    }
    return parsed;
}
