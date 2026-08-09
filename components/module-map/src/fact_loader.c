#include "../include/fact_loader.h"
#include "../include/constants.h"
#include "../include/errors.h"
#include "../include/model_mutation.h"
#include "../include/model_notes.h"
#include "../include/native_analysis.h"
#include "../include/strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <stdbool.h>
#include <stdio.h>

static struct source_file *p101_module_map_find_fact_file(const struct p101_env *env, struct project_map *map, const char *path);
static struct source_file *p101_module_map_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact);
static void                p101_module_map_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact);
static bool                p101_module_map_fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line);

static struct source_file *p101_module_map_find_fact_file(const struct p101_env *env, struct project_map *map, const char *path)
{
    int                 p101_call_result_1;
    struct source_file *file;

    P101_TRACE_SCOPE(env);
    file = NULL;
    for(size_t i = 0; i < map->file_count; i++)
    {
        p101_call_result_1 = p101_strcmp(env, map->files[i].path, path);
        if(p101_call_result_1 == 0)
        {
            file = &map->files[i];
            goto done;
        }
    }

done:
    return file;
}

static struct source_file *p101_module_map_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    struct source_file *file;

    P101_TRACE_SCOPE(env);
    file = NULL;
    if(fact == NULL || fact->path == NULL)
    {
        goto done;
    }

    file = p101_module_map_find_fact_file(env, map, fact->path);
    if(file == NULL)
    {
        char module_name[MAX_NAME];

        p101_module_map_normalize_module_name(env, module_name, sizeof(module_name), fact->module);
        p101_module_map_add_named_source_file(env, err, map, fact->path, module_name, fact->is_header);
        file = p101_module_map_find_fact_file(env, map, fact->path);
    }

done:
    return file;
}

static void p101_module_map_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    const struct source_file *file;

    P101_TRACE_SCOPE(env);
    if(fact == NULL)
    {
        goto done;
    }

    file = p101_module_map_file_for_fact(env, err, map, fact);
    if(file == NULL)
    {
        goto done;
    }

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(fact->kind)
    {
        case P101_C_FACT_KIND_UNKNOWN:
        case P101_C_FACT_KIND_FILE:
            break;
        case P101_C_FACT_KIND_INCLUDE:
            p101_module_map_add_include(env, err, map, file, fact->value, fact->resolved, fact->line, fact->is_local);
            break;
        case P101_C_FACT_KIND_FUNCTION:
            /*
             * A non-defining declaration describes a module interface only
             * when it sits in a header. The forward declarations this house
             * style puts at the top of every .c file are a compilation-order
             * artefact, and recording them made each static helper look like
             * a public declaration with no definition.
             */
            if(!fact->is_declaration || file->is_header)
            {
                p101_module_map_add_function(env, err, map, file, fact->value, fact->usr, fact->line, fact->is_static, fact->is_declaration);
            }
            break;
        case P101_C_FACT_KIND_CALL:
            p101_module_map_add_call(env, err, map, file, fact->value, fact->usr, fact->caller_usr, fact->line);
            break;
        case P101_C_FACT_KIND_TYPE:
        case P101_C_FACT_KIND_ENUM:
            p101_module_map_add_type(env, err, map, file, fact->value, fact->line);
            break;
        case P101_C_FACT_KIND_ENUMERATOR:
            break;
        case P101_C_FACT_KIND_MACRO:
            /*
             * The fact stream also contains source-level macro expansions so
             * policy tools can associate them with their enclosing function.
             * A module's declared macro surface contains definitions only.
             */
            if(fact->is_definition)
            {
                p101_module_map_add_macro(env, err, map, file, fact->value, fact->line);
            }
            break;
        case P101_C_FACT_KIND_NOTE:
        {
            enum p101_c_note_kind note;

            note = p101_c_note_kind_from_name(env, fact->value);
            if(note == P101_C_NOTE_ERROR_USE)
            {
                p101_module_map_note_error_use(env, err, map, file);
            }
            else if(note == P101_C_NOTE_ERROR_CHECK)
            {
                p101_module_map_note_error_use(env, err, map, file);
                p101_module_map_note_error_check(env, err, map, file);
            }
            /*
             * The macro-hygiene and signal-handler notes are intentionally
             * absent: clang-tidy reports those, so storing them here would
             * only cost note budget for findings this tool no longer writes.
             */
            else if(note == P101_C_NOTE_SIGNATURE_ENV_ORDER || note == P101_C_NOTE_ALLOC_SIZEOF_TYPE)
            {
                p101_module_map_add_note(env, err, map, file, fact->value, fact->caller, fact->caller_usr, fact->line);
            }
        }
        break;
        default:
            break;
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif

done:
    return;
}

