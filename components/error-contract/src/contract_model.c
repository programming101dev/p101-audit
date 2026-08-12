#include "../include/contract_model.h"
#include "../include/contract_builder.h"
#include "../include/errors.h"
#include "../include/native_analysis.h"
#include "contract_event.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>

static void apply_fact(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_fact *fact);
static void add_function(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_fact *fact);
static void add_event(const struct p101_env *env, struct p101_error *err, struct contract_model *model, enum contract_event_kind kind, const struct p101_c_fact *fact);
static void set_function_contract(const struct p101_env *env, struct contract_model *model, const struct p101_c_fact *fact, bool is_env);
static bool record_ownership_role(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_fact *fact);
static bool fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line);

void p101_error_contract_load_facts(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct contract_model *model)
{
    P101_TRACE_SCOPE(env);
    if(args->facts_path == NULL)
    {
        p101_error_contract_load_analysis(env, err, args, model);
    }
    else
    {
        int    p101_expression_result_20;
        bool   p101_call_result_21;
        FILE  *stream;
        char   line[READ_BUF_LEN];
        size_t fact_count;

        stream     = p101_fopen(env, err, args->facts_path, "r");
        fact_count = 0U;

        for(;;)
        {
            const char *line_result;
            bool        p101_call_result_1;

            if(stream == NULL)
            {
                break;
            }
            line_result = p101_fgets(env, err, line, sizeof(line), stream);
            if(line_result == NULL)
            {
                break;
            }
            p101_call_result_1 = fact_line_is_complete(env, err, stream, line);
            if(p101_call_result_1)
            {
                struct p101_c_fact      fact;
                enum p101_c_fact_status status;

                status = p101_c_fact_parse_line(env, err, line, &fact);
                if(status != P101_C_FACT_OTHER)
                {
                    if(status != P101_C_FACT_OK)
                    {
                        P101_ERROR_RAISE_USER(err, "The saved fact stream contains an invalid record.", ERR_USAGE);
                        break;
                    }

                    apply_fact(env, err, model, &fact);
                    fact_count++;
                }
            }
        }

        if(stream != NULL)
        {
            bool p101_call_result_2;

            p101_call_result_2 = p101_error_has_error(err);
            if(p101_call_result_2)
            {
                p101_fclose(env, P101_ERROR_OPTIONAL, stream);    // P101_ERROR_OPTIONAL rationale: cleanup preserves the fact-loading error.
            }
            else
            {
                p101_fclose(env, err, stream);
            }
        }
        p101_call_result_21       = p101_error_has_no_error(err);
        p101_expression_result_20 = 0;
        if(p101_call_result_21)
        {
            if(fact_count == 0U)
            {
                p101_expression_result_20 = 1;
            }
        }
        if(p101_expression_result_20)
        {
            P101_ERROR_RAISE_USER(err, "The fact stream did not contain any p101 C facts.", ERR_USAGE);
        }
    }
}

static void apply_fact(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_fact *fact)
{
    P101_TRACE_SCOPE(env);

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
    switch(fact->kind)
    {
        case P101_C_FACT_KIND_UNKNOWN:
            break;
        case P101_C_FACT_KIND_FILE:
            model->files_scanned++;
            break;
        case P101_C_FACT_KIND_INCLUDE:
        case P101_C_FACT_KIND_PARAMETER:
        case P101_C_FACT_KIND_TYPE:
        case P101_C_FACT_KIND_ENUM:
        case P101_C_FACT_KIND_ENUMERATOR:
        case P101_C_FACT_KIND_MACRO:
            break;
        case P101_C_FACT_KIND_FUNCTION:
            if(!fact->is_declaration)
            {
                add_function(env, err, model, fact);
            }
            break;
        case P101_C_FACT_KIND_CALL:
        {
            bool p101_call_result_5;

            p101_call_result_5 = p101_error_contract_is_process_termination_call(env, fact->usr);
            if(p101_call_result_5)
            {
                add_event(env, err, model, CONTRACT_EVENT_PROCESS_TERMINATION, fact);
            }
            else if(fact->has_env_parameter || fact->has_error_parameter)
            {
                add_event(env, err, model, CONTRACT_EVENT_CALL, fact);
            }
        }
        break;
        case P101_C_FACT_KIND_NOTE:
        {
            bool                  p101_call_result_6;
            enum p101_c_note_kind note;

            note               = p101_c_note_kind_from_name(env, fact->value);
            p101_call_result_6 = record_ownership_role(env, err, model, fact);
            if(!p101_call_result_6)
            {
                if(note == P101_C_NOTE_TERMINATION_ADAPTER)
                {
                    p101_contract_model_set_termination_adapter(env, model, fact->path, fact->caller_usr);
                }
                else if(note == P101_C_NOTE_ENV_CONTRACT)
                {
                    set_function_contract(env, model, fact, true);
                }
                else if(note == P101_C_NOTE_ERROR_CONTRACT)
                {
                    set_function_contract(env, model, fact, false);
                }
                else
                {
                    enum contract_event_kind event_kind;
                    bool                     p101_call_result_7;

                    p101_call_result_7 = p101_contract_event_kind_from_note(note, &event_kind);
                    if(p101_call_result_7)
                    {
                        add_event(env, err, model, event_kind, fact);
                    }
                }
            }
        }
        break;
        default:
            break;
    }
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
}

