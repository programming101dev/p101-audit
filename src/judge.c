#include "model.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/p101_fnmatch.h>
#include <p101_filesystem/p101_ftw.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_filesystem/p101_libgen.h>
#include <p101_filesystem/p101_stdio.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_filesystem/p101_unistd.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_filesystem/sys/p101_statvfs.h>

enum
{
    INITIAL_CAPACITY = 256
};

static void copy_field(const struct p101_env *env, char *destination, size_t size, const char *source)
{
    P101_TRACE_SCOPE(env);
    destination[0] = '\0';
    if(source != NULL)
    {
        size_t length;

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
    bool                         grown;

    P101_TRACE_SCOPE(env);
    grown    = false;
    capacity = model->finding_capacity == 0U ? INITIAL_CAPACITY : model->finding_capacity * 2U;
    findings = (struct p101_wrapper_finding *)p101_realloc(env, err, model->findings, capacity * sizeof(*findings));
    if(findings == NULL)
    {
        goto done;
    }
    model->findings         = findings;
    model->finding_capacity = capacity;
    grown                   = true;

done:
    return grown;
}

static const char *canonical_native_usr(const struct p101_env *env, const char *usr)
{
    static const struct
    {
        const char *lowered;
        const char *canonical;
    } mappings[] = {
        {"c:@F@__builtin___fprintf_chk",   "c:@F@fprintf"  },
        {"c:@F@__builtin___memcpy_chk",    "c:@F@memcpy"   },
        {"c:@F@__builtin___memmove_chk",   "c:@F@memmove"  },
        {"c:@F@__builtin___memset_chk",    "c:@F@memset"   },
        {"c:@F@__builtin___printf_chk",    "c:@F@printf"   },
        {"c:@F@__builtin___snprintf_chk",  "c:@F@snprintf" },
        {"c:@F@__builtin___strcpy_chk",    "c:@F@strcpy"   },
        {"c:@F@__builtin___vsnprintf_chk", "c:@F@vsnprintf"},
        {"c:@F@__builtin_memcpy",          "c:@F@memcpy"   },
        {"c:@F@__builtin_memmove",         "c:@F@memmove"  },
        {"c:@F@__builtin_memset",          "c:@F@memset"   },
        {"c:@F@__builtin_va_copy",         "c:@F@va_copy"  },
        {"c:@F@__builtin_va_end",          "c:@F@va_end"   },
        {"c:@F@__builtin_va_start",        "c:@F@va_start" },
    };

    size_t      index;
    const char *canonical;

    P101_TRACE_SCOPE(env);
    canonical = usr;
    for(index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); index++)
    {
        if(p101_strcmp(env, usr, mappings[index].lowered) == 0)
        {
            canonical = mappings[index].canonical;
            break;
        }
    }
    return canonical;
}

static const struct p101_wrapper_inventory *find_wrapper(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *fact)
{
    const char                          *callee_usr;
    const struct p101_wrapper_inventory *wrapper;

    P101_TRACE_SCOPE(env);
    wrapper    = NULL;
    callee_usr = canonical_native_usr(env, fact->usr);
    for(size_t index = 0U; index < model->inventory_count; index++)
    {
        const struct p101_wrapper_inventory *candidate;

        candidate = &model->inventory[index];
        if((callee_usr != NULL && callee_usr[0] != '\0' && p101_strcmp(env, candidate->original_usr, callee_usr) == 0) || (fact->kind == P101_C_ANALYSIS_MACRO && candidate->original[0] != '\0' && p101_strcmp(env, candidate->original, fact->name) == 0))
        {
            wrapper = candidate;
            break;
        }
    }
    return wrapper;
}

static bool call_is_inventory_wrapper(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    bool known;

    P101_TRACE_SCOPE(env);
    known = false;
    if(call->kind != P101_C_ANALYSIS_CALL || call->usr[0] == '\0')
    {
        goto done;
    }
    for(size_t inventory_index = 0U; inventory_index < model->inventory_count && !known; inventory_index++)
    {
        if(model->inventory[inventory_index].wrapper_usr[0] != '\0' && p101_strcmp(env, model->inventory[inventory_index].wrapper_usr, call->usr) == 0)
        {
            known = true;
        }
    }

done:
    return known;
}

static bool caller_is_declared_wrapper(const struct p101_env *env, const struct p101_wrapper_fact *call, const struct p101_wrapper_inventory *wrapper)
{
    return (wrapper != NULL && wrapper->wrapper_usr[0] != '\0' && call->caller_usr[0] != '\0' && p101_strcmp(env, wrapper->wrapper_usr, call->caller_usr) == 0) != 0;
}

