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
static bool identity_fits(const struct p101_env *env, const char *identity, size_t size);
static void report_oversized_field(const struct p101_env *env, const char *field, const char *value, size_t capacity);

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

static bool identity_fits(const struct p101_env *env, const char *identity, size_t size)
{
    int p101_expression_result_3;

    if(identity == NULL)
    {
        p101_expression_result_3 = 1;
    }
    else
    {
        size_t p101_call_result_4;

        p101_call_result_4 = p101_strlen(env, identity);
        if(p101_call_result_4 < size)
        {
            p101_expression_result_3 = 1;
        }
        else
        {
            p101_expression_result_3 = 0;
        }
    }
    return p101_expression_result_3 != 0;
}

static void report_oversized_field(const struct p101_env *env, const char *field, const char *value, size_t capacity)
{
    size_t length;

    P101_TRACE_SCOPE(env);
    length = p101_strlen(env, value);
    p101_fprintf(env, P101_ERROR_OPTIONAL, stderr, "audit-facts: %s requires %zu bytes; model capacity is %zu bytes\n", field, length + 1U, capacity);
}

static bool grow_facts(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model)
{
    void                     *p101_call_result_1;
    size_t                    capacity;
    struct p101_wrapper_fact *facts;
    bool                      grown;

    P101_TRACE_SCOPE(env);
    grown              = false;
    capacity           = model->fact_capacity == 0U ? INITIAL_CAPACITY : model->fact_capacity * 2U;
    p101_call_result_1 = p101_realloc(env, err, model->facts, capacity * sizeof(*facts));
    facts              = (struct p101_wrapper_fact *)p101_call_result_1;
    if(facts == NULL)
    {
        goto done;
    }
    model->facts         = facts;
    model->fact_capacity = capacity;
    grown                = true;

done:
    return grown;
}

