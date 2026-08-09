#include "../include/contract.h"
#include "../include/constants.h"
#include "../include/contract_model.h"
#include "../include/native_analysis.h"
#include "../include/report.h"
#include "contract_event.h"
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static void   analyze_model(const struct p101_env *env, struct p101_error *err, const struct contract_model *model, struct contract_report *report, bool strict_sequence);
static size_t next_function_line(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function);
static bool   event_is_in_function(const struct p101_env *env, const struct contract_event *event, const struct contract_function *function, size_t end_line);
static bool   visible_env_before_event(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, const struct contract_event *event, size_t end_line);
static bool   visible_error_before_event(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, const struct contract_event *event, size_t end_line);
static bool   error_is_explicitly_optional(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, const struct contract_event *event, size_t end_line);
static size_t function_exit_count(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, size_t end_line, size_t *second_exit_line);
static size_t function_early_return_line(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, size_t end_line);
static bool   exit_event_is_duplicate(const struct p101_env *env, const struct contract_model *model, size_t event_index);
static void   analyze_ownership(const struct p101_env *env, struct p101_error *err, const struct contract_model *model, struct contract_report *report);

int p101_error_contract_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    void                  *p101_call_result_1;
    bool                   p101_call_result_2;
    bool                   p101_call_result_3;
    bool                   p101_call_result_4;
    struct contract_model *model;
    struct contract_report report;
    int                    ret_val;

    P101_TRACE_SCOPE(env);
    ret_val            = EXIT_TROUBLE;
    p101_call_result_1 = p101_calloc(env, err, 1U, sizeof(*model));
    model              = (struct contract_model *)p101_call_result_1;
    if(model == NULL)
    {
        goto done;
    }

    p101_error_contract_report_begin(env, err, &report, args);
    p101_error_contract_load_facts(env, err, args, model);

    p101_call_result_2 = p101_error_has_no_error(err);
    if(p101_call_result_2)
    {
        report.files_scanned = model->files_scanned;
        analyze_model(env, err, model, &report, args->strict_sequence);
        analyze_ownership(env, err, model, &report);
    }

    p101_call_result_3 = p101_error_has_no_error(err);
    if(p101_call_result_3)
    {
        p101_error_contract_report_end(env, err, &report);
    }

    p101_call_result_4 = p101_error_has_error(err);
    if(p101_call_result_4)
    {
        goto done;
    }

    ret_val = (report.findings == 0U) ? EXIT_SUCCESS : EXIT_FINDINGS;

done:
    p101_free(env, model);
    return ret_val;
}

static void analyze_ownership(const struct p101_env *env, struct p101_error *err, const struct contract_model *model, struct contract_report *report)
{
    P101_TRACE_SCOPE(env);
    for(size_t i = 0U; i < model->ownership_file_count; i++)
    {
        const struct contract_ownership_file *owner;
        bool                                  no_error;

        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
        owner = &model->ownership_files[i];
        if(owner->error_create_count > owner->error_destroy_count)
        {
            p101_error_contract_report_finding(env,
                                               err,
                                               report,
                                               P101_ERROR_CONTRACT_RULE_ERROR_OWNERSHIP,
                                               owner->path,
                                               owner->first_error_create_line,
                                               "-",
                                               "this file creates more p101_error objects than it destroys; keep ownership local or make transfer explicit");
        }
        if(owner->env_create_count > owner->env_destroy_count)
        {
            p101_error_contract_report_finding(env,
                                               err,
                                               report,
                                               P101_ERROR_CONTRACT_RULE_ENV_OWNERSHIP,
                                               owner->path,
                                               owner->first_env_create_line,
                                               "-",
                                               "this file creates more p101_env objects than it destroys; keep ownership local or make transfer explicit");
        }
    }
}

