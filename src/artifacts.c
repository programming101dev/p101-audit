#include "output.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <stddef.h>

struct instrumentation_capabilities
{
    bool trace_entry;
    bool trace_exit;
    bool fault;
    bool fd;
    bool allocation;
    bool resource;
};

static void add_role_capability(const struct p101_env *env, struct instrumentation_capabilities *capabilities, const char *role)
{
    static const char *const ROLE_NAMES[] = {
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:trace-entry",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:trace-exit",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:fault",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:fd",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:allocation",
        "CALLEE_SEMANTIC_ROLE:p101:instrumentation:resource",
    };

    /* Parallel to ROLE_NAMES: each entry is the flag that role sets. */
    bool *const ROLE_FLAGS[] = {
        &capabilities->trace_entry,
        &capabilities->trace_exit,
        &capabilities->fault,
        &capabilities->fd,
        &capabilities->allocation,
        &capabilities->resource,
    };

    P101_TRACE_SCOPE(env);
    for(size_t index = 0U; index < sizeof(ROLE_NAMES) / sizeof(ROLE_NAMES[0]); index++)
    {
        int p101_call_result_1;

        p101_call_result_1 = p101_strcmp(env, role, ROLE_NAMES[index]);
        if(p101_call_result_1 == 0)
        {
            *ROLE_FLAGS[index] = true;
            break;
        }
    }
}

static size_t find_function_fact(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    size_t found;

    P101_TRACE_SCOPE(env);
    found = model->fact_count;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *candidate;
        int                             p101_expression_result_22;
        int                             p101_expression_result_23;
        int                             p101_expression_result_24;

        candidate                 = &model->facts[index];
        p101_expression_result_24 = 0;
        if(candidate->kind == P101_C_ANALYSIS_FUNCTION)
        {
            if(candidate->is_definition)
            {
                p101_expression_result_24 = 1;
            }
        }
        p101_expression_result_23 = 0;
        if(p101_expression_result_24)
        {
            if(call->usr[0] != '\0')
            {
                p101_expression_result_23 = 1;
            }
        }
        p101_expression_result_22 = 0;
        if(p101_expression_result_23)
        {
            int p101_call_result_25;

            p101_call_result_25 = p101_strcmp(env, candidate->usr, call->usr);
            if(p101_call_result_25 == 0)
            {
                p101_expression_result_22 = 1;
            }
        }
        if(p101_expression_result_22)
        {
            found = index;
            break;
        }
    }
    return found;
}

static bool function_is_inventory_wrapper(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *function)
{
    int  p101_expression_result_26;
    int  p101_call_result_27;
    bool wrapper;

    P101_TRACE_SCOPE(env);
    wrapper = false;
    if(function->kind != P101_C_ANALYSIS_FUNCTION || !function->is_definition || function->usr[0] == '\0')
    {
        goto done;
    }
    for(size_t inventory_index = 0U; inventory_index < model->inventory_count && !wrapper; inventory_index++)
    {
        p101_expression_result_26 = 0;
        if(model->inventory[inventory_index].wrapper_usr[0] != '\0')
        {
            p101_call_result_27 = p101_strcmp(env, model->inventory[inventory_index].wrapper_usr, function->usr);
            if(p101_call_result_27 == 0)
            {
                p101_expression_result_26 = 1;
            }
        }
        if(p101_expression_result_26)
        {
            wrapper = true;
        }
    }

done:
    return wrapper;
}

static bool merge_capabilities(struct instrumentation_capabilities *destination, const struct instrumentation_capabilities *source)
{
    bool changed;

    changed = false;
    if(!destination->trace_entry && source->trace_entry)
    {
        destination->trace_entry = true;
        changed                  = true;
    }
    if(!destination->trace_exit && source->trace_exit)
    {
        destination->trace_exit = true;
        changed                 = true;
    }
    if(!destination->fault && source->fault)
    {
        destination->fault = true;
        changed            = true;
    }
    if(!destination->fd && source->fd)
    {
        destination->fd = true;
        changed         = true;
    }
    if(!destination->allocation && source->allocation)
    {
        destination->allocation = true;
        changed                 = true;
    }
    if(!destination->resource && source->resource)
    {
        destination->resource = true;
        changed               = true;
    }
    return changed;
}

