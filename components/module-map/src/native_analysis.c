#include "../include/native_analysis.h"
#include "../include/constants.h"
#include "../include/errors.h"
#include "../include/model_mutation.h"
#include "../include/model_notes.h"
#include "../include/strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/analysis.h>
#include <p101_c_facts/facts.h>
#include <p101_c_facts/project.h>

/*
 * The scan needs one fact the record stream cannot carry: whether a compile
 * database was in play. Without one, libclang parses with no -I flags and the
 * first local header fails to resolve, which reads as "your include is broken"
 * when the real cause is an unbuilt repo. Carry the flag so the diagnostic can
 * say which it is.
 */
struct native_scan
{
    struct project_map *map;
    bool                compile_database_active;
};

static bool                apply_record(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context);
static struct source_file *file_for_record(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_analysis_record *record);

void p101_module_map_load_native_analysis(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct arguments *args)
{
    int                            p101_expression_result_8;
    bool                           p101_call_result_9;
    bool                           p101_call_result_1;
    bool                           p101_call_result_2;
    bool                           p101_call_result_3;
    static const char              default_path[] = ".";
    const char                    *paths[P101_MODULE_MAP_MAX_PATHS];
    size_t                         path_count;
    char                           discovered_compile_db[PATH_LEN];
    const char                    *compile_db;
    struct p101_c_analysis_options options;
    struct native_scan             scan;

    P101_TRACE_SCOPE(env);
    path_count = args->path_count;
    if(path_count == 0U)
    {
        paths[0]   = default_path;
        path_count = 1U;
    }
    else
    {
        for(size_t index = 0U; index < path_count; index++)
        {
            paths[index] = args->paths[index];
        }
    }

    compile_db               = args->compile_db_path;
    p101_expression_result_8 = 0;
    if(compile_db == NULL)
    {
        p101_call_result_9 = p101_c_facts_find_clang_compile_database(env, err, ".", discovered_compile_db, sizeof(discovered_compile_db));
        if(p101_call_result_9)
        {
            p101_expression_result_8 = 1;
        }
    }
    if(p101_expression_result_8)
    {
        compile_db = discovered_compile_db;
    }
    p101_call_result_1 = p101_error_has_error(err);
    if(p101_call_result_1)
    {
        goto done;
    }

    p101_memset(env, &options, 0, sizeof(options));
    options.compile_database                     = compile_db;
    options.paths                                = paths;
    options.path_count                           = path_count;
    options.compile_database_only                = compile_db != NULL;
    options.detailed_preprocessing               = true;
    options.include_headers_as_translation_units = false;
    options.keep_going                           = false;
    if(args->verbose)
    {
        p101_fprintf(env, err, stderr, "audit-modules: native lib_c_facts scan (%zu path%s%s)\n", path_count, path_count == 1U ? "" : "s", compile_db == NULL ? "" : ", compile database active");
    }
    scan.map                     = map;
    scan.compile_database_active = compile_db != NULL;
    p101_call_result_2           = p101_error_has_no_error(err);
    if(p101_call_result_2)
    {
        p101_call_result_3 = p101_c_analysis_scan(env, err, &options, apply_record, &scan);
        (void)p101_call_result_3;
    }

done:
    return;
}