static void analyze_model(const struct p101_env *env, struct p101_error *err, const struct contract_model *model, struct contract_report *report, bool strict_sequence)
{
    int    p101_expression_result_11;
    int    p101_expression_result_12;
    int    p101_call_result_13;
    int    p101_expression_result_14;
    int    p101_expression_result_15;
    bool   p101_call_result_16;
    bool   p101_call_result_17;
    int    p101_expression_result_18;
    int    p101_expression_result_19;
    int    p101_expression_result_20;
    bool   p101_call_result_21;
    bool   p101_call_result_22;
    bool   p101_call_result_23;
    int    p101_expression_result_24;
    bool   p101_call_result_25;
    int    p101_expression_result_26;
    int    p101_expression_result_27;
    bool   p101_call_result_28;
    size_t p101_call_result_10;
    bool   p101_call_result_5;
    P101_TRACE_SCOPE(env);
    for(size_t i = 0U; i < model->function_count; i++)
    {
        struct contract_function function;
        size_t                   end_line;
        size_t                   second_exit_line;
        size_t                   early_return_line;
        bool                     no_error;

        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
        function          = model->functions[i];
        end_line          = next_function_line(env, model, &function);
        early_return_line = function_early_return_line(env, model, &function, end_line);
        if(early_return_line != 0U)
        {
            p101_error_contract_report_finding(env, err, report, P101_ERROR_CONTRACT_RULE_SINGLE_EXIT, function.path, early_return_line, function.name, "this return is not the function's final top-level statement; converge control flow on one final return");
        }
        else
        {
            p101_call_result_10 = function_exit_count(env, model, &function, end_line, &second_exit_line);
            if(p101_call_result_10 > 1U)
            {
                p101_error_contract_report_finding(env,
                                                   err,
                                                   report,
                                                   P101_ERROR_CONTRACT_RULE_SINGLE_EXIT,
                                                   function.path,
                                                   second_exit_line,
                                                   function.name,
                                                   "this function has more than one exit point; converge control flow on one final return or, for main, one final process-status decision");
            }
        }

        for(size_t j = 0U; j < model->event_count; j++)
        {
            const struct contract_event *event;

            no_error = p101_error_has_no_error(err);
            if(!no_error)
            {
                break;
            }
            event              = &model->events[j];
            p101_call_result_5 = event_is_in_function(env, event, &function, end_line);
            if(!p101_call_result_5)
            {
                continue;
            }

            /*
             * C defines main by spelling, so its exact Clang identity is the
             * unavoidable lexical boundary. Internal exceptions are semantic
             * annotations, never inferred from their names.
             */
            p101_expression_result_12 = 0;
            if(event->kind == CONTRACT_EVENT_PROCESS_TERMINATION)
            {
                p101_call_result_13 = p101_strcmp(env, function.usr, "c:@F@main");
                if(p101_call_result_13 != 0)
                {
                    p101_expression_result_12 = 1;
                }
            }
            p101_expression_result_11 = 0;
            if(p101_expression_result_12)
            {
                if(!function.is_termination_adapter)
                {
                    p101_expression_result_11 = 1;
                }
            }
            if(p101_expression_result_11)
            {
                p101_error_contract_report_finding(env,
                                                   err,
                                                   report,
                                                   P101_ERROR_CONTRACT_RULE_PROCESS_TERMINATION,
                                                   event->path,
                                                   event->line,
                                                   function.name,
                                                   "only main may terminate the process; return a status or raise an error so the caller controls shutdown");
                continue;
            }
            if(event->kind == CONTRACT_EVENT_CALL_NOT_ISOLATED)
            {
                p101_error_contract_report_finding(env,
                                                   err,
                                                   report,
                                                   P101_ERROR_CONTRACT_RULE_ISOLATED_CALL,
                                                   event->path,
                                                   event->line,
                                                   function.name,
                                                   "this call is embedded in another expression; store its result in a named local before using it as an argument, condition, return value, or larger expression");
                continue;
            }

            p101_call_result_16       = p101_contract_event_needs_env(event);
            p101_expression_result_15 = 0;
            if(p101_call_result_16)
            {
                if(!function.env_reported)
                {
                    p101_expression_result_15 = 1;
                }
            }
            p101_expression_result_14 = 0;
            if(p101_expression_result_15)
            {
                p101_call_result_17 = visible_env_before_event(env, model, &function, event, end_line);
                if(!p101_call_result_17)
                {
                    p101_expression_result_14 = 1;
                }
            }
            if(p101_expression_result_14)
            {
                p101_error_contract_report_finding(env, err, report, P101_ERROR_CONTRACT_RULE_ENV_REQUIRED, event->path, event->line, function.name, "p101 call or P101_TRACE appears before a visible p101_env/env contract");
                function.env_reported = true;
            }

            p101_call_result_21       = p101_contract_event_needs_error(event);
            p101_expression_result_20 = 0;
            if(p101_call_result_21)
            {
                if(!function.error_reported)
                {
                    p101_expression_result_20 = 1;
                }
            }
            p101_expression_result_19 = 0;
            if(p101_expression_result_20)
            {
                p101_call_result_22 = visible_error_before_event(env, model, &function, event, end_line);
                if(!p101_call_result_22)
                {
                    p101_expression_result_19 = 1;
                }
            }
            p101_expression_result_18 = 0;
            if(p101_expression_result_19)
            {
                p101_call_result_23 = error_is_explicitly_optional(env, model, &function, event, end_line);
                if(!p101_call_result_23)
                {
                    p101_expression_result_18 = 1;
                }
            }
            if(p101_expression_result_18)
            {
                p101_error_contract_report_finding(env, err, report, P101_ERROR_CONTRACT_RULE_ERROR_REQUIRED, event->path, event->line, function.name, "fallible p101 call or error macro appears before a visible p101_error/err contract");
                function.error_reported = true;
            }

            p101_expression_result_24 = 0;
            if(event->kind == CONTRACT_EVENT_ERROR_DISCARD)
            {
                p101_call_result_25 = error_is_explicitly_optional(env, model, &function, event, end_line);
                if(!p101_call_result_25)
                {
                    p101_expression_result_24 = 1;
                }
            }
            if(p101_expression_result_24)
            {
                p101_error_contract_report_finding(env,
                                                   err,
                                                   report,
                                                   P101_ERROR_CONTRACT_RULE_ERROR_DISCARDED,
                                                   event->path,
                                                   event->line,
                                                   function.name,
                                                   "fallible p101 call passes NULL instead of an error object; handle the failure or document the intentional best-effort boundary");
            }

            p101_expression_result_27 = 0;
            if(strict_sequence)
            {
                if(event->kind == CONTRACT_EVENT_ERROR_UNCHECKED_CHAIN)
                {
                    p101_expression_result_27 = 1;
                }
            }
            p101_expression_result_26 = 0;
            if(p101_expression_result_27)
            {
                p101_call_result_28 = error_is_explicitly_optional(env, model, &function, event, end_line);
                if(!p101_call_result_28)
                {
                    p101_expression_result_26 = 1;
                }
            }
            if(p101_expression_result_26)
            {
                p101_error_contract_report_finding(env, err, report, P101_ERROR_CONTRACT_RULE_UNCHECKED_CHAIN, event->path, event->line, function.name, "another fallible p101 call is reachable before the previous error state is checked or returned");
            }
        }
    }
}

