#include "cli.h"
#include "model.h"
#include "output.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>

enum
{
    EXIT_TROUBLE = 2
};

int main(int argc, char *argv[])
{
    int                           p101_expression_result_5;
    int                           p101_expression_result_6;
    int                           p101_call_result_7;
    int                           p101_call_result_8;
    int                           p101_expression_result_9;
    bool                          p101_call_result_10;
    bool                          p101_call_result_4;
    bool                          p101_call_result_1;
    bool                          p101_call_result_2;
    const char                   *p101_call_result_3;
    struct p101_error            *err;
    struct p101_env              *env;
    struct p101_wrapper_arguments arguments;
    struct p101_wrapper_model     model;
    int                           status;
    bool                          help;

    err                      = p101_error_create(false);
    env                      = p101_env_create(err, NULL);
    status                   = EXIT_TROUBLE;
    help                     = false;
    p101_expression_result_5 = 0;
    if(argc == 2)
    {
        p101_call_result_7 = p101_strcmp(env, argv[1], "-h");
        if(p101_call_result_7 == 0)
        {
            p101_expression_result_6 = 1;
        }
        else
        {
            p101_call_result_8 = p101_strcmp(env, argv[1], "--help");
            if(p101_call_result_8 == 0)
            {
                p101_expression_result_6 = 1;
            }
            else
            {
                p101_expression_result_6 = 0;
            }
        }
        if(p101_expression_result_6)
        {
            p101_expression_result_5 = 1;
        }
    }
    if(p101_expression_result_5)
    {
        help = true;
    }
    p101_wrapper_model_init(&model);
    p101_call_result_1 = p101_wrapper_parse_arguments(env, err, argc, argv, &arguments, true);
    if(!p101_call_result_1)
    {
        p101_wrapper_usage(env, P101_ERROR_OPTIONAL, argv[0], true);    // P101_ERROR_OPTIONAL rationale: usage must survive argument errors.
        if(help)
        {
            status = EXIT_SUCCESS;
        }
        goto done;
    }
    p101_call_result_2 = p101_wrapper_model_scan(env, err, &model, &arguments);
    if(!p101_call_result_2)
    {
        goto done;
    }
    p101_wrapper_write_facts(env, err, &model, stdout);
    p101_wrapper_write_diagnostics(env, err, &model, stderr);
    status                   = EXIT_TROUBLE;
    p101_call_result_10      = p101_error_has_no_error(err);
    p101_expression_result_9 = 0;
    if(p101_call_result_10)
    {
        if(model.parse_failures == 0U)
        {
            p101_expression_result_9 = 1;
        }
    }
    if(p101_expression_result_9)
    {
        status = EXIT_SUCCESS;
    }

done:
{
    p101_call_result_4 = p101_error_has_error(err);
    if(p101_call_result_4)
    {
        /* P101_ERROR_OPTIONAL rationale: diagnostic output must not overwrite the reported failure. */
        p101_call_result_3 = p101_error_get_message(err);
        p101_fprintf(env, P101_ERROR_OPTIONAL, stderr, "p101-c-facts: %s\n", p101_call_result_3);
        status = EXIT_TROUBLE;
    }
}
    p101_wrapper_model_destroy(env, &model);
    p101_env_destroy(env);
    p101_error_destroy(err);
    return status;
}