static size_t find_calling_function(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *call)
{
    int    p101_expression_result_28;
    int    p101_expression_result_29;
    int    p101_call_result_30;
    int    p101_expression_result_31;
    int    p101_call_result_32;
    size_t nearest;

    P101_TRACE_SCOPE(env);
    nearest = model->fact_count;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *candidate;

        candidate = &model->facts[index];
        if(candidate->kind != P101_C_ANALYSIS_FUNCTION)
        {
            p101_expression_result_29 = 1;
        }
        else
        {
            if(!candidate->is_definition)
            {
                p101_expression_result_29 = 1;
            }
            else
            {
                p101_expression_result_29 = 0;
            }
        }
        if(p101_expression_result_29)
        {
            p101_expression_result_28 = 1;
        }
        else
        {
            p101_call_result_30 = p101_strcmp(env, candidate->path, call->path);
            if(p101_call_result_30 != 0)
            {
                p101_expression_result_28 = 1;
            }
            else
            {
                p101_expression_result_28 = 0;
            }
        }
        if(p101_expression_result_28)
        {
            continue;
        }
        p101_expression_result_31 = 0;
        if(call->caller_usr[0] != '\0')
        {
            p101_call_result_32 = p101_strcmp(env, candidate->usr, call->caller_usr);
            if(p101_call_result_32 == 0)
            {
                p101_expression_result_31 = 1;
            }
        }
        if(p101_expression_result_31)
        {
            nearest = index;
            break;
        }
        /*
         * Detailed preprocessing records are not children of the function
         * cursor, so libclang cannot supply their semantic caller. Associate
         * a macro expansion only when its expansion offset is inside the
         * resolved function definition's source extent.
         */
        if(call->caller_usr[0] == '\0' && call->start >= candidate->start && call->start < candidate->end)
        {
            nearest = index;
            break;
        }
    }
    return nearest;
}

static size_t find_function_at_source_location(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *fact)
{
    size_t nearest;

    P101_TRACE_SCOPE(env);
    nearest = model->fact_count;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *candidate;
        int                             p101_expression_result_33;
        int                             p101_expression_result_34;
        int                             p101_expression_result_35;
        int                             p101_expression_result_36;

        candidate                 = &model->facts[index];
        p101_expression_result_36 = 0;
        if(candidate->kind == P101_C_ANALYSIS_FUNCTION)
        {
            if(candidate->is_definition)
            {
                p101_expression_result_36 = 1;
            }
        }
        p101_expression_result_35 = 0;
        if(p101_expression_result_36)
        {
            int p101_call_result_37;

            p101_call_result_37 = p101_strcmp(env, candidate->path, fact->path);
            if(p101_call_result_37 == 0)
            {
                p101_expression_result_35 = 1;
            }
        }
        p101_expression_result_34 = 0;
        if(p101_expression_result_35)
        {
            if(fact->start >= candidate->start)
            {
                p101_expression_result_34 = 1;
            }
        }
        p101_expression_result_33 = 0;
        if(p101_expression_result_34)
        {
            if(fact->start < candidate->end)
            {
                p101_expression_result_33 = 1;
            }
        }
        if(p101_expression_result_33)
        {
            nearest = index;
            break;
        }
    }
    return nearest;
}