static size_t function_early_return_line(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, size_t end_line)
{
    bool   p101_call_result_30;
    size_t line;

    P101_TRACE_SCOPE(env);
    line = 0U;
    for(size_t index = 0U; index < model->event_count; index++)
    {
        const struct contract_event *event;
        int                          p101_expression_result_29;

        event                     = &model->events[index];
        p101_expression_result_29 = 0;
        if(event->kind == CONTRACT_EVENT_FUNCTION_EARLY_RETURN)
        {
            p101_call_result_30 = event_is_in_function(env, event, function, end_line);
            if(p101_call_result_30)
            {
                p101_expression_result_29 = 1;
            }
        }
        if(p101_expression_result_29)
        {
            line = event->line;
            break;
        }
    }
    return line;
}

static size_t function_exit_count(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, size_t end_line, size_t *second_exit_line)
{
    int    p101_expression_result_31;
    bool   p101_call_result_32;
    bool   p101_call_result_33;
    size_t count;

    P101_TRACE_SCOPE(env);
    count             = 0U;
    *second_exit_line = function->line;
    for(size_t index = 0U; index < model->event_count; index++)
    {
        const struct contract_event *event;

        event = &model->events[index];
        if(event->kind != CONTRACT_EVENT_FUNCTION_RETURN && event->kind != CONTRACT_EVENT_PROCESS_TERMINATION)
        {
            continue;
        }
        p101_call_result_32 = event_is_in_function(env, event, function, end_line);
        if(!p101_call_result_32)
        {
            p101_expression_result_31 = 1;
        }
        else
        {
            p101_call_result_33 = exit_event_is_duplicate(env, model, index);
            if(p101_call_result_33)
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
            continue;
        }
        count++;
        if(count == 2U)
        {
            *second_exit_line = event->line;
        }
    }
    return count;
}

