#include "cli.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/project.h>
#include <p101_record/record.h>

enum
{
    INPUT_LINE_SIZE = 1024
};

enum
{
    ALLOW_FIELD_PATH = 0,
    ALLOW_FIELD_CALLER,
    ALLOW_FIELD_CALLEE,
    ALLOW_FIELD_COUNT
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
        p101_fputs(env, err, "Emit P101FACT v6 records using lib_c_facts/libclang.\n", stderr);
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
        p101_fputs(env, err, "  --allow-usr USR         allow an exact external declaration identity\n", stderr);
        p101_fputs(env, err, "  --allow-file FILE       read TSV path/caller-USR/callee-USR rules\n", stderr);
        p101_fputs(env, err, "  --header-root PATH      trust public declarations and wrapper roles under PATH\n", stderr);
        p101_fputs(env, err, "  --show-inventory[-json] print the wrapper inventory\n", stderr);
        p101_fputs(env, err, "  --emit-module-facts     emit P101FACT v6 records\n", stderr);
        p101_fputs(env, err, "  --facts-output FILE     write a reusable fact snapshot\n", stderr);
        p101_fputs(env, err, "  --input-manifest FILE   write an admitted-input receipt\n", stderr);
        p101_fputs(env, err, "  --instrumentation-output FILE  write instrumentation coverage\n", stderr);
        p101_fputs(env, err, "  --mutation-candidates-output FILE  write exact mutation ranges\n", stderr);
        p101_fputs(env, err, "  --check-portability-includes  reject platform-specific headers\n", stderr);
    }
}

static bool add_value(const char **values, size_t *count, size_t capacity, const char *value)
{
    bool added;

    added = (*count < capacity && value != NULL && value[0] != '\0') != 0;
    if(added)
    {
        values[(*count)++] = value;
    }
    return added;
}

static bool add_allowed_copy(const struct p101_env *env, struct p101_wrapper_arguments *arguments, const char *value)
{
    size_t index;
    size_t length;
    bool   added;

    P101_TRACE_SCOPE(env);
    added = false;
    if(arguments->allowed_usr_count >= P101_WRAPPER_MAX_NAMES || value == NULL || value[0] == '\0')
    {
        goto done;
    }
    length = p101_strlen(env, value);
    if(length >= sizeof(arguments->allowed_usr_storage[0]))
    {
        goto done;
    }
    index = arguments->allowed_usr_count;
    p101_memcpy(env, arguments->allowed_usr_storage[index], value, length + 1U);
    arguments->allowed_usrs[index] = arguments->allowed_usr_storage[index];
    arguments->allowed_usr_count++;
    added = true;

done:
    return added;
}

static bool load_allow_file(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_arguments *arguments, const char *path)
{
    const char *p101_call_result_17;
    const char *line_result;
    FILE       *stream;
    char        line[INPUT_LINE_SIZE];
    bool        loaded;

    P101_TRACE_SCOPE(env);
    loaded = false;
    stream = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    for(;;)
    {
        char       *fields[ALLOW_FIELD_COUNT];
        const char *comment;
        char       *cursor;
        size_t      count;
        size_t      length;
        size_t      rule_index;
        bool        blank;

        line_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(line_result == NULL)
        {
            break;
        }
        length              = p101_strlen(env, line);
        p101_call_result_17 = p101_strchr(env, line, '\n');

        /*
         * A rule that filled the buffer without a newline was truncated by
         * p101_fgets; its tail must not resynchronise as another rule.
         */
        if(length == sizeof(line) - 1U && p101_call_result_17 == NULL)
        {
            P101_ERROR_RAISE_USER(err, "A wrapper boundary rule row is too long.", 1);
            break;
        }
        comment = p101_strchr(env, line, '#');
        if(comment != NULL)
        {
            length       = (size_t)(comment - line);
            line[length] = '\0';
        }
        while(length > 0U && (line[length - 1U] == '\n' || line[length - 1U] == '\r'))
        {
            length--;
            line[length] = '\0';
        }
        blank  = line[0] == '\0';
        cursor = line;
        for(count = 0U; count < ALLOW_FIELD_COUNT && cursor != NULL; count++)
        {
            fields[count] = p101_record_split(&cursor);
            p101_record_unescape_field(fields[count]);
        }
        if(count != ALLOW_FIELD_COUNT || cursor != NULL || fields[ALLOW_FIELD_PATH][0] == '\0' || fields[ALLOW_FIELD_CALLEE][0] == '\0')
        {
            if(blank)
            {
                continue;
            }
            P101_ERROR_RAISE_USER(err, "Boundary rules must use path<TAB>caller-USR<TAB>callee-USR form; caller-USR may be empty.", 1);
            break;
        }
        if(arguments->allow_rule_count >= P101_WRAPPER_MAX_NAMES)
        {
            P101_ERROR_RAISE_USER(err, "Too many wrapper boundary rules.", 1);
            break;
        }
        rule_index = arguments->allow_rule_count;
        p101_memset(env, &arguments->allow_rules[rule_index], 0, sizeof(arguments->allow_rules[rule_index]));
        p101_snprintf(env, err, arguments->allow_rules[rule_index].path, sizeof(arguments->allow_rules[rule_index].path), "%s", fields[ALLOW_FIELD_PATH]);
        p101_snprintf(env, err, arguments->allow_rules[rule_index].caller_usr, sizeof(arguments->allow_rules[rule_index].caller_usr), "%s", fields[ALLOW_FIELD_CALLER]);
        p101_snprintf(env, err, arguments->allow_rules[rule_index].callee_usr, sizeof(arguments->allow_rules[rule_index].callee_usr), "%s", fields[ALLOW_FIELD_CALLEE]);
        arguments->allow_rule_count++;
    }
    p101_fclose(env, err, stream);
    loaded = p101_error_has_no_error(err);

done:
    return loaded;
}

