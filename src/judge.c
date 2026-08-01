#include "model.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/filesystem.h>

enum
{
    INITIAL_CAPACITY = 256
};

static void copy_field(const struct p101_env *env, char *destination, size_t size, const char *source)
{
    size_t length;

    P101_TRACE_SCOPE(env);
    destination[0] = '\0';
    if(source != NULL)
    {
        length = p101_strlen(env, source);
        if(length >= size)
        {
            length = size - 1U;
        }
        p101_memcpy(env, destination, source, length);
        destination[length] = '\0';
    }
}

static bool grow_findings(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model)
{
    size_t                       capacity;
    struct p101_wrapper_finding *findings;

    P101_TRACE_SCOPE(env);
    capacity = model->finding_capacity == 0U ? INITIAL_CAPACITY : model->finding_capacity * 2U;
    findings = (struct p101_wrapper_finding *)p101_realloc(env, err, model->findings, capacity * sizeof(*findings));
    if(findings == NULL)
    {
        return false;
    }
    model->findings         = findings;
    model->finding_capacity = capacity;
    return true;
}

static const char *canonical_callee(const struct p101_env *env, const char *name)
{
    static const struct
    {
        const char *lowered;
        const char *source;
    } mappings[] = {
        {"__builtin___fprintf_chk",  "fprintf" },
        {"__builtin___memcpy_chk",   "memcpy"  },
        {"__builtin___memmove_chk",  "memmove" },
        {"__builtin___memset_chk",   "memset"  },
        {"__builtin___printf_chk",   "printf"  },
        {"__builtin___snprintf_chk", "snprintf"},
        {"__builtin___strcpy_chk",   "strcpy"  },
        {"__builtin_memcpy",         "memcpy"  },
        {"__builtin_memmove",        "memmove" },
        {"__builtin_memset",         "memset"  },
    };

    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); index++)
    {
        if(p101_strcmp(env, name, mappings[index].lowered) == 0)
        {
            return mappings[index].source;
        }
    }
    return name;
}

static const char *find_wrapper(const struct p101_env *env, const struct p101_wrapper_model *model, const char *name)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < model->inventory_count; index++)
    {
        if(p101_strcmp(env, model->inventory[index].original, name) == 0)
        {
            return model->inventory[index].wrapper;
        }
    }
    return NULL;
}

static bool is_local(const struct p101_env *env, const struct p101_wrapper_model *model, const char *name)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < model->fact_count; index++)
    {
        if(model->facts[index].kind == P101_C_ANALYSIS_FUNCTION && model->facts[index].is_definition && p101_strcmp(env, model->facts[index].name, name) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool path_matches(const struct p101_env *env, const char *pattern, const char *path)
{
    const char *candidate;

    P101_TRACE_SCOPE(env);
    candidate = path;
    for(;;)
    {
        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: match failure and no-match are both a false probe. */
        if(p101_fnmatch(env, NULL, pattern, candidate, 0) == 0)
        {
            return true;
        }
        candidate = p101_strchr(env, candidate, '/');
        if(candidate == NULL)
        {
            break;
        }
        candidate++;
    }
    return false;
}

static bool is_allowed(const struct p101_env *env, struct p101_wrapper_arguments *arguments, const struct p101_wrapper_fact *fact, const char *name)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < arguments->allowed_count; index++)
    {
        if(p101_strcmp(env, arguments->allowed[index], name) == 0)
        {
            return true;
        }
    }
    for(index = 0U; index < arguments->allow_rule_count; index++)
    {
        /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: match failure and no-match both reject the allow rule. */
        if(path_matches(env, arguments->allow_rules[index].path, fact->path) && p101_fnmatch(env, NULL, arguments->allow_rules[index].function, fact->caller, 0) == 0 && p101_fnmatch(env, NULL, arguments->allow_rules[index].callee, name, 0) == 0)
        {
            arguments->allow_rules[index].uses++;
            return true;
        }
    }
    return false;
}

