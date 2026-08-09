#include "output.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_c_facts/facts.h>
#include <p101_record/record.h>
#include <p101_tool_event/diagnostic.h>
#include <p101_tool_event/report.h>

enum
{
    FINDING_MESSAGE_EXTRA_SIZE = 64U
};

static void              tsv_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);
static void              module_name(const struct p101_env *env, const char *path, char *module, size_t size);
static p101_tool_finding finding_rule(enum p101_wrapper_finding_kind kind);
static const char       *finding_label(enum p101_wrapper_finding_kind kind);
static const char       *bool_text(bool value);
static void              format_finding_message(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_finding *finding, char *message, size_t message_size);
static void              report_check(const struct p101_env *env, struct p101_error *err, int status);

void p101_wrapper_output_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    int p101_call_result_1;
    P101_TRACE_SCOPE(env);
    p101_call_result_1 = p101_record_write_json_string(stream, text == NULL ? "" : text);
    if(p101_call_result_1 != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}

/*
 * Deliberately not p101_record_escape_field: that escaper maps NULL to "-", a
 * lone "-" to "\-", and every other control byte (and DEL) to '?'. The fact
 * stream emitted here predates those mappings, so adopting them would change
 * the bytes of existing P101FACT records. Only \\, \t, \n and \r are escaped,
 * exactly as the fact readers expect.
 */
static void tsv_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    P101_TRACE_SCOPE(env);
    if(text != NULL)
    {
        const unsigned char *cursor;

        for(cursor = (const unsigned char *)text; *cursor != '\0'; cursor++)
        {
            if(*cursor == '\\')
            {
                p101_fputs(env, err, "\\\\", stream);
            }
            else if(*cursor == '\t')
            {
                p101_fputs(env, err, "\\t", stream);
            }
            else if(*cursor == '\n')
            {
                p101_fputs(env, err, "\\n", stream);
            }
            else if(*cursor == '\r')
            {
                p101_fputs(env, err, "\\r", stream);
            }
            else
            {
                p101_fputc(env, err, *cursor, stream);
            }
        }
    }
}

static void module_name(const struct p101_env *env, const char *path, char *module, size_t size)
{
    const char *name;
    const char *dot;
    const char *root;
    size_t      length;

    P101_TRACE_SCOPE(env);
    root = p101_strstr(env, path, "/src/");
    if(root != NULL)
    {
        name = root + sizeof("/src/") - 1U;
    }
    else
    {
        root = p101_strstr(env, path, "/include/");
        if(root != NULL)
        {
            int         p101_expression_result_16;
            const char *package_end;

            name                      = root + sizeof("/include/") - 1U;
            package_end               = p101_strchr(env, name, '/');
            p101_expression_result_16 = 0;
            if(package_end != NULL)
            {
                int p101_call_result_17;

                p101_call_result_17 = p101_strncmp(env, name, "p101_", sizeof("p101_") - 1U);
                if(p101_call_result_17 == 0)
                {
                    p101_expression_result_16 = 1;
                }
            }
            if(p101_expression_result_16)
            {
                name = package_end + 1;
            }
        }
        else
        {
            name = p101_strrchr(env, path, '/');
            name = name == NULL ? path : name + 1;
        }
    }
    length = p101_strlen(env, name);
    if(length >= size)
    {
        length = size - 1U;
    }
    p101_memcpy(env, module, name, length);
    module[length] = '\0';
    dot            = p101_strrchr(env, module, '.');
    if(dot != NULL)
    {
        module[(size_t)(dot - module)] = '\0';
    }
}

static p101_tool_finding finding_rule(enum p101_wrapper_finding_kind kind)
{
    static const p101_tool_finding rules[] = {P101_TOOL_FINDING_WRAP_001, P101_TOOL_FINDING_WRAP_002, P101_TOOL_FINDING_WRAP_003, P101_TOOL_FINDING_WRAP_004};
    p101_tool_finding              rule;

    if(kind < P101_WRAPPER_MISSED || kind > P101_WRAPPER_PORTABILITY)
    {
        rule = P101_TOOL_FINDING_COUNT;
    }
    else
    {
        rule = rules[kind];
    }
    return rule;
}