bool p101_wrapper_analysis_observer(const struct p101_env *env, struct p101_error *err, const struct p101_c_analysis_record *record, void *context)
{
    int                        p101_expression_result_5;
    bool                       p101_call_result_6;
    bool                       p101_call_result_7;
    bool                       type_fits;
    bool                       canonical_type_fits;
    bool                       return_type_fits;
    int                        p101_expression_result_8;
    bool                       p101_call_result_9;
    struct p101_wrapper_model *model;
    struct p101_wrapper_fact  *fact;
    bool                       keep_going;

    P101_TRACE_SCOPE(env);
    model      = (struct p101_wrapper_model *)context;
    keep_going = false;
    if(record->kind == P101_C_ANALYSIS_DIAGNOSTIC)
    {
        model->parse_failures++;
    }
    p101_call_result_6 = identity_fits(env, record->usr, sizeof(model->facts[0].usr));
    if(!p101_call_result_6)
    {
        p101_expression_result_5 = 1;
    }
    else
    {
        p101_call_result_7 = identity_fits(env, record->caller_usr, sizeof(model->facts[0].caller_usr));
        if(!p101_call_result_7)
        {
            p101_expression_result_5 = 1;
        }
        else
        {
            p101_expression_result_5 = 0;
        }
    }
    if(p101_expression_result_5)
    {
        if(!p101_call_result_6)
        {
            report_oversized_field(env, "declaration identity", record->usr, sizeof(model->facts[0].usr));
        }
        else
        {
            report_oversized_field(env, "caller identity", record->caller_usr, sizeof(model->facts[0].caller_usr));
        }
        P101_ERROR_RAISE_USER(err, "A resolved declaration identity is too long for the wrapper-audit model.", 1);
        goto done;
    }
    type_fits           = identity_fits(env, record->type, sizeof(model->facts[0].type));
    canonical_type_fits = identity_fits(env, record->canonical_type, sizeof(model->facts[0].canonical_type));
    return_type_fits    = identity_fits(env, record->return_type, sizeof(model->facts[0].return_type));
    if(!type_fits || !canonical_type_fits || !return_type_fits)
    {
        if(!type_fits)
        {
            report_oversized_field(env, "type", record->type, sizeof(model->facts[0].type));
        }
        else if(!canonical_type_fits)
        {
            report_oversized_field(env, "canonical type", record->canonical_type, sizeof(model->facts[0].canonical_type));
        }
        else
        {
            report_oversized_field(env, "return type", record->return_type, sizeof(model->facts[0].return_type));
        }
        P101_ERROR_RAISE_USER(err, "A resolved C type is too long for the wrapper-audit model.", 1);
        goto done;
    }
    p101_expression_result_8 = 0;
    if(model->fact_count == model->fact_capacity)
    {
        p101_call_result_9 = grow_facts(env, err, model);
        if(!p101_call_result_9)
        {
            p101_fputs(env, P101_ERROR_OPTIONAL, "audit-facts: could not grow the semantic fact model\n", stderr);
            p101_expression_result_8 = 1;
        }
    }
    if(p101_expression_result_8)
    {
        goto done;
    }
    fact = &model->facts[model->fact_count++];
    p101_memset(env, fact, 0, sizeof(*fact));
    fact->kind             = record->kind;
    fact->line             = record->line;
    fact->column           = record->column;
    fact->start            = record->start_offset;
    fact->end              = record->end_offset;
    fact->parameter_index  = record->parameter_index;
    fact->mutation         = record->mutation;
    fact->is_header        = record->is_header;
    fact->is_definition    = record->is_definition;
    fact->is_static        = record->is_static;
    fact->is_public        = record->is_public;
    fact->is_variadic      = record->is_variadic;
    fact->is_local_include = record->is_local_include;
    fact->is_indirect      = record->is_indirect;
    fact->needs_env        = record->has_env_parameter;
    fact->needs_error      = record->has_error_parameter;
    copy_field(env, fact->path, sizeof(fact->path), record->path);
    copy_field(env, fact->resolved, sizeof(fact->resolved), record->resolved_include);
    copy_field(env, fact->name, sizeof(fact->name), record->name);
    copy_field(env, fact->type, sizeof(fact->type), record->type);
    copy_field(env, fact->canonical_type, sizeof(fact->canonical_type), record->canonical_type);
    copy_field(env, fact->return_type, sizeof(fact->return_type), record->return_type);
    copy_field(env, fact->caller, sizeof(fact->caller), record->caller);
    copy_field(env, fact->usr, sizeof(fact->usr), record->usr);
    copy_field(env, fact->caller_usr, sizeof(fact->caller_usr), record->caller_usr);
    copy_field(env, fact->replacement, sizeof(fact->replacement), record->replacement);
    keep_going = true;

done:
    return keep_going;
}

bool p101_wrapper_model_scan(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    struct p101_c_analysis_options options;
    bool                           scanned;

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
    scanned            = p101_c_analysis_scan(env, err, &options, p101_wrapper_analysis_observer, model);
    if(scanned)
    {
        assign_macro_callers(env, model);
        deduplicate_facts(env, model);
    }
    return scanned;
}

