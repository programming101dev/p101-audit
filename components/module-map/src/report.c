#include "../include/report.h"
#include "../include/idioms.h"
#include "../include/idioms_includes.h"
#include "../include/model.h"
#include "../include/model_query.h"
#include "../include/strings.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <p101_record/record.h>
#include <p101_tool_event/report.h>
#include <stdarg.h>
#include <stdio.h>

enum p101_module_rule
{
    P101_MODULE_RULE_MODULE_SIZE = 0,
    P101_MODULE_RULE_PUBLIC_API_SIZE,
    P101_MODULE_RULE_ENTRYPOINT_SIZE,
    P101_MODULE_RULE_UNUSED_PUBLIC_FUNCTION,
    P101_MODULE_RULE_DECLARATION_WITHOUT_DEFINITION,
    P101_MODULE_RULE_UNUSED_INTERFACE,
    P101_MODULE_RULE_UNUSED_PUBLIC_MACRO,
    P101_MODULE_RULE_UNUSED_PUBLIC_TYPE,
    P101_MODULE_RULE_INCLUDE_CYCLE,
    P101_MODULE_RULE_MISSING_INCLUDE_TARGET,
    P101_MODULE_RULE_LAYER_VIOLATION,
    P101_MODULE_RULE_MISSING_DESTROY,
    P101_MODULE_RULE_COLLECTION_ACCESSOR_PAIR,
    P101_MODULE_RULE_MODULE_PREFIX,
    P101_MODULE_RULE_INCLUDE_GUARD,
    P101_MODULE_RULE_OWN_HEADER_FIRST,
    P101_MODULE_RULE_MISSING_TEARDOWN,
    P101_MODULE_RULE_NAME_CONVERSION_PAIR,
    P101_MODULE_RULE_RESERVED_TYPE_SUFFIX,
    P101_MODULE_RULE_CONTEXT_ARGUMENT_ORDER,
    P101_MODULE_RULE_ALLOCATION_SIZE_TYPE,
    P101_MODULE_RULE_COUNT
};

static const p101_tool_finding module_findings[] = {P101_TOOL_FINDING_MOD_002, P101_TOOL_FINDING_MOD_003, P101_TOOL_FINDING_MOD_004, P101_TOOL_FINDING_MOD_006, P101_TOOL_FINDING_MOD_007, P101_TOOL_FINDING_MOD_008, P101_TOOL_FINDING_MOD_009,
                                                    P101_TOOL_FINDING_MOD_010, P101_TOOL_FINDING_MOD_011, P101_TOOL_FINDING_MOD_012, P101_TOOL_FINDING_MOD_013, P101_TOOL_FINDING_MOD_014, P101_TOOL_FINDING_MOD_015, P101_TOOL_FINDING_MOD_016,
                                                    P101_TOOL_FINDING_MOD_017, P101_TOOL_FINDING_MOD_018, P101_TOOL_FINDING_MOD_019, P101_TOOL_FINDING_MOD_020, P101_TOOL_FINDING_MOD_021, P101_TOOL_FINDING_MOD_022, P101_TOOL_FINDING_MOD_027};

static bool p101_module_map_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target);
static bool p101_module_map_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name);
static bool p101_module_map_module_contains_entrypoint(const struct p101_env *env, const struct project_map *map, const char *module_name);
static void p101_module_map_report_check(struct p101_error *err, int status);
static void p101_module_map_write_finding(const struct p101_env *env, struct p101_error *err, struct p101_tool_report *report, size_t *finding_count, enum p101_module_rule rule_kind, const char *path, size_t line, const char *format, ...)
    P101_ATTR_PRINTF(8, 9);
#ifdef P101_MODULE_MAP_TESTING
static void p101_module_map_write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);
#endif

static bool p101_module_map_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    bool has_source;

    has_source = false;
    for(size_t i = 0U; i < map->module_count; i++)
    {
        int p101_call_result_1;

        p101_call_result_1 = p101_strcmp(env, map->modules[i].name, module_name);
        if(p101_call_result_1 == 0)
        {
            has_source = map->modules[i].source_count > 0U;
            break;
        }
    }
    return has_source;
}

static bool p101_module_map_module_contains_entrypoint(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    int  p101_call_result_17;
    bool contains_entrypoint;

    contains_entrypoint = false;
    for(size_t index = 0U; index < map->function_count; index++)
    {
        int p101_expression_result_15;
        int p101_call_result_16;

        p101_call_result_16       = p101_strcmp(env, map->functions[index].module, module_name);
        p101_expression_result_15 = 0;
        if(p101_call_result_16 == 0)
        {
            p101_call_result_17 = p101_strcmp(env, map->functions[index].usr, "c:@F@main");
            if(p101_call_result_17 == 0)
            {
                p101_expression_result_15 = 1;
            }
        }
        if(p101_expression_result_15)
        {
            contains_entrypoint = true;
            break;
        }
    }
    return contains_entrypoint;
}

