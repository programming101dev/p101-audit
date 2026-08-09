#include "../include/model_query.h"
#include <p101_c/p101_string.h>

bool p101_module_map_module_has_direct_include(const struct p101_env *env, const struct project_map *map, const char *from, const char *target)
{
    int  p101_call_result_4;
    int  p101_call_result_5;
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->include_count; i++)
    {
        int p101_expression_result_2;
        int p101_expression_result_3;

        p101_expression_result_3 = 0;
        if(map->includes[i].is_local)
        {
            p101_call_result_4 = p101_strcmp(env, map->includes[i].from_module, from);
            if(p101_call_result_4 == 0)
            {
                p101_expression_result_3 = 1;
            }
        }
        p101_expression_result_2 = 0;
        if(p101_expression_result_3)
        {
            p101_call_result_5 = p101_strcmp(env, map->includes[i].target, target);
            if(p101_call_result_5 == 0)
            {
                p101_expression_result_2 = 1;
            }
        }
        if(p101_expression_result_2)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_symbol_used_outside_module(const struct p101_env *env, const struct project_map *map, const char *module_name, const char *symbol_name)
{
    int  p101_call_result_8;
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->call_count; i++)
    {
        int p101_expression_result_6;
        int p101_call_result_7;

        p101_call_result_7       = p101_strcmp(env, map->calls[i].module, module_name);
        p101_expression_result_6 = 0;
        if(p101_call_result_7 != 0)
        {
            p101_call_result_8 = p101_strcmp(env, map->calls[i].name, symbol_name);
            if(p101_call_result_8 == 0)
            {
                p101_expression_result_6 = 1;
            }
        }
        if(p101_expression_result_6)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_include_target_exists(const struct p101_env *env, const struct project_map *map, const char *target)
{
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->module_count; i++)
    {
        int p101_call_result_1;

        p101_call_result_1 = p101_strcmp(env, map->modules[i].name, target);
        if(p101_call_result_1 == 0)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_module_used_outside_module(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    int  p101_call_result_11;
    int  p101_call_result_12;
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->include_count; i++)
    {
        int p101_expression_result_9;
        int p101_expression_result_10;

        p101_expression_result_10 = 0;
        if(map->includes[i].is_local)
        {
            p101_call_result_11 = p101_strcmp(env, map->includes[i].target, module_name);
            if(p101_call_result_11 == 0)
            {
                p101_expression_result_10 = 1;
            }
        }
        p101_expression_result_9 = 0;
        if(p101_expression_result_10)
        {
            p101_call_result_12 = p101_strcmp(env, map->includes[i].from_module, module_name);
            if(p101_call_result_12 != 0)
            {
                p101_expression_result_9 = 1;
            }
        }
        if(p101_expression_result_9)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_function_used_outside_module(const struct p101_env *env, const struct project_map *map, const struct function_record *function)
{
    int  p101_call_result_16;
    bool used;

    used = false;
    for(size_t index = 0U; index < map->call_count; index++)
    {
        int p101_expression_result_13;
        int p101_expression_result_14;
        int p101_call_result_15;

        p101_call_result_15       = p101_strcmp(env, map->calls[index].module, function->module);
        p101_expression_result_14 = 0;
        if(p101_call_result_15 != 0)
        {
            if(function->usr[0] != '\0')
            {
                p101_expression_result_14 = 1;
            }
        }
        p101_expression_result_13 = 0;
        if(p101_expression_result_14)
        {
            p101_call_result_16 = p101_strcmp(env, map->calls[index].usr, function->usr);
            if(p101_call_result_16 == 0)
            {
                p101_expression_result_13 = 1;
            }
        }
        if(p101_expression_result_13)
        {
            used = true;
            break;
        }
    }
    return used;
}

bool p101_module_map_function_has_header_declaration(const struct p101_env *env, const struct project_map *map, const struct function_record *function)
{
    int  p101_call_result_19;
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->function_count; i++)
    {
        int p101_expression_result_17;
        int p101_expression_result_18;

        /*
         * A public declaration may intentionally live in an umbrella header
         * or a narrower interface whose basename differs from the
         * implementation file. C linkage is by symbol, not by filename.
         */
        p101_expression_result_18 = 0;
        if(map->functions[i].is_header_declaration)
        {
            if(function->usr[0] != '\0')
            {
                p101_expression_result_18 = 1;
            }
        }
        p101_expression_result_17 = 0;
        if(p101_expression_result_18)
        {
            p101_call_result_19 = p101_strcmp(env, map->functions[i].usr, function->usr);
            if(p101_call_result_19 == 0)
            {
                p101_expression_result_17 = 1;
            }
        }
        if(p101_expression_result_17)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}

bool p101_module_map_function_has_non_static_definition(const struct p101_env *env, const struct project_map *map, const struct function_record *function)
{
    int  p101_call_result_23;
    bool ret_val;

    ret_val = false;
    for(size_t i = 0; i < map->function_count; i++)
    {
        int p101_expression_result_20;
        int p101_expression_result_21;
        int p101_expression_result_22;

        p101_expression_result_22 = 0;
        if(!map->functions[i].is_header_declaration)
        {
            if(!map->functions[i].is_static)
            {
                p101_expression_result_22 = 1;
            }
        }
        p101_expression_result_21 = 0;
        if(p101_expression_result_22)
        {
            if(function->usr[0] != '\0')
            {
                p101_expression_result_21 = 1;
            }
        }
        p101_expression_result_20 = 0;
        if(p101_expression_result_21)
        {
            p101_call_result_23 = p101_strcmp(env, map->functions[i].usr, function->usr);
            if(p101_call_result_23 == 0)
            {
                p101_expression_result_20 = 1;
            }
        }
        if(p101_expression_result_20)
        {
            ret_val = true;
            break;
        }
    }

    return ret_val;
}
