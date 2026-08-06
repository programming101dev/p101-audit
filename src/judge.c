#include "judge_support.h"
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
    void                        *p101_call_result_1;
    size_t                       capacity;
    struct p101_wrapper_finding *findings;
    bool                         grown;

    P101_TRACE_SCOPE(env);
    grown              = false;
    capacity           = model->finding_capacity == 0U ? INITIAL_CAPACITY : model->finding_capacity * 2U;
    p101_call_result_1 = p101_realloc(env, err, model->findings, capacity * sizeof(*findings));
    findings           = (struct p101_wrapper_finding *)p101_call_result_1;
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
    int p101_call_result_2;

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
        p101_call_result_2 = p101_strcmp(env, usr, mappings[index].lowered);
        if(p101_call_result_2 == 0)
        {
            canonical = mappings[index].canonical;
            break;
        }
    }
    return canonical;
}

static const struct p101_wrapper_inventory *find_wrapper(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *fact)
{
    int                                  p101_expression_result_16;
    int                                  p101_expression_result_17;
    int                                  p101_expression_result_18;
    int                                  p101_call_result_19;
    int                                  p101_expression_result_20;
    int                                  p101_expression_result_21;
    int                                  p101_call_result_22;
    const char                          *callee_usr;
    const struct p101_wrapper_inventory *wrapper;

    P101_TRACE_SCOPE(env);
    wrapper    = NULL;
    callee_usr = canonical_native_usr(env, fact->usr);
    for(size_t index = 0U; index < model->inventory_count; index++)
    {
        const struct p101_wrapper_inventory *candidate;

        candidate                 = &model->inventory[index];
        p101_expression_result_18 = 0;
        if(callee_usr != NULL)
        {
            if(callee_usr[0] != '\0')
            {
                p101_expression_result_18 = 1;
            }
        }
        p101_expression_result_17 = 0;
        if(p101_expression_result_18)
        {
            p101_call_result_19 = p101_strcmp(env, candidate->original_usr, callee_usr);
            if(p101_call_result_19 == 0)
            {
                p101_expression_result_17 = 1;
            }
        }
        if(p101_expression_result_17)
        {
            p101_expression_result_16 = 1;
        }
        else
        {
            p101_expression_result_21 = 0;
            if(fact->kind == P101_C_ANALYSIS_MACRO)
            {
                if(candidate->original[0] != '\0')
                {
                    p101_expression_result_21 = 1;
                }
            }
            p101_expression_result_20 = 0;
            if(p101_expression_result_21)
            {
                p101_call_result_22 = p101_strcmp(env, candidate->original, fact->name);
                if(p101_call_result_22 == 0)
                {
                    p101_expression_result_20 = 1;
                }
            }
            if(p101_expression_result_20)
            {
                p101_expression_result_16 = 1;
            }
            else
            {
                p101_expression_result_16 = 0;
            }
        }
        if(p101_expression_result_16)
        {
            wrapper = candidate;
            break;
        }
    }
    return wrapper;
}

static bool call_is_inventory_wrapper(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    int  p101_expression_result_23;
    int  p101_call_result_24;
    bool known;

    P101_TRACE_SCOPE(env);
    known = false;
    if(call->kind != P101_C_ANALYSIS_CALL || call->usr[0] == '\0')
    {
        goto done;
    }
    for(size_t inventory_index = 0U; inventory_index < model->inventory_count && !known; inventory_index++)
    {
        p101_expression_result_23 = 0;
        if(model->inventory[inventory_index].wrapper_usr[0] != '\0')
        {
            p101_call_result_24 = p101_strcmp(env, model->inventory[inventory_index].wrapper_usr, call->usr);
            if(p101_call_result_24 == 0)
            {
                p101_expression_result_23 = 1;
            }
        }
        if(p101_expression_result_23)
        {
            known = true;
        }
    }

done:
    return known;
}

