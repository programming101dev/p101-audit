#include "cli.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/project.h>

enum
{
    INPUT_LINE_SIZE = 1024
};

static bool add_value(const char **values, size_t *count, size_t capacity, const char *value);
static bool add_allowed_copy(const struct p101_env *env, struct p101_wrapper_arguments *arguments, const char *value);
static bool load_allow_file(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_arguments *arguments, const char *path);

void p101_wrapper_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, bool facts_only)
{
    P101_TRACE_SCOPE(env);
    p101_fprintf(env, err, stderr, "Usage: %s [options] [path...]\n", program_name);
    if(facts_only)
    {
        p101_fputs(env, err, "Emit P101FACT v2 records using lib_c_facts/libclang.\n", stderr);
    }
    else
    {
        p101_fputs(env, err, "Audit calls that cross outside the local/p101 wrapper boundary.\n", stderr);
    }
    p101_fputs(env, err, "  --compile-db FILE       use a compilation database\n", stderr);
    p101_fputs(env, err, "  --compile-db-only       admit only active translation units\n", stderr);
    p101_fputs(env, err, "  --cflag FLAG            add a parser argument; repeatable\n", stderr);
    p101_fputs(env, err, "  --keep-going            report incomplete parsing instead of stopping\n", stderr);
    if(!facts_only)
    {
        p101_fputs(env, err, "  -j, --json              emit JSON findings\n", stderr);
        p101_fputs(env, err, "  -e, --strict-external   fail for external and indirect calls\n", stderr);
        p101_fputs(env, err, "  -a, --allow NAME        allow an external callee\n", stderr);
        p101_fputs(env, err, "  --allow-file FILE       read path:function:callee boundary rules\n", stderr);
        p101_fputs(env, err, "  --header-root DIR       add a wrapper inventory root\n", stderr);
        p101_fputs(env, err, "  --show-inventory[-json] print the wrapper inventory\n", stderr);
        p101_fputs(env, err, "  --emit-module-facts     emit P101FACT v2 records\n", stderr);
        p101_fputs(env, err, "  --facts-output FILE     write a reusable fact snapshot\n", stderr);
        p101_fputs(env, err, "  --input-manifest FILE   write an admitted-input receipt\n", stderr);
        p101_fputs(env, err, "  --instrumentation-output FILE  write instrumentation coverage\n", stderr);
        p101_fputs(env, err, "  --mutation-candidates-output FILE  write exact mutation ranges\n", stderr);
        p101_fputs(env, err, "  --check-portability-includes  reject platform-specific headers\n", stderr);
    }
}

static bool add_value(const char **values, size_t *count, size_t capacity, const char *value)
{
    if(*count >= capacity || value == NULL || value[0] == '\0')
    {
        return false;
    }
    values[(*count)++] = value;
    return true;
}

static bool add_allowed_copy(const struct p101_env *env, struct p101_wrapper_arguments *arguments, const char *value)
{
    size_t index;
    size_t length;

    P101_TRACE_SCOPE(env);
    if(arguments->allowed_count >= P101_WRAPPER_MAX_NAMES || value == NULL || value[0] == '\0')
    {
        return false;
    }
    length = p101_strlen(env, value);
    if(length >= sizeof(arguments->allowed_storage[0]))
    {
        return false;
    }
    index = arguments->allowed_count;
    p101_memcpy(env, arguments->allowed_storage[index], value, length + 1U);
    arguments->allowed[index] = arguments->allowed_storage[index];
    arguments->allowed_count++;
    return true;
}

static bool load_allow_file(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_arguments *arguments, const char *path)
{
    FILE *stream;
    char  line[INPUT_LINE_SIZE];

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        return false;
    }
    while(p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        const char *comment;
        const char *newline;
        const char *first;
        const char *last;

        comment = p101_strchr(env, line, '#');
        if(comment != NULL)
        {
            line[(size_t)(comment - line)] = '\0';
        }
        newline = p101_strchr(env, line, '\n');
        if(newline != NULL)
        {
            line[(size_t)(newline - line)] = '\0';
        }
        first = p101_strchr(env, line, ':');
        last  = p101_strrchr(env, line, ':');
        if(first != NULL && last != NULL && first != last && first[1] != '\0' && last[1] != '\0')
        {
            size_t rule_index;

            if(arguments->allow_rule_count >= P101_WRAPPER_MAX_NAMES)
            {
                P101_ERROR_RAISE_USER(err, "Too many wrapper boundary rules.", 1);
                break;
            }
            rule_index                   = arguments->allow_rule_count;
            line[(size_t)(first - line)] = '\0';
            line[(size_t)(last - line)]  = '\0';
            p101_memset(env, &arguments->allow_rules[rule_index], 0, sizeof(arguments->allow_rules[rule_index]));
            p101_snprintf(env, err, arguments->allow_rules[rule_index].path, sizeof(arguments->allow_rules[rule_index].path), "%s", line);
            p101_snprintf(env, err, arguments->allow_rules[rule_index].function, sizeof(arguments->allow_rules[rule_index].function), "%s", first + 1);
            p101_snprintf(env, err, arguments->allow_rules[rule_index].callee, sizeof(arguments->allow_rules[rule_index].callee), "%s", last + 1);
            arguments->allow_rule_count++;
        }
        else if(line[0] != '\0')
        {
            P101_ERROR_RAISE_USER(err, "Boundary rules must use path:function:callee form.", 1);
            break;
        }
    }
    p101_fclose(env, err, stream);
    return p101_error_has_no_error(err);
}