static bool is_wrapper_implementation(const struct p101_env *env, const struct p101_wrapper_fact *call, const struct p101_wrapper_inventory *wrapper)
{
    static const struct
    {
        const char *lowered_usr;
        const char *wrapper_usr;
    } aliases[] = {
        {"c:@F@fgetc",  "c:@F@p101_getc"    },
        {"c:@F@fgetc",  "c:@F@p101_getchar" },
        {"c:@F@fgetwc", "c:@F@p101_getwc"   },
        {"c:@F@fgetwc", "c:@F@p101_getwchar"},
        {"c:@F@fputc",  "c:@F@p101_putc"    },
        {"c:@F@fputc",  "c:@F@p101_putchar" },
        {"c:@F@fputwc", "c:@F@p101_putwc"   },
        {"c:@F@fputwc", "c:@F@p101_putwchar"},
    };

    size_t index;
    bool   implementation;

    P101_TRACE_SCOPE(env);
    implementation = false;
    if(caller_is_declared_wrapper(env, call, wrapper))
    {
        implementation = true;
        goto done;
    }
    /*
     * The C standard permits the getc/putc families to be macros. Several
     * libcs lower those aliases to their fgetc/fputc counterparts, so the AST
     * names the implementation function rather than the API written in the
     * wrapper source.
     */
    for(index = 0U; index < sizeof(aliases) / sizeof(aliases[0]); index++)
    {
        if(p101_strcmp(env, call->usr, aliases[index].lowered_usr) == 0 && p101_strcmp(env, call->caller_usr, aliases[index].wrapper_usr) == 0)
        {
            implementation = true;
            break;
        }
    }

done:
    return implementation;
}

static bool is_local(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    size_t index;
    bool   local;

    P101_TRACE_SCOPE(env);
    local = false;
    for(index = 0U; index < model->fact_count; index++)
    {
        if(model->facts[index].kind == P101_C_ANALYSIS_FUNCTION && model->facts[index].is_definition && call->usr[0] != '\0' && p101_strcmp(env, model->facts[index].usr, call->usr) == 0)
        {
            local = true;
            break;
        }
    }
    return local;
}

static bool path_matches(const struct p101_env *env, const char *pattern, const char *path)
{
    const char *candidate;
    bool        matches;

    P101_TRACE_SCOPE(env);
    candidate = path;
    matches   = false;
    for(;;)
    {
        /* P101_ERROR_OPTIONAL rationale: match failure and no-match are both a false probe. */
        if(p101_fnmatch(env, P101_ERROR_OPTIONAL, pattern, candidate, 0) == 0)
        {
            matches = true;
            break;
        }
        candidate = p101_strchr(env, candidate, '/');
        if(candidate == NULL)
        {
            break;
        }
        candidate++;
    }
    return matches;
}

static const char *fact_callee_identity(const struct p101_env *env, const struct p101_wrapper_fact *fact, const struct p101_wrapper_inventory *wrapper, char *macro_identity, size_t macro_identity_size)
{
    const char *identity;
    const char *type_marker;

    P101_TRACE_SCOPE(env);
    identity    = canonical_native_usr(env, fact->usr);
    type_marker = NULL;
    if(identity != NULL)
    {
        type_marker = p101_strstr(env, identity, "@T@");
    }
    if(type_marker != NULL)
    {
        p101_snprintf(env, P101_ERROR_OPTIONAL, macro_identity, macro_identity_size, "c:%s", type_marker);
        identity = macro_identity;
    }
    if(fact->kind == P101_C_ANALYSIS_MACRO && wrapper != NULL && wrapper->original_usr[0] != '\0')
    {
        identity = wrapper->original_usr;
    }
    if((identity == NULL || identity[0] == '\0') && fact->kind == P101_C_ANALYSIS_MACRO)
    {
        p101_snprintf(env, P101_ERROR_OPTIONAL, macro_identity, macro_identity_size, "macro:%s", fact->name);
        identity = macro_identity;
    }
    return identity;
}

static bool is_allowed(const struct p101_env *env, struct p101_wrapper_arguments *arguments, const struct p101_wrapper_fact *fact, const struct p101_wrapper_inventory *wrapper)
{
    char        macro_identity[P101_WRAPPER_NAME_SIZE];
    const char *callee_usr;
    size_t      index;
    bool        allowed;

    P101_TRACE_SCOPE(env);
    macro_identity[0] = '\0';
    callee_usr        = fact_callee_identity(env, fact, wrapper, macro_identity, sizeof(macro_identity));
    allowed           = false;
    for(index = 0U; callee_usr != NULL && index < arguments->allowed_usr_count; index++)
    {
        if(p101_strcmp(env, arguments->allowed_usrs[index], callee_usr) == 0)
        {
            allowed = true;
            break;
        }
    }
    for(index = 0U; !allowed && index < arguments->allow_rule_count; index++)
    {
        if(callee_usr != NULL && path_matches(env, arguments->allow_rules[index].path, fact->path) && (arguments->allow_rules[index].caller_usr[0] == '\0' || p101_strcmp(env, arguments->allow_rules[index].caller_usr, fact->caller_usr) == 0) &&
           p101_strcmp(env, arguments->allow_rules[index].callee_usr, callee_usr) == 0)
        {
            arguments->allow_rules[index].uses++;
            allowed = true;
        }
    }
    return allowed;
}