static bool exit_event_is_duplicate(const struct p101_env *env, const struct contract_model *model, size_t event_index)
{
    int                          p101_call_result_38;
    int                          p101_call_result_39;
    const struct contract_event *event;
    bool                         duplicate;

    P101_TRACE_SCOPE(env);
    event     = &model->events[event_index];
    duplicate = false;
    for(size_t index = 0U; index < event_index; index++)
    {
        const struct contract_event *candidate;
        int                          p101_expression_result_34;
        int                          p101_expression_result_35;
        int                          p101_expression_result_36;
        int                          p101_expression_result_37;

        candidate                 = &model->events[index];
        p101_expression_result_37 = 0;
        if(candidate->kind == event->kind)
        {
            if(candidate->line == event->line)
            {
                p101_expression_result_37 = 1;
            }
        }
        p101_expression_result_36 = 0;
        if(p101_expression_result_37)
        {
            if(candidate->start == event->start)
            {
                p101_expression_result_36 = 1;
            }
        }
        p101_expression_result_35 = 0;
        if(p101_expression_result_36)
        {
            p101_call_result_38 = p101_strcmp(env, candidate->path, event->path);
            if(p101_call_result_38 == 0)
            {
                p101_expression_result_35 = 1;
            }
        }
        p101_expression_result_34 = 0;
        if(p101_expression_result_35)
        {
            p101_call_result_39 = p101_strcmp(env, candidate->caller_usr, event->caller_usr);
            if(p101_call_result_39 == 0)
            {
                p101_expression_result_34 = 1;
            }
        }
        if(p101_expression_result_34)
        {
            duplicate = true;
            break;
        }
    }
    return duplicate;
}

static size_t next_function_line(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function)
{
    int    p101_call_result_6;
    size_t line;

    P101_TRACE_SCOPE(env);
    line = (size_t)-1;
    for(size_t i = 0U; i < model->function_count; i++)
    {
        const struct contract_function *candidate;

        candidate = &model->functions[i];
        if(candidate->line <= function->line)
        {
            continue;
        }
        if(candidate->line >= line)
        {
            continue;
        }
        p101_call_result_6 = p101_strcmp(env, candidate->path, function->path);
        if(p101_call_result_6 != 0)
        {
            continue;
        }
        line = candidate->line;
    }

    return line;
}

static bool event_is_in_function(const struct p101_env *env, const struct contract_event *event, const struct contract_function *function, size_t end_line)
{
    int  p101_call_result_7;
    bool result;

    P101_TRACE_SCOPE(env);
    p101_call_result_7 = p101_strcmp(env, event->path, function->path);
    result             = p101_call_result_7 == 0;
    if(result && event->caller_usr[0] != '\0')
    {
        int p101_call_result_8;

        p101_call_result_8 = p101_strcmp(env, event->caller_usr, function->usr);
        result             = p101_call_result_8 == 0;
    }
    else if(result && event->start != 0U && function->end > function->start)
    {
        result = (event->start >= function->start && event->start < function->end) != 0;
    }
    else if(result)
    {
        result = (event->line >= function->line && event->line < end_line) != 0;
    }
    return result;
}

