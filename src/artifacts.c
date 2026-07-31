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

static void add_call_capability(const struct p101_env *env, struct instrumentation_capabilities *capabilities, const char *name)
{
    static const char *const trace_entry[] = {"p101_env_trace"};
    static const char *const trace_exit[]  = {"p101_env_trace_exit", "p101_env_trace_scope_cleanup"};
    static const char *const fault[]       = {"p101_env_check_fault", "p101_env_check_fault_action"};
    static const char *const fd[]          = {"p101_env_track_open", "p101_env_track_close", "p101_env_track_fork", "p101_env_track_spawn", "p101_env_track_exec", "p101_env_track_exec_failure"};
    static const char *const allocation[]  = {"p101_env_track_alloc", "p101_env_track_free", "p101_env_track_realloc"};
    static const char *const resource[]    = {"p101_env_track_resource", "p101_env_track_pointer_resource", "p101_env_track_integer_resource"};

    const struct
    {
        const char *const *names;
        size_t             count;
        bool              *present;
    } groups[] = {
        {trace_entry, sizeof(trace_entry) / sizeof(trace_entry[0]), &capabilities->trace_entry},
        {trace_exit,  sizeof(trace_exit) / sizeof(trace_exit[0]),   &capabilities->trace_exit },
        {fault,       sizeof(fault) / sizeof(fault[0]),             &capabilities->fault      },
        {fd,          sizeof(fd) / sizeof(fd[0]),                   &capabilities->fd         },
        {allocation,  sizeof(allocation) / sizeof(allocation[0]),   &capabilities->allocation },
        {resource,    sizeof(resource) / sizeof(resource[0]),       &capabilities->resource   },
    };

    P101_TRACE_SCOPE(env);
    for(size_t group = 0U; group < sizeof(groups) / sizeof(groups[0]); group++)
    {
        for(size_t index = 0U; index < groups[group].count; index++)
        {
            if(p101_strcmp(env, name, groups[group].names[index]) == 0)
            {
                *groups[group].present = true;
            }
        }
    }
}

static size_t find_function_fact(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *caller, const char *name)
{
    P101_TRACE_SCOPE(env);
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *candidate;

        candidate = &model->facts[index];
        if(candidate->kind == P101_C_ANALYSIS_FUNCTION && candidate->is_definition && p101_strcmp(env, candidate->path, caller->path) == 0 && p101_strcmp(env, candidate->name, name) == 0)
        {
            return index;
        }
    }
    return model->fact_count;
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
        if(call->caller[0] != '\0' && p101_strcmp(env, candidate->name, call->caller) == 0)
        {
            return index;
        }
        /*
         * Detailed preprocessing records are not children of the function
         * cursor, so libclang cannot supply their lexical caller.  A trace
         * macro still has the invocation's source line; associate it with the
         * nearest preceding definition in that file.
         */
        if(call->caller[0] == '\0' && candidate->line <= call->line && (nearest == model->fact_count || model->facts[nearest].line < candidate->line))
        {
            nearest = index;
        }
    }
    return nearest;
}

static size_t find_function_at_source_line(const struct p101_env *env, const struct p101_wrapper_model *model, const struct p101_wrapper_fact *fact)
{
    size_t nearest;

    P101_TRACE_SCOPE(env);
    nearest = model->fact_count;
    for(size_t index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *candidate;

        candidate = &model->facts[index];
        if(candidate->kind == P101_C_ANALYSIS_FUNCTION && candidate->is_definition && p101_strcmp(env, candidate->path, fact->path) == 0 && candidate->line <= fact->line && (nearest == model->fact_count || model->facts[nearest].line < candidate->line))
        {
            nearest = index;
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
        if(fact->kind == P101_C_ANALYSIS_NOTE && p101_strcmp(env, fact->name, "TRACE_USE") == 0)
        {
            function = find_function_at_source_line(env, model, fact);
        }
        else
        {
            function = find_calling_function(env, model, fact);
        }
        if(function == model->fact_count)
        {
            continue;
        }
        if(fact->kind == P101_C_ANALYSIS_NOTE && p101_strcmp(env, fact->name, "TRACE_USE") == 0)
        {
            capabilities[function].trace_entry = true;
            capabilities[function].trace_exit  = true;
        }
        else if(fact->kind == P101_C_ANALYSIS_CALL)
        {
            add_call_capability(env, &capabilities[function], fact->name);
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
            callee = find_function_fact(env, model, &model->facts[caller], call->name);
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

    stream = p101_fopen(env, err, path, "w");
    if(stream == NULL)
    {
        return false;
    }
    p101_wrapper_write_facts(env, err, model, stream);
    p101_fclose(env, err, stream);
    return p101_error_has_no_error(err);
}

static bool write_manifest(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    FILE  *stream;
    size_t index;
    bool   first;

    stream = p101_fopen(env, err, arguments->input_manifest, "w");
    if(stream == NULL)
    {
        return false;
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
    return p101_error_has_no_error(err);
}

static bool write_mutations(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const char *path)
{
    FILE  *stream;
    size_t index;
    bool   first;

    stream = p101_fopen(env, err, path, "w");
    if(stream == NULL)
    {
        return false;
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
    return p101_error_has_no_error(err);
}

static bool write_instrumentation(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const char *path)
{
    FILE                                *stream;
    struct instrumentation_capabilities *capabilities;
    size_t                               index;
    bool                                 first;

    stream = p101_fopen(env, err, path, "w");
    if(stream == NULL)
    {
        return false;
    }
    capabilities = NULL;
    if(model->fact_count > 0U)
    {
        capabilities = (struct instrumentation_capabilities *)p101_calloc(env, err, model->fact_count, sizeof(*capabilities));
    }
    if(model->fact_count > 0U && capabilities == NULL)
    {
        p101_fclose(env, err, stream);
        return false;
    }
    collect_capabilities(env, model, capabilities);
    p101_fputs(env, err, "{\"schema\":\"p101-instrumentation-coverage-v1\",\"producer\":\"p101-wrapper-audit\",\"functions\":[", stream);
    first = true;
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind != P101_C_ANALYSIS_FUNCTION || !fact->is_definition || p101_strncmp(env, fact->name, "p101_", sizeof("p101_") - 1U) != 0)
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
    p101_free(env, capabilities);
    p101_fclose(env, err, stream);
    return p101_error_has_no_error(err);
}

bool p101_wrapper_write_optional_outputs(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    if(arguments->facts_output != NULL && !write_facts_file(env, err, model, arguments->facts_output))
    {
        return false;
    }
    if(arguments->input_manifest != NULL && !write_manifest(env, err, model, arguments))
    {
        return false;
    }
    if(arguments->mutation_output != NULL && !write_mutations(env, err, model, arguments->mutation_output))
    {
        return false;
    }
    if(arguments->instrumentation_output != NULL && !write_instrumentation(env, err, model, arguments->instrumentation_output))
    {
        return false;
    }
    return true;
}