static const char *finding_label(enum p101_wrapper_finding_kind kind)
{
    static const char *const labels[] = {"missed-wrapper", "external-call", "indirect-call", "portability-include"};
    const char              *label;

    if(kind < P101_WRAPPER_MISSED || kind > P101_WRAPPER_PORTABILITY)
    {
        label = "unknown";
    }
    else
    {
        label = labels[kind];
    }
    return label;
}

static const char *bool_text(bool value)
{
    const char *text;

    if(value)
    {
        text = "1";
    }
    else
    {
        text = "0";
    }
    return text;
}

static void report_check(const struct p101_env *env, struct p101_error *err, int status)
{
    P101_TRACE_SCOPE(env);
    if(status != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}

static void format_finding_message(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_finding *finding, char *message, size_t message_size)
{
    const char *label;
    int         written;

    P101_TRACE_SCOPE(env);
    label = finding_label(finding->kind);
    if(finding->replacement[0] != '\0')
    {
        written = p101_snprintf(env, err, message, message_size, "%s: %s -> %s", label, finding->name, finding->replacement);
    }
    else if(finding->allow_identity[0] != '\0')
    {
        written = p101_snprintf(env, err, message, message_size, "%s: %s; allow-rule identity %s", label, finding->name, finding->allow_identity);
    }
    else
    {
        written = p101_snprintf(env, err, message, message_size, "%s: %s", label, finding->name);
    }
    if(written < 0 || (size_t)written >= message_size)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
    }
}

const char *p101_wrapper_output_json_bool_text(bool value)
{
    const char *text;

    if(value)
    {
        text = "true";
    }
    else
    {
        text = "false";
    }
    return text;
}