static bool p101_module_map_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target)
{
    int         p101_expression_result_18;
    int         p101_call_result_19;
    int         p101_call_result_20;
    int         p101_call_result_2;
    FILE       *stream;
    bool        ret_val;
    char        line[MAX_LINE];
    bool        no_error;
    const char *line_result;

    ret_val = true;
    stream  = NULL;

    p101_call_result_2 = p101_strcmp(env, from_module, target);
    if(p101_call_result_2 == 0)
    {
        goto done;
    }

    if(args->layer_config_path == NULL)
    {
        goto done;
    }

    ret_val = false;
    stream  = p101_fopen(env, err, args->layer_config_path, "r");

    if(stream == NULL)
    {
        goto done;
    }

    for(;;)
    {
        char       *left;
        char       *right;
        char       *arrow;
        const char *found;

        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
        line_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(line_result == NULL)
        {
            break;
        }
        p101_module_map_trim_right(env, line);
        left = p101_module_map_trim_left(env, line);

        if(left[0] == '\0' || left[0] == '#')
        {
            continue;
        }

        found = p101_strstr(env, left, "->");

        if(found == NULL)
        {
            continue;
        }

        arrow    = &left[found - left];
        arrow[0] = '\0';
        p101_module_map_trim_right(env, left);
        right = p101_module_map_trim_left(env, arrow + 2);
        p101_module_map_trim_right(env, right);

        p101_call_result_19       = p101_strcmp(env, left, from_module);
        p101_expression_result_18 = 0;
        if(p101_call_result_19 == 0)
        {
            p101_call_result_20 = p101_strcmp(env, right, target);
            if(p101_call_result_20 == 0)
            {
                p101_expression_result_18 = 1;
            }
        }
        if(p101_expression_result_18)
        {
            ret_val = true;
            break;
        }
    }

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }

    return ret_val;
}

bool p101_module_map_write_report(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map)
{
    FILE *human_stream;
    bool  has_findings;

    P101_TRACE_SCOPE(env);
    human_stream = NULL;
    if(args->human && args->json)
    {
        human_stream = stderr;
    }
    else if(args->human || !args->json)
    {
        human_stream = stream;
    }
    if(human_stream != NULL)
    {
        p101_fputs(env, err, "# p101 module map\n\n", human_stream);
        p101_fputs(env, err, "> Parser note: this report consumes Clang AST facts from `audit-wrappers`; the module design checks are still teaching heuristics, not proof obligations.\n\n", human_stream);
        if(args->library_mode)
        {
            p101_fputs(env, err, "> Library mode: public API use requires external consumers, so this report omits closed-world unused-public-symbol checks.\n\n", human_stream);
        }
        p101_fprintf(env, err, human_stream, "Files scanned: `%zu`\n\n", map->file_count);
        p101_fprintf(env, err, human_stream, "Modules found: `%zu`\n\n", map->module_count);
        p101_fprintf(env, err, human_stream, "Functions found: `%zu`\n\n", map->function_count);
        if(map->calls_dropped > 0U)
        {
            p101_fprintf(env, err, human_stream, "Call-like tokens dropped after cap: `%zu`\n\n", map->calls_dropped);
        }
        p101_module_map_write_modules(env, err, human_stream, map);
        p101_module_map_write_include_graph(env, err, human_stream, map);
    }
    has_findings = p101_module_map_write_findings(env, err, stream, args, map);
    return has_findings;
}

void p101_module_map_write_modules(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map)
{
    P101_TRACE_SCOPE(env);
    p101_fputs(env, err, "## Modules\n\n", stream);
    p101_fputs(env, err, "| Module | Source | Header | Functions | Public | Static | Header declarations | Macros | Types | Includes |\n", stream);
    p101_fputs(env, err, "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n", stream);

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;

        module = &map->modules[i];
        p101_fprintf(env,
                     err,
                     stream,
                     "| `%s` | %zu | %zu | %zu | %zu | %zu | %zu | %zu | %zu | %zu local / %zu external |\n",
                     module->name,
                     module->source_count,
                     module->header_count,
                     module->function_count,
                     module->public_function_count,
                     module->static_function_count,
                     module->header_declaration_count,
                     module->macro_count,
                     module->type_count,
                     module->local_include_count,
                     module->external_include_count);
    }

    p101_fputs(env, err, "\n## Functions by module\n\n", stream);
    for(size_t i = 0; i < map->module_count; i++)
    {
        p101_fprintf(env, err, stream, "### `%s`\n\n", map->modules[i].name);
        p101_module_map_write_functions_for_module(env, err, stream, map, map->modules[i].name);
    }
}

void p101_module_map_write_include_graph(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map)
{
    P101_TRACE_SCOPE(env);
    p101_fputs(env, err, "## Local include graph\n\n", stream);

    for(size_t i = 0; i < map->include_count; i++)
    {
        const struct include_record *include;

        include = &map->includes[i];
        if(include->is_local)
        {
            p101_fprintf(env, err, stream, "- `%s` -> `%s` (%s:%zu)\n", include->from_module, include->target, include->path, include->line);
        }
    }

    p101_fputs(env, err, "\n", stream);
}