static bool apply_record(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context)
{
    struct native_scan       *scan;
    struct project_map       *map;
    const struct source_file *file;
    bool                      keep_going;

    P101_TRACE_SCOPE(env);
    scan       = (struct native_scan *)context;
    map        = scan->map;
    keep_going = false;
    if(record->kind == P101_C_ANALYSIS_DIAGNOSTIC)
    {
        const char *detail;

        detail = record->name == NULL ? "lib_c_facts could not parse an admitted source file." : record->name;
        if(scan->compile_database_active)
        {
            P101_ERROR_RAISE_USER(err, detail, ERR_USAGE);
        }
        else
        {
            char message[MAX_LINE];
            int  written;

            /*
             * No compile database means no include paths, so a repo's own
             * public header is the first thing to fail. Point at the missing
             * build rather than at the include.
             */
            written = p101_snprintf(env, err, message, sizeof(message), "%s (no compile database was found; build this repository first or pass -C <compile_commands.json>)", detail);
            if(written < 0 || (size_t)written >= sizeof(message))
            {
                P101_ERROR_RAISE_USER(err, detail, ERR_USAGE);
            }
            else
            {
                P101_ERROR_RAISE_USER(err, message, ERR_USAGE);
            }
        }
        goto done;
    }

    file = file_for_record(env, err, map, record);
    if(file == NULL)
    {
        keep_going = p101_error_has_no_error(err);
        goto done;
    }

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(record->kind)
    {
        case P101_C_ANALYSIS_FILE:
        case P101_C_ANALYSIS_PARAMETER:
        case P101_C_ANALYSIS_MUTATION:
        case P101_C_ANALYSIS_DIAGNOSTIC:
            break;
        case P101_C_ANALYSIS_INCLUDE:
            p101_module_map_add_include(env, err, map, file, record->name, record->resolved_include, record->line, record->is_local_include);
            break;
        case P101_C_ANALYSIS_FUNCTION:
        {
            bool declaration;

            declaration = true;
            if(record->is_definition)
            {
                declaration = false;
            }
            /*
             * A non-defining declaration describes a module interface only
             * when it sits in a header. The forward declarations this house
             * style puts at the top of every .c file are a compilation-order
             * artefact, and recording them made each static helper look like
             * a public declaration with no definition.
             */
            if(!declaration || file->is_header)
            {
                p101_module_map_add_function(env, err, map, file, record->name, record->usr, record->line, record->is_static, declaration);
            }
            break;
        }
        case P101_C_ANALYSIS_CALL:
            p101_module_map_add_call(env, err, map, file, record->name, record->usr, record->caller_usr, record->line);
            break;
        case P101_C_ANALYSIS_TYPE:
        case P101_C_ANALYSIS_ENUM:
            p101_module_map_add_type(env, err, map, file, record->name, record->line);
            break;
        case P101_C_ANALYSIS_ENUMERATOR:
            break;
        case P101_C_ANALYSIS_MACRO:
            /*
             * The analysis also reports source-level macro expansions so
             * policy tools can associate them with their enclosing function.
             * A module's declared macro surface contains definitions only.
             */
            if(record->is_definition)
            {
                p101_module_map_add_macro(env, err, map, file, record->name, record->line);
            }
            break;
        case P101_C_ANALYSIS_NOTE:
        {
            enum p101_c_note_kind note;

            note = p101_c_note_kind_from_name(env, record->name);
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
             * The idiom notes must be recorded here as well as in the fact
             * stream loader. This is the path taken when no -i snapshot is
             * given, which is how the tool is normally run; without this the
             * idiom rules only ever fire against a recorded fact file.
             *
             * The macro-hygiene and signal-handler notes are absent on
             * purpose: clang-tidy reports those, so storing them here would
             * only cost note budget for findings this tool no longer writes.
             */
            else if(note == P101_C_NOTE_SIGNATURE_ENV_ORDER || note == P101_C_NOTE_ALLOC_SIZEOF_TYPE)
            {
                p101_module_map_add_note(env, err, map, file, record->name, record->caller, record->caller_usr, record->line);
            }
        }
        break;
        default:
            break;
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
    keep_going = p101_error_has_no_error(err);

done:
    return keep_going;
}

static struct source_file *file_for_record(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct p101_c_analysis_record *record)
{
    int                 p101_call_result_4;
    int                 p101_call_result_5;
    struct source_file *file;
    char                module_name[MAX_NAME];

    file = NULL;
    if(record->path == NULL)
    {
        goto done;
    }
    for(size_t index = 0U; index < map->file_count; index++)
    {
        p101_call_result_4 = p101_strcmp(env, map->files[index].path, record->path);
        if(p101_call_result_4 == 0)
        {
            file = &map->files[index];
            goto done;
        }
    }

    /*
     * The fact-stream loader normalizes the producer's short module id; here
     * the only thing on hand is a filesystem path, and normalize_module_name
     * deliberately keeps the directory prefix. Feeding it a path made every
     * module id an absolute path, so an include of "errors" could never match
     * the module named "/.../include/p101_fsm/errors". Derive the same short
     * name the producer would emit.
     */
    p101_module_map_basename_no_suffix(env, module_name, sizeof(module_name), record->path);
    p101_module_map_add_named_source_file(env, err, map, record->path, module_name, record->is_header);
    for(size_t index = 0U; index < map->file_count; index++)
    {
        p101_call_result_5 = p101_strcmp(env, map->files[index].path, record->path);
        if(p101_call_result_5 == 0)
        {
            file = &map->files[index];
            break;
        }
    }

done:
    return file;
}
