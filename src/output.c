#include "output.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_record/record.h>

static void        tsv_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);
static void        module_name(const struct p101_env *env, const char *path, char *module, size_t size);
static const char *finding_id(enum p101_wrapper_finding_kind kind);
static const char *finding_label(enum p101_wrapper_finding_kind kind);
static const char *bool_text(bool value);

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

static void tsv_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    const unsigned char *cursor;

    P101_TRACE_SCOPE(env);
    if(text != NULL)
    {
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
    int         p101_expression_result_16;
    int         p101_call_result_17;
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
            const char *package_end;

            name                      = root + sizeof("/include/") - 1U;
            package_end               = p101_strchr(env, name, '/');
            p101_expression_result_16 = 0;
            if(package_end != NULL)
            {
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

static const char *finding_id(enum p101_wrapper_finding_kind kind)
{
    static const char *const ids[] = {"P101-WRAP-001", "P101-WRAP-002", "P101-WRAP-003", "P101-WRAP-004"};
    const char              *id;

    if(kind < P101_WRAPPER_MISSED || kind > P101_WRAPPER_PORTABILITY)
    {
        id = "P101-WRAP-000";
    }
    else
    {
        id = ids[kind];
    }
    return id;
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
    const char *p101_call_result_2;
    const char *p101_call_result_3;
    const char *p101_call_result_4;
    const char *p101_call_result_5;
    const char *p101_call_result_6;
    size_t      counts[4] = {0U, 0U, 0U, 0U};
    size_t      index;

    P101_TRACE_SCOPE(env);
    for(index = 0U; index < model->finding_count; index++)
    {
        if(model->findings[index].kind >= P101_WRAPPER_MISSED && model->findings[index].kind <= P101_WRAPPER_PORTABILITY)
        {
            counts[model->findings[index].kind]++;
        }
    }
    if(arguments->json)
    {
        bool first;

        p101_fprintf(env,
                     err,
                     stdout,
                     "{\"schema\":\"p101-wrapper-audit-findings-v2\",\"missed_wrappers\":%zu,\"external_calls\":%zu,\"indirect_calls\":%zu,\"portability_includes\":%zu,\"parse_failures\":%zu,\"findings\":[",
                     counts[P101_WRAPPER_MISSED],
                     counts[P101_WRAPPER_EXTERNAL],
                     counts[P101_WRAPPER_INDIRECT],
                     counts[P101_WRAPPER_PORTABILITY],
                     model->parse_failures);
        first = true;
        for(index = 0U; index < model->finding_count; index++)
        {
            const struct p101_wrapper_finding *finding;

            finding = &model->findings[index];
            if(!first)
            {
                p101_fputc(env, err, ',', stdout);
            }
            first = false;
            p101_fputs(env, err, "{\"id\":", stdout);
            p101_call_result_2 = finding_id(finding->kind);
            p101_wrapper_output_json_string(env, err, stdout, p101_call_result_2);
            p101_fputs(env, err, ",\"severity\":", stdout);
            if(finding->kind == P101_WRAPPER_EXTERNAL || finding->kind == P101_WRAPPER_INDIRECT)
            {
                p101_wrapper_output_json_string(env, err, stdout, "note");
            }
            else
            {
                p101_wrapper_output_json_string(env, err, stdout, "error");
            }
            p101_fputs(env, err, ",\"location\":{\"path\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, finding->path);
            p101_fprintf(env, err, stdout, ",\"line\":%zu,\"column\":%zu,\"function\":", finding->line, finding->column);
            p101_wrapper_output_json_string(env, err, stdout, finding->caller);
            p101_fputs(env, err, "},\"message\":", stdout);
            p101_call_result_3 = finding_label(finding->kind);
            p101_wrapper_output_json_string(env, err, stdout, p101_call_result_3);
            p101_fputs(env, err, ",\"evidence\":{\"parser\":\"libclang\",\"kind\":", stdout);
            p101_call_result_4 = finding_label(finding->kind);
            p101_wrapper_output_json_string(env, err, stdout, p101_call_result_4);
            p101_fputs(env, err, ",\"callee\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, finding->name);
            p101_fputs(env, err, ",\"replacement\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, finding->replacement);
            p101_fputs(env, err, "}}", stdout);
        }
        for(index = 0U; index < model->fact_count; index++)
        {
            const struct p101_wrapper_fact *fact;

            fact = &model->facts[index];
            if(fact->kind != P101_C_ANALYSIS_DIAGNOSTIC)
            {
                continue;
            }
            if(!first)
            {
                p101_fputc(env, err, ',', stdout);
            }
            first = false;
            p101_fputs(env, err, "{\"id\":\"P101-WRAP-900\",\"severity\":\"error\",\"location\":{\"path\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, fact->path);
            p101_fprintf(env, err, stdout, ",\"line\":%zu,\"column\":%zu,\"function\":\"?\"},\"message\":", fact->line, fact->column);
            p101_wrapper_output_json_string(env, err, stdout, fact->name);
            p101_fputs(env, err, ",\"evidence\":{\"parser\":\"libclang\",\"kind\":\"parse-failure\"}}", stdout);
        }
        p101_fputs(env, err, "]}\n", stdout);
        goto done;
    }

    p101_fputs(env, err, "p101-wrapper-audit summary\n", stdout);
    p101_fprintf(env,
                 err,
                 stdout,
                 "missed_wrappers: %zu\nexternal_calls: %zu\nindirect_calls: %zu\nportability_includes: %zu\nparse_failures: %zu\n",
                 counts[P101_WRAPPER_MISSED],
                 counts[P101_WRAPPER_EXTERNAL],
                 counts[P101_WRAPPER_INDIRECT],
                 counts[P101_WRAPPER_PORTABILITY],
                 model->parse_failures);
    for(index = 0U; index < model->finding_count; index++)
    {
        const struct p101_wrapper_finding *finding;

        finding            = &model->findings[index];
        p101_call_result_5 = finding_id(finding->kind);
        p101_call_result_6 = finding_label(finding->kind);
        p101_fprintf(env, err, stdout, "%s:%zu:%zu: %s: %s: %s", finding->path, finding->line, finding->column, p101_call_result_5, p101_call_result_6, finding->name);
        if(finding->replacement[0] != '\0')
        {
            p101_fprintf(env, err, stdout, " -> %s", finding->replacement);
        }
        p101_fprintf(env, err, stdout, " [%s]\n", finding->caller);
        if(finding->kind == P101_WRAPPER_MISSED)
        {
            p101_fprintf(env, err, stdout, "  hint: use %s(env, err, ...) instead of raw %s(...) when the surrounding code is p101-aware\n", finding->replacement, finding->name);
        }
        else if(finding->kind == P101_WRAPPER_INDIRECT)
        {
            p101_fputs(env, err, "  hint: indirect/non-p101 call; document this runtime boundary\n", stdout);
        }
        else if(finding->kind == P101_WRAPPER_PORTABILITY)
        {
            p101_fputs(env, err, "  hint: isolate this platform header behind a portable boundary\n", stdout);
        }
        else
        {
            p101_fputs(env, err, "  hint: allow it explicitly, wrap it, or keep it at a documented boundary\n", stdout);
        }
    }
    for(index = 0U; index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *fact;

        fact = &model->facts[index];
        if(fact->kind == P101_C_ANALYSIS_DIAGNOSTIC)
        {
            p101_fprintf(env, err, stdout, "%s:%zu:%zu: P101-WRAP-900: parse-failure: %s [?]\n", fact->path, fact->line, fact->column, fact->name);
        }
    }

done:
    return;
}

static void write_fact_prefix(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct p101_wrapper_fact *fact)
{
    const char *p101_call_result_7;
    const char *p101_call_result_8;
    char        module[P101_WRAPPER_NAME_SIZE];

    module_name(env, fact->path, module, sizeof(module));
    p101_fputs(env, err, "P101FACT\t6\t", stream);
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