static void collect_capabilities(const struct p101_env *env, const struct p101_wrapper_model *model, struct instrumentation_capabilities *capabilities)
{
    int  p101_expression_result_42;
    bool p101_call_result_43;
    bool changed;

    P101_TRACE_SCOPE(env);
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;
        enum p101_c_note_kind           p101_call_result_39;
        size_t                          function;
        bool                            is_trace_scope;

        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_CALL && fact->kind != P101_C_ANALYSIS_NOTE)
        {
            continue;
        }
        p101_call_result_39 = P101_C_NOTE_OTHER;
        if(fact->kind == P101_C_ANALYSIS_NOTE)
        {
            p101_call_result_39 = p101_c_note_kind_from_name(env, fact->name);
        }
        is_trace_scope = p101_call_result_39 == P101_C_NOTE_TRACE_USE;
        if(is_trace_scope)
        {
            function = find_function_at_source_location(env, model, fact);
        }
        else
        {
            function = find_calling_function(env, model, fact);
        }
        if(function == model->fact_count)
        {
            continue;
        }
        if(is_trace_scope)
        {
            capabilities[function].trace_entry = true;
            capabilities[function].trace_exit  = true;
        }
        else if(fact->kind == P101_C_ANALYSIS_NOTE)
        {
            add_role_capability(env, &capabilities[function], fact->name);
        }
    }

    do
    {
        changed = false;
        for(size_t index = 0U; index < model->fact_count; index++)
        {
            const struct p101_wrapper_fact *call;
            size_t                          caller;
            size_t                          callee;

            call = &model->facts[index];
            if(call->kind != P101_C_ANALYSIS_CALL)
            {
                continue;
            }
            caller = find_calling_function(env, model, call);
            if(caller == model->fact_count)
            {
                continue;
            }
            callee                    = find_function_fact(env, model, call);
            p101_expression_result_42 = 0;
            if(callee != model->fact_count)
            {
                p101_call_result_43 = merge_capabilities(&capabilities[caller], &capabilities[callee]);
                if(p101_call_result_43)
                {
                    p101_expression_result_42 = 1;
                }
            }
            if(p101_expression_result_42)
            {
                changed = true;
            }
        }
    } while(changed);
}

static bool write_facts_file(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const char *path)
{
    FILE *stream;
    bool  success;

    success = false;
    stream  = p101_fopen(env, err, path, "w");
    if(stream == NULL)
    {
        goto done;
    }
    p101_wrapper_write_facts(env, err, model, stream);
    p101_fclose(env, err, stream);
    success = p101_error_has_no_error(err);

done:
    return success;
}

static bool write_manifest(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    const char *p101_call_result_2;
    const char *p101_call_result_3;
    const char *p101_call_result_4;
    FILE       *stream;
    size_t      index;
    bool        first;
    bool        success;

    success = false;
    stream  = p101_fopen(env, err, arguments->input_manifest, "w");
    if(stream == NULL)
    {
        goto done;
    }
    p101_fputs(env, err, "{\"schema\":\"p101-wrapper-input-manifest-v3\",\"parser\":\"libclang\",\"compile_database\":", stream);
    p101_wrapper_output_json_string(env, err, stream, arguments->compile_database);
    p101_call_result_2 = p101_wrapper_output_json_bool_text(arguments->compile_database_only);
    p101_call_result_3 = p101_wrapper_output_json_bool_text(arguments->active_headers_only);
    p101_call_result_4 = p101_wrapper_output_json_bool_text(arguments->keep_going);
    p101_fprintf(env, err, stream, ",\"compile_database_only\":%s,\"active_headers_only\":%s,\"keep_going\":%s", p101_call_result_2, p101_call_result_3, p101_call_result_4);
    p101_fputs(env, err, ",\"paths\":[", stream);
    for(index = 0U; index < arguments->path_count; index++)
    {
        if(index > 0U)
        {
            p101_fputc(env, err, ',', stream);
        }
        p101_wrapper_output_json_string(env, err, stream, arguments->paths[index]);
    }
    p101_fputs(env, err, "],\"header_roots\":[", stream);
    for(index = 0U; index < arguments->header_root_count; index++)
    {
        if(index > 0U)
        {
            p101_fputc(env, err, ',', stream);
        }
        p101_wrapper_output_json_string(env, err, stream, arguments->header_roots[index]);
    }
    p101_fputs(env, err, "],\"extra_arguments\":[", stream);
    for(index = 0U; index < arguments->extra_argument_count; index++)
    {
        if(index > 0U)
        {
            p101_fputc(env, err, ',', stream);
        }
        p101_wrapper_output_json_string(env, err, stream, arguments->extra_arguments[index]);
    }
    p101_fputs(env, err, "],\"allow_files\":[", stream);
    for(index = 0U; index < arguments->allow_file_count; index++)
    {
        if(index > 0U)
        {
            p101_fputc(env, err, ',', stream);
        }
        p101_wrapper_output_json_string(env, err, stream, arguments->allow_files[index]);
    }
    p101_fputs(env, err, "],\"translation_units\":[", stream);
    first = true;
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_FILE)
        {
            continue;
        }
        if(!first)
        {
            p101_fputc(env, err, ',', stream);
        }
        first = false;
        p101_wrapper_output_json_string(env, err, stream, fact->path);
    }
    p101_fprintf(env, err, stream, "],\"inventory_entries\":%zu,\"allow_rules\":%zu,\"facts\":%zu,\"parse_failures\":%zu}\n", model->inventory_count, arguments->allow_rule_count, model->fact_count, model->parse_failures);
    p101_fclose(env, err, stream);
    success = p101_error_has_no_error(err);