static void assign_macro_callers(const struct p101_env *env, struct p101_wrapper_model *model)
{
    int    p101_expression_result_10;
    int    p101_expression_result_11;
    int    p101_expression_result_12;
    int    p101_expression_result_13;
    int    p101_call_result_14;
    int    p101_expression_result_15;
    int    p101_expression_result_16;
    int    p101_expression_result_17;
    int    p101_expression_result_18;
    int    p101_expression_result_19;
    int    p101_expression_result_20;
    int    p101_call_result_21;
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

            call                      = &model->facts[function_index];
            p101_expression_result_13 = 0;
            if(call->kind == P101_C_ANALYSIS_CALL)
            {
                if(call->caller_usr[0] != '\0')
                {
                    p101_expression_result_13 = 1;
                }
            }
            p101_expression_result_12 = 0;
            if(p101_expression_result_13)
            {
                if(call->start <= macro->start)
                {
                    p101_expression_result_12 = 1;
                }
            }
            p101_expression_result_11 = 0;
            if(p101_expression_result_12)
            {
                if(call->end >= macro->end)
                {
                    p101_expression_result_11 = 1;
                }
            }
            p101_expression_result_10 = 0;
            if(p101_expression_result_11)
            {
                p101_call_result_14 = p101_strcmp(env, call->path, macro->path);
                if(p101_call_result_14 == 0)
                {
                    p101_expression_result_10 = 1;
                }
            }
            if(p101_expression_result_10)
            {
                copy_field(env, macro->caller, sizeof(macro->caller), call->caller);
                copy_field(env, macro->caller_usr, sizeof(macro->caller_usr), call->caller_usr);
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
            if(function->kind != P101_C_ANALYSIS_FUNCTION)
            {
                p101_expression_result_20 = 1;
            }
            else
            {
                if(!function->is_definition)
                {
                    p101_expression_result_20 = 1;
                }
                else
                {
                    p101_expression_result_20 = 0;
                }
            }
            if(p101_expression_result_20)
            {
                p101_expression_result_19 = 1;
            }
            else
            {
                if(function->end < function->start)
                {
                    p101_expression_result_19 = 1;
                }
                else
                {
                    p101_expression_result_19 = 0;
                }
            }
            if(p101_expression_result_19)
            {
                p101_expression_result_18 = 1;
            }
            else
            {
                if(macro->end < macro->start)
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
                p101_expression_result_17 = 1;
            }
            else
            {
                p101_call_result_21 = p101_strcmp(env, function->path, macro->path);
                if(p101_call_result_21 != 0)
                {
                    p101_expression_result_17 = 1;
                }
                else
                {
                    p101_expression_result_17 = 0;
                }
            }
            if(p101_expression_result_17)
            {
                p101_expression_result_16 = 1;
            }
            else
            {
                if(function->start > macro->start)
                {
                    p101_expression_result_16 = 1;
                }
                else
                {
                    p101_expression_result_16 = 0;
                }
            }
            if(p101_expression_result_16)
            {
                p101_expression_result_15 = 1;
            }
            else
            {
                if(function->end < macro->end)
                {
                    p101_expression_result_15 = 1;
                }
                else
                {
                    p101_expression_result_15 = 0;
                }
            }
            if(p101_expression_result_15)
            {
                continue;
            }
            span = function->end - function->start;
            if(span < narrowest_span)
            {
                copy_field(env, macro->caller, sizeof(macro->caller), function->name);
                copy_field(env, macro->caller_usr, sizeof(macro->caller_usr), function->usr);
                narrowest_span = span;
            }
        }
    }
}

static int compare_size(size_t left, size_t right)
{
    int result;

    if(left < right)
    {
        result = -1;
    }
    else if(left > right)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }
    return result;
}

static int compare_text(const char *left, const char *right)
{
    int result;

    while(*left != '\0' && *right != '\0' && *left == *right)
    {
        left++;
        right++;
    }
    if((unsigned char)*left < (unsigned char)*right)
    {
        result = -1;
    }
    else if((unsigned char)*left > (unsigned char)*right)
    {
        result = 1;
    }
    else
    {
        result = 0;
    }
    return result;
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
        result = compare_text(a->usr, b->usr);
    }
    if(result == 0)
    {
        result = compare_text(a->caller_usr, b->caller_usr);
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
    P101_TRACE_SCOPE(env);
    if(model->fact_count >= 2U)
    {
        size_t read_index;
        size_t write_index;

        p101_qsort(env, model->facts, model->fact_count, sizeof(model->facts[0]), compare_facts);
        write_index = 1U;
        for(read_index = 1U; read_index < model->fact_count; read_index++)
        {
            int p101_call_result_2;

            p101_call_result_2 = compare_facts(&model->facts[write_index - 1U], &model->facts[read_index]);
            if(p101_call_result_2 != 0)
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
}