bool p101_wrapper_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct p101_wrapper_arguments *arguments, bool facts_only)
{
    int  p101_expression_result_18;
    int  p101_call_result_20;
    int  p101_expression_result_21;
    int  p101_call_result_23;
    int  p101_expression_result_24;
    int  p101_call_result_25;
    int  p101_call_result_26;
    int  p101_expression_result_27;
    int  p101_call_result_28;
    int  p101_expression_result_29;
    int  p101_call_result_30;
    int  p101_expression_result_31;
    bool p101_call_result_32;
    bool p101_call_result_33;
    int  p101_expression_result_34;
    int  p101_call_result_35;
    int  p101_expression_result_36;
    int  p101_call_result_37;
    int  p101_expression_result_38;
    int  p101_call_result_39;
    int  p101_expression_result_40;
    int  p101_call_result_41;
    int  p101_expression_result_42;
    int  p101_call_result_43;
    int  p101_expression_result_44;
    int  p101_call_result_45;
    int  p101_expression_result_46;
    int  p101_call_result_47;
    int  p101_expression_result_48;
    bool p101_call_result_49;
    int  p101_expression_result_50;
    int  p101_expression_result_51;
    int  p101_expression_result_53;
    int  p101_call_result_7;
    int  p101_call_result_8;
    int  p101_call_result_9;
    int  p101_call_result_10;
    int  p101_call_result_11;
    int  p101_call_result_12;
    int  p101_call_result_6;
    int  p101_call_result_5;
    bool p101_call_result_1;
    bool p101_call_result_2;
    bool p101_call_result_3;
    bool p101_call_result_4;
    int  index;
    char discovered[P101_WRAPPER_PATH_SIZE];
    bool valid;

    P101_TRACE_SCOPE(env);
    p101_memset(env, arguments, 0, sizeof(*arguments));
    valid = true;
    for(index = 1; index < argc && valid; index++)
    {
        const char *argument;
        int         p101_call_result_19;
        int         p101_call_result_22;

        argument            = argv[index];
        p101_call_result_19 = p101_strcmp(env, argument, "-h");
        if(p101_call_result_19 == 0)
        {
            p101_expression_result_18 = 1;
        }
        else
        {
            p101_call_result_20 = p101_strcmp(env, argument, "--help");
            if(p101_call_result_20 == 0)
            {
                p101_expression_result_18 = 1;
            }
            else
            {
                p101_expression_result_18 = 0;
            }
        }
        if(p101_expression_result_18)
        {
            valid = false;
            break;
        }
        p101_call_result_22 = p101_strcmp(env, argument, "-j");
        if(p101_call_result_22 == 0)
        {
            p101_expression_result_21 = 1;
        }
        else
        {
            p101_call_result_23 = p101_strcmp(env, argument, "--json");
            if(p101_call_result_23 == 0)
            {
                p101_expression_result_21 = 1;
            }
            else
            {
                p101_expression_result_21 = 0;
            }
        }
        if(p101_expression_result_21)
        {
            arguments->json = true;
        }
        else
        {
            p101_call_result_25 = p101_strcmp(env, argument, "-e");
            if(p101_call_result_25 == 0)
            {
                p101_expression_result_24 = 1;
            }
            else
            {
                p101_call_result_26 = p101_strcmp(env, argument, "--strict-external");
                if(p101_call_result_26 == 0)
                {
                    p101_expression_result_24 = 1;
                }
                else
                {
                    p101_expression_result_24 = 0;
                }
            }
            if(p101_expression_result_24)
            {
                arguments->strict_external = true;
            }
            else
            {
                p101_call_result_28       = p101_strcmp(env, argument, "--allow-usr");
                p101_expression_result_27 = 0;
                if(p101_call_result_28 == 0)
                {
                    if(index + 1 < argc)
                    {
                        p101_expression_result_27 = 1;
                    }
                }
                if(p101_expression_result_27)
                {
                    p101_call_result_1 = add_allowed_copy(env, arguments, argv[++index]);
                    if(!p101_call_result_1)
                    {
                        valid = false;
                    }
                }
                else
                {
                    p101_call_result_30       = p101_strcmp(env, argument, "--allow-file");
                    p101_expression_result_29 = 0;
                    if(p101_call_result_30 == 0)
                    {
                        if(index + 1 < argc)
                        {
                            p101_expression_result_29 = 1;
                        }
                    }
                    if(p101_expression_result_29)
                    {
                        const char *allow_path;

                        allow_path          = argv[++index];
                        p101_call_result_32 = add_value(arguments->allow_files, &arguments->allow_file_count, P101_WRAPPER_MAX_PATHS, allow_path);
                        if(!p101_call_result_32)
                        {
                            p101_expression_result_31 = 1;
                        }
                        else
                        {
                            p101_call_result_33 = load_allow_file(env, err, arguments, allow_path);
                            if(!p101_call_result_33)
                            {
                                p101_expression_result_31 = 1;
                            }
                            else
                            {
                                p101_expression_result_31 = 0;
                            }
                        }
                        if(p101_expression_result_31)
                        {
                            valid = false;
                        }
                    }
                    else
                    {
                        p101_call_result_35       = p101_strcmp(env, argument, "--compile-db");
                        p101_expression_result_34 = 0;
                        if(p101_call_result_35 == 0)
                        {
                            if(index + 1 < argc)
                            {
                                p101_expression_result_34 = 1;
                            }
                        }
                        if(p101_expression_result_34)
                        {
                            arguments->compile_database = argv[++index];
                        }
                        else
                        {
                            p101_call_result_5 = p101_strcmp(env, argument, "--compile-db-only");
                            if(p101_call_result_5 == 0)
                            {
                                arguments->compile_database_only = true;
                            }
                            else
                            {
                                p101_call_result_6 = p101_strcmp(env, argument, "--active-headers-only");
                                if(p101_call_result_6 == 0)
                                {
                                    arguments->active_headers_only = true;
                                }
                                else
                                {
                                    p101_call_result_37       = p101_strcmp(env, argument, "--cflag");
                                    p101_expression_result_36 = 0;
                                    if(p101_call_result_37 == 0)
                                    {
                                        if(index + 1 < argc)
                                        {
                                            p101_expression_result_36 = 1;
                                        }
                                    }
                                    if(p101_expression_result_36)
                                    {
                                        p101_call_result_2 = add_value(arguments->extra_arguments, &arguments->extra_argument_count, P101_WRAPPER_MAX_NAMES, argv[++index]);
                                        if(!p101_call_result_2)
                                        {
                                            valid = false;
                                        }
                                    }
                                    else
                                    {
                                        p101_call_result_7 = p101_strncmp(env, argument, "--cflag=", sizeof("--cflag=") - 1U);
                                        if(p101_call_result_7 == 0)
                                        {
                                            p101_call_result_3 = add_value(arguments->extra_arguments, &arguments->extra_argument_count, P101_WRAPPER_MAX_NAMES, argument + sizeof("--cflag=") - 1U);
                                            if(!p101_call_result_3)
                                            {
                                                valid = false;
                                            }
                                        }
                                        else
                                        {
                                            p101_call_result_39       = p101_strcmp(env, argument, "--header-root");
                                            p101_expression_result_38 = 0;
                                            if(p101_call_result_39 == 0)
                                            {
                                                if(index + 1 < argc)
                                                {
                                                    p101_expression_result_38 = 1;
                                                }
                                            }
                                            if(p101_expression_result_38)
                                            {
                                                p101_call_result_4 = add_value(arguments->header_roots, &arguments->header_root_count, P101_WRAPPER_MAX_PATHS, argv[++index]);
                                                if(!p101_call_result_4)
                                                {
                                                    valid = false;
                                                }
                                            }
                                            else
                                            {
                                                p101_call_result_8 = p101_strcmp(env, argument, "--keep-going");
                                                if(p101_call_result_8 == 0)
                                                {
                                                    arguments->keep_going = true;
                                                }
                                                else
                                                {
                                                    p101_call_result_9 = p101_strcmp(env, argument, "--show-inventory");
                                                    if(p101_call_result_9 == 0)
                                                    {
                                                        arguments->show_inventory = true;
                                                    }
                                                    else
                                                    {
                                                        p101_call_result_10 = p101_strcmp(env, argument, "--show-inventory-json");
                                                        if(p101_call_result_10 == 0)
                                                        {
                                                            arguments->show_inventory_json = true;
                                                        }
                                                        else
                                                        {
                                                            p101_call_result_11 = p101_strcmp(env, argument, "--emit-module-facts");
                                                            if(p101_call_result_11 == 0)
                                                            {
                                                                arguments->emit_facts = true;
                                                            }
                                                            else
                                                            {
                                                                p101_call_result_41       = p101_strcmp(env, argument, "--facts-output");
                                                                p101_expression_result_40 = 0;
                                                                if(p101_call_result_41 == 0)
                                                                {
                                                                    if(index + 1 < argc)
                                                                    {
                                                                        p101_expression_result_40 = 1;
                                                                    }
                                                                }
                                                                if(p101_expression_result_40)
                                                                {
                                                                    arguments->facts_output = argv[++index];
                                                                }
                                                                else
                                                                {
                                                                    p101_call_result_43       = p101_strcmp(env, argument, "--input-manifest");
                                                                    p101_expression_result_42 = 0;
                                                                    if(p101_call_result_43 == 0)
                                                                    {
                                                                        if(index + 1 < argc)
                                                                        {
                                                                            p101_expression_result_42 = 1;
                                                                        }
                                                                    }
                                                                    if(p101_expression_result_42)
                                                                    {
                                                                        arguments->input_manifest = argv[++index];
                                                                    }
                                                                    else
                                                                    {
                                                                        p101_call_result_45       = p101_strcmp(env, argument, "--instrumentation-output");
                                                                        p101_expression_result_44 = 0;
                                                                        if(p101_call_result_45 == 0)
                                                                        {
                                                                            if(index + 1 < argc)
                                                                            {
                                                                                p101_expression_result_44 = 1;
                                                                            }
                                                                        }
                                                                        if(p101_expression_result_44)
                                                                        {
                                                                            arguments->instrumentation_output = argv[++index];
                                                                        }
                                                                        else
                                                                        {
                                                                            p101_call_result_47       = p101_strcmp(env, argument, "--mutation-candidates-output");
                                                                            p101_expression_result_46 = 0;
                                                                            if(p101_call_result_47 == 0)
                                                                            {
                                                                                if(index + 1 < argc)
                                                                                {
                                                                                    p101_expression_result_46 = 1;
                                                                                }
                                                                            }
                                                                            if(p101_expression_result_46)
                                                                            {
                                                                                arguments->mutation_output = argv[++index];
                                                                            }
                                                                            else
                                                                            {
                                                                                p101_call_result_12 = p101_strcmp(env, argument, "--check-portability-includes");
                                                                                if(argument[0] == '-')
                                                                                {
                                                                                    p101_expression_result_48 = 1;
                                                                                }
                                                                                else
                                                                                {
                                                                                    p101_call_result_49 = add_value(arguments->paths, &arguments->path_count, P101_WRAPPER_MAX_PATHS, argument);
                                                                                    if(!p101_call_result_49)
                                                                                    {
                                                                                        p101_expression_result_48 = 1;
                                                                                    }
                                                                                    else
                                                                                    {
                                                                                        p101_expression_result_48 = 0;
                                                                                    }
                                                                                }
                                                                                if(p101_call_result_12 == 0)
                                                                                {
                                                                                    arguments->check_portability = true;
                                                                                }
                                                                                else if(p101_expression_result_48)
                                                                                {
                                                                                    valid = false;
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if(valid && arguments->path_count == 0U)
    {
        arguments->paths[arguments->path_count++] = ".";
    }
    /* P101_ERROR_OPTIONAL rationale: automatic discovery is an optional convenience. */
    p101_expression_result_51 = 0;
    if(valid)
    {
        if(arguments->compile_database == NULL)
        {
            p101_expression_result_51 = 1;
        }
    }
    p101_expression_result_50 = 0;
    if(p101_expression_result_51)
    {
        bool p101_call_result_52;

        p101_call_result_52 = p101_c_facts_find_clang_compile_database(env, P101_ERROR_OPTIONAL, arguments->paths[0], discovered, sizeof(discovered));
        if(p101_call_result_52)
        {
            p101_expression_result_50 = 1;
        }
    }
    if(p101_expression_result_50)
    {
        p101_snprintf(env, err, arguments->compile_database_storage, sizeof(arguments->compile_database_storage), "%s", discovered);
        arguments->compile_database = arguments->compile_database_storage;
    }
    if(valid && arguments->compile_database_only && arguments->compile_database == NULL)
    {
        P101_ERROR_RAISE_USER(err, "--compile-db-only requires a compilation database.", 1);
        valid = false;
    }
    if(facts_only)
    {
        arguments->emit_facts = true;
    }
    p101_expression_result_53 = 0;
    if(valid)
    {
        bool p101_call_result_54;

        p101_call_result_54 = p101_error_has_no_error(err);
        if(p101_call_result_54)
        {
            p101_expression_result_53 = 1;
        }
    }
    valid = p101_expression_result_53 != 0;
    return valid;
}
