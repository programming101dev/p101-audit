#include "../include/cli.h"
#include "../include/constants.h"
#include "../include/contract.h"
#include "../include/errors.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int                p101_expression_result_7;
    bool               p101_call_result_8;
    bool               p101_call_result_6;
    bool               p101_call_result_1;
    bool               p101_call_result_2;
    bool               p101_call_result_3;
    const char        *p101_call_result_4;
    const char        *p101_call_result_5;
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;
    int                ret_val;

    ret_val = EXIT_TROUBLE;
    err     = p101_error_create(false);
    env     = p101_env_create(err, NULL);

    p101_error_contract_arguments_init(env, &args);
    p101_error_contract_parse_arguments(env, err, argc, argv, &args);

    p101_expression_result_7 = 0;
    if(args.show_help)
    {
        p101_call_result_8 = p101_error_has_no_error(err);
        if(p101_call_result_8)
        {
            p101_expression_result_7 = 1;
        }
    }
    if(p101_expression_result_7)
    {
        p101_error_contract_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
        ret_val = EXIT_SUCCESS;
        goto done;
    }

    p101_call_result_1 = p101_error_has_error(err);
    if(p101_call_result_1)
    {
        goto done;
    }

    p101_error_contract_check_arguments(env, err, &args);

    p101_call_result_2 = p101_error_has_error(err);
    if(p101_call_result_2)
    {
        goto done;
    }

    ret_val = p101_error_contract_run(env, err, &args);

done:
{
    p101_call_result_6 = p101_error_has_error(err);
    if(p101_call_result_6)
    {
        p101_call_result_3 = p101_error_is_error(err, P101_ERROR_USER, ERR_USAGE);
        if(p101_call_result_3)
        {
            p101_call_result_4 = p101_error_get_message(err);
            p101_error_contract_usage(env, err, argv[0], EXIT_TROUBLE, p101_call_result_4);
        }
        else
        {
            p101_call_result_5 = p101_error_get_message(err);
            p101_fprintf(env, err, stderr, "%s\n", p101_call_result_5);
        }
        ret_val = EXIT_TROUBLE;
    }
}

    p101_error_contract_arguments_deinit(env, &args);
    p101_env_destroy(env);
    p101_error_destroy(err);

    return ret_val;
}