static bool path_matches(const struct p101_env *env, const char *pattern, const char *path)
{
    int         p101_call_result_4;
    const char *candidate;
    bool        matches;

    P101_TRACE_SCOPE(env);
    candidate = path;
    matches   = false;
    for(;;)
    {
        /* P101_ERROR_OPTIONAL rationale: match failure and no-match are both a false probe. */
        p101_call_result_4 = p101_fnmatch(env, P101_ERROR_OPTIONAL, pattern, candidate, 0);
        if(p101_call_result_4 == 0)
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
    int         p101_expression_result_36;
    int         p101_expression_result_37;
    int         p101_expression_result_38;
    bool        p101_call_result_39;
    int         p101_expression_result_40;
    int         p101_call_result_41;
    int         p101_call_result_42;
    int         p101_call_result_5;
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
        p101_call_result_5 = p101_strcmp(env, arguments->allowed_usrs[index], callee_usr);
        if(p101_call_result_5 == 0)
        {
            allowed = true;
            break;
        }
    }
    for(index = 0U; !allowed && index < arguments->allow_rule_count; index++)
    {
        p101_expression_result_38 = 0;
        if(callee_usr != NULL)
        {
            p101_call_result_39 = path_matches(env, arguments->allow_rules[index].path, fact->path);
            if(p101_call_result_39)
            {
                p101_expression_result_38 = 1;
            }
        }
        p101_expression_result_37 = 0;
        if(p101_expression_result_38)
        {
            if(arguments->allow_rules[index].caller_usr[0] == '\0')
            {
                p101_expression_result_40 = 1;
            }
            else
            {
                p101_call_result_41 = p101_strcmp(env, arguments->allow_rules[index].caller_usr, fact->caller_usr);
                if(p101_call_result_41 == 0)
                {
                    p101_expression_result_40 = 1;
                }
                else
                {
                    p101_expression_result_40 = 0;
                }
            }
            if(p101_expression_result_40)
            {
                p101_expression_result_37 = 1;
            }
        }
        p101_expression_result_36 = 0;
        if(p101_expression_result_37)
        {
            p101_call_result_42 = p101_strcmp(env, arguments->allow_rules[index].callee_usr, callee_usr);
            if(p101_call_result_42 == 0)
            {
                p101_expression_result_36 = 1;
            }
        }
        if(p101_expression_result_36)
        {
            arguments->allow_rules[index].uses++;
            allowed = true;
        }
    }
    return allowed;
}

static bool is_allowed_macro_lowering(const struct p101_env *env, struct p101_wrapper_arguments *arguments, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    int  p101_expression_result_43;
    int  p101_expression_result_44;
    int  p101_expression_result_45;
    int  p101_expression_result_46;
    int  p101_expression_result_47;
    int  p101_call_result_48;
    int  p101_call_result_49;
    int  p101_expression_result_50;
    bool p101_call_result_51;
    bool allowed;

    P101_TRACE_SCOPE(env);
    allowed = false;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact      *macro;
        const struct p101_wrapper_inventory *wrapper;

        macro = &model->facts[index];
        if(macro->kind != P101_C_ANALYSIS_MACRO)
        {
            p101_expression_result_47 = 1;
        }
        else
        {
            if(macro->is_definition)
            {
                p101_expression_result_47 = 1;
            }
            else
            {
                p101_expression_result_47 = 0;
            }
        }
        if(p101_expression_result_47)
        {
            p101_expression_result_46 = 1;
        }
        else
        {
            p101_call_result_48 = p101_strcmp(env, macro->path, call->path);
            if(p101_call_result_48 != 0)
            {
                p101_expression_result_46 = 1;
            }
            else
            {
                p101_expression_result_46 = 0;
            }
        }
        if(p101_expression_result_46)
        {
            p101_expression_result_45 = 1;
        }
        else
        {
            p101_call_result_49 = p101_strcmp(env, macro->caller_usr, call->caller_usr);
            if(p101_call_result_49 != 0)
            {
                p101_expression_result_45 = 1;
            }
            else
            {
                p101_expression_result_45 = 0;
            }
        }
        if(p101_expression_result_45)
        {
            p101_expression_result_44 = 1;
        }
        else
        {
            if(call->start > macro->start)
            {
                p101_expression_result_44 = 1;
            }
            else
            {
                p101_expression_result_44 = 0;
            }
        }
        if(p101_expression_result_44)
        {
            p101_expression_result_43 = 1;
        }
        else
        {
            if(call->end < macro->end)
            {
                p101_expression_result_43 = 1;
            }
            else
            {
                p101_expression_result_43 = 0;
            }
        }
        if(p101_expression_result_43)
        {
            continue;
        }
        wrapper                   = find_wrapper(env, model, macro);
        p101_expression_result_50 = 0;
        if(wrapper != NULL)
        {
            p101_call_result_51 = is_allowed(env, arguments, macro, wrapper);
            if(p101_call_result_51)
            {
                p101_expression_result_50 = 1;
            }
        }
        if(p101_expression_result_50)
        {
            allowed = true;
            break;
        }
    }
    return allowed;
}

