#include "output.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>

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
    P101_TRACE_SCOPE(env);
    if(p101_strcmp(env, role, "CALLEE_SEMANTIC_ROLE:p101:instrumentation:trace-entry") == 0)
    {
        capabilities->trace_entry = true;
    }
    else if(p101_strcmp(env, role, "CALLEE_SEMANTIC_ROLE:p101:instrumentation:trace-exit") == 0)
    {
        capabilities->trace_exit = true;
    }
    else if(p101_strcmp(env, role, "CALLEE_SEMANTIC_ROLE:p101:instrumentation:fault") == 0)
    {
        capabilities->fault = true;
    }
    else if(p101_strcmp(env, role, "CALLEE_SEMANTIC_ROLE:p101:instrumentation:fd") == 0)
    {
        capabilities->fd = true;
    }
    else if(p101_strcmp(env, role, "CALLEE_SEMANTIC_ROLE:p101:instrumentation:allocation") == 0)
    {
        capabilities->allocation = true;
    }
    else if(p101_strcmp(env, role, "CALLEE_SEMANTIC_ROLE:p101:instrumentation:resource") == 0)
    {
        capabilities->resource = true;
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

        candidate = &model->facts[index];
        if(candidate->kind == P101_C_ANALYSIS_FUNCTION && candidate->is_definition && call->usr[0] != '\0' && p101_strcmp(env, candidate->usr, call->usr) == 0)
        {
            found = index;
            break;
        }
    }
    return found;
}

static bool function_is_inventory_wrapper(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *function)
{
    bool wrapper;

    P101_TRACE_SCOPE(env);
    wrapper = false;
    if(function->kind != P101_C_ANALYSIS_FUNCTION || !function->is_definition || function->usr[0] == '\0')
    {
        goto done;
    }
    for(size_t inventory_index = 0U; inventory_index < model->inventory_count && !wrapper; inventory_index++)
    {
        if(model->inventory[inventory_index].wrapper_usr[0] != '\0' && p101_strcmp(env, model->inventory[inventory_index].wrapper_usr, function->usr) == 0)
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
    size_t nearest;

    P101_TRACE_SCOPE(env);
    nearest = model->fact_count;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *candidate;

        candidate = &model->facts[index];
        if(candidate->kind != P101_C_ANALYSIS_FUNCTION || !candidate->is_definition || p101_strcmp(env, candidate->path, call->path) != 0)
        {
            continue;
        }
        if(call->caller_usr[0] != '\0' && p101_strcmp(env, candidate->usr, call->caller_usr) == 0)
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

        candidate = &model->facts[index];
        if(candidate->kind == P101_C_ANALYSIS_FUNCTION && candidate->is_definition && p101_strcmp(env, candidate->path, fact->path) == 0 && fact->start >= candidate->start && fact->start < candidate->end)
        {
            nearest = index;
            break;
        }
    }
    return nearest;
}

static void collect_capabilities(const struct p101_env *env, const struct p101_wrapper_model *model, struct instrumentation_capabilities *capabilities)
{
    bool changed;

    P101_TRACE_SCOPE(env);
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;
        size_t                          function;

        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_CALL && fact->kind != P101_C_ANALYSIS_NOTE)
        {
            continue;
        }
        if(fact->kind == P101_C_ANALYSIS_NOTE && p101_strcmp(env, fact->name, "TYPE_SEMANTIC_ROLE:p101:trace-scope") == 0)
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
        if(fact->kind == P101_C_ANALYSIS_NOTE && p101_strcmp(env, fact->name, "TYPE_SEMANTIC_ROLE:p101:trace-scope") == 0)
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
            callee = find_function_fact(env, model, call);
            if(callee != model->fact_count && merge_capabilities(&capabilities[caller], &capabilities[callee]))
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
    FILE  *stream;
    size_t index;
    bool   first;
    bool   success;

    success = false;
    stream  = p101_fopen(env, err, arguments->input_manifest, "w");
    if(stream == NULL)
    {
        goto done;
    }
    p101_fputs(env, err, "{\"schema\":\"p101-wrapper-input-manifest-v3\",\"parser\":\"libclang\",\"compile_database\":", stream);
    p101_wrapper_output_json_string(env, err, stream, arguments->compile_database);
    p101_fprintf(env,
                 err,
                 stream,
                 ",\"compile_database_only\":%s,\"active_headers_only\":%s,\"keep_going\":%s",
                 p101_wrapper_output_json_bool_text(arguments->compile_database_only),
                 p101_wrapper_output_json_bool_text(arguments->active_headers_only),
                 p101_wrapper_output_json_bool_text(arguments->keep_going));
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
    FILE  *stream;
    size_t index;
    bool   first;
    bool   success;

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
        p101_wrapper_output_json_string(env, err, stream, p101_c_mutation_kind_name(fact->mutation));
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
        capabilities = (struct instrumentation_capabilities *)p101_calloc(env, err, model->fact_count, sizeof(*capabilities));
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

        fact = &model->facts[index];
        if(!function_is_inventory_wrapper(env, model, fact))
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
        p101_fprintf(env,
                     err,
                     stream,
                     ",\"line\":%zu,\"public\":%s,\"has_env\":%s,\"has_error\":%s,\"trace_entry\":%s,\"trace_exit\":%s,\"fault\":%s,\"fd\":%s,\"allocation\":%s,\"resource\":%s}",
                     fact->line,
                     p101_wrapper_output_json_bool_text(fact->is_public),
                     p101_wrapper_output_json_bool_text(fact->needs_env),
                     p101_wrapper_output_json_bool_text(fact->needs_error),
                     p101_wrapper_output_json_bool_text(capabilities[index].trace_entry),
                     p101_wrapper_output_json_bool_text(capabilities[index].trace_exit),
                     p101_wrapper_output_json_bool_text(capabilities[index].fault),
                     p101_wrapper_output_json_bool_text(capabilities[index].fd),
                     p101_wrapper_output_json_bool_text(capabilities[index].allocation),
                     p101_wrapper_output_json_bool_text(capabilities[index].resource));
    }
    p101_fputs(env, err, "]}\n", stream);
    success = p101_error_has_no_error(err);

done:
    p101_free(env, capabilities);
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
    success = (success && p101_error_has_no_error(err)) != 0;
    return success;
}

bool p101_wrapper_write_optional_outputs(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    bool success;

    success = true;
    if(arguments->facts_output != NULL && !write_facts_file(env, err, model, arguments->facts_output))
    {
        success = false;
    }
    if(success && arguments->input_manifest != NULL && !write_manifest(env, err, model, arguments))
    {
        success = false;
    }
    if(success && arguments->mutation_output != NULL && !write_mutations(env, err, model, arguments->mutation_output))
    {
        success = false;
    }
    if(success && arguments->instrumentation_output != NULL && !write_instrumentation(env, err, model, arguments->instrumentation_output))
    {
        success = false;
    }
    return success;
}