bool p101_module_map_write_findings(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, const struct project_map *map)
{
    const struct p101_tool_report_counter counters[] = {
        {"files_scanned", map->file_count    },
        {"modules",       map->module_count  },
        {"functions",     map->function_count},
        {"calls_dropped", map->calls_dropped }
    };
    struct p101_tool_report_options report_options = {"audit-modules",
                                                      "Clang AST module facts and an optional declared layer policy for the selected translation units.",
                                                      "This heuristic report cannot see unselected translation units, external consumers, runtime behavior, or undeclared architectural intent.",
                                                      0U,
                                                      true};
    bool                            p101_call_result_28;
    bool                            p101_call_result_34;
    bool                            p101_call_result_35;
    int                             p101_call_result_36;
    int                             p101_expression_result_39;
    bool                            p101_call_result_40;
    bool                            p101_call_result_41;
    bool                            p101_call_result_45;
    bool                            p101_call_result_46;
    bool                            p101_call_result_49;
    bool                            p101_call_result_52;
    int                             p101_call_result_53;
    bool                            p101_call_result_55;
    bool                            p101_call_result_57;
    bool                            wrote;
    struct p101_tool_report         report;
    p101_tool_outcome               outcome;
    size_t                          finding_count;
    int                             exit_status;
    int                             report_status;

    P101_TRACE_SCOPE(env);
    wrote         = false;
    finding_count = 0U;
    if(args->human)
    {
        report_options.outputs |= P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN;
    }
    if(args->json)
    {
        report_options.outputs |= P101_TOOL_DIAGNOSTIC_OUTPUT_JSON;
    }
    if(report_options.outputs == 0U)
    {
        report_options.outputs = P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN;
    }
    report_status = p101_tool_report_begin(&report, stream, stderr, &report_options);
    p101_module_map_report_check(err, report_status);
    if(report_status != 0)
    {
        goto done;
    }
    if(report.human_stream != NULL)
    {
        p101_fputs(env, err, "## Teaching notes\n\n", report.human_stream);
    }

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;
        int                  p101_expression_result_26;
        int                  p101_expression_result_27;

        module = &map->modules[i];

        if(!args->library_mode && module->function_count > args->max_functions)
        {
            p101_module_map_write_finding(env, err, &report, &finding_count, P101_MODULE_RULE_MODULE_SIZE, module->name, 0U, "`%s` has %zu functions. Consider splitting one responsibility into another module.", module->name, module->function_count);
            wrote = true;
        }

        if(!args->library_mode && module->public_function_count > args->max_public)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_PUBLIC_API_SIZE,
                                          module->name,
                                          0U,
                                          "`%s` exposes %zu non-static functions. Consider making helpers `static` or narrowing the public API.",
                                          module->name,
                                          module->public_function_count);
            wrote = true;
        }

        p101_expression_result_27 = 0;
        if(!args->library_mode)
        {
            p101_call_result_28 = p101_module_map_module_contains_entrypoint(env, map, module->name);
            if(p101_call_result_28)
            {
                p101_expression_result_27 = 1;
            }
        }
        p101_expression_result_26 = 0;
        if(p101_expression_result_27)
        {
            if(module->function_count > 3U)
            {
                p101_expression_result_26 = 1;
            }
        }
        if(p101_expression_result_26)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_ENTRYPOINT_SIZE,
                                          module->source_path,
                                          0U,
                                          "`main` has %zu functions. Students usually do better when `main.c` only wires argument parsing, setup, and top-level control flow.",
                                          module->function_count);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;
        int                           p101_expression_result_29;
        int                           p101_expression_result_30;
        int                           p101_expression_result_31;
        int                           p101_expression_result_32;
        int                           p101_expression_result_33;
        int                           p101_expression_result_37;
        int                           p101_expression_result_38;
        int                           p101_expression_result_42;
        int                           p101_expression_result_43;
        int                           p101_expression_result_44;

        function                  = &map->functions[i];
        p101_expression_result_33 = 0;
        if(!args->library_mode)
        {
            if(!function->is_header_declaration)
            {
                p101_expression_result_33 = 1;
            }
        }
        p101_expression_result_32 = 0;
        if(p101_expression_result_33)
        {
            if(!function->is_static)
            {
                p101_expression_result_32 = 1;
            }
        }
        p101_expression_result_31 = 0;
        if(p101_expression_result_32)
        {
            p101_call_result_34 = p101_module_map_function_has_header_declaration(env, map, function);
            if(!p101_call_result_34)
            {
                p101_expression_result_31 = 1;
            }
        }
        p101_expression_result_30 = 0;
        if(p101_expression_result_31)
        {
            p101_call_result_35 = p101_module_map_function_used_outside_module(env, map, function);
            if(!p101_call_result_35)
            {
                p101_expression_result_30 = 1;
            }
        }
        p101_expression_result_29 = 0;
        if(p101_expression_result_30)
        {
            p101_call_result_36 = p101_strcmp(env, function->usr, "c:@F@main");
            if(p101_call_result_36 != 0)
            {
                p101_expression_result_29 = 1;
            }
        }
        if(p101_expression_result_29)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_UNUSED_PUBLIC_FUNCTION,
                                          function->path,
                                          function->line,
                                          "`%s` is non-static but does not appear to be part of a used module interface. Prefer `static`; only leave it public when another module must call it through the header.",
                                          function->name);
            wrote = true;
        }

        p101_expression_result_38 = 0;
        if(function->is_header_declaration)
        {
            if(!args->library_mode)
            {
                p101_expression_result_39 = 1;
            }
            else
            {
                p101_call_result_40 = p101_module_map_module_has_source(env, map, function->module);
                if(p101_call_result_40)
                {
                    p101_expression_result_39 = 1;
                }
                else
                {
                    p101_expression_result_39 = 0;
                }
            }
            if(p101_expression_result_39)
            {
                p101_expression_result_38 = 1;
            }
        }
        p101_expression_result_37 = 0;
        if(p101_expression_result_38)
        {
            p101_call_result_41 = p101_module_map_function_has_non_static_definition(env, map, function);
            if(!p101_call_result_41)
            {
                p101_expression_result_37 = 1;
            }
        }
        if(p101_expression_result_37)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_DECLARATION_WITHOUT_DEFINITION,
                                          function->path,
                                          function->line,
                                          "`%s` is declared here, but no matching non-static definition was found. Remove the declaration or make the implementation match the public API.",
                                          function->name);
            wrote = true;
        }

        p101_expression_result_44 = 0;
        if(!args->library_mode)
        {
            if(function->is_header_declaration)
            {
                p101_expression_result_44 = 1;
            }
        }
        p101_expression_result_43 = 0;
        if(p101_expression_result_44)
        {
            p101_call_result_45 = p101_module_map_module_used_outside_module(env, map, function->module);
            if(!p101_call_result_45)
            {
                p101_expression_result_43 = 1;
            }
        }
        p101_expression_result_42 = 0;
        if(p101_expression_result_43)
        {
            p101_call_result_46 = p101_module_map_module_contains_entrypoint(env, map, function->module);
            if(!p101_call_result_46)
            {
                p101_expression_result_42 = 1;
            }
        }
        if(p101_expression_result_42)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_UNUSED_INTERFACE,
                                          function->path,
                                          function->line,
                                          "`%s` is declared here, but no other module includes `%s`'s interface. Keep helpers `static` and remove header declarations until another file needs them.",
                                          function->name,
                                          function->module);
            wrote = true;
        }
    }

    for(size_t i = 0; !args->library_mode && i < map->macro_count; i++)
    {
        const struct macro_record *macro;
        int                        p101_expression_result_47;
        bool                       p101_call_result_48;

        macro                     = &map->macros[i];
        p101_call_result_48       = p101_module_map_module_used_outside_module(env, map, macro->module);
        p101_expression_result_47 = 0;
        /*
         * Only a header states a module's macro surface. A macro defined in a
         * .c file is already the private constant this finding recommends.
         */
        if(!p101_call_result_48 && macro->is_header)
        {
            p101_call_result_49 = p101_module_map_symbol_used_outside_module(env, map, macro->module, macro->name);
            if(!p101_call_result_49)
            {
                p101_expression_result_47 = 1;
            }
        }
        if(p101_expression_result_47)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_UNUSED_PUBLIC_MACRO,
                                          macro->path,
                                          macro->line,
                                          "Macro `%s` is exposed, but the module interface does not appear to be used outside `%s`. Prefer a private enum/constant in the `.c` file until another module needs it.",
                                          macro->name,
                                          macro->module);
            wrote = true;
        }
    }

    for(size_t i = 0; !args->library_mode && i < map->type_count; i++)
    {
        const struct type_record *type;
        bool                      p101_call_result_3;

        type               = &map->types[i];
        p101_call_result_3 = p101_module_map_module_used_outside_module(env, map, type->module);
        /*
         * Only a header exposes a type. A record declared inside a .c file is
         * already private to its module, which is what this finding asks for.
         */
        if(!p101_call_result_3 && type->is_header)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_UNUSED_PUBLIC_TYPE,
                                          type->path,
                                          type->line,
                                          "Type `%s` is exposed, but no other module includes `%s`'s interface. Keep representation details private or make the type opaque until callers need it.",
                                          type->name,
                                          type->module);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->include_count; i++)
    {
        const struct include_record *include;
        int                          p101_expression_result_50;
        int                          p101_expression_result_51;
        int                          p101_expression_result_54;
        int                          p101_expression_result_56;

        include                   = &map->includes[i];
        p101_expression_result_51 = 0;
        if(include->is_local)
        {
            p101_call_result_52 = p101_module_map_module_has_direct_include(env, map, include->target, include->from_module);
            if(p101_call_result_52)
            {
                p101_expression_result_51 = 1;
            }
        }
        p101_expression_result_50 = 0;
        if(p101_expression_result_51)
        {
            p101_call_result_53 = p101_strcmp(env, include->from_module, include->target);
            if(p101_call_result_53 != 0)
            {
                p101_expression_result_50 = 1;
            }
        }
        if(p101_expression_result_50)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_INCLUDE_CYCLE,
                                          include->path,
                                          include->line,
                                          "`%s` and `%s` include each other. That direct cycle is a design smell; introduce a smaller shared interface.",
                                          include->from_module,
                                          include->target);
            wrote = true;
        }

        p101_expression_result_54 = 0;
        if(include->is_local)
        {
            p101_call_result_55 = p101_module_map_include_target_exists(env, map, include->target);
            if(!p101_call_result_55)
            {
                p101_expression_result_54 = 1;
            }
        }
        if(p101_expression_result_54)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_MISSING_INCLUDE_TARGET,
                                          include->path,
                                          include->line,
                                          "This file includes local header `%s`, but no scanned module named `%s` was found. Check for a stale include or add the missing module to the scan path.",
                                          include->target,
                                          include->target);
            wrote = true;
        }

        p101_expression_result_56 = 0;
        if(include->is_local)
        {
            p101_call_result_57 = p101_module_map_layer_allows_include(env, err, args, include->from_module, include->target);
            if(!p101_call_result_57)
            {
                p101_expression_result_56 = 1;
            }
        }
        if(p101_expression_result_56)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_LAYER_VIOLATION,
                                          include->path,
                                          include->line,
                                          "`%s` includes `%s`, but that edge is not allowed by `%s`. Add `%s -> %s` only if this dependency is intentional.",
                                          include->from_module,
                                          include->target,
                                          args->layer_config_path,
                                          include->from_module,
                                          include->target);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;
        bool                          eligible;
        bool                          wraps_platform_name;

        function = &map->functions[i];
        eligible = false;
        if(!function->is_static)
        {
            if(!function->is_header_declaration)
            {
                eligible = true;
            }
        }
        if(!eligible)
        {
            continue;
        }
        wraps_platform_name = p101_module_map_idiom_wraps_platform_name(env, map, function);
        if(wraps_platform_name)
        {
            continue;
        }
        {
            char peer[MAX_NAME];
            bool has_pair_name;

            has_pair_name = p101_module_map_idiom_swap_suffix(env, function->name, "_create", "_destroy", peer, sizeof(peer));
            if(has_pair_name)
            {
                bool pair_exists;

                pair_exists = p101_module_map_idiom_public_function_exists(env, map, function->module, peer);
                if(!pair_exists)
                {
                    p101_module_map_write_finding(env,
                                                  err,
                                                  &report,
                                                  &finding_count,
                                                  P101_MODULE_RULE_MISSING_DESTROY,
                                                  function->path,
                                                  function->line,
                                                  "Naming convention: `%s` has no matching `%s` in `%s`. The `_create` spelling promises a paired `_destroy` spelling; this check does not infer ownership or allocation behavior.",
                                                  function->name,
                                                  peer,
                                                  function->module);
                    wrote = true;
                }
            }
        }
        if(!args->library_mode)
        {
            char peer[MAX_NAME];
            bool has_pair_name;

            has_pair_name = p101_module_map_idiom_swap_suffix(env, function->name, "_count", "_at", peer, sizeof(peer));
            if(has_pair_name)
            {
                bool pair_exists;

                pair_exists = p101_module_map_idiom_public_function_exists(env, map, function->module, peer);
                if(!pair_exists)
                {
                    p101_module_map_write_finding(env,
                                                  err,
                                                  &report,
                                                  &finding_count,
                                                  P101_MODULE_RULE_COLLECTION_ACCESSOR_PAIR,
                                                  function->path,
                                                  function->line,
                                                  "`%s` has no matching `%s` in `%s`. Expose collections through a `_count`/`_at` accessor pair so callers can iterate without reaching into the representation.",
                                                  function->name,
                                                  peer,
                                                  function->module);
                    wrote = true;
                }
            }
        }
        {
            char peer[MAX_NAME];
            bool has_pair_name;

            has_pair_name = p101_module_map_idiom_swap_suffix(env, function->name, "_init", "_deinit", peer, sizeof(peer));
            if(has_pair_name)
            {
                char alternate[MAX_NAME];
                bool has_alternate_name;
                bool pair_exists;

                pair_exists = p101_module_map_idiom_public_function_anywhere(env, map, peer);
                if(!pair_exists)
                {
                    has_alternate_name = p101_module_map_idiom_swap_suffix(env, function->name, "_init", "_destroy", alternate, sizeof(alternate));
                    if(has_alternate_name)
                    {
                        pair_exists = p101_module_map_idiom_public_function_anywhere(env, map, alternate);
                    }
                }
                if(!pair_exists)
                {
                    has_alternate_name = p101_module_map_idiom_swap_suffix(env, function->name, "_init", "_fini", alternate, sizeof(alternate));
                    if(has_alternate_name)
                    {
                        pair_exists = p101_module_map_idiom_public_function_anywhere(env, map, alternate);
                    }
                }
                if(!pair_exists)
                {
                    p101_module_map_write_finding(env,
                                                  err,
                                                  &report,
                                                  &finding_count,
                                                  P101_MODULE_RULE_MISSING_TEARDOWN,
                                                  function->path,
                                                  function->line,
                                                  "Naming convention: `%s` has no matching `%s`. The `_init` spelling promises a teardown counterpart; this check does not infer that initialization acquired a resource.",
                                                  function->name,
                                                  peer);
                    wrote = true;
                }
            }
        }
        {
            char peer[MAX_NAME];
            bool has_pair_name;

            has_pair_name = p101_module_map_idiom_swap_suffix(env, function->name, "_open", "_close", peer, sizeof(peer));
            if(has_pair_name)
            {
                bool pair_exists;

                pair_exists = p101_module_map_idiom_public_function_anywhere(env, map, peer);
                if(!pair_exists)
                {
                    p101_module_map_write_finding(env,
                                                  err,
                                                  &report,
                                                  &finding_count,
                                                  P101_MODULE_RULE_MISSING_TEARDOWN,
                                                  function->path,
                                                  function->line,
                                                  "Naming convention: `%s` has no matching `%s`. The `_open` spelling promises a `_close` counterpart; this check does not infer that the function acquired a resource.",
                                                  function->name,
                                                  peer);
                    wrote = true;
                }
            }
        }
        {
            char peer[MAX_NAME];
            bool has_pair_name;

            has_pair_name = p101_module_map_idiom_swap_suffix(env, function->name, "_from_name", "_name", peer, sizeof(peer));
            if(has_pair_name)
            {
                bool pair_exists;

                pair_exists = p101_module_map_idiom_public_function_exists(env, map, function->module, peer);
                if(!pair_exists)
                {
                    p101_module_map_write_finding(env,
                                                  err,
                                                  &report,
                                                  &finding_count,
                                                  P101_MODULE_RULE_NAME_CONVERSION_PAIR,
                                                  function->path,
                                                  function->line,
                                                  "Naming convention: `%s` has no matching `%s`. The `_from_name` spelling suggests a paired name conversion; only a round-trip test can establish behavioral symmetry.",
                                                  function->name,
                                                  peer);
                    wrote = true;
                }
            }
        }
    }

    for(size_t i = 0; i < map->module_count; i++)
    {
        const struct module *module;
        bool                 p101_call_result_63;

        bool require_owner_prefix;

        module               = &map->modules[i];
        require_owner_prefix = false;
        if(!args->library_mode)
        {
            require_owner_prefix = true;
        }
        p101_call_result_63 = p101_module_map_idiom_module_shares_prefix(env, map, module->name, require_owner_prefix);
        if(!p101_call_result_63)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_MODULE_PREFIX,
                                          module->source_path[0] != '\0' ? module->source_path : module->header_path,
                                          0U,
                                          "`%s` exposes public functions that do not share a name prefix. Give one module one vocabulary: a common `%s_` prefix tells readers which module owns each call.",
                                          module->name,
                                          module->name);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->file_count; i++)
    {
        const struct source_file *file;

        file = &map->files[i];
        if(!file->is_header)
        {
            bool module_has_header;

            module_has_header = p101_module_map_idiom_module_has_header(env, map, file->module);
            if(module_has_header)
            {
                bool p101_call_result_71;

                p101_call_result_71 = p101_module_map_idiom_source_includes_own(env, map, file->path, file->module);
                if(!p101_call_result_71)
                {
                    p101_module_map_write_finding(env,
                                                  err,
                                                  &report,
                                                  &finding_count,
                                                  P101_MODULE_RULE_OWN_HEADER_FIRST,
                                                  file->path,
                                                  1U,
                                                  "`%s` never includes its own interface. A source that skips its own header can drift from it; the compiler only proves declarations match when it sees both.",
                                                  file->path);
                    wrote = true;
                }
                else if(!args->library_mode)
                {
                    const struct include_record *leading;

                    leading = p101_module_map_idiom_first_local_include(env, map, file->path);
                    if(leading != NULL)
                    {
                        bool p101_call_result_72;

                        p101_call_result_72 = p101_module_map_idiom_target_matches_module(env, leading->target, file->module);
                        if(!p101_call_result_72)
                        {
                            p101_module_map_write_finding(env,
                                                          err,
                                                          &report,
                                                          &finding_count,
                                                          P101_MODULE_RULE_OWN_HEADER_FIRST,
                                                          leading->path,
                                                          leading->line,
                                                          "`%s` includes `%s` before its own header. Include your own header first so it is proven to stand alone; any name it forgot surfaces here, not in another file's build.",
                                                          file->path,
                                                          leading->target);
                            wrote = true;
                        }
                    }
                }
            }
        }
    }

    /*
     * Leading underscores are clang-tidy's job: bugprone-reserved-identifier
     * (alias cert-dcl37-c) already flags every function, type, and macro that
     * takes one, and reporting it here too would give a student two
     * diagnostics for one name. The POSIX `_t` suffix is the half no tool
     * checks, so it is the half this rule keeps.
     */
    for(size_t i = 0; i < map->type_count; i++)
    {
        const struct type_record *type;
        int                       p101_expression_result_73;
        bool                      p101_call_result_74;

        type                      = &map->types[i];
        p101_expression_result_73 = 0;
        p101_call_result_74       = p101_module_map_idiom_name_ends_with(env, type->name, "_t");
        if(p101_call_result_74)
        {
            int p101_call_result_75;

            /*
             * errno_t is the deliberate C11 Annex K compatibility name; it
             * is the one _t this workspace claims on purpose.
             */
            p101_call_result_75 = p101_strcmp(env, type->name, "errno_t");
            if(p101_call_result_75 != 0)
            {
                p101_expression_result_73 = 1;
            }
        }
        if(p101_expression_result_73)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_RESERVED_TYPE_SUFFIX,
                                          type->path,
                                          type->line,
                                          "Type `%s` uses a reserved spelling. POSIX owns the `_t` suffix, so a type that claims one can collide with a platform header that adds it later.",
                                          type->name);
            wrote = true;
        }
    }

    for(size_t i = 0; i < map->note_count; i++)
    {
        const struct note_record *note;
        int                       p101_call_result_76;
        int                       p101_call_result_77;

        note                = &map->notes[i];
        p101_call_result_76 = p101_strcmp(env, note->name, "SIGNATURE_ENV_ORDER");
        if(p101_call_result_76 == 0 && !note->is_header)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_CONTEXT_ARGUMENT_ORDER,
                                          note->path,
                                          note->line,
                                          "`%s` takes the shared context out of order. The house signature reads `(const struct p101_env *, struct p101_error *, ...)`: env first, error second, so every call site scans the same way.",
                                          note->symbol);
            wrote = true;
        }
        p101_call_result_77 = p101_strcmp(env, note->name, "ALLOC_SIZEOF_TYPE");
        if(p101_call_result_77 == 0)
        {
            p101_module_map_write_finding(env,
                                          err,
                                          &report,
                                          &finding_count,
                                          P101_MODULE_RULE_ALLOCATION_SIZE_TYPE,
                                          note->path,
                                          note->line,
                                          "This allocation sizes a type name. Write `malloc(sizeof(*pointer))` so the size follows the variable if its type ever changes.");
            wrote = true;
        }
        /*
         * MACRO_ARGUMENT_BARE, MACRO_STATEMENT_BARE, and HANDLER_REGISTERED are
         * deliberately not reported here. clang-tidy already owns all three:
         * bugprone-macro-parentheses, bugprone-multiple-statement-macro, and
         * bugprone-signal-handler (alias cert-sig30-c). The notes stay in the
         * fact stream as evidence; the explanation lives in the lesson.
         */
    }

    for(size_t i = 0; i < map->file_count; i++)
    {
        const struct source_file *file;

        file = &map->files[i];
        if(file->is_header)
        {
            const struct macro_record *guard;

            guard = p101_module_map_idiom_guard_macro(env, map, file->path);
            if(guard == NULL)
            {
                p101_module_map_write_finding(env,
                                              err,
                                              &report,
                                              &finding_count,
                                              P101_MODULE_RULE_INCLUDE_GUARD,
                                              file->path,
                                              1U,
                                              "`%s` has no include guard macro. Open every header with `#ifndef`/`#define` of a guard derived from the header's name so double inclusion stays harmless.",
                                              file->path);
                wrote = true;
            }
            else
            {
                char expected[MAX_NAME];
                bool expected_available;

                expected_available = p101_module_map_idiom_guard_suffix(env, file->path, expected, sizeof(expected));
                if(expected_available)
                {
                    bool guard_matches;

                    guard_matches = p101_module_map_idiom_name_ends_with(env, guard->name, expected);
                    if(!guard_matches)
                    {
                        p101_module_map_write_finding(env,
                                                      err,
                                                      &report,
                                                      &finding_count,
                                                      P101_MODULE_RULE_INCLUDE_GUARD,
                                                      file->path,
                                                      guard->line,
                                                      "Include guard `%s` does not end with `%s`. Derive the guard from the header's own name so no two headers can collide.",
                                                      guard->name,
                                                      expected);
                        wrote = true;
                    }
                }
            }
        }
    }

    if(!wrote && report.human_stream != NULL)
    {
        p101_fputs(env, err, "No obvious module-structure issues found by the current heuristics.\n", report.human_stream);
    }
    if(wrote)
    {
        outcome = P101_TOOL_OUTCOME_FINDINGS;
    }
    else
    {
        outcome = P101_TOOL_OUTCOME_CLEAN;
    }
    exit_status   = p101_tool_outcome_exit_status(outcome);
    report_status = p101_tool_report_end(&report, outcome, exit_status, counters, sizeof(counters) / sizeof(counters[0]));
    p101_module_map_report_check(err, report_status);

