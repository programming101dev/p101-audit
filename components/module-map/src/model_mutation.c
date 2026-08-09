#include "../include/model_mutation.h"
#include "../include/errors.h"
#include "../include/strings.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>

static struct module *p101_module_map_get_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *name);
static void           normalize_relative_path(const struct p101_env *env, char destination[MAX_NAME], const char *path);
static void           local_include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const struct source_file *file, const char *target);
static bool           same_declaration_site(const struct p101_env *env, const char *left_name, const char *left_path, size_t left_line, const char *right_name, const char *right_path, size_t right_line);

/*
 * A header is parsed once per translation unit that includes it, so the native
 * scan reports each of its declarations once per includer. Two records naming
 * the same symbol at the same file and line are the same declaration seen
 * twice; without this, a header included by two sources doubles every count
 * and every finding derived from it.
 */
static bool same_declaration_site(const struct p101_env *env, const char *left_name, const char *left_path, size_t left_line, const char *right_name, const char *right_path, size_t right_line)
{
    bool ret_val;

    ret_val = false;
    if(left_line == right_line)
    {
        int p101_call_result_30;

        p101_call_result_30 = p101_strcmp(env, left_name, right_name);
        if(p101_call_result_30 == 0)
        {
            int p101_call_result_31;

            p101_call_result_31 = p101_strcmp(env, left_path, right_path);
            if(p101_call_result_31 == 0)
            {
                ret_val = true;
            }
        }
    }

    return ret_val;
}

static void normalize_relative_path(const struct p101_env *env, char destination[MAX_NAME], const char *path)
{
    const char *cursor;
    size_t      used;

    P101_TRACE_SCOPE(env);
    destination[0] = '\0';
    cursor         = path;
    used           = 0U;

    while(*cursor != '\0')
    {
        const char *component;
        size_t      component_length;

        while(*cursor == '/')
        {
            cursor++;
        }
        component = cursor;
        while(*cursor != '\0' && *cursor != '/')
        {
            cursor++;
        }
        component_length = (size_t)(cursor - component);

        if(component_length == 0U || (component_length == 1U && component[0] == '.'))
        {
            continue;
        }

        if(component_length == 2U && component[0] == '.' && component[1] == '.' && used > 0U)
        {
            size_t previous_start;

            previous_start = used;
            while(previous_start > 0U && destination[previous_start - 1U] != '/')
            {
                previous_start--;
            }
            if(used - previous_start != 2U || destination[previous_start] != '.' || destination[previous_start + 1U] != '.')
            {
                used              = previous_start == 0U ? 0U : previous_start - 1U;
                destination[used] = '\0';
                continue;
            }
        }

        /*
         * path is already bounded by MAX_NAME and lexical normalization can
         * only shorten it, so every copied component fits destination.
         */
        if(used > 0U)
        {
            destination[used++] = '/';
            destination[used]   = '\0';
        }
        for(size_t index = 0U; index < component_length; index++)
        {
            destination[used++] = component[index];
        }
        destination[used] = '\0';
    }
}

static void local_include_to_module(const struct p101_env *env, char *destination, size_t destination_size, const struct source_file *file, const char *target)
{
    int p101_expression_result_2;
    int p101_call_result_3;

    p101_call_result_3 = p101_strncmp(env, target, "./", sizeof("./") - 1U);
    if(p101_call_result_3 == 0)
    {
        p101_expression_result_2 = 1;
    }
    else
    {
        int p101_call_result_4;

        p101_call_result_4 = p101_strncmp(env, target, "../", sizeof("../") - 1U);
        if(p101_call_result_4 == 0)
        {
            p101_expression_result_2 = 1;
        }
        else
        {
            p101_expression_result_2 = 0;
        }
    }
    if(p101_expression_result_2)
    {
        char        joined[MAX_NAME];
        char        normalized[MAX_NAME];
        const char *slash;
        size_t      directory_length;

        joined[0]        = '\0';
        slash            = p101_strrchr(env, file->module, '/');
        directory_length = slash == NULL ? 0U : (size_t)(slash - file->module);
        for(size_t index = 0U; index < directory_length && index + 1U < sizeof(joined); index++)
        {
            joined[index]     = file->module[index];
            joined[index + 1] = '\0';
        }
        if(directory_length > 0U)
        {
            p101_module_map_append_string(env, joined, sizeof(joined), "/");
        }
        p101_module_map_append_string(env, joined, sizeof(joined), target);
        normalize_relative_path(env, normalized, joined);
        p101_module_map_include_to_module(env, destination, destination_size, normalized);
    }
    else
    {
        p101_module_map_include_to_module(env, destination, destination_size, target);
    }
}

