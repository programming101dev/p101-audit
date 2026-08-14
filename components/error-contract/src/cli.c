#include "../include/cli.h"
#include "../include/constants.h"
#include "../include/errors.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <p101_cli/p101_getopt.h>
#include <p101_cli/p101_stdlib.h>
#include <p101_cli/p101_unistd.h>
#include <p101_tool_support/report.h>
#include <stdlib.h>

void p101_error_contract_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->human = true;
}

void p101_error_contract_arguments_deinit(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    /*
     * The struct owns nothing today; zeroing it keeps a freed view from
     * looking live and keeps every call site uniform if that changes.
     */
    p101_memset(env, args, 0, sizeof(*args));
}

void p101_error_contract_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    bool no_error;
    int  p101_expression_result_2;
    int  p101_call_result_3;
    int  p101_call_result_1;
    int  opt;
#ifdef P101_ERROR_CONTRACT_TESTING
    const char *forced_option;
#endif

    P101_TRACE_SCOPE(env);
    opterr = 0;
#ifdef P101_ERROR_CONTRACT_TESTING
    forced_option = getenv("P101_ERROR_CONTRACT_TEST_OPTION");
#endif

    p101_expression_result_2 = 0;
    if(argc == 2)
    {
        p101_call_result_3 = p101_strcmp(env, argv[1], "--help");
        if(p101_call_result_3 == 0)
        {
            p101_expression_result_2 = 1;
        }
    }
    if(p101_expression_result_2)
    {
        args->show_help = true;
        goto done;
    }

    for(;;)
    {
#ifdef P101_ERROR_CONTRACT_TESTING
        if(forced_option == NULL)
        {
            opt = p101_getopt(env, argc, argv, ":hd:qSvi:C:");
        }
        else
        {
            opt = (unsigned char)*forced_option;
        }
#else
        opt = p101_getopt(env, argc, argv, ":hd:qSvi:C:");
#endif
        if(opt == -1)
        {
            break;
        }
        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
#ifdef P101_ERROR_CONTRACT_TESTING
        forced_option = NULL;
#endif
        switch(opt)
        {
            case 'h':
            {
                args->show_help = true;
                break;
            }
            case 'd':
            {
                unsigned int outputs;
                int          parse_status;

                outputs      = 0U;
                parse_status = p101_tool_report_parse_output_option(argv[optind - 1], &outputs);
                if(parse_status != 0)
                {
                    P101_ERROR_RAISE_USER(err, "Diagnostic output must be human, json, or human,json.", ERR_USAGE);
                    break;
                }
                args->human = (outputs & P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN) != 0U;
                args->json  = (outputs & P101_TOOL_DIAGNOSTIC_OUTPUT_JSON) != 0U;
                break;
            }
            case 'q':
            {
                args->quiet = true;
                break;
            }
            case 'S':
            {
                args->strict_sequence = true;
                break;
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'i':
            {
                args->facts_path = optarg;
                break;
            }
            case 'C':
            {
                args->compile_db_path = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt);
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            {
                char msg[MSG_LEN];

                p101_call_result_1 = p101_isprint(env, optopt);
                if(p101_call_result_1)
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option '-%c'.", optopt);
                }
                else
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option character 0x%02X.", (unsigned)(unsigned char)optopt);
                }

                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            default:
            {
                char msg[MSG_LEN];
                int  option_character;

                p101_call_result_1 = p101_isprint(env, opt);
                if(p101_call_result_1)
                {
                    option_character = opt;
                }
                else
                {
                    option_character = '?';
                }
                p101_snprintf(env, err, msg, sizeof(msg), "Internal error: unhandled option '-%c' returned by getopt.", option_character);
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    while(optind < argc && args->path_count < P101_ERROR_CONTRACT_MAX_PATHS)
    {
        args->paths[args->path_count++] = argv[optind++];
    }

    if(optind < argc)
    {
        P101_ERROR_RAISE_USER(err, "Too many paths.", ERR_USAGE);
    }

done:
    return;
}

void p101_error_contract_check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE_SCOPE(env);

    if(args->compile_db_path != NULL && args->compile_db_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The compile database path must not be empty.", ERR_USAGE);
    }
    if(args->facts_path != NULL && args->facts_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The facts path must not be empty.", ERR_USAGE);
    }
    if(args->facts_path != NULL && args->compile_db_path != NULL)
    {
        P101_ERROR_RAISE_USER(err, "The facts snapshot cannot be combined with -C.", ERR_USAGE);
    }
}

void p101_error_contract_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = (exit_code == EXIT_SUCCESS) ? stdout : stderr;

    if(message != NULL)
    {
        p101_fprintf(env, err, stream, "%s\n\n", message);
    }

    p101_fprintf(env, err, stream, "Usage: %s [-h] [-d:FORMAT] [-q] [-S] [-v] [-i <facts.tsv>] [-C <compile_commands.json>] [path ...]\n", program_name);
    p101_fputs(env, err, "\nChecks p101 error-handling contracts in C source files.\n\n", stream);
    p101_fputs(env, err, "Options:\n", stream);
    p101_fputs(env, err, "  -d:FORMAT Diagnostics: human, json, or human,json.\n", stream);
    p101_fputs(env, err, "  -q        Quiet: print only findings, not the clean summary.\n", stream);
    p101_fputs(env, err, "  -S        Strict sequencing: report unchecked chains of fallible calls.\n", stream);
    p101_fputs(env, err, "  -v        Describe the native lib_c_facts scan on stderr.\n", stream);
    p101_fputs(env, err, "  -i <file> Read a reusable P101FACT v" P101_C_FACT_VERSION " snapshot instead of invoking Clang.\n", stream);
    p101_fputs(env, err, "  -C <file> Compile database used by lib_c_facts.\n", stream);
    p101_fputs(env, err, "  -h        Show this help.\n", stream);
    p101_fputs(env, err, "\nIf no path is given, src is scanned.\n", stream);
    p101_fputs(env, err, "\nExit status: 0 clean, 1 findings, 2 usage/tool trouble.\n", stream);
}