done:
    return wrote;
}

static void p101_module_map_report_check(struct p101_error *err, int status)
{
    if(status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}

static void p101_module_map_write_finding(const struct p101_env *env, struct p101_error *err, struct p101_tool_report *report, size_t *finding_count, enum p101_module_rule rule_kind, const char *path, size_t line, const char *format, ...)
{
    int     p101_call_result_4;
    bool    p101_call_result_5;
    char    message[MAX_LINE];
    va_list arguments;

    P101_TRACE_SCOPE(env);
    va_start(arguments, format);
    p101_call_result_4 = p101_vsnprintf(env, err, message, sizeof(message), format, arguments);
    va_end(arguments);
    if(p101_call_result_4 < 0 || (size_t)p101_call_result_4 >= sizeof(message))
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    p101_call_result_5 = p101_error_has_error(err);
    if(p101_call_result_5)
    {
        goto done;
    }

    if(rule_kind < P101_MODULE_RULE_MODULE_SIZE || rule_kind >= P101_MODULE_RULE_COUNT)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto done;
    }

    {
        struct p101_tool_diagnostic diagnostic;
        int                         write_status;

        write_status = p101_tool_diagnostic_initialize(&diagnostic, module_findings[rule_kind], P101_TOOL_DIAGNOSTIC_WARNING, path, line, 0U, "", message);
        p101_module_map_report_check(err, write_status);
        if(write_status != 0)
        {
            goto done;
        }
        write_status = p101_tool_report_emit(report, &diagnostic);
        p101_module_map_report_check(err, write_status);
        if(write_status == 0)
        {
            (*finding_count)++;
        }
    }

done:
    return;
}