static bool add_finding(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, enum p101_wrapper_finding_kind kind, const struct p101_wrapper_fact *fact, const char *name, const char *replacement)
{
    int                          p101_expression_result_52;
    int                          p101_expression_result_53;
    int                          p101_expression_result_54;
    int                          p101_call_result_55;
    int                          p101_call_result_56;
    int                          p101_expression_result_57;
    bool                         p101_call_result_58;
    size_t                       index;
    struct p101_wrapper_finding *finding;
    bool                         added;

    P101_TRACE_SCOPE(env);
    added = true;
    for(index = 0U; index < model->finding_count; index++)
    {
        p101_expression_result_54 = 0;
        if(model->findings[index].line == fact->line)
        {
            if(model->findings[index].column == fact->column)
            {
                p101_expression_result_54 = 1;
            }
        }
        p101_expression_result_53 = 0;
        if(p101_expression_result_54)
        {
            p101_call_result_55 = p101_strcmp(env, model->findings[index].path, fact->path);
            if(p101_call_result_55 == 0)
            {
                p101_expression_result_53 = 1;
            }
        }
        p101_expression_result_52 = 0;
        if(p101_expression_result_53)
        {
            p101_call_result_56 = p101_strcmp(env, model->findings[index].name, name);
            if(p101_call_result_56 == 0)
            {
                p101_expression_result_52 = 1;
            }
        }
        if(p101_expression_result_52)
        {
            goto done;
        }
    }
    p101_expression_result_57 = 0;
    if(model->finding_count == model->finding_capacity)
    {
        p101_call_result_58 = grow_findings(env, err, model);
        if(!p101_call_result_58)
        {
            p101_expression_result_57 = 1;
        }
    }
    if(p101_expression_result_57)
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
    int         p101_expression_result_59;
    int         p101_expression_result_60;
    int         p101_expression_result_61;
    int         p101_expression_result_62;
    int         p101_expression_result_63;
    int         p101_call_result_64;
    int         p101_call_result_65;
    int         p101_call_result_66;
    int         p101_call_result_67;
    int         p101_call_result_68;
    int         p101_call_result_69;
    const char *p101_call_result_15;
    const char *p101_call_result_14;
    const char *p101_call_result_6;
    const char *p101_call_result_7;
    const char *p101_call_result_8;
    const char *p101_call_result_9;
    const char *relative;
    bool        platform_specific;

    P101_TRACE_SCOPE(env);
    relative           = name;
    p101_call_result_6 = p101_strstr(env, name, "/linux/");
    if(p101_call_result_6 != NULL)
    {
        p101_call_result_7 = p101_strstr(env, name, "/linux/");
        relative           = p101_call_result_7 + 1;
    }
    else
    {
        p101_call_result_14 = p101_strstr(env, name, "/mach/");
        if(p101_call_result_14 != NULL)
        {
            p101_call_result_8 = p101_strstr(env, name, "/mach/");
            relative           = p101_call_result_8 + 1;
        }
        else
        {
            p101_call_result_15 = p101_strstr(env, name, "/windows/");
            if(p101_call_result_15 != NULL)
            {
                p101_call_result_9 = p101_strstr(env, name, "/windows/");
                relative           = p101_call_result_9 + 1;
            }
        }
    }
    p101_call_result_64 = p101_strcmp(env, relative, "sys/event.h");
    if(p101_call_result_64 == 0)
    {
        p101_expression_result_63 = 1;
    }
    else
    {
        p101_call_result_65 = p101_strcmp(env, relative, "sys/kqueue.h");
        if(p101_call_result_65 == 0)
        {
            p101_expression_result_63 = 1;
        }
        else
        {
            p101_expression_result_63 = 0;
        }
    }
    if(p101_expression_result_63)
    {
        p101_expression_result_62 = 1;
    }
    else
    {
        p101_call_result_66 = p101_strcmp(env, relative, "sys/sysctl.h");
        if(p101_call_result_66 == 0)
        {
            p101_expression_result_62 = 1;
        }
        else
        {
            p101_expression_result_62 = 0;
        }
    }
    if(p101_expression_result_62)
    {
        p101_expression_result_61 = 1;
    }
    else
    {
        p101_call_result_67 = p101_strncmp(env, relative, "linux/", sizeof("linux/") - 1U);
        if(p101_call_result_67 == 0)
        {
            p101_expression_result_61 = 1;
        }
        else
        {
            p101_expression_result_61 = 0;
        }
    }
    if(p101_expression_result_61)
    {
        p101_expression_result_60 = 1;
    }
    else
    {
        p101_call_result_68 = p101_strncmp(env, relative, "mach/", sizeof("mach/") - 1U);
        if(p101_call_result_68 == 0)
        {
            p101_expression_result_60 = 1;
        }
        else
        {
            p101_expression_result_60 = 0;
        }
    }
    if(p101_expression_result_60)
    {
        p101_expression_result_59 = 1;
    }
    else
    {
        p101_call_result_69 = p101_strncmp(env, relative, "windows/", sizeof("windows/") - 1U);
        if(p101_call_result_69 == 0)
        {
            p101_expression_result_59 = 1;
        }
        else
        {
            p101_expression_result_59 = 0;
        }
    }
    platform_specific = p101_expression_result_59 != 0;
    return platform_specific;
}

