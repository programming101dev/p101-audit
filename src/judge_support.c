#include "judge_support.h"
#include <p101_c/p101_string.h>

static bool caller_is_declared_wrapper(const struct p101_env *env, const struct p101_wrapper_fact *call, const struct p101_wrapper_inventory *wrapper)
{
    int  comparison;
    bool declared;

    P101_TRACE_SCOPE(env);
    declared = false;
    if(wrapper == NULL || wrapper->wrapper_usr[0] == '\0' || call->caller_usr[0] == '\0')
    {
        goto done;
    }
    comparison = p101_strcmp(env, wrapper->wrapper_usr, call->caller_usr);
    if(comparison == 0)
    {
        declared = true;
    }

done:
    return declared;
}

bool p101_wrapper_is_wrapper_implementation(const struct p101_env *env, const struct p101_wrapper_fact *call, const struct p101_wrapper_inventory *wrapper)
{
    static const struct
    {
        const char *lowered_usr;
        const char *wrapper_usr;
    } aliases[] = {
        {"c:@F@__xpg_basename", "c:@F@p101_basename"},
        {"c:@F@fgetc",          "c:@F@p101_getc"    },
        {"c:@F@fgetc",          "c:@F@p101_getchar" },
        {"c:@F@fgetwc",         "c:@F@p101_getwc"   },
        {"c:@F@fgetwc",         "c:@F@p101_getwchar"},
        {"c:@F@fputc",          "c:@F@p101_putc"    },
        {"c:@F@fputc",          "c:@F@p101_putchar" },
        {"c:@F@fputwc",         "c:@F@p101_putwc"   },
        {"c:@F@fputwc",         "c:@F@p101_putwchar"},
    };

    size_t index;
    bool   implementation;
    bool   declared;

    P101_TRACE_SCOPE(env);
    implementation = false;
    declared       = caller_is_declared_wrapper(env, call, wrapper);
    if(declared)
    {
        implementation = true;
        goto done;
    }
    for(index = 0U; index < sizeof(aliases) / sizeof(aliases[0]); index++)
    {
        int lowered_comparison;
        int wrapper_comparison;

        lowered_comparison = p101_strcmp(env, call->usr, aliases[index].lowered_usr);
        if(lowered_comparison != 0)
        {
            continue;
        }
        wrapper_comparison = p101_strcmp(env, call->caller_usr, aliases[index].wrapper_usr);
        if(wrapper_comparison == 0)
        {
            implementation = true;
            break;
        }
    }

done:
    return implementation;
}

bool p101_wrapper_is_local(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    size_t index;
    bool   local;

    P101_TRACE_SCOPE(env);
    local = false;
    if(call->usr[0] == '\0')
    {
        goto done;
    }
    for(index = 0U; index < model->fact_count; index++)
    {
        int comparison;

        if(model->facts[index].kind != P101_C_ANALYSIS_FUNCTION || !model->facts[index].is_definition)
        {
            continue;
        }
        comparison = p101_strcmp(env, model->facts[index].usr, call->usr);
        if(comparison == 0)
        {
            local = true;
            break;
        }
    }

done:
    return local;
}

bool p101_wrapper_is_errno_macro_lowering(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    static const char *const provider_usrs[] = {
        "c:@F@__errno_location",
        "c:@F@__error",
    };
    size_t provider_index;
    bool   provider;
    bool   lowering;

    P101_TRACE_SCOPE(env);
    provider = false;
    lowering = false;
    for(provider_index = 0U; provider_index < sizeof(provider_usrs) / sizeof(provider_usrs[0]); provider_index++)
    {
        int comparison;

        comparison = p101_strcmp(env, call->usr, provider_usrs[provider_index]);
        if(comparison == 0)
        {
            provider = true;
            break;
        }
    }
    if(!provider)
    {
        goto done;
    }
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *macro;
        int                             path_comparison;
        int                             caller_comparison;
        int                             name_comparison;

        macro = &model->facts[index];
        if(macro->kind != P101_C_ANALYSIS_MACRO || macro->is_definition || macro->line != call->line)
        {
            continue;
        }
        path_comparison = p101_strcmp(env, macro->path, call->path);
        if(path_comparison != 0)
        {
            continue;
        }
        caller_comparison = p101_strcmp(env, macro->caller_usr, call->caller_usr);
        if(caller_comparison != 0)
        {
            continue;
        }
        name_comparison = p101_strcmp(env, macro->name, "errno");
        if(name_comparison == 0)
        {
            lowering = true;
            break;
        }
    }

done:
    return lowering;
}

/*
 * A directory pattern is written with both separators so it can be probed
 * anywhere in a path; the same text minus its leading separator matches an
 * include spelling that opens with that directory.
 */
bool p101_wrapper_path_has_directory_component(const struct p101_env *env, const char *path, const char *component)
{
    const char *found;
    bool        contains;

    P101_TRACE_SCOPE(env);
    contains = false;
    found    = p101_strstr(env, path, component);
    if(found != NULL)
    {
        contains = true;
    }
    else
    {
        size_t length;
        int    comparison;

        length     = p101_strlen(env, component);
        comparison = p101_strncmp(env, path, component + 1, length - 1U);
        if(comparison == 0)
        {
            contains = true;
        }
    }
    return contains;
}

/*
 * A suffix pattern matches a whole trailing path component sequence, so
 * "sys/event.h" matches both the bare include spelling and the resolved
 * "/usr/include/sys/event.h", but never a file merely ending in those bytes.
 */
bool p101_wrapper_path_has_trailing_components(const struct p101_env *env, const char *path, const char *suffix)
{
    size_t path_length;
    size_t suffix_length;
    bool   matches;

    P101_TRACE_SCOPE(env);
    matches       = false;
    path_length   = p101_strlen(env, path);
    suffix_length = p101_strlen(env, suffix);
    if(path_length >= suffix_length)
    {
        int comparison;

        comparison = p101_strcmp(env, path + path_length - suffix_length, suffix);
        if(comparison == 0 && (path_length == suffix_length || path[path_length - suffix_length - 1U] == '/'))
        {
            matches = true;
        }
    }
    return matches;
}