static struct module *p101_module_map_get_module(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *name)
{
    int            p101_call_result_1;
    struct module *module;

    P101_TRACE_SCOPE(env);
    module = NULL;

    for(size_t i = 0; i < map->module_count; i++)
    {
        p101_call_result_1 = p101_strcmp(env, map->modules[i].name, name);
        if(p101_call_result_1 == 0)
        {
            module = &map->modules[i];
            goto done;
        }
    }

    if(map->module_count >= MAX_MODULES)
    {
        P101_ERROR_RAISE_USER(err, "Too many modules for audit-modules.", ERR_USAGE);
        goto done;
    }

    module = &map->modules[map->module_count++];
    p101_memset(env, module, 0, sizeof(*module));
    p101_module_map_copy_string(env, module->name, sizeof(module->name), name);

done:
    return module;
}

void p101_module_map_add_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, bool is_header)
{
    char module_name[MAX_NAME];

    P101_TRACE_SCOPE(env);
    p101_module_map_basename_no_suffix(env, module_name, sizeof(module_name), path);
    p101_module_map_add_named_source_file(env, err, map, path, module_name, is_header);
}

void p101_module_map_add_named_source_file(const struct p101_env *env, struct p101_error *err, struct project_map *map, const char *path, const char *module_name, bool is_header)
{
    struct source_file *file;
    struct module      *module;

    P101_TRACE_SCOPE(env);
    if(map->file_count >= MAX_FILES)
    {
        P101_ERROR_RAISE_USER(err, "Too many files for audit-modules.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, module_name);
    if(module == NULL)
    {
        goto done;
    }

    file = &map->files[map->file_count++];
    p101_module_map_copy_string(env, file->path, sizeof(file->path), path);
    p101_module_map_copy_string(env, file->module, sizeof(file->module), module_name);
    file->is_header = is_header;

    if(is_header)
    {
        module->header_count++;
        if(module->header_path[0] == '\0')
        {
            p101_module_map_copy_string(env, module->header_path, sizeof(module->header_path), path);
        }
    }
    else
    {
        module->source_count++;
        if(module->source_path[0] == '\0')
        {
            p101_module_map_copy_string(env, module->source_path, sizeof(module->source_path), path);
        }
    }

done:
    return;
}

void p101_module_map_add_include(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *target, const char *resolved, size_t line, bool is_local)
{
    struct include_record *include;
    struct module         *module;

    P101_TRACE_SCOPE(env);
    if(map->include_count >= MAX_INCLUDES)
    {
        P101_ERROR_RAISE_USER(err, "Too many includes for audit-modules.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    include = &map->includes[map->include_count++];
    p101_module_map_copy_string(env, include->from_module, sizeof(include->from_module), file->module);
    p101_module_map_copy_string(env, include->path, sizeof(include->path), file->path);
    p101_module_map_copy_string(env, include->resolved, sizeof(include->resolved), resolved == NULL ? "" : resolved);
    include->line     = line;
    include->is_local = is_local;

    if(is_local)
    {
        local_include_to_module(env, include->target, sizeof(include->target), file, target);
        module->local_include_count++;
    }
    else
    {
        p101_module_map_copy_string(env, include->target, sizeof(include->target), target);
        module->external_include_count++;
    }

done:
    return;
}

void p101_module_map_add_function(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, const char *usr, size_t line, bool is_static, bool is_header_declaration)
{
    int                     p101_expression_result_5;
    size_t                  p101_call_result_6;
    struct function_record *function;
    struct module          *module;

    P101_TRACE_SCOPE(env);
    if(map->function_count >= MAX_FUNCTIONS)
    {
        P101_ERROR_RAISE_USER(err, "Too many functions for audit-modules.", ERR_USAGE);
        goto done;
    }
    if(usr == NULL)
    {
        p101_expression_result_5 = 1;
    }
    else
    {
        p101_call_result_6 = p101_strlen(env, usr);
        if(p101_call_result_6 >= sizeof(map->functions[0].usr))
        {
            p101_expression_result_5 = 1;
        }
        else
        {
            p101_expression_result_5 = 0;
        }
    }
    if(p101_expression_result_5)
    {
        P101_ERROR_RAISE_USER(err, "A resolved function identity is absent or too long for audit-modules.", ERR_USAGE);
        goto done;
    }

    for(size_t index = 0U; index < map->function_count; index++)
    {
        bool duplicate;

        duplicate = same_declaration_site(env, map->functions[index].name, map->functions[index].path, map->functions[index].line, name, file->path, line);
        if(duplicate)
        {
            goto done;
        }
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    function = &map->functions[map->function_count++];
    p101_module_map_copy_string(env, function->name, sizeof(function->name), name);
    p101_module_map_copy_string(env, function->usr, sizeof(function->usr), usr);
    p101_module_map_copy_string(env, function->module, sizeof(function->module), file->module);
    p101_module_map_copy_string(env, function->path, sizeof(function->path), file->path);
    function->line                  = line;
    function->is_static             = is_static;
    function->is_header_declaration = is_header_declaration;

    if(is_header_declaration)
    {
        module->header_declaration_count++;
    }
    else
    {
        module->function_count++;
        if(is_static)
        {
            module->static_function_count++;
        }
        else
        {
            module->public_function_count++;
        }
    }

done:
    return;
}

void p101_module_map_add_macro(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line)
{
    struct macro_record *macro;
    struct module       *module;

    P101_TRACE_SCOPE(env);
    if(map->macro_count >= MAX_MACROS)
    {
        P101_ERROR_RAISE_USER(err, "Too many macros for audit-modules.", ERR_USAGE);
        goto done;
    }

    for(size_t index = 0U; index < map->macro_count; index++)
    {
        bool duplicate;

        duplicate = same_declaration_site(env, map->macros[index].name, map->macros[index].path, map->macros[index].line, name, file->path, line);
        if(duplicate)
        {
            goto done;
        }
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    macro = &map->macros[map->macro_count++];
    p101_module_map_copy_string(env, macro->name, sizeof(macro->name), name);
    p101_module_map_copy_string(env, macro->module, sizeof(macro->module), file->module);
    p101_module_map_copy_string(env, macro->path, sizeof(macro->path), file->path);
    macro->line      = line;
    macro->is_header = file->is_header;
    module->macro_count++;

done:
    return;
}

void p101_module_map_add_type(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, size_t line)
{
    struct type_record *type;
    struct module      *module;

    P101_TRACE_SCOPE(env);
    if(map->type_count >= MAX_TYPES)
    {
        P101_ERROR_RAISE_USER(err, "Too many types for audit-modules.", ERR_USAGE);
        goto done;
    }

    module = p101_module_map_get_module(env, err, map, file->module);
    if(module == NULL)
    {
        goto done;
    }

    for(size_t index = 0U; index < map->type_count; index++)
    {
        bool duplicate;

        duplicate = same_declaration_site(env, map->types[index].name, map->types[index].path, map->types[index].line, name, file->path, line);
        if(duplicate)
        {
            goto done;
        }
    }

    type = &map->types[map->type_count++];
    p101_module_map_copy_string(env, type->name, sizeof(type->name), name);
    p101_module_map_copy_string(env, type->module, sizeof(type->module), file->module);
    p101_module_map_copy_string(env, type->path, sizeof(type->path), file->path);
    type->line      = line;
    type->is_header = file->is_header;
    module->type_count++;

done:
    return;
}

void p101_module_map_add_call(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, const char *usr, const char *caller_usr, size_t line)
{
    struct call_record *call;
    size_t              usr_length;

    P101_TRACE_SCOPE(env);
    usr_length = 0U;
    if(map->call_count >= MAX_CALLS)
    {
        map->calls_dropped++;
        P101_ERROR_RAISE_USER(err, "Too many call facts for audit-modules; refusing an incomplete analysis.", ERR_USAGE);
        goto done;
    }
    if(usr != NULL)
    {
        usr_length = p101_strlen(env, usr);
    }
    if(name == NULL || usr == NULL || usr_length >= sizeof(map->calls[0].usr))
    {
        P101_ERROR_RAISE_USER(err, "A resolved call identity is absent or too long for audit-modules.", ERR_USAGE);
        goto done;
    }

    call = &map->calls[map->call_count++];
    p101_module_map_copy_string(env, call->name, sizeof(call->name), name);
    p101_module_map_copy_string(env, call->usr, sizeof(call->usr), usr);
    p101_module_map_copy_string(env, call->caller_usr, sizeof(call->caller_usr), caller_usr == NULL ? "" : caller_usr);
    p101_module_map_copy_string(env, call->module, sizeof(call->module), file->module);
    p101_module_map_copy_string(env, call->path, sizeof(call->path), file->path);
    call->line      = line;
    call->is_header = file->is_header;

done:
    return;
}

void p101_module_map_add_note(const struct p101_env *env, struct p101_error *err, struct project_map *map, const struct source_file *file, const char *name, const char *symbol, const char *symbol_usr, size_t line)
{
    struct note_record *note;
    const char         *note_name;
    const char         *note_symbol;
    const char         *note_symbol_usr;
    bool                recorded;

    (void)err;
    note_name       = name == NULL ? "" : name;
    note_symbol     = symbol == NULL ? "" : symbol;
    note_symbol_usr = symbol_usr == NULL ? "" : symbol_usr;
    recorded        = false;
    if(map->note_count >= MAX_NOTES)
    {
        goto done;
    }
    for(size_t i = 0; i < map->note_count && !recorded; i++)
    {
        int p101_call_result_20;
        int p101_call_result_21;
        int p101_call_result_22;

        p101_call_result_20 = p101_strcmp(env, map->notes[i].name, note_name);
        p101_call_result_21 = p101_strcmp(env, map->notes[i].symbol, note_symbol);
        p101_call_result_22 = p101_strcmp(env, map->notes[i].path, file->path);
        if(p101_call_result_20 == 0 && p101_call_result_21 == 0 && p101_call_result_22 == 0 && map->notes[i].line == line)
        {
            recorded = true;
        }
    }
    if(recorded)
    {
        goto done;
    }
    note = &map->notes[map->note_count];
    p101_module_map_copy_string(env, note->name, sizeof(note->name), note_name);
    p101_module_map_copy_string(env, note->symbol, sizeof(note->symbol), note_symbol);
    p101_module_map_copy_string(env, note->symbol_usr, sizeof(note->symbol_usr), note_symbol_usr);
    p101_module_map_copy_string(env, note->module, sizeof(note->module), file->module);
    p101_module_map_copy_string(env, note->path, sizeof(note->path), file->path);
    note->line      = line;
    note->is_header = file->is_header;
    map->note_count++;

done:
    return;
}