bool p101_wrapper_model_judge(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, struct p101_wrapper_arguments *arguments)
{
    int    p101_expression_result_70;
    int    p101_expression_result_71;
    bool   p101_call_result_72;
    int    p101_expression_result_73;
    int    p101_expression_result_74;
    int    p101_expression_result_75;
    int    p101_expression_result_76;
    bool   p101_call_result_77;
    bool   p101_call_result_78;
    bool   p101_call_result_79;
    bool   p101_call_result_80;
    bool   p101_call_result_81;
    bool   p101_call_result_10;
    bool   p101_call_result_11;
    bool   p101_call_result_12;
    bool   p101_call_result_13;
    size_t index;
    bool   judged;

    P101_TRACE_SCOPE(env);
    judged = true;
    for(index = 0U; judged && index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact                      = &model->facts[index];
        p101_expression_result_71 = 0;
        if(fact->kind == P101_C_ANALYSIS_INCLUDE)
        {
            if(arguments->check_portability)
            {
                p101_expression_result_71 = 1;
            }
        }
        p101_expression_result_70 = 0;
        if(p101_expression_result_71)
        {
            p101_call_result_72 = include_is_platform_specific(env, fact->name);
            if(p101_call_result_72)
            {
                p101_expression_result_70 = 1;
            }
        }
        if(p101_expression_result_70)
        {
            p101_call_result_10 = add_finding(env, err, model, P101_WRAPPER_PORTABILITY, fact, fact->name, "");
            if(!p101_call_result_10)
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
            if(name[0] == '\0')
            {
                p101_expression_result_76 = 1;
            }
            else
            {
                p101_call_result_77 = call_is_inventory_wrapper(env, model, fact);
                if(p101_call_result_77)
                {
                    p101_expression_result_76 = 1;
                }
                else
                {
                    p101_expression_result_76 = 0;
                }
            }
            if(p101_expression_result_76)
            {
                p101_expression_result_75 = 1;
            }
            else
            {
                p101_call_result_78 = p101_wrapper_is_local(env, model, fact);
                if(p101_call_result_78)
                {
                    p101_expression_result_75 = 1;
                }
                else
                {
                    p101_expression_result_75 = 0;
                }
            }
            if(p101_expression_result_75)
            {
                p101_expression_result_74 = 1;
            }
            else
            {
                p101_call_result_81 = p101_wrapper_is_errno_macro_lowering(env, model, fact);
                if(p101_call_result_81)
                {
                    p101_expression_result_74 = 1;
                }
                else
                {
                    p101_call_result_79 = is_allowed(env, arguments, fact, wrapper);
                    if(p101_call_result_79)
                    {
                        p101_expression_result_74 = 1;
                    }
                    else
                    {
                        p101_expression_result_74 = 0;
                    }
                }
            }
            if(p101_expression_result_74)
            {
                p101_expression_result_73 = 1;
            }
            else
            {
                p101_call_result_80 = is_allowed_macro_lowering(env, arguments, model, fact);
                if(p101_call_result_80)
                {
                    p101_expression_result_73 = 1;
                }
                else
                {
                    p101_expression_result_73 = 0;
                }
            }
            if(p101_expression_result_73)
            {
                continue;
            }
            if(fact->is_indirect)
            {
                wrapper = NULL;
            }
            p101_call_result_11 = p101_wrapper_is_wrapper_implementation(env, fact, wrapper);
            if(p101_call_result_11)
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
                replacement         = wrapper == NULL ? "" : wrapper->wrapper;
                p101_call_result_12 = add_finding(env, err, model, finding_kind, fact, name, replacement);
                if(!p101_call_result_12)
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
            p101_call_result_13 = p101_error_has_no_error(err);
            if(p101_call_result_13)
            {
                P101_ERROR_RAISE_USER(err, message, 1);
            }
            judged = false;
        }
    }
    return judged;
}