static bool visible_env_before_event(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, const struct contract_event *event, size_t end_line)
{
    bool p101_call_result_42;
    bool p101_call_result_43;
    bool visible;

    P101_TRACE_SCOPE(env);
    visible = function->has_env_contract;
    for(size_t i = 0U; i < model->event_count && !visible; i++)
    {
        const struct contract_event *candidate;
        int                          p101_expression_result_40;
        int                          p101_expression_result_41;

        candidate                 = &model->events[i];
        p101_expression_result_41 = 0;
        if(candidate->kind == CONTRACT_EVENT_ENV_USE)
        {
            p101_call_result_42 = p101_contract_event_is_not_after(candidate, event);
            if(p101_call_result_42)
            {
                p101_expression_result_41 = 1;
            }
        }
        p101_expression_result_40 = 0;
        if(p101_expression_result_41)
        {
            p101_call_result_43 = event_is_in_function(env, candidate, function, end_line);
            if(p101_call_result_43)
            {
                p101_expression_result_40 = 1;
            }
        }
        if(p101_expression_result_40)
        {
            visible = true;
        }
    }

    return visible;
}

static bool visible_error_before_event(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, const struct contract_event *event, size_t end_line)
{
    bool p101_call_result_46;
    bool p101_call_result_47;
    bool visible;

    P101_TRACE_SCOPE(env);
    visible = function->has_error_contract;
    for(size_t i = 0U; i < model->event_count && !visible; i++)
    {
        const struct contract_event *candidate;
        int                          p101_expression_result_44;
        int                          p101_expression_result_45;

        candidate                 = &model->events[i];
        p101_expression_result_45 = 0;
        if(candidate->kind == CONTRACT_EVENT_ERROR_USE)
        {
            p101_call_result_46 = p101_contract_event_is_not_after(candidate, event);
            if(p101_call_result_46)
            {
                p101_expression_result_45 = 1;
            }
        }
        p101_expression_result_44 = 0;
        if(p101_expression_result_45)
        {
            p101_call_result_47 = event_is_in_function(env, candidate, function, end_line);
            if(p101_call_result_47)
            {
                p101_expression_result_44 = 1;
            }
        }
        if(p101_expression_result_44)
        {
            visible = true;
        }
    }

    return visible;
}

static bool error_is_explicitly_optional(const struct p101_env *env, const struct contract_model *model, const struct contract_function *function, const struct contract_event *event, size_t end_line)
{
    int  p101_expression_result_48;
    int  p101_expression_result_49;
    int  p101_call_result_50;
    bool p101_call_result_9;
    bool optional;

    P101_TRACE_SCOPE(env);
    optional = false;
    for(size_t i = 0U; i < model->event_count && !optional; i++)
    {
        const struct contract_event *candidate;

        candidate = &model->events[i];
        if(candidate->kind != CONTRACT_EVENT_ERROR_OPTIONAL)
        {
            continue;
        }
        if(candidate->line != event->line)
        {
            p101_expression_result_49 = 1;
        }
        else
        {
            if(candidate->start != event->start)
            {
                p101_expression_result_49 = 1;
            }
            else
            {
                p101_expression_result_49 = 0;
            }
        }
        if(p101_expression_result_49)
        {
            p101_expression_result_48 = 1;
        }
        else
        {
            p101_call_result_50 = p101_strcmp(env, candidate->caller_usr, event->caller_usr);
            if(p101_call_result_50 != 0)
            {
                p101_expression_result_48 = 1;
            }
            else
            {
                p101_expression_result_48 = 0;
            }
        }
        if(p101_expression_result_48)
        {
            continue;
        }
        p101_call_result_9 = event_is_in_function(env, candidate, function, end_line);
        if(p101_call_result_9)
        {
            optional = true;
        }
    }

    return optional;
}

#ifdef P101_ERROR_CONTRACT_TESTING
void p101_error_contract_test_analyze(const struct p101_env *env, struct p101_error *err, const struct contract_model *model, struct contract_report *report)
{
    analyze_ownership(env, err, model, report);
    analyze_model(env, err, model, report, true);
}
#endif
