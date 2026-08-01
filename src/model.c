#include "model.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stdint.h>

enum
{
    INITIAL_CAPACITY = 256
};

static void deduplicate_facts(const struct p101_env *env, struct p101_wrapper_model *model);
static void assign_macro_callers(const struct p101_env *env, struct p101_wrapper_model *model);

void p101_wrapper_model_init(struct p101_wrapper_model *model)
{
    *model = (struct p101_wrapper_model){.facts = NULL};
}

void p101_wrapper_model_destroy(const struct p101_env *env, struct p101_wrapper_model *model)
{
    P101_TRACE_SCOPE(env);
    p101_free(env, model->findings);
    p101_free(env, model->inventory);
    p101_free(env, model->facts);
    p101_memset(env, model, 0, sizeof(*model));
}

static void copy_field(const struct p101_env *env, char *destination, size_t size, const char *source)
{
    P101_TRACE_SCOPE(env);
    destination[0] = '\0';
    if(source != NULL)
    {
        size_t length;

        length = p101_strlen(env, source);
        if(length >= size)
        {
            length = size - 1U;
        }
        p101_memcpy(env, destination, source, length);
        destination[length] = '\0';
    }
}

static bool grow_facts(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model)
{
    size_t                    capacity;
    struct p101_wrapper_fact *facts;

    P101_TRACE_SCOPE(env);
    capacity = model->fact_capacity == 0U ? INITIAL_CAPACITY : model->fact_capacity * 2U;
    facts    = (struct p101_wrapper_fact *)p101_realloc(env, err, model->facts, capacity * sizeof(*facts));
    if(facts == NULL)
    {
        return false;
    }
    model->facts         = facts;
    model->fact_capacity = capacity;
    return true;
}

bool p101_wrapper_analysis_observer(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context)
{
    struct p101_wrapper_model *model;
    struct p101_wrapper_fact  *fact;

    P101_TRACE_SCOPE(env);
    model = (struct p101_wrapper_model *)context;
    if(record->kind == P101_C_ANALYSIS_DIAGNOSTIC)
    {
        model->parse_failures++;
    }
    if(model->fact_count == model->fact_capacity && !grow_facts(env, err, model))
    {
        return false;
    }
    fact = &model->facts[model->fact_count++];
    p101_memset(env, fact, 0, sizeof(*fact));
    fact->kind             = record->kind;
    fact->line             = record->line;
    fact->column           = record->column;
    fact->start            = record->start_offset;
    fact->end              = record->end_offset;
    fact->mutation         = record->mutation;
    fact->is_header        = record->is_header;
    fact->is_definition    = record->is_definition;
    fact->is_static        = record->is_static;
    fact->is_public        = record->is_public;
    fact->is_local_include = record->is_local_include;
    fact->is_indirect      = record->is_indirect;
    fact->needs_env        = record->has_env_parameter;
    fact->needs_error      = record->has_error_parameter;
    copy_field(env, fact->path, sizeof(fact->path), record->path);
    copy_field(env, fact->name, sizeof(fact->name), record->name);
    copy_field(env, fact->caller, sizeof(fact->caller), record->caller);
    copy_field(env, fact->replacement, sizeof(fact->replacement), record->replacement);
    return true;
}

bool p101_wrapper_model_scan(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    struct p101_c_analysis_options options;

    P101_TRACE_SCOPE(env);
    p101_memset(env, &options, 0, sizeof(options));
    options.compile_database                     = arguments->compile_database;
    options.paths                                = arguments->paths;
    options.path_count                           = arguments->path_count;
    options.extra_arguments                      = arguments->extra_arguments;
    options.extra_argument_count                 = arguments->extra_argument_count;
    options.compile_database_only                = arguments->compile_database_only;
    options.detailed_preprocessing               = true;
    options.include_headers_as_translation_units = false;
    if(!arguments->active_headers_only)
    {
        options.include_headers_as_translation_units = true;
    }
    options.keep_going = arguments->keep_going;
    if(!p101_c_analysis_scan(env, err, &options, p101_wrapper_analysis_observer, model))
    {
        return false;
    }
    assign_macro_callers(env, model);
    deduplicate_facts(env, model);
    return true;
}

