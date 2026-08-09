#include "../include/runner.h"
#include "../include/constants.h"
#include "../include/fact_loader.h"
#include "../include/model.h"
#include "../include/report.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdio.h>
#include <stdlib.h>

int p101_module_map_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    bool                      p101_call_result_1;
    bool                      p101_call_result_2;
    static struct project_map map;
    FILE                     *stream;
    int                       ret_val;
    bool                      has_findings;

    P101_TRACE_SCOPE(env);
    p101_memset(env, &map, 0, sizeof(map));
    stream  = stdout;
    ret_val = EXIT_TROUBLE;

    p101_module_map_load_clang_facts(env, err, &map, args);
    p101_call_result_1 = p101_error_has_error(err);
    if(p101_call_result_1)
    {
        goto done;
    }

    if(args->output_path != NULL)
    {
        stream = p101_fopen(env, err, args->output_path, "w");
        if(stream == NULL)
        {
            goto done;
        }
    }

    has_findings       = p101_module_map_write_report(env, err, stream, args, &map);
    p101_call_result_2 = p101_error_has_no_error(err);
    if(p101_call_result_2)
    {
        if(has_findings)
        {
            ret_val = EXIT_FINDINGS;
        }
        else
        {
            ret_val = EXIT_SUCCESS;
        }
    }
    else
    {
        ret_val = EXIT_TROUBLE;
    }

done:
    if(stream != NULL && stream != stdout)
    {
        p101_fclose(env, err, stream);
    }
    return ret_val;
}
