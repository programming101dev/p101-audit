#include "cli.h"
#include "model.h"
#include "output.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>

enum
{
    EXIT_FINDINGS = 1,
    EXIT_TROUBLE  = 2
};

int main(int argc, char *argv[])
{
    int                           p101_expression_result_7;
    int                           p101_expression_result_8;
    int                           p101_call_result_9;
    int                           p101_call_result_10;
    int                           p101_expression_result_11;
    bool                          p101_call_result_12;
    bool                          p101_call_result_13;
    int                           p101_expression_result_14;
    bool                          p101_call_result_15;
    int                           p101_expression_result_16;
    bool                          p101_call_result_17;
    bool                          p101_call_result_6;
    bool                          p101_call_result_1;
    bool                          p101_call_result_2;
    bool                          p101_call_result_3;
    bool                          p101_call_result_4;
    const char                   *p101_call_result_5;
    struct p101_error            *err;
    struct p101_env              *env;
    struct p101_wrapper_arguments arguments;
    struct p101_wrapper_model     model;
    size_t                        index;
    int                           status;
    bool                          help;

    err                      = p101_error_create(false);
    env                      = p101_env_create(err, NULL);
    status                   = EXIT_TROUBLE;
    help                     = false;
    p101_expression_result_7 = 0;
    if(argc == 2)
    {
        p101_call_result_9 = p101_strcmp(env, argv[1], "-h");
        if(p101_call_result_9 == 0)
        {
            p101_expression_result_8 = 1;
        }
        else
        {
            p101_call_result_10 = p101_strcmp(env, argv[1], "--help");
            if(p101_call_result_10 == 0)
            {
                p101_expression_result_8 = 1;
            }
            else
            {
                p101_expression_result_8 = 0;
            }
        }
        if(p101_expression_result_8)
        {
            p101_expression_result_7 = 1;
        }
    }
    if(p101_expression_result_7)
    {
        help = true;
    }
    p101_wrapper_model_init(&model);
    p101_call_result_1 = p101_wrapper_parse_arguments(env, err, argc, argv, &arguments, false);
    if(!p101_call_result_1)
    {
        p101_wrapper_usage(env, P101_ERROR_OPTIONAL, argv[0], false);    // P101_ERROR_OPTIONAL rationale: usage must survive argument errors.
        if(help)
        {
            status = EXIT_SUCCESS;
        }
        goto done;
    }
    p101_call_result_2 = p101_wrapper_model_load_inventory(env, err, &model, &arguments, argv[0]);
    if(!p101_call_result_2)
    {
        goto done;
    }
    if(arguments.show_inventory || arguments.show_inventory_json)
    {
        p101_wrapper_write_inventory(env, err, &model, arguments.show_inventory_json);
        status             = EXIT_TROUBLE;
        p101_call_result_3 = p101_error_has_no_error(err);
        if(p101_call_result_3)
        {
            status = EXIT_SUCCESS;
        }
        goto done;
    }
    p101_call_result_12 = p101_wrapper_model_scan(env, err, &model, &arguments);
    if(!p101_call_result_12)
    {
        p101_expression_result_11 = 1;
    }
    else
    {
        p101_call_result_13 = p101_wrapper_write_optional_outputs(env, err, &model, &arguments);
        if(!p101_call_result_13)
        {
            p101_expression_result_11 = 1;
        }
        else
        {
            p101_expression_result_11 = 0;
        }
    }
    if(p101_expression_result_11)
    {
        goto done;
    }
    if(arguments.emit_facts)
    {
        p101_wrapper_write_facts(env, err, &model, stdout);
        status                    = EXIT_TROUBLE;
        p101_call_result_15       = p101_error_has_no_error(err);
        p101_expression_result_14 = 0;
        if(p101_call_result_15)
        {
            if(model.parse_failures == 0U)
            {
                p101_expression_result_14 = 1;
            }
        }
        if(p101_expression_result_14)
        {
            status = EXIT_SUCCESS;
        }
        goto done;
    }
    p101_call_result_4 = p101_wrapper_model_judge(env, err, &model, &arguments);
    if(!p101_call_result_4)
    {
        goto done;
    }
    p101_wrapper_write_audit(env, err, &model, &arguments);
    status                    = EXIT_TROUBLE;
    p101_call_result_17       = p101_error_has_no_error(err);
    p101_expression_result_16 = 0;
    if(p101_call_result_17)
    {
        if(model.parse_failures == 0U)
        {
            p101_expression_result_16 = 1;
        }
    }
    if(p101_expression_result_16)
    {
        status = EXIT_SUCCESS;
    }
    for(index = 0U; index < model.finding_count && status == EXIT_SUCCESS; index++)
    {
        if(model.findings[index].kind == P101_WRAPPER_MISSED || model.findings[index].kind == P101_WRAPPER_PORTABILITY ||
           (arguments.strict_external && (model.findings[index].kind == P101_WRAPPER_EXTERNAL || model.findings[index].kind == P101_WRAPPER_INDIRECT)))
        {
            status = EXIT_FINDINGS;
        }
    }

done:
{
    p101_call_result_6 = p101_error_has_error(err);
    if(p101_call_result_6)
    {
        /* P101_ERROR_OPTIONAL rationale: diagnostic output must not overwrite the reported failure. */
        p101_call_result_5 = p101_error_get_message(err);
        p101_fprintf(env, P101_ERROR_OPTIONAL, stderr, "audit-wrappers: %s\n", p101_call_result_5);
        status = EXIT_TROUBLE;
    }
}
    p101_wrapper_model_destroy(env, &model);
    p101_env_destroy(env);
    p101_error_destroy(err);
    return status;
}