static bool p101_module_map_fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line)
{
    int    p101_expression_result_8;
    bool   complete;
    size_t length;

    P101_TRACE_SCOPE(env);
    complete = true;
    length   = p101_strlen(env, line);

    p101_expression_result_8 = 0;
    if(length == MAX_LINE - 1U)
    {
        const char *p101_call_result_9;

        p101_call_result_9 = p101_strchr(env, line, '\n');
        if(p101_call_result_9 == NULL)
        {
            p101_expression_result_8 = 1;
        }
    }
    if(p101_expression_result_8)
    {
        char discard[MAX_LINE];

        complete = false;
        for(;;)
        {
            const char *p101_call_result_2;
            const char *line_result;
            bool        no_error;

            no_error = p101_error_has_no_error(err);
            if(!no_error)
            {
                break;
            }
            line_result = p101_fgets(env, err, discard, sizeof(discard), stream);
            if(line_result == NULL)
            {
                break;
            }
            p101_call_result_2 = p101_strchr(env, discard, '\n');
            if(p101_call_result_2 != NULL)
            {
                break;
            }
        }
    }

    return complete;
}

#ifdef P101_MODULE_MAP_TESTING
struct source_file *p101_module_map_test_file_for_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    return p101_module_map_file_for_fact(env, err, map, fact);
}

void p101_module_map_test_apply_fact(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_fact *fact)
{
    p101_module_map_apply_fact(env, err, map, fact);
}

bool p101_module_map_test_fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line)
{
    return p101_module_map_fact_line_is_complete(env, err, stream, line);
}
#endif

void p101_module_map_load_clang_facts(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct arguments *args)
{
    int         p101_expression_result_10;
    int         p101_expression_result_11;
    bool        p101_call_result_12;
    bool        p101_call_result_3;
    bool        p101_call_result_4;
    bool        p101_call_result_5;
    const char *line_result;
    bool        no_error;
    FILE       *stream;
    char        line[MAX_LINE];
    size_t      fact_count;

    P101_TRACE_SCOPE(env);
    stream     = NULL;
    fact_count = 0U;
    if(args->facts_path == NULL)
    {
        p101_module_map_load_native_analysis(env, err, map, args);
        goto done;
    }

    stream = p101_fopen(env, err, args->facts_path, "r");
    if(stream == NULL)
    {
        goto done;
    }

    for(;;)
    {
        struct p101_c_fact      fact;
        enum p101_c_fact_status status;

        line_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(line_result == NULL)
        {
            break;
        }
        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
        p101_call_result_3 = p101_module_map_fact_line_is_complete(env, err, stream, line);
        if(!p101_call_result_3)
        {
            continue;
        }

        status = p101_c_fact_parse_line(env, err, line, &fact);
        if(status == P101_C_FACT_OTHER)
        {
            continue;
        }
        if(status != P101_C_FACT_OK)
        {
            p101_call_result_4 = p101_error_has_no_error(err);
            if(p101_call_result_4)    // GCOVR_EXCL_BR_LINE -- parser errors already populate err
            {
                P101_ERROR_RAISE_USER(err, "The saved fact stream contains an invalid module fact record.", ERR_USAGE);
            }
            break;
        }

        p101_module_map_apply_fact(env, err, map, &fact);
        fact_count++;
    }

done:
    if(stream != NULL)
    {
        p101_call_result_5 = p101_error_has_error(err);
        if(p101_call_result_5)
        {
            p101_fclose(env, P101_ERROR_OPTIONAL, stream);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the fact-loading error.
        }
        else
        {
            p101_fclose(env, err, stream);
        }
    }
    p101_expression_result_11 = 0;
    if(args->facts_path != NULL)
    {
        p101_call_result_12 = p101_error_has_no_error(err);
        if(p101_call_result_12)
        {
            p101_expression_result_11 = 1;
        }
    }
    p101_expression_result_10 = 0;
    if(p101_expression_result_11)
    {
        if(fact_count == 0U)
        {
            p101_expression_result_10 = 1;
        }
    }
    if(p101_expression_result_10)
    {
        P101_ERROR_RAISE_USER(err, "The fact stream did not contain any p101 C facts.", ERR_USAGE);
    }
}
