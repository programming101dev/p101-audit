#include "../include/source_inputs.h"
#include <p101_c/p101_string.h>
#include <p101_c_facts/project.h>

void p101_doctor_copy_text(const struct p101_env *env, char dest[PATH_LEN], const char *src)
{
    P101_TRACE_SCOPE(env);
    p101_strncpy(env, dest, src, PATH_LEN - 1U);
    dest[PATH_LEN - 1U] = '\0';
}

void p101_doctor_copy_source_paths(const struct p101_env *env, const struct arguments *args, char source_paths[MAX_SOURCE_PATHS][PATH_LEN])
{
    P101_TRACE_SCOPE(env);
    for(int i = 0; i < args->source_count; i++)
    {
        p101_doctor_copy_text(env, source_paths[i], args->source_paths[i]);
    }
}

size_t p101_doctor_append_source_paths(char *tool_argv[], size_t index, char source_paths[MAX_SOURCE_PATHS][PATH_LEN], int source_count)
{
    for(int i = 0; i < source_count; i++)
    {
        tool_argv[index] = source_paths[i];
        index++;
    }

    return index;
}

bool p101_doctor_resolve_compile_database(const struct p101_env *env, struct p101_error *err, const struct arguments *args, char path[PATH_LEN])
{
    bool found;

    P101_TRACE_SCOPE(env);
    path[0] = '\0';
    if(args->compile_db_path != NULL)
    {
        p101_doctor_copy_text(env, path, args->compile_db_path);
        found = true;
    }
    else
    {
        found = p101_c_facts_find_clang_compile_database(env, err, ".", path, PATH_LEN);
    }
    return found;
}