static bool record_ownership_role(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_fact *fact)
{
    enum contract_ownership_kind kind;
    bool                         found;

    P101_TRACE_SCOPE(env);
    found = p101_contract_ownership_kind_from_role(env, fact->value, &kind);
    if(found)
    {
        p101_contract_model_record_ownership(env, err, model, fact->path, fact->line, kind);
    }
    return found;
}

static void add_function(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_fact *fact)
{
    P101_TRACE_SCOPE(env);
    p101_contract_model_add_function(env, err, model, fact->path, fact->value, fact->usr, fact->line, fact->start, fact->end, "Too many functions in fact stream.");
}

static void add_event(const struct p101_env *env, struct p101_error *err, struct contract_model *model, enum contract_event_kind kind, const struct p101_c_fact *fact)
{
    size_t event_start;

    P101_TRACE_SCOPE(env);
    event_start = fact->start;
    if((kind == CONTRACT_EVENT_FUNCTION_RETURN || kind == CONTRACT_EVENT_FUNCTION_EARLY_RETURN) && event_start == 0U)
    {
        event_start = fact->column;
    }
    p101_contract_model_add_event(env,
                                  err,
                                  model,
                                  kind,
                                  fact->path,
                                  fact->value,
                                  fact->caller,
                                  fact->usr,
                                  fact->caller_usr,
                                  fact->line,
                                  event_start,
                                  fact->end,
                                  (kind == CONTRACT_EVENT_CALL && fact->has_env_parameter) != 0,
                                  (kind == CONTRACT_EVENT_CALL && fact->has_error_parameter) != 0,
                                  "Too many events in fact stream.");
}

static void set_function_contract(const struct p101_env *env, struct contract_model *model, const struct p101_c_fact *fact, bool is_env)
{
    p101_contract_model_set_contract(env, model, fact->path, fact->caller_usr, is_env);
}

static bool fact_line_is_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line)
{
    int         p101_expression_result_22;
    const char *p101_call_result_23;
    const char *p101_call_result_4;
    const char *line_result;
    bool        complete;
    size_t      length;

    P101_TRACE_SCOPE(env);
    complete = true;
    length   = p101_strlen(env, line);

    if(length != READ_BUF_LEN - 1U)
    {
        p101_expression_result_22 = 1;
    }
    else
    {
        p101_call_result_23 = p101_strchr(env, line, '\n');
        if(p101_call_result_23 != NULL)
        {
            p101_expression_result_22 = 1;
        }
        else
        {
            p101_expression_result_22 = 0;
        }
    }
    if(p101_expression_result_22)
    {
        goto done;
    }

    {
        char discard[READ_BUF_LEN];

        complete = false;
        for(;;)
        {
            line_result = p101_fgets(env, err, discard, sizeof(discard), stream);
            if(line_result == NULL)
            {
                break;
            }
            p101_call_result_4 = p101_strchr(env, discard, '\n');
            if(p101_call_result_4 != NULL)
            {
                break;
            }
        }
    }

done:
    return complete;
}

#ifdef P101_ERROR_CONTRACT_TESTING
void p101_error_contract_test_apply_fact(const struct p101_env *env, struct p101_error *err, struct contract_model *model, const struct p101_c_fact *fact)
{
    apply_fact(env, err, model, fact);
}

bool p101_error_contract_test_fact_line_complete(const struct p101_env *env, struct p101_error *err, FILE *stream, char *line)
{
    return fact_line_is_complete(env, err, stream, line);
}

#endif