void p101_wrapper_write_inventory(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, bool json)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    if(json)
    {
        p101_fputs(env, err, "{\"schema\":\"p101-wrapper-inventory-v3\",\"wrappers\":[", stdout);
    }
    for(index = 0U; index < model->inventory_count; index++)
    {
        if(json)
        {
            if(index > 0U)
            {
                p101_fputc(env, err, ',', stdout);
            }
            p101_fputs(env, err, "{\"original\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, model->inventory[index].original);
            p101_fputs(env, err, ",\"original_usr\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, model->inventory[index].original_usr);
            p101_fputs(env, err, ",\"wrapper\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, model->inventory[index].wrapper);
            p101_fputs(env, err, ",\"wrapper_usr\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, model->inventory[index].wrapper_usr);
            p101_fputc(env, err, '}', stdout);
        }
        else
        {
            p101_fprintf(env, err, stdout, "%s\t%s\t%s\t%s\n", model->inventory[index].original, model->inventory[index].original_usr, model->inventory[index].wrapper, model->inventory[index].wrapper_usr);
        }
    }
    if(json)
    {
        p101_fputs(env, err, "]}\n", stdout);
    }
}

void p101_wrapper_write_audit(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    const struct p101_tool_report_options options    = {"audit-wrappers",
                                                        "Clang AST facts and the wrapper inventory for the selected translation units.",
                                                        "Calls hidden from Clang, unscanned translation units, and third-party implementation details are outside this report.",
                                                        0U,
                                                        true};
    struct p101_tool_report_counter       counters[] = {
        {"missed_wrappers",      0U},
        {"external_calls",       0U},
        {"indirect_calls",       0U},
        {"portability_includes", 0U},
        {"parse_failures",       0U}
    };
    struct p101_tool_report         report;
    struct p101_tool_report_options selected_options;
    p101_tool_outcome               outcome;
    size_t                          index;
    int                             exit_status;
    int                             report_status;

    P101_TRACE_SCOPE(env);
    selected_options         = options;
    selected_options.outputs = 0U;
    if(arguments->human)
    {
        selected_options.outputs |= P101_TOOL_DIAGNOSTIC_OUTPUT_HUMAN;
    }
    if(arguments->json)
    {
        selected_options.outputs |= P101_TOOL_DIAGNOSTIC_OUTPUT_JSON;
    }
    report_status = p101_tool_report_begin(&report, stdout, stderr, &selected_options);
    report_check(env, err, report_status);
    if(report_status != 0)
    {
        goto done;
    }
    for(index = 0U; index < model->finding_count; index++)
    {
        if(model->findings[index].kind >= P101_WRAPPER_MISSED && model->findings[index].kind <= P101_WRAPPER_PORTABILITY)
        {
            counters[model->findings[index].kind].value++;
        }
        {
            const struct p101_wrapper_finding *finding;
            p101_tool_finding                  rule;
            struct p101_tool_diagnostic        diagnostic;
            char                               message[(P101_WRAPPER_NAME_SIZE * 3U) + FINDING_MESSAGE_EXTRA_SIZE];
            p101_tool_diagnostic_severity      severity;

            finding = &model->findings[index];
            rule    = finding_rule(finding->kind);
            format_finding_message(env, err, finding, message, sizeof(message));
            if(finding->kind == P101_WRAPPER_EXTERNAL || finding->kind == P101_WRAPPER_INDIRECT)
            {
                severity = P101_TOOL_DIAGNOSTIC_NOTE;
            }
            else
            {
                severity = P101_TOOL_DIAGNOSTIC_ERROR;
            }
            report_status = p101_tool_diagnostic_initialize(&diagnostic, rule, severity, finding->path, finding->line, finding->column, finding->caller, message);
            report_check(env, err, report_status);
            if(report_status != 0)
            {
                goto done;
            }
            report_status = p101_tool_report_emit(&report, &diagnostic);
            report_check(env, err, report_status);
            if(report_status != 0)
            {
                goto done;
            }
        }
    }
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind == P101_C_ANALYSIS_DIAGNOSTIC)
        {
            struct p101_tool_diagnostic diagnostic;

            report_status = p101_tool_diagnostic_initialize(&diagnostic, P101_TOOL_FINDING_WRAP_900, P101_TOOL_DIAGNOSTIC_ERROR, fact->path, fact->line, fact->column, "?", fact->name);
            report_check(env, err, report_status);
            if(report_status != 0)
            {
                goto done;
            }
            report_status = p101_tool_report_emit(&report, &diagnostic);
            report_check(env, err, report_status);
            if(report_status != 0)
            {
                goto done;
            }
        }
    }
    counters[4].value = model->parse_failures;
    outcome           = P101_TOOL_OUTCOME_CLEAN;
    if(model->parse_failures > 0U)
    {
        outcome = P101_TOOL_OUTCOME_TOOL_ERROR;
    }
    for(index = 0U; index < model->finding_count && outcome == P101_TOOL_OUTCOME_CLEAN; index++)
    {
        if(model->findings[index].kind == P101_WRAPPER_MISSED || model->findings[index].kind == P101_WRAPPER_PORTABILITY ||
           (arguments->strict_external && (model->findings[index].kind == P101_WRAPPER_EXTERNAL || model->findings[index].kind == P101_WRAPPER_INDIRECT)))
        {
            outcome = P101_TOOL_OUTCOME_FINDINGS;
        }
    }
    exit_status   = p101_tool_outcome_exit_status(outcome);
    report_status = p101_tool_report_end(&report, outcome, exit_status, counters, sizeof(counters) / sizeof(counters[0]));
    report_check(env, err, report_status);

done:
    return;
}

static void write_fact_prefix(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct p101_wrapper_fact *fact)
{
    const char *p101_call_result_7;
    const char *p101_call_result_8;
    char        module[P101_WRAPPER_NAME_SIZE];

    module_name(env, fact->path, module, sizeof(module));
    p101_fputs(env, err, P101_C_FACT_PREFIX P101_C_FACT_VERSION "\t", stream);
    p101_call_result_7 = p101_c_analysis_kind_name(fact->kind);
    p101_fputs(env, err, p101_call_result_7, stream);
    p101_fputc(env, err, '\t', stream);
    tsv_string(env, err, stream, fact->path);
    p101_fputc(env, err, '\t', stream);
    tsv_string(env, err, stream, module);
    p101_call_result_8 = bool_text(fact->is_header);
    p101_fprintf(env, err, stream, "\t%s\t%zu", p101_call_result_8, fact->line);
}

void p101_wrapper_write_facts(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, FILE *stream)
{
    const char *p101_call_result_9;
    const char *p101_call_result_10;
    const char *p101_call_result_11;
    const char *p101_call_result_12;
    const char *p101_call_result_13;
    const char *p101_call_result_14;
    const char *p101_call_result_15;
    size_t      index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind > P101_C_ANALYSIS_NOTE)
        {
            continue;
        }
        if(fact->kind == P101_C_ANALYSIS_FUNCTION && !fact->is_definition && !fact->is_header)
        {
            continue;
        }
        if((fact->kind == P101_C_ANALYSIS_TYPE || fact->kind == P101_C_ANALYSIS_ENUM || fact->kind == P101_C_ANALYSIS_ENUMERATOR || (fact->kind == P101_C_ANALYSIS_MACRO && fact->is_definition)) && !fact->is_header)
        {
            continue;
        }
        write_fact_prefix(env, err, stream, fact);
        if(fact->kind == P101_C_ANALYSIS_INCLUDE)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_call_result_9 = bool_text(fact->is_local_include);
            p101_fprintf(env, err, stream, "\t%s", p101_call_result_9);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->resolved);
        }
        else if(fact->kind == P101_C_ANALYSIS_TYPE || fact->kind == P101_C_ANALYSIS_ENUM)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->usr);
        }
        else if(fact->kind == P101_C_ANALYSIS_MACRO)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_call_result_10 = bool_text(fact->is_definition);
            p101_fprintf(env, err, stream, "\t%s\t", p101_call_result_10);
            tsv_string(env, err, stream, fact->caller_usr);
            p101_fprintf(env, err, stream, "\t%zu\t%zu", fact->start, fact->end);
        }
        else if(fact->kind == P101_C_ANALYSIS_ENUMERATOR)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->type);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->usr);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->caller_usr);
        }
        else if(fact->kind == P101_C_ANALYSIS_NOTE)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->caller);
            p101_fprintf(env, err, stream, "\t%zu", fact->column);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->caller_usr);
            p101_fprintf(env, err, stream, "\t%zu\t%zu", fact->start, fact->end);
        }
        else if(fact->kind == P101_C_ANALYSIS_FUNCTION)
        {
            bool declaration;

            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            declaration = false;
            if(fact->is_header && !fact->is_definition)
            {
                declaration = true;
            }
            p101_call_result_11 = bool_text(fact->is_static);
            p101_call_result_12 = bool_text(declaration);
            p101_fprintf(env, err, stream, "\t%s\t%s", p101_call_result_11, p101_call_result_12);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->usr);
            p101_fprintf(env, err, stream, "\t%zu\t%zu", fact->start, fact->end);
        }
        else if(fact->kind == P101_C_ANALYSIS_CALL)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_call_result_13 = bool_text(fact->needs_env);
            p101_call_result_14 = bool_text(fact->needs_error);
            p101_call_result_15 = bool_text(fact->is_indirect);
            p101_fprintf(env, err, stream, "\t%s\t%s\t%s", p101_call_result_13, p101_call_result_14, p101_call_result_15);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->caller);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->usr);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->caller_usr);
            p101_fprintf(env, err, stream, "\t%zu\t%zu", fact->start, fact->end);
        }
        p101_fputc(env, err, '\n', stream);
    }
}

void p101_wrapper_write_diagnostics(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, FILE *stream)
{
    size_t index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind == P101_C_ANALYSIS_DIAGNOSTIC)
        {
            p101_fprintf(env, err, stream, "%s:%zu:%zu: %s\n", fact->path, fact->line, fact->column, fact->name);
        }
    }
}
