#include "../include/idioms_includes.h"
#include "../include/constants.h"
#include <p101_c/p101_string.h>

const struct include_record *p101_module_map_idiom_first_local_include(const struct p101_env *env, const struct project_map *map, const char *path)
{
    const struct include_record *ret_val;

    ret_val = NULL;
    for(size_t i = 0; i < map->include_count; i++)
    {
        int p101_expression_result_2;

        p101_expression_result_2 = 0;
        if(map->includes[i].is_local)
        {
            int comparison;

            comparison = p101_strcmp(env, map->includes[i].path, path);
            if(comparison == 0)
            {
                if(ret_val == NULL || map->includes[i].line < ret_val->line)
                {
                    p101_expression_result_2 = 1;
                }
            }
        }
        if(p101_expression_result_2)
        {
            ret_val = &map->includes[i];
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_target_matches_module(const struct p101_env *env, const char *target, const char *module_name)
{
    const char *base;
    const char *tail;
    const char *p101_call_result_3;
    int         p101_call_result_4;
    size_t      base_length;
    bool        ret_val;

    p101_call_result_3 = p101_strrchr(env, module_name, '/');
    if(p101_call_result_3 != NULL)
    {
        base = p101_call_result_3 + 1;
    }
    else
    {
        base = module_name;
    }
    p101_call_result_3 = p101_strrchr(env, target, '/');
    if(p101_call_result_3 != NULL)
    {
        tail = p101_call_result_3 + 1;
    }
    else
    {
        tail = target;
    }
    p101_call_result_4 = p101_strncmp(env, tail, "p101_", (size_t)WRAPPER_PREFIX_LEN);
    if(p101_call_result_4 == 0)
    {
        tail += WRAPPER_PREFIX_LEN;
    }
    base_length        = p101_strlen(env, base);
    ret_val            = false;
    p101_call_result_4 = p101_strncmp(env, tail, base, base_length);
    if(p101_call_result_4 == 0)
    {
        if(tail[base_length] == '\0')
        {
            ret_val = true;
        }
        else
        {
            int comparison;

            comparison = p101_strcmp(env, tail + base_length, "_internal");
            if(comparison == 0)
            {
                ret_val = true;
            }
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_source_includes_own(const struct p101_env *env, const struct project_map *map, const char *path, const char *module_name)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->include_count; i++)
    {
        int p101_expression_result_7;

        p101_expression_result_7 = 0;
        if(map->includes[i].is_local)
        {
            int comparison;

            comparison = p101_strcmp(env, map->includes[i].path, path);
            if(comparison == 0)
            {
                bool p101_call_result_8;

                p101_call_result_8 = p101_module_map_idiom_target_matches_module(env, map->includes[i].target, module_name);
                if(p101_call_result_8)
                {
                    p101_expression_result_7 = 1;
                }
            }
        }
        if(p101_expression_result_7)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_idiom_module_has_header(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->module_count; i++)
    {
        int p101_call_result_9;

        p101_call_result_9 = p101_strcmp(env, map->modules[i].name, module_name);
        if(p101_call_result_9 == 0)
        {
            ret_val = map->modules[i].header_count > 0U;
            break;
        }
    }

    return ret_val;
}
