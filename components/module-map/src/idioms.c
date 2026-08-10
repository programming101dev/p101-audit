#include "../include/idioms.h"
#include "../include/constants.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_string.h>

bool p101_module_map_idiom_public_function_exists(const struct p101_env *env, const struct project_map *map, const char *module_name, const char *function_name)
{
    int  p101_call_result_2;
    int  p101_call_result_3;
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->function_count; i++)
    {
        int p101_expression_result_1;

        p101_expression_result_1 = 0;
        if(!map->functions[i].is_static)
        {
            p101_call_result_2 = p101_strcmp(env, map->functions[i].module, module_name);
            if(p101_call_result_2 == 0)
            {
                p101_call_result_3 = p101_strcmp(env, map->functions[i].name, function_name);
                if(p101_call_result_3 == 0)
                {
                    p101_expression_result_1 = 1;
                }
            }
        }
        if(p101_expression_result_1)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_public_function_anywhere(const struct p101_env *env, const struct project_map *map, const char *function_name)
{
    int  p101_call_result_20;
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->function_count; i++)
    {
        int p101_expression_result_21;

        p101_expression_result_21 = 0;
        if(!map->functions[i].is_static)
        {
            p101_call_result_20 = p101_strcmp(env, map->functions[i].name, function_name);
            if(p101_call_result_20 == 0)
            {
                p101_expression_result_21 = 1;
            }
        }
        if(p101_expression_result_21)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_name_ends_with(const struct p101_env *env, const char *name, const char *suffix)
{
    size_t name_length;
    size_t suffix_length;
    bool   ret_val;

    name_length   = p101_strlen(env, name);
    suffix_length = p101_strlen(env, suffix);
    ret_val       = false;
    if(name_length > suffix_length)
    {
        int comparison;

        comparison = p101_strcmp(env, name + (name_length - suffix_length), suffix);
        if(comparison == 0)
        {
            ret_val = true;
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_swap_suffix(const struct p101_env *env, const char *name, const char *suffix, const char *replacement, char *peer, size_t peer_size)
{
    bool p101_call_result_5;
    bool ret_val;

    ret_val            = false;
    p101_call_result_5 = p101_module_map_idiom_name_ends_with(env, name, suffix);
    if(p101_call_result_5)
    {
        size_t name_length;
        size_t suffix_length;
        size_t replacement_length;
        size_t stem_length;

        name_length        = p101_strlen(env, name);
        suffix_length      = p101_strlen(env, suffix);
        replacement_length = p101_strlen(env, replacement);
        stem_length        = name_length - suffix_length;
        if(stem_length + replacement_length < peer_size)
        {
            p101_strncpy(env, peer, name, stem_length);
            peer[stem_length] = '\0';
            p101_strncat(env, peer, replacement, replacement_length);
            ret_val = true;
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_module_shares_prefix(const struct p101_env *env, const struct project_map *map, const char *module_name, bool require_owner_prefix)
{
    const char *first;
    size_t      prefix_length;
    size_t      public_count;
    int         p101_call_result_8;
    int         p101_call_result_9;
    bool        ret_val;

    first         = NULL;
    prefix_length = 0;
    public_count  = 0;
    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;
        int                           p101_expression_result_6;
        int                           p101_expression_result_7;

        function                 = &map->functions[i];
        p101_expression_result_7 = 0;
        if(!function->is_static)
        {
            if(!function->is_header_declaration)
            {
                p101_expression_result_7 = 1;
            }
        }
        p101_expression_result_6 = 0;
        if(p101_expression_result_7)
        {
            p101_call_result_8 = p101_strcmp(env, function->module, module_name);
            if(p101_call_result_8 == 0)
            {
                p101_call_result_9 = p101_strcmp(env, function->usr, "c:@F@main");
                if(p101_call_result_9 != 0)
                {
                    p101_expression_result_6 = 1;
                }
            }
        }
        if(p101_expression_result_6)
        {
            if(first == NULL)
            {
                first         = function->name;
                prefix_length = p101_strlen(env, first);
            }
            else
            {
                size_t shared;

                shared = 0;
                while(shared < prefix_length && function->name[shared] != '\0' && function->name[shared] == first[shared])
                {
                    shared++;
                }
                prefix_length = shared;
            }
            public_count++;
        }
    }
    ret_val = true;
    if(public_count >= 2U)
    {
        while(prefix_length > 0U && first[prefix_length - 1U] != '_')
        {
            prefix_length--;
        }
        if(prefix_length == 0U)
        {
            ret_val = false;
        }
        else if(require_owner_prefix)
        {
            int comparison;

            comparison = p101_strncmp(env, first, "p101_", prefix_length);
            if(prefix_length == (size_t)WRAPPER_PREFIX_LEN && comparison == 0)
            {
                ret_val = false;
            }
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_wraps_platform_name(const struct p101_env *env, const struct project_map *map, const struct function_record *function)
{
    int  p101_call_result_16;
    bool ret_val;

    ret_val             = false;
    p101_call_result_16 = p101_strncmp(env, function->name, "p101_", (size_t)WRAPPER_PREFIX_LEN);
    if(p101_call_result_16 == 0)
    {
        for(size_t i = 0; i < map->call_count; i++)
        {
            int module_comparison;
            int p101_expression_result_19;

            module_comparison         = p101_strcmp(env, map->calls[i].module, function->module);
            p101_expression_result_19 = 0;
            if(module_comparison == 0)
            {
                int name_comparison;

                name_comparison = p101_strcmp(env, map->calls[i].name, function->name + WRAPPER_PREFIX_LEN);
                if(name_comparison == 0)
                {
                    p101_expression_result_19 = 1;
                }
            }
            if(p101_expression_result_19)
            {
                ret_val = true;
                break;
            }
        }
    }

    return ret_val;
}

const struct macro_record *p101_module_map_idiom_guard_macro(const struct p101_env *env, const struct project_map *map, const char *path)
{
    const struct macro_record *ret_val;

    ret_val = NULL;
    for(size_t i = 0; i < map->macro_count; i++)
    {
        int p101_call_result_11;
        int p101_expression_result_12;

        p101_call_result_11       = p101_strcmp(env, map->macros[i].path, path);
        p101_expression_result_12 = 0;
        if(p101_call_result_11 == 0)
        {
            if(ret_val == NULL || map->macros[i].line < ret_val->line)
            {
                p101_expression_result_12 = 1;
            }
        }
        if(p101_expression_result_12)
        {
            ret_val = &map->macros[i];
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_guard_suffix(const struct p101_env *env, const char *path, char *expected, size_t expected_size)
{
    const char *base;
    const char *p101_call_result_13;
    size_t      written;
    bool        ret_val;

    ret_val             = false;
    p101_call_result_13 = p101_strrchr(env, path, '/');
    if(p101_call_result_13 != NULL)
    {
        base = p101_call_result_13 + 1;
    }
    else
    {
        base = path;
    }
    written = 0;
    while(base[written] != '\0' && base[written] != '.' && written + 3U < expected_size)
    {
        int p101_call_result_14;
        p101_call_result_14 = p101_isalnum(env, (unsigned char)base[written]);
        if(p101_call_result_14)
        {
            int uppercase_character;

            uppercase_character = p101_toupper(env, (unsigned char)base[written]);
            expected[written]   = (char)uppercase_character;
        }
        else
        {
            expected[written] = '_';
        }
        written++;
    }
    if(written > 0U && written + 3U < expected_size)
    {
        expected[written]      = '_';
        expected[written + 1U] = 'H';
        expected[written + 2U] = '\0';
        ret_val                = true;
    }

    return ret_val;
}