static bool add_finding(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, enum p101_wrapper_finding_kind kind, const struct p101_wrapper_fact *fact, const char *name, const char *replacement)
{
    size_t                       index;
    struct p101_wrapper_finding *finding;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < model->finding_count; index++)
    {
        if(model->findings[index].line == fact->line && model->findings[index].column == fact->column && p101_strcmp(env, model->findings[index].path, fact->path) == 0 && p101_strcmp(env, model->findings[index].name, name) == 0)
        {
            return true;
        }
    }
    if(model->finding_count == model->finding_capacity && !grow_findings(env, err, model))
    {
        return false;
    }
    finding = &model->findings[model->finding_count++];
    p101_memset(env, finding, 0, sizeof(*finding));
    finding->kind   = kind;
    finding->line   = fact->line;
    finding->column = fact->column;
    copy_field(env, finding->path, sizeof(finding->path), fact->path);
    copy_field(env, finding->name, sizeof(finding->name), name);
    copy_field(env, finding->caller, sizeof(finding->caller), fact->caller[0] == '\0' ? "?" : fact->caller);
    copy_field(env, finding->replacement, sizeof(finding->replacement), replacement);
    return true;
}

static bool include_is_platform_specific(const struct p101_env *env, const char *name)
{
    const char *relative;

    P101_TRACE_SCOPE(env);
    relative = name;
    if(p101_strstr(env, name, "/linux/") != NULL)
    {
        relative = p101_strstr(env, name, "/linux/") + 1;
    }
    else if(p101_strstr(env, name, "/mach/") != NULL)
    {
        relative = p101_strstr(env, name, "/mach/") + 1;
    }
    else if(p101_strstr(env, name, "/windows/") != NULL)
    {
        relative = p101_strstr(env, name, "/windows/") + 1;
    }
    if(p101_strcmp(env, relative, "sys/event.h") == 0 || p101_strcmp(env, relative, "sys/kqueue.h") == 0 || p101_strcmp(env, relative, "sys/sysctl.h") == 0 || p101_strncmp(env, relative, "linux/", sizeof("linux/") - 1U) == 0 ||
       p101_strncmp(env, relative, "mach/", sizeof("mach/") - 1U) == 0 || p101_strncmp(env, relative, "windows/", sizeof("windows/") - 1U) == 0)
    {
        return true;
    }
    return false;
}

bool p101_wrapper_model_judge(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, struct p101_wrapper_arguments *arguments)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind == P101_C_ANALYSIS_INCLUDE && arguments->check_portability && include_is_platform_specific(env, fact->name))
        {
            if(!add_finding(env, err, model, P101_WRAPPER_PORTABILITY, fact, fact->name, ""))
            {
                return false;
            }
        }
        else if(fact->kind == P101_C_ANALYSIS_CALL)
        {
            const char *name;
            const char *wrapper;

            name = canonical_callee(env, fact->name);
            if(name[0] == '\0' || p101_strncmp(env, name, "p101_", sizeof("p101_") - 1U) == 0 || p101_strncmp(env, name, "P101_", sizeof("P101_") - 1U) == 0 || p101_strncmp(env, name, "__", sizeof("__") - 1U) == 0 || is_local(env, model, name) ||
               is_allowed(env, arguments, fact, name))
            {
                continue;
            }
            wrapper = NULL;
            if(!fact->is_indirect)
            {
                wrapper = find_wrapper(env, model, name);
            }
            if(wrapper != NULL && p101_strcmp(env, fact->caller, wrapper) == 0)
            {
                continue;
            }
            {
                enum p101_wrapper_finding_kind finding_kind;
                const char                    *replacement;

                finding_kind = P101_WRAPPER_EXTERNAL;
                if(fact->is_indirect)
                {
                    finding_kind = P101_WRAPPER_INDIRECT;
                }
                else if(wrapper != NULL)
                {
                    finding_kind = P101_WRAPPER_MISSED;
                }
                replacement = wrapper == NULL ? "" : wrapper;
                if(!add_finding(env, err, model, finding_kind, fact, name, replacement))
                {
                    return false;
                }
            }
        }
    }
    for(index = 0U; index < arguments->allow_rule_count; index++)
    {
        if(arguments->allow_rules[index].uses == 0U)
        {
            char message[P101_WRAPPER_PATH_SIZE];

            p101_snprintf(env, err, message, sizeof(message), "Wrapper boundary rule did not match any call: %s:%s:%s", arguments->allow_rules[index].path, arguments->allow_rules[index].function, arguments->allow_rules[index].callee);
            if(p101_error_has_no_error(err))
            {
                P101_ERROR_RAISE_USER(err, message, 1);
            }
            return false;
        }
    }
    return true;
}
