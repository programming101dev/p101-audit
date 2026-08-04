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
    P101_TRACE_SCOPE(env);
    if(p101_record_write_json_string(stream, text == NULL ? "" : text) != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
}

static void tsv_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    const unsigned char *cursor;

    P101_TRACE_SCOPE(env);
    if(text == NULL)
    {
        return;
    }
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
            const char *package_end;

            name        = root + sizeof("/include/") - 1U;
            package_end = p101_strchr(env, name, '/');
            if(package_end != NULL && p101_strncmp(env, name, "p101_", sizeof("p101_") - 1U) == 0)
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
        p101_fputs(env, err, "{\"schema\":\"p101-wrapper-inventory-v2\",\"wrappers\":[", stdout);
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
            p101_fputs(env, err, ",\"wrapper\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, model->inventory[index].wrapper);
            p101_fputc(env, err, '}', stdout);
        }
        else
        {
            p101_fprintf(env, err, stdout, "%s\t%s\n", model->inventory[index].original, model->inventory[index].wrapper);
        }
    }
    if(json)
    {
        p101_fputs(env, err, "]}\n", stdout);
    }
}

void p101_wrapper_write_audit(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments)
{
    size_t counts[4] = {0U, 0U, 0U, 0U};
    size_t index;

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
            p101_wrapper_output_json_string(env, err, stdout, finding_id(finding->kind));
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
            p101_wrapper_output_json_string(env, err, stdout, finding_label(finding->kind));
            p101_fputs(env, err, ",\"evidence\":{\"parser\":\"libclang\",\"kind\":", stdout);
            p101_wrapper_output_json_string(env, err, stdout, finding_label(finding->kind));
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
        return;
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

        finding = &model->findings[index];
        p101_fprintf(env, err, stdout, "%s:%zu:%zu: %s: %s: %s", finding->path, finding->line, finding->column, finding_id(finding->kind), finding_label(finding->kind), finding->name);
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
}

static void write_fact_prefix(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct p101_wrapper_fact *fact)
{
    char module[P101_WRAPPER_NAME_SIZE];

    module_name(env, fact->path, module, sizeof(module));
    p101_fputs(env, err, "P101FACT\t4\t", stream);
    p101_fputs(env, err, p101_c_analysis_kind_name(fact->kind), stream);
    p101_fputc(env, err, '\t', stream);
    tsv_string(env, err, stream, fact->path);
    p101_fputc(env, err, '\t', stream);
    tsv_string(env, err, stream, module);
    p101_fprintf(env, err, stream, "\t%s\t%zu", bool_text(fact->is_header), fact->line);
}

void p101_wrapper_write_facts(const struct p101_env *env, struct p101_error *err, const struct p101_wrapper_model *model, FILE *stream)
{
    size_t index;

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
        if((fact->kind == P101_C_ANALYSIS_TYPE || fact->kind == P101_C_ANALYSIS_ENUM || fact->kind == P101_C_ANALYSIS_ENUMERATOR || fact->kind == P101_C_ANALYSIS_MACRO) && !fact->is_header)
        {
            continue;
        }
        write_fact_prefix(env, err, stream, fact);
        if(fact->kind == P101_C_ANALYSIS_INCLUDE)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_fprintf(env, err, stream, "\t%s", bool_text(fact->is_local_include));
        }
        else if(fact->kind == P101_C_ANALYSIS_TYPE || fact->kind == P101_C_ANALYSIS_ENUM || fact->kind == P101_C_ANALYSIS_MACRO)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
        }
        else if(fact->kind == P101_C_ANALYSIS_ENUMERATOR)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->type);
        }
        else if(fact->kind == P101_C_ANALYSIS_NOTE)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->caller);
            p101_fprintf(env, err, stream, "\t%zu", fact->column);
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
            p101_fprintf(env, err, stream, "\t%s\t%s", bool_text(fact->is_static), bool_text(declaration));
        }
        else if(fact->kind == P101_C_ANALYSIS_CALL)
        {
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->name);
            p101_fprintf(env, err, stream, "\t%s\t%s", bool_text(fact->needs_env), bool_text(fact->needs_error));
            p101_fputc(env, err, '\t', stream);
            tsv_string(env, err, stream, fact->caller);
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