#ifdef P101_MODULE_MAP_TESTING
static void p101_module_map_write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    int p101_call_result_6;
    P101_TRACE_SCOPE(env);
    p101_call_result_6 = p101_record_write_json_string(stream, text == NULL ? "" : text);
    if(p101_call_result_6 != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}

bool p101_module_map_test_module_has_source(const struct p101_env *env, const struct project_map *map, const char *module_name)
{
    return p101_module_map_module_has_source(env, map, module_name);
}

bool p101_module_map_test_layer_allows_include(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *from_module, const char *target)
{
    return p101_module_map_layer_allows_include(env, err, args, from_module, target);
}

void p101_module_map_test_write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    p101_module_map_write_json_string(env, err, stream, text);
}

void p101_module_map_test_write_finding(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct arguments *args, bool *first_json, size_t *finding_count, const char *path)
{
    struct p101_tool_report_options options = {"audit-modules-test", "test facts", "test helper", P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN, false};
    struct p101_tool_report         report;
    p101_tool_outcome               outcome;
    int                             exit_status;
    int                             status;

    (void)first_json;
    if(args->json)
    {
        options.outputs = P101_TOOL_DIAGNOSTIC_OUTPUT_JSON;
    }
    status = p101_tool_report_begin(&report, stream, stream, &options);
    p101_module_map_report_check(err, status);
    if(status == 0)
    {
        p101_module_map_write_finding(env, err, &report, finding_count, P101_MODULE_RULE_MODULE_SIZE, path, 1U, "message");
        outcome     = P101_TOOL_OUTCOME_FINDINGS;
        exit_status = p101_tool_outcome_exit_status(outcome);
        status      = p101_tool_report_end(&report, outcome, exit_status, NULL, 0U);
        p101_module_map_report_check(err, status);
    }
}
#endif

void p101_module_map_write_functions_for_module(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct project_map *map, const char *module_name)
{
    bool wrote;

    P101_TRACE_SCOPE(env);
    wrote = false;

    for(size_t i = 0; i < map->function_count; i++)
    {
        const struct function_record *function;
        int                           p101_call_result_7;

        function           = &map->functions[i];
        p101_call_result_7 = p101_strcmp(env, function->module, module_name);
        if(p101_call_result_7 == 0)
        {
            const char *visibility;

            if(function->is_header_declaration)
            {
                visibility = "header";
            }
            else if(function->is_static)
            {
                visibility = "private";
            }
            else
            {
                visibility = "public";
            }

            p101_fprintf(env, err, stream, "- `%s` — %s (%s:%zu)\n", function->name, visibility, function->path, function->line);
            wrote = true;
        }
    }

    if(!wrote)
    {
        p101_fputs(env, err, "- No functions detected.\n", stream);
    }

    p101_fputs(env, err, "\n", stream);
}
