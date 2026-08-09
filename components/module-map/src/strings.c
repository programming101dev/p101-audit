#include "../include/strings.h"
#include "../include/constants.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_string.h>

void p101_module_map_copy_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source)
{
    P101_TRACE_SCOPE(env);
    if(destination_size > 0U)
    {
        p101_strncpy(env, destination, source, destination_size - 1U);
        destination[destination_size - 1U] = '\0';
    }
}

void p101_module_map_append_string(const struct p101_env *env, char *destination, size_t destination_size, const char *source)
{
    size_t used;

    P101_TRACE_SCOPE(env);
    used = p101_strlen(env, destination);
    if(used + 1U < destination_size)
    {
        p101_module_map_copy_string(env, destination + used, destination_size - used, source);
    }
}

void p101_module_map_basename_no_suffix(const struct p101_env *env, char *destination, size_t destination_size, const char *path)
{
    int         p101_call_result_1;
    const char *base;
    const char *dot;
    size_t      len;

    P101_TRACE_SCOPE(env);
    base               = p101_module_map_path_basename(env, path);
    p101_call_result_1 = p101_strncmp(env, base, "p101_", sizeof("p101_") - 1U);
    if(p101_call_result_1 == 0)
    {
        base += sizeof("p101_") - 1U;
    }
    dot = p101_strrchr(env, base, '.');
    if(dot == NULL)
    {
        len = p101_strlen(env, base);
    }
    else
    {
        len = (size_t)(dot - base);
    }
    if(len >= destination_size)
    {
        len = destination_size - 1U;
    }

    for(size_t i = 0; i < len; i++)
    {
        destination[i] = base[i];
    }
    destination[len] = '\0';
}

void p101_module_map_include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const char *include_name)
{
    int p101_expression_result_2;
    int p101_expression_result_3;
    int p101_call_result_4;
    int p101_call_result_5;
    if(include_name[0] == '@')
    {
        p101_module_map_copy_string(env, destination, destination_size, include_name + 1);
    }
    else
    {
        char        normalized[MAX_NAME];
        const char *module_name;
        const char *slash;

        p101_module_map_normalize_module_name(env, normalized, sizeof(normalized), include_name);
        module_name              = normalized;
        slash                    = p101_strchr(env, normalized, '/');
        p101_expression_result_2 = 0;
        if(slash != NULL)
        {
            p101_call_result_4 = p101_strncmp(env, normalized, "p101_", sizeof("p101_") - 1U);
            if(p101_call_result_4 == 0)
            {
                p101_expression_result_3 = 1;
            }
            else
            {
                p101_call_result_5 = p101_strncmp(env, normalized, "p101/", sizeof("p101/") - 1U);
                if(p101_call_result_5 == 0)
                {
                    p101_expression_result_3 = 1;
                }
                else
                {
                    p101_expression_result_3 = 0;
                }
            }
            if(p101_expression_result_3)
            {
                p101_expression_result_2 = 1;
            }
        }
        if(p101_expression_result_2)
        {
            module_name = slash + 1;
        }
        p101_module_map_copy_string(env, destination, destination_size, module_name);
    }
}

void p101_module_map_normalize_module_name(const struct p101_env *env, char *destination, size_t destination_size, const char *module_name)
{
    const char *base;
    const char *slash;
    char        normalized[MAX_NAME];
    size_t      prefix_length;

    slash         = p101_strrchr(env, module_name, '/');
    base          = slash == NULL ? module_name : slash + 1;
    prefix_length = slash == NULL ? 0U : (size_t)(base - module_name);
    p101_module_map_basename_no_suffix(env, normalized, sizeof(normalized), base);

    if(prefix_length >= destination_size)
    {
        prefix_length = destination_size - 1U;
    }
    p101_memcpy(env, destination, module_name, prefix_length);
    destination[prefix_length] = '\0';
    p101_module_map_append_string(env, destination, destination_size, normalized);
}

char *p101_module_map_trim_left(const struct p101_env *env, char *text)
{
    int is_space;

    for(;;)
    {
        if(*text == '\0')
        {
            break;
        }
        is_space = p101_isspace(env, (unsigned char)*text);
        if(!is_space)
        {
            break;
        }
        text++;
    }
    return text;
}

void p101_module_map_trim_right(const struct p101_env *env, char *text)
{
    size_t len;
    int    is_space;

    len = p101_strlen(env, text);
    for(;;)
    {
        if(len == 0U)
        {
            break;
        }
        is_space = p101_isspace(env, (unsigned char)text[len - 1U]);
        if(!is_space)
        {
            break;
        }
        text[--len] = '\0';
    }
}

const char *p101_module_map_path_basename(const struct p101_env *env, const char *path)
{
    const char *slash;

    slash = p101_strrchr(env, path, '/');
    return (slash == NULL) ? path : slash + 1;
}
