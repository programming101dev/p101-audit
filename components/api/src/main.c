#include "api.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stdlib.h>

enum
{
    EXIT_FINDINGS = 1,
    EXIT_TROUBLE  = 2
};

static void usage(const struct p101_env *env, struct p101_error *err, const char *program);

int main(int argc, char **argv)
{
    struct p101_error *err;
    struct p101_env   *env;
    const char        *message;
    int                comparison;
    int                status;
    bool               has_error;

    err    = p101_error_create(false);
    env    = p101_env_create(err, NULL);
    status = EXIT_TROUBLE;
    if(argc == 2)
    {
        comparison = p101_strcmp(env, argv[1], "--help");
        if(comparison == 0)
        {
            usage(env, P101_ERROR_OPTIONAL, argv[0]);
            status = EXIT_SUCCESS;
            goto done;
        }
    }
    if(argc == 5)
    {
        comparison = p101_strcmp(env, argv[1], "snapshot");
        if(comparison == 0)
        {
            status = p101_api_snapshot(env, err, argv[2], argv[3], argv[4]);
            goto done;
        }
    }
    if(argc == 4)
    {
        comparison = p101_strcmp(env, argv[1], "compare");
        if(comparison == 0)
        {
            status = p101_api_compare(env, err, argv[2], argv[3]);
            goto done;
        }
    }
    usage(env, P101_ERROR_OPTIONAL, argv[0]);

done:
    has_error = p101_error_has_error(err);
    if(has_error)
    {
        message = p101_error_get_message(err);
        p101_fprintf(env, P101_ERROR_OPTIONAL, stderr, "%s:1:1: error: %s [P101-API-TROUBLE]\n", argv[0], message);
        status = EXIT_TROUBLE;
    }
    p101_env_destroy(env);
    p101_error_destroy(err);
    return status;
}

static void usage(const struct p101_env *env, struct p101_error *err, const char *program)
{
    p101_fprintf(env, err, stderr, "Usage: %s snapshot WORKSPACE FACTS OUTPUT\n       %s compare OLD NEW\n", program, program);
}