done:
    return success;
}

static bool write_mutations(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const char *path)
{
    const char *p101_call_result_5;
    FILE       *stream;
    size_t      index;
    bool        first;
    bool        success;

    success = false;
    stream  = p101_fopen(env, err, path, "w");
    if(stream == NULL)
    {
        goto done;
    }
    p101_fputs(env, err, "{\"schema\":\"p101-mutation-candidates-v2\",\"producer\":\"lib_c_facts\",\"candidates\":[", stream);
    first = true;
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_MUTATION)
        {
            continue;
        }
        if(!first)
        {
            p101_fputc(env, err, ',', stream);
        }
        first = false;
        p101_fputs(env, err, "{\"operator\":", stream);
        p101_call_result_5 = p101_c_mutation_kind_name(fact->mutation);
        p101_wrapper_output_json_string(env, err, stream, p101_call_result_5);
        p101_fputs(env, err, ",\"path\":", stream);
        p101_wrapper_output_json_string(env, err, stream, fact->path);
        p101_fprintf(env, err, stream, ",\"line\":%zu,\"start\":%zu,\"end\":%zu,\"original\":", fact->line, fact->start, fact->end);
        p101_wrapper_output_json_string(env, err, stream, fact->name);
        p101_fputs(env, err, ",\"replacement\":", stream);
        p101_wrapper_output_json_string(env, err, stream, fact->replacement);
        p101_fputc(env, err, '}', stream);
    }
    p101_fputs(env, err, "]}\n", stream);
    p101_fclose(env, err, stream);
    success = p101_error_has_no_error(err);

done:
    return success;
}

static bool write_instrumentation(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const char *path)
{
    int                                  p101_expression_result_44;
    bool                                 p101_call_result_45;
    void                                *p101_call_result_6;
    bool                                 p101_call_result_7;
    const char                          *p101_call_result_8;
    const char                          *p101_call_result_9;
    const char                          *p101_call_result_10;
    const char                          *p101_call_result_11;
    const char                          *p101_call_result_12;
    const char                          *p101_call_result_13;
    const char                          *p101_call_result_14;
    const char                          *p101_call_result_15;
    const char                          *p101_call_result_16;
    FILE                                *stream;
    struct instrumentation_capabilities *capabilities;
    size_t                               index;
    bool                                 first;
    bool                                 success;

    capabilities = NULL;
    success      = false;
    stream       = p101_fopen(env, err, path, "w");
    if(stream == NULL)
    {
        goto done;
    }
    if(model->fact_count > 0U)
    {
        p101_call_result_6 = p101_calloc(env, err, model->fact_count, sizeof(*capabilities));
        capabilities       = (struct instrumentation_capabilities *)p101_call_result_6;
    }
    if(model->fact_count > 0U && capabilities == NULL)
    {
        goto done;
    }
    collect_capabilities(env, model, capabilities);
    p101_fputs(env, err, "{\"schema\":\"p101-instrumentation-coverage-v1\",\"producer\":\"p101-wrapper-audit\",\"functions\":[", stream);
    first = true;
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact               = &model->facts[index];
        p101_call_result_7 = function_is_inventory_wrapper(env, model, fact);
        if(!p101_call_result_7)
        {
            continue;
        }
        if(!first)
        {
            p101_fputc(env, err, ',', stream);
        }
        first = false;
        p101_fputs(env, err, "{\"path\":", stream);
        p101_wrapper_output_json_string(env, err, stream, fact->path);
        p101_fputs(env, err, ",\"function\":", stream);
        p101_wrapper_output_json_string(env, err, stream, fact->name);
        p101_fputs(env, err, ",\"usr\":", stream);
        p101_wrapper_output_json_string(env, err, stream, fact->usr);
        p101_call_result_8  = p101_wrapper_output_json_bool_text(fact->is_public);
        p101_call_result_9  = p101_wrapper_output_json_bool_text(fact->needs_env);
        p101_call_result_10 = p101_wrapper_output_json_bool_text(fact->needs_error);
        p101_call_result_11 = p101_wrapper_output_json_bool_text(capabilities[index].trace_entry);
        p101_call_result_12 = p101_wrapper_output_json_bool_text(capabilities[index].trace_exit);
        p101_call_result_13 = p101_wrapper_output_json_bool_text(capabilities[index].fault);
        p101_call_result_14 = p101_wrapper_output_json_bool_text(capabilities[index].fd);
        p101_call_result_15 = p101_wrapper_output_json_bool_text(capabilities[index].allocation);
        p101_call_result_16 = p101_wrapper_output_json_bool_text(capabilities[index].resource);
        p101_fprintf(env,
                     err,
                     stream,
                     ",\"line\":%zu,\"public\":%s,\"has_env\":%s,\"has_error\":%s,\"trace_entry\":%s,\"trace_exit\":%s,\"fault\":%s,\"fd\":%s,\"allocation\":%s,\"resource\":%s}",
                     fact->line,
                     p101_call_result_8,
                     p101_call_result_9,
                     p101_call_result_10,
                     p101_call_result_11,
                     p101_call_result_12,
                     p101_call_result_13,
                     p101_call_result_14,
                     p101_call_result_15,
                     p101_call_result_16);
    }
    p101_fputs(env, err, "]}\n", stream);
    success = p101_error_has_no_error(err);