bool p101_wrapper_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct p101_wrapper_arguments *arguments, bool facts_only)
{
    int  index;
    char discovered[P101_WRAPPER_PATH_SIZE];

    P101_TRACE_SCOPE(env);
    p101_memset(env, arguments, 0, sizeof(*arguments));
    for(index = 1; index < argc; index++)
    {
        const char *argument;

        argument = argv[index];
        if(p101_strcmp(env, argument, "-h") == 0 || p101_strcmp(env, argument, "--help") == 0)
        {
            return false;
        }
        if(p101_strcmp(env, argument, "-j") == 0 || p101_strcmp(env, argument, "--json") == 0)
        {
            arguments->json = true;
        }
        else if(p101_strcmp(env, argument, "-e") == 0 || p101_strcmp(env, argument, "--strict-external") == 0)
        {
            arguments->strict_external = true;
        }
        else if((p101_strcmp(env, argument, "-a") == 0 || p101_strcmp(env, argument, "--allow") == 0) && index + 1 < argc)
        {
            if(!add_allowed_copy(env, arguments, argv[++index]))
            {
                return false;
            }
        }
        else if(p101_strcmp(env, argument, "--allow-file") == 0 && index + 1 < argc)
        {
            const char *allow_path;

            allow_path = argv[++index];
            if(!add_value(arguments->allow_files, &arguments->allow_file_count, P101_WRAPPER_MAX_PATHS, allow_path) || !load_allow_file(env, err, arguments, allow_path))
            {
                return false;
            }
        }
        else if(p101_strcmp(env, argument, "--compile-db") == 0 && index + 1 < argc)
        {
            arguments->compile_database = argv[++index];
        }
        else if(p101_strcmp(env, argument, "--compile-db-only") == 0)
        {
            arguments->compile_database_only = true;
        }
        else if(p101_strcmp(env, argument, "--active-headers-only") == 0)
        {
            arguments->active_headers_only = true;
        }
        else if(p101_strcmp(env, argument, "--cflag") == 0 && index + 1 < argc)
        {
            if(!add_value(arguments->extra_arguments, &arguments->extra_argument_count, P101_WRAPPER_MAX_NAMES, argv[++index]))
            {
                return false;
            }
        }
        else if(p101_strncmp(env, argument, "--cflag=", sizeof("--cflag=") - 1U) == 0)
        {
            if(!add_value(arguments->extra_arguments, &arguments->extra_argument_count, P101_WRAPPER_MAX_NAMES, argument + sizeof("--cflag=") - 1U))
            {
                return false;
            }
        }
        else if(p101_strcmp(env, argument, "--header-root") == 0 && index + 1 < argc)
        {
            if(!add_value(arguments->header_roots, &arguments->header_root_count, P101_WRAPPER_MAX_PATHS, argv[++index]))
            {
                return false;
            }
        }
        else if(p101_strcmp(env, argument, "--keep-going") == 0)
        {
            arguments->keep_going = true;
        }
        else if(p101_strcmp(env, argument, "--show-inventory") == 0)
        {
            arguments->show_inventory = true;
        }
        else if(p101_strcmp(env, argument, "--show-inventory-json") == 0)
        {
            arguments->show_inventory_json = true;
        }
        else if(p101_strcmp(env, argument, "--emit-module-facts") == 0)
        {
            arguments->emit_facts = true;
        }
        else if(p101_strcmp(env, argument, "--facts-output") == 0 && index + 1 < argc)
        {
            arguments->facts_output = argv[++index];
        }
        else if(p101_strcmp(env, argument, "--input-manifest") == 0 && index + 1 < argc)
        {
            arguments->input_manifest = argv[++index];
        }
        else if(p101_strcmp(env, argument, "--instrumentation-output") == 0 && index + 1 < argc)
        {
            arguments->instrumentation_output = argv[++index];
        }
        else if(p101_strcmp(env, argument, "--mutation-candidates-output") == 0 && index + 1 < argc)
        {
            arguments->mutation_output = argv[++index];
        }
        else if(p101_strcmp(env, argument, "--check-portability-includes") == 0)
        {
            arguments->check_portability = true;
        }
        else if(argument[0] == '-' || !add_value(arguments->paths, &arguments->path_count, P101_WRAPPER_MAX_PATHS, argument))
        {
            return false;
        }
    }
    if(arguments->path_count == 0U)
    {
        arguments->paths[arguments->path_count++] = ".";
    }
    /* P101_ERROR_CONTRACT_ALLOW_NO_ERROR: automatic discovery is an optional convenience. */
    if(arguments->compile_database == NULL && p101_c_facts_find_clang_compile_database(env, NULL, arguments->paths[0], discovered, sizeof(discovered)))
    {
        p101_snprintf(env, err, arguments->compile_database_storage, sizeof(arguments->compile_database_storage), "%s", discovered);
        arguments->compile_database = arguments->compile_database_storage;
    }
    if(arguments->compile_database_only && arguments->compile_database == NULL)
    {
        P101_ERROR_RAISE_USER(err, "--compile-db-only requires a compilation database.", 1);
        return false;
    }
    if(facts_only)
    {
        arguments->emit_facts = true;
    }
    return p101_error_has_no_error(err);
}