static bool is_allowed_macro_lowering(const struct p101_env *env, struct p101_wrapper_arguments *arguments, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    bool allowed;

    P101_TRACE_SCOPE(env);
    allowed = false;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact      *macro;
        const struct p101_wrapper_inventory *wrapper;

        macro = &model->facts[index];
        if(macro->kind != P101_C_ANALYSIS_MACRO || macro->is_definition || p101_strcmp(env, macro->path, call->path) != 0 || p101_strcmp(env, macro->caller_usr, call->caller_usr) != 0 || call->start > macro->start || call->end < macro->end)
        {
            continue;
        }
        wrapper = find_wrapper(env, model, macro);
        if(wrapper != NULL && is_allowed(env, arguments, macro, wrapper))
        {
            allowed = true;
            break;
        }
    }
    return allowed;
}

static bool add_finding(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, enum p101_wrapper_finding_kind kind, const struct p101_wrapper_fact *fact, const char *name, const char *replacement)
{
    size_t                       index;
    struct p101_wrapper_finding *finding;
    bool                         added;

    P101_TRACE_SCOPE(env);
    added = true;
    for(index = 0U; index < model->finding_count; index++)
    {
        if(model->findings[index].line == fact->line && model->findings[index].column == fact->column && p101_strcmp(env, model->findings[index].path, fact->path) == 0 && p101_strcmp(env, model->findings[index].name, name) == 0)
        {
            goto done;
        }
    }
    if(model->finding_count == model->finding_capacity && !grow_findings(env, err, model))
    {
        added = false;
        goto done;
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

done:
    return added;
}

static bool include_is_platform_specific(const struct p101_env *env, const char *name)
{
    const char *relative;
    bool        platform_specific;

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
    platform_specific = (p101_strcmp(env, relative, "sys/event.h") == 0 || p101_strcmp(env, relative, "sys/kqueue.h") == 0 || p101_strcmp(env, relative, "sys/sysctl.h") == 0 || p101_strncmp(env, relative, "linux/", sizeof("linux/") - 1U) == 0 ||
                         p101_strncmp(env, relative, "mach/", sizeof("mach/") - 1U) == 0 || p101_strncmp(env, relative, "windows/", sizeof("windows/") - 1U) == 0) != 0;
    return platform_specific;
}

bool p101_wrapper_model_judge(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, struct p101_wrapper_arguments *arguments)
{
    size_t index;
    bool   judged;

    P101_TRACE_SCOPE(env);
    judged = true;
    for(index = 0U; judged && index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind == P101_C_ANALYSIS_INCLUDE && arguments->check_portability && include_is_platform_specific(env, fact->name))
        {
            if(!add_finding(env, err, model, P101_WRAPPER_PORTABILITY, fact, fact->name, ""))
            {
                judged = false;
            }
        }
        else if(fact->kind == P101_C_ANALYSIS_CALL || (fact->kind == P101_C_ANALYSIS_MACRO && !fact->is_definition))
        {
            const char                          *name;
            const struct p101_wrapper_inventory *wrapper;

            name    = fact->name;
            wrapper = find_wrapper(env, model, fact);
            /*
             * Function-like libc APIs are macros on some platforms. Treat a
             * macro invocation as a boundary operation only when the wrapper
             * inventory knows that API; arbitrary project macros are not
             * external calls.
             */
            if(fact->kind == P101_C_ANALYSIS_MACRO && wrapper == NULL)
            {
                continue;
            }
            if(name[0] == '\0' || call_is_inventory_wrapper(env, model, fact) || is_local(env, model, fact) || is_allowed(env, arguments, fact, wrapper) || is_allowed_macro_lowering(env, arguments, model, fact))
            {
                continue;
            }
            if(fact->is_indirect)
            {
                wrapper = NULL;
            }
            if(is_wrapper_implementation(env, fact, wrapper))
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
                replacement = wrapper == NULL ? "" : wrapper->wrapper;
                if(!add_finding(env, err, model, finding_kind, fact, name, replacement))
                {
                    judged = false;
                }
            }
        }
    }
    for(index = 0U; judged && index < arguments->allow_rule_count; index++)
    {
        if(arguments->allow_rules[index].uses == 0U)
        {
            char message[P101_WRAPPER_PATH_SIZE];

            p101_snprintf(env, err, message, sizeof(message), "Wrapper boundary rule did not match any call: %s<TAB>%s<TAB>%s", arguments->allow_rules[index].path, arguments->allow_rules[index].caller_usr, arguments->allow_rules[index].callee_usr);
            if(p101_error_has_no_error(err))
            {
                P101_ERROR_RAISE_USER(err, message, 1);
            }
            judged = false;
        }
    }
    return judged;
}
