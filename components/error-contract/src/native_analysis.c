#include "../include/native_analysis.h"
#include "../include/contract_builder.h"
#include "../include/errors.h"
#include "contract_event.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/analysis.h>
#include <p101_c_facts/facts.h>
#include <p101_c_facts/project.h>

static bool apply_record(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context);
static void apply_note(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_analysis_record *record);

void p101_error_contract_load_analysis(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct contract_model *model)
{
    int                            p101_expression_result_13;
    bool                           p101_call_result_14;
    bool                           p101_call_result_1;
    bool                           p101_call_result_2;
    bool                           p101_call_result_3;
    static const char              default_path[] = DEFAULT_SOURCE_PATH;
    const char                    *paths[P101_ERROR_CONTRACT_MAX_PATHS];
    size_t                         path_count;
    char                           discovered_compile_db[CONTRACT_PATH_LEN];
    const char                    *compile_db;
    struct p101_c_analysis_options options;

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
    compile_db                = args->compile_db_path;
    p101_expression_result_13 = 0;
    if(compile_db == NULL)
    {
        p101_call_result_14 = p101_c_facts_find_clang_compile_database(env, err, ".", discovered_compile_db, sizeof(discovered_compile_db));
        if(p101_call_result_14)
        {
            p101_expression_result_13 = 1;
        }
    }
    if(p101_expression_result_13)
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
        p101_fprintf(env, err, stderr, "audit-errors: native lib_c_facts scan (%zu path%s%s)\n", path_count, path_count == 1U ? "" : "s", compile_db == NULL ? "" : ", compile database active");
    }
    p101_call_result_2 = p101_error_has_no_error(err);
    if(p101_call_result_2)
    {
        p101_call_result_3 = p101_c_analysis_scan(env, err, &options, apply_record, model);
        (void)p101_call_result_3;
    }

done:
    return;
}

static bool apply_record(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context)
{
    bool                   p101_call_result_5;
    struct contract_model *model;

    P101_TRACE_SCOPE(env);
    model = (struct contract_model *)context;
    if(record->kind == P101_C_ANALYSIS_FILE)
    {
        model->files_scanned++;
    }
    else if(record->kind == P101_C_ANALYSIS_FUNCTION && record->is_definition)
    {
        p101_contract_model_add_function(env, err, model, record->path, record->name, record->usr, record->line, record->start_offset, record->end_offset, "Too many functions in native analysis.");
    }
    else if(record->kind == P101_C_ANALYSIS_CALL)
    {
        bool p101_call_result_4;

        p101_call_result_4 = p101_error_contract_is_process_termination_call(env, record->usr);
        if(p101_call_result_4)
        {
            p101_contract_model_add_event(env,
                                          err,
                                          model,
                                          CONTRACT_EVENT_PROCESS_TERMINATION,
                                          record->path,
                                          record->name,
                                          record->caller,
                                          record->usr,
                                          record->caller_usr,
                                          record->line,
                                          record->start_offset,
                                          record->end_offset,
                                          false,
                                          false,
                                          "Too many events in native analysis.");
        }
        else if(record->has_env_parameter || record->has_error_parameter)
        {
            p101_contract_model_add_event(env,
                                          err,
                                          model,
                                          CONTRACT_EVENT_CALL,
                                          record->path,
                                          record->name,
                                          record->caller,
                                          record->usr,
                                          record->caller_usr,
                                          record->line,
                                          record->start_offset,
                                          record->end_offset,
                                          record->has_env_parameter,
                                          record->has_error_parameter,
                                          "Too many events in native analysis.");
        }
    }
    else if(record->kind == P101_C_ANALYSIS_NOTE)
    {
        apply_note(env, err, model, record);
    }
    else if(record->kind == P101_C_ANALYSIS_DIAGNOSTIC)
    {
        P101_ERROR_RAISE_USER(err, record->name == NULL ? "lib_c_facts could not parse an admitted source file." : record->name, ERR_USAGE);
    }
    p101_call_result_5 = p101_error_has_no_error(err);
    return p101_call_result_5;
}

static void apply_note(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_analysis_record *record)
{
    enum p101_c_note_kind        note;
    enum contract_event_kind     kind;
    enum contract_ownership_kind ownership;

    note = p101_c_note_kind_from_name(env, record->name);
    if(note == P101_C_NOTE_TERMINATION_ADAPTER)
    {
        p101_contract_model_set_termination_adapter(env, model, record->path, record->caller_usr);
    }
    else
    {
        bool p101_call_result_9;

        p101_call_result_9 = p101_contract_ownership_kind_from_role(env, record->name, &ownership);
        if(p101_call_result_9)
        {
            p101_contract_model_record_ownership(env, err, model, record->path, record->line, ownership);
        }
        else
        {
            if(note == P101_C_NOTE_ENV_CONTRACT)
            {
                p101_contract_model_set_contract(env, model, record->path, record->caller_usr, true);
            }
            else if(note == P101_C_NOTE_ERROR_CONTRACT)
            {
                p101_contract_model_set_contract(env, model, record->path, record->caller_usr, false);
            }
            else
            {
                bool p101_call_result_12;

                p101_call_result_12 = p101_contract_event_kind_from_note(note, &kind);
                if(p101_call_result_12)
                {
                    size_t event_start;

                    event_start = record->start_offset;
                    if((kind == CONTRACT_EVENT_FUNCTION_RETURN || kind == CONTRACT_EVENT_FUNCTION_EARLY_RETURN) && event_start == 0U)
                    {
                        event_start = record->column;
                    }
                    p101_contract_model_add_event(env, err, model, kind, record->path, record->name, record->caller, record->usr, record->caller_usr, record->line, event_start, record->end_offset, false, false, "Too many events in native analysis.");
                }
            }
        }
    }
}

bool p101_error_contract_is_process_termination_call(const struct p101_env *env, const char *usr)
{
    static const char *const termination_function_usrs[] = {
        "c:@F@_Exit",
        "c:@F@_exit",
        "c:@F@abort",
        "c:@F@err",
        "c:@F@errx",
        "c:@F@exit",
        "c:@F@quick_exit",
        "c:@F@verr",
        "c:@F@verrx",
    };
    bool found;

    found = false;
    for(size_t index = 0U; usr != NULL && index < sizeof(termination_function_usrs) / sizeof(termination_function_usrs[0]) && !found; index++)
    {
        int p101_call_result_8;

        p101_call_result_8 = p101_strcmp(env, usr, termination_function_usrs[index]);
        if(p101_call_result_8 == 0)
        {
            found = true;
        }
    }
    return found;
}