static void assign_macro_callers(const struct p101_env *env, struct p101_wrapper_model *model)
{
    size_t macro_index;

    P101_TRACE_SCOPE(env);
    for(macro_index = 0U; macro_index < model->fact_count; macro_index++)
    {
        struct p101_wrapper_fact *macro;
        size_t                    function_index;
        size_t                    narrowest_span;

        macro = &model->facts[macro_index];
        if(macro->kind != P101_C_ANALYSIS_MACRO || macro->is_definition || macro->caller[0] != '\0')
        {
            continue;
        }
        /*
         * A function-like macro commonly lowers to a call expression at the
         * same spelling location. That call retains its semantic caller even
         * when the preprocessing cursor itself is attached to the translation
         * unit.
         */
        for(function_index = 0U; function_index < model->fact_count; function_index++)
        {
            const struct p101_wrapper_fact *call;

            call = &model->facts[function_index];
            if(call->kind == P101_C_ANALYSIS_CALL && call->line == macro->line && call->caller[0] != '\0' && p101_strcmp(env, call->path, macro->path) == 0)
            {
                copy_field(env, macro->caller, sizeof(macro->caller), call->caller);
                break;
            }
        }
        if(macro->caller[0] != '\0')
        {
            continue;
        }
        narrowest_span = SIZE_MAX;
        for(function_index = 0U; function_index < model->fact_count; function_index++)
        {
            const struct p101_wrapper_fact *function;
            size_t                          span;

            function = &model->facts[function_index];
            if(function->kind != P101_C_ANALYSIS_FUNCTION || !function->is_definition || function->end < function->start || macro->end < macro->start || p101_strcmp(env, function->path, macro->path) != 0 || function->start > macro->start ||
               function->end < macro->end)
            {
                continue;
            }
            span = function->end - function->start;
            if(span < narrowest_span)
            {
                copy_field(env, macro->caller, sizeof(macro->caller), function->name);
                narrowest_span = span;
            }
        }
    }
}

static int compare_size(size_t left, size_t right)
{
    if(left < right)
    {
        return -1;
    }
    if(left > right)
    {
        return 1;
    }
    return 0;
}

static int compare_text(const char *left, const char *right)
{
    while(*left != '\0' && *right != '\0' && *left == *right)
    {
        left++;
        right++;
    }
    if((unsigned char)*left < (unsigned char)*right)
    {
        return -1;
    }
    if((unsigned char)*left > (unsigned char)*right)
    {
        return 1;
    }
    return 0;
}

static int compare_facts(const void *left, const void *right)
{
    const struct p101_wrapper_fact *a;
    const struct p101_wrapper_fact *b;
    int                             result;

    a      = (const struct p101_wrapper_fact *)left;
    b      = (const struct p101_wrapper_fact *)right;
    result = compare_text(a->path, b->path);
    if(result == 0)
    {
        result = compare_size(a->line, b->line);
    }
    if(result == 0)
    {
        result = compare_size(a->column, b->column);
    }
    if(result == 0)
    {
        result = compare_size((size_t)a->kind, (size_t)b->kind);
    }
    if(result == 0)
    {
        result = compare_text(a->name, b->name);
    }
    if(result == 0)
    {
        result = compare_text(a->caller, b->caller);
    }
    if(result == 0)
    {
        result = compare_size(a->start, b->start);
    }
    if(result == 0)
    {
        result = compare_size(a->end, b->end);
    }
    if(result == 0)
    {
        result = compare_size((size_t)a->is_definition, (size_t)b->is_definition);
    }
    return result;
}

static void deduplicate_facts(const struct p101_env *env, struct p101_wrapper_model *model)
{
    size_t read_index;
    size_t write_index;

    P101_TRACE_SCOPE(env);
    if(model->fact_count < 2U)
    {
        return;
    }
    p101_qsort(env, model->facts, model->fact_count, sizeof(model->facts[0]), compare_facts);
    write_index = 1U;
    for(read_index = 1U; read_index < model->fact_count; read_index++)
    {
        if(compare_facts(&model->facts[write_index - 1U], &model->facts[read_index]) != 0)
        {
            if(write_index != read_index)
            {
                model->facts[write_index] = model->facts[read_index];
            }
            write_index++;
        }
    }
    model->fact_count = write_index;
}