done:
    p101_free(env, capabilities);
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
    p101_expression_result_44 = 0;
    if(success)
    {
        p101_call_result_45 = p101_error_has_no_error(err);
        if(p101_call_result_45)
        {
            p101_expression_result_44 = 1;
        }
    }
    success = p101_expression_result_44 != 0;
    return success;
}

bool p101_wrapper_write_optional_outputs(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    int  p101_expression_result_46;
    int  p101_expression_result_48;
    int  p101_expression_result_49;
    int  p101_expression_result_51;
    int  p101_expression_result_52;
    int  p101_expression_result_54;
    int  p101_expression_result_55;
    bool success;

    success                   = true;
    p101_expression_result_46 = 0;
    if(arguments->facts_output != NULL)
    {
        bool p101_call_result_47;

        p101_call_result_47 = write_facts_file(env, err, model, arguments->facts_output);
        if(!p101_call_result_47)
        {
            p101_expression_result_46 = 1;
        }
    }
    if(p101_expression_result_46)
    {
        success = false;
    }
    p101_expression_result_49 = 0;
    if(success)
    {
        if(arguments->input_manifest != NULL)
        {
            p101_expression_result_49 = 1;
        }
    }
    p101_expression_result_48 = 0;
    if(p101_expression_result_49)
    {
        bool p101_call_result_50;

        p101_call_result_50 = write_manifest(env, err, model, arguments);
        if(!p101_call_result_50)
        {
            p101_expression_result_48 = 1;
        }
    }
    if(p101_expression_result_48)
    {
        success = false;
    }
    p101_expression_result_52 = 0;
    if(success)
    {
        if(arguments->mutation_output != NULL)
        {
            p101_expression_result_52 = 1;
        }
    }
    p101_expression_result_51 = 0;
    if(p101_expression_result_52)
    {
        bool p101_call_result_53;

        p101_call_result_53 = write_mutations(env, err, model, arguments->mutation_output);
        if(!p101_call_result_53)
        {
            p101_expression_result_51 = 1;
        }
    }
    if(p101_expression_result_51)
    {
        success = false;
    }
    p101_expression_result_55 = 0;
    if(success)
    {
        if(arguments->instrumentation_output != NULL)
        {
            p101_expression_result_55 = 1;
        }
    }
    p101_expression_result_54 = 0;
    if(p101_expression_result_55)
    {
        bool p101_call_result_56;

        p101_call_result_56 = write_instrumentation(env, err, model, arguments->instrumentation_output);
        if(!p101_call_result_56)
        {
            p101_expression_result_54 = 1;
        }
    }
    if(p101_expression_result_54)
    {
        success = false;
    }
    return success;
}
