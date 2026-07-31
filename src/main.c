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
    struct p101_error            *err;
    struct p101_env              *env;
    struct p101_wrapper_arguments arguments;
    struct p101_wrapper_model     model;
    size_t                        index;
    int                           status;
    bool                          help;

    err    = p101_error_create(false);
    env    = p101_env_create(err, NULL);
    status = EXIT_TROUBLE;
    help   = false;
    if(argc == 2 && (p101_strcmp(env, argv[1], "-h") == 0 || p101_strcmp(env, argv[1], "--help") == 0))
    {
        help = true;
    }
    p101_wrapper_model_init(&model);
    if(!p101_wrapper_parse_arguments(env, err, argc, argv, &arguments, false))
    {
        p101_wrapper_usage(env, NULL, argv[0], false);
        if(help)
        {
            status = EXIT_SUCCESS;
        }
        goto done;
    }
    if(!p101_wrapper_model_load_inventory(env, err, &model, &arguments, argv[0]))
    {
        goto done;
    }
    if(arguments.show_inventory || arguments.show_inventory_json)
    {
        p101_wrapper_write_inventory(env, err, &model, arguments.show_inventory_json);
        status = EXIT_TROUBLE;
        if(p101_error_has_no_error(err))
        {
            status = EXIT_SUCCESS;
        }
        goto done;
    }
    if(!p101_wrapper_model_scan(env, err, &model, &arguments) || !p101_wrapper_write_optional_outputs(env, err, &model, &arguments))
    {
        goto done;
    }
    if(arguments.emit_facts)
    {
        p101_wrapper_write_facts(env, err, &model, stdout);
        status = EXIT_TROUBLE;
        if(p101_error_has_no_error(err) && model.parse_failures == 0U)
        {
            status = EXIT_SUCCESS;
        }
        goto done;
    }
    if(!p101_wrapper_model_judge(env, err, &model, &arguments))
    {
        goto done;
    }
    p101_wrapper_write_audit(env, err, &model, &arguments);
    status = EXIT_TROUBLE;
    if(p101_error_has_no_error(err) && model.parse_failures == 0U)
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
    if(p101_error_has_error(err))
    {
        p101_fprintf(env, NULL, stderr, "p101-wrapper-audit: %s\n", p101_error_get_message(err));
        status = EXIT_TROUBLE;
    }
    p101_wrapper_model_destroy(env, &model);
    p101_env_destroy(env);
    p101_error_destroy(err);
    return status;
}
