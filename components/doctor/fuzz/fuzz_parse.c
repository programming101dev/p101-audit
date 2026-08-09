/*
 * libFuzzer harness for audit-doctor's argument parser.
 */
#include "cli.h"
#include "constants.h"
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FUZZ_MAX_ARGS 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char              *buf;
    char              *argv[FUZZ_MAX_ARGS];
    int                argc;
    char              *p;
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;

    buf = NULL;
    err = p101_error_create(false);
    env = p101_env_create(err, NULL);

    buf = (char *)p101_malloc(env, err, size + 1U);
    if(buf == NULL)
    {
        goto done;
    }
    p101_memcpy(env, buf, data, size);
    buf[size] = '\0';

    argv[0] = (char *)"audit-doctor";
    argc    = 1;
    p       = buf;
    while(argc < FUZZ_MAX_ARGS - 1)
    {
        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f')
        {
            p++;
        }
        if(*p == '\0')
        {
            break;
        }
        argv[argc++] = p;
        while(*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '\v' && *p != '\f')
        {
            p++;
        }
        if(*p != '\0')
        {
            *p++ = '\0';
        }
    }
    argv[argc] = NULL;

#ifdef __GLIBC__
    optind = 0;
#else
    {
        extern int optreset;
        optreset = 1;
        optind   = 1;
    }
#endif

    p101_doctor_arguments_init(env, &args);

    for(int i = 1; i < argc; i++)
    {
        if(p101_strcmp(env, argv[i], "--help") == 0)
        {
            goto done;
        }

        if(argv[i][0] == '-' && argv[i][1] != '-' && p101_strchr(env, argv[i] + 1, 'h') != NULL)
        {
            goto done;
        }
    }

    p101_doctor_parse_arguments(env, err, argc, argv, &args);

    if(!args.show_help && p101_error_has_no_error(err))
    {
        p101_doctor_check_arguments(env, err, &args);
    }

    if(!args.show_help && p101_error_has_no_error(err))
    {
        p101_doctor_convert_arguments(env, err, &args);
    }

done:
    p101_free(env, buf);
    p101_env_destroy(env);
    p101_error_destroy(err);
    return 0;
}
