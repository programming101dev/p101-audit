#include "model.h"
#include <dirent.h>
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/p101_fnmatch.h>
#include <p101_filesystem/p101_ftw.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_filesystem/p101_libgen.h>
#include <p101_filesystem/p101_stdio.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_filesystem/p101_unistd.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_filesystem/sys/p101_statvfs.h>
#include <stdint.h>
#include <sys/stat.h>

enum
{
    INITIAL_CAPACITY       = 256,
    MANIFEST_FIELD_LIMIT   = 32,
    MANIFEST_LINE_SIZE     = 1024,
    WORKSPACE_PARENT_LIMIT = 8
};

static bool   add_inventory_mapping(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *original, const char *original_usr, const char *wrapper, const char *wrapper_usr);
static size_t split_manifest_fields(char *line, char *fields[], size_t capacity);
static bool   add_annotated_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, size_t first_fact);

static void copy_field(const struct p101_env *env, char *destination, size_t size, const char *source)
{
    P101_TRACE_SCOPE(env);
    destination[0] = '\0';
    if(source != NULL)
    {
        size_t length;

        length = p101_strlen(env, source);
        if(length >= size)
        {
            length = size - 1U;
        }
        p101_memcpy(env, destination, source, length);
        destination[length] = '\0';
    }
}

static bool grow_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model)
{
    size_t                         capacity;
    struct p101_wrapper_inventory *inventory;
    bool                           grown;

    P101_TRACE_SCOPE(env);
    grown     = false;
    capacity  = model->inventory_capacity == 0U ? INITIAL_CAPACITY : model->inventory_capacity * 2U;
    inventory = (struct p101_wrapper_inventory *)p101_realloc(env, err, model->inventory, capacity * sizeof(*inventory));
    if(inventory == NULL)
    {
        goto done;
    }
    model->inventory          = inventory;
    model->inventory_capacity = capacity;
    grown                     = true;

done:
    return grown;
}

static bool add_inventory_mapping(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *original, const char *original_usr, const char *wrapper, const char *wrapper_usr)
{
    size_t index;
    bool   added;

    P101_TRACE_SCOPE(env);
    added = true;
    for(index = 0U; index < model->inventory_count; index++)
    {
        if(wrapper_usr != NULL && wrapper_usr[0] != '\0' && p101_strcmp(env, model->inventory[index].wrapper_usr, wrapper_usr) == 0)
        {
            if(original_usr != NULL && original_usr[0] != '\0' && model->inventory[index].original_usr[0] == '\0')
            {
                copy_field(env, model->inventory[index].original, sizeof(model->inventory[index].original), original);
                copy_field(env, model->inventory[index].original_usr, sizeof(model->inventory[index].original_usr), original_usr);
            }
            goto done;
        }
    }
    if(model->inventory_count == model->inventory_capacity && !grow_inventory(env, err, model))
    {
        added = false;
        goto done;
    }
    copy_field(env, model->inventory[model->inventory_count].original, sizeof(model->inventory[model->inventory_count].original), original);
    copy_field(env, model->inventory[model->inventory_count].original_usr, sizeof(model->inventory[model->inventory_count].original_usr), original_usr);
    copy_field(env, model->inventory[model->inventory_count].wrapper, sizeof(model->inventory[model->inventory_count].wrapper), wrapper);
    copy_field(env, model->inventory[model->inventory_count].wrapper_usr, sizeof(model->inventory[model->inventory_count].wrapper_usr), wrapper_usr);
    model->inventory_count++;

done:
    return added;
}

static size_t split_manifest_fields(char *line, char *fields[], size_t capacity)
{
    size_t count;
    char  *cursor;

    count  = 0U;
    cursor = line;
    while(count < capacity)
    {
        char *separator;

        fields[count++] = cursor;
        separator       = cursor;
        while(*separator != '\0' && *separator != '\t' && *separator != '\n' && *separator != '\r')
        {
            separator++;
        }
        if(*separator == '\0' || *separator == '\n' || *separator == '\r')
        {
            *separator = '\0';
            break;
        }
        *separator = '\0';
        cursor     = separator + 1;
    }
    return count;
}

static bool load_manifest_file(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *path)
{
    FILE  *stream;
    char   line[MANIFEST_LINE_SIZE];
    size_t wrapper_column;
    size_t wrapper_usr_column;
    size_t original_column;
    size_t original_usr_column;
    bool   loaded;

    P101_TRACE_SCOPE(env);
    loaded              = false;
    wrapper_column      = SIZE_MAX;
    wrapper_usr_column  = SIZE_MAX;
    original_column     = SIZE_MAX;
    original_usr_column = SIZE_MAX;
    stream              = p101_fopen(env, err, path, "r");
    if(stream == NULL)
    {
        goto done;
    }
    if(p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        char  *fields[MANIFEST_FIELD_LIMIT];
        size_t field_count;

        field_count = split_manifest_fields(line, fields, sizeof(fields) / sizeof(fields[0]));
        for(size_t index = 0U; index < field_count; index++)
        {
            if(p101_strcmp(env, fields[index], "function") == 0)
            {
                wrapper_column = index;
            }
            else if(p101_strcmp(env, fields[index], "function_usr") == 0)
            {
                wrapper_usr_column = index;
            }
            else if(p101_strcmp(env, fields[index], "native_function") == 0)
            {
                original_column = index;
            }
            else if(p101_strcmp(env, fields[index], "native_function_usr") == 0)
            {
                original_usr_column = index;
            }
        }
    }
    if(wrapper_column == SIZE_MAX || wrapper_usr_column == SIZE_MAX || original_column == SIZE_MAX || original_usr_column == SIZE_MAX)
    {
        P101_ERROR_RAISE_USER(err, "An API manifest lacks semantic wrapper/native identity columns.", 1);
        goto close_stream;
    }
    while(p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        char       *fields[MANIFEST_FIELD_LIMIT];
        const char *original;
        const char *original_usr;
        size_t      field_count;
        size_t      maximum_column;

        field_count    = split_manifest_fields(line, fields, sizeof(fields) / sizeof(fields[0]));
        maximum_column = wrapper_column;
        if(wrapper_usr_column > maximum_column)
        {
            maximum_column = wrapper_usr_column;
        }
        if(original_column > maximum_column)
        {
            maximum_column = original_column;
        }
        if(original_usr_column > maximum_column)
        {
            maximum_column = original_usr_column;
        }
        if(field_count <= maximum_column)
        {
            P101_ERROR_RAISE_USER(err, "An API manifest row lacks semantic wrapper/native identity fields.", 1);
            break;
        }
        original     = fields[original_column];
        original_usr = fields[original_usr_column];
        if(p101_strcmp(env, original, "-") == 0)
        {
            original = "";
        }
        if(p101_strcmp(env, original_usr, "-") == 0)
        {
            original_usr = "";
        }
        if(!add_inventory_mapping(env, err, model, original, original_usr, fields[wrapper_column], fields[wrapper_usr_column]))
        {
            break;
        }
    }

close_stream:
    p101_fclose(env, err, stream);
    loaded = p101_error_has_no_error(err);

done:
    return loaded;
}

static bool load_manifests(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *directory)    // NOLINT(misc-no-recursion)
{
    DIR           *stream;
    struct dirent *entry;
    bool           loaded;

    P101_TRACE_SCOPE(env);
    loaded = false;
    stream = p101_opendir(env, err, directory);
    if(stream == NULL)
    {
        goto done;
    }
    while((entry = p101_readdir(env, err, stream)) != NULL && p101_error_has_no_error(err))
    {
        char        path[P101_WRAPPER_PATH_SIZE];
        struct stat status;

        if(p101_strcmp(env, entry->d_name, ".") == 0 || p101_strcmp(env, entry->d_name, "..") == 0 || p101_strcmp(env, entry->d_name, ".git") == 0 || p101_strcmp(env, entry->d_name, "build") == 0 ||
           p101_strncmp(env, entry->d_name, "build-", sizeof("build-") - 1U) == 0)
        {
            continue;
        }
        p101_snprintf(env, err, path, sizeof(path), "%s/%s", directory, entry->d_name);
        if(p101_stat(env, err, path, &status) != 0)
        {
            break;
        }
        if(S_ISDIR(status.st_mode))
        {
            if(!load_manifests(env, err, model, path))
            {
                break;
            }
        }
        else if(S_ISREG(status.st_mode) && p101_strcmp(env, entry->d_name, "api-manifest.tsv") == 0)
        {
            if(!load_manifest_file(env, err, model, path))
            {
                break;
            }
        }
    }
    p101_closedir(env, err, stream);
    loaded = p101_error_has_no_error(err);

done:
    return loaded;
}

static bool find_workspace_libraries(const struct p101_env *env, const char *program_path, char *path, size_t size)
{
    char   current[P101_WRAPPER_PATH_SIZE];
    size_t attempt;
    bool   found;

    P101_TRACE_SCOPE(env);
    found = false;
    /* P101_ERROR_OPTIONAL rationale: discovery probes candidate roots. */
    if(program_path != NULL && p101_realpath(env, P101_ERROR_OPTIONAL, program_path, current) != NULL)
    {
        const char *slash;

        slash = p101_strrchr(env, current, '/');
        if(slash != NULL)
        {
            current[(size_t)(slash - current)] = '\0';
        }
    }
    /* P101_ERROR_OPTIONAL rationale: discovery failure is returned as false. */
    else if(p101_getcwd(env, P101_ERROR_OPTIONAL, current, sizeof(current)) == NULL)
    {
        goto done;
    }
    for(attempt = 0U; attempt < WORKSPACE_PARENT_LIMIT; attempt++)
    {
        struct stat status;
        const char *slash;

        /* P101_ERROR_OPTIONAL rationale: an empty path rejects this discovery candidate. */
        p101_snprintf(env, P101_ERROR_OPTIONAL, path, size, "%s/libraries", current);
        /* P101_ERROR_OPTIONAL rationale: discovery probes candidate roots. */
        if(p101_stat(env, P101_ERROR_OPTIONAL, path, &status) == 0 && S_ISDIR(status.st_mode))
        {
            found = true;
            break;
        }
        slash = p101_strrchr(env, current, '/');
        if(slash == NULL || slash == current)
        {
            break;
        }
        current[(size_t)(slash - current)] = '\0';
    }
    if(!found)
    {
        path[0] = '\0';
    }

done:
    return found;
}

static bool add_annotated_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, size_t first_fact)
{
    static const char wrapper_role[]    = "SEMANTIC_ROLE:p101:wrapper";
    static const char wrapper_of_role[] = "SEMANTIC_ROLE:p101:wrapper-of:";
    bool              added;

    added = true;
    for(size_t note_index = first_fact; added && note_index < model->fact_count; note_index++)
    {
        const struct p101_wrapper_fact *note;
        const char                     *original_usr;
        bool                            resolved;

        note = &model->facts[note_index];
        if(note->kind != P101_C_ANALYSIS_NOTE || (p101_strcmp(env, note->name, wrapper_role) != 0 && p101_strncmp(env, note->name, wrapper_of_role, sizeof(wrapper_of_role) - 1U) != 0))
        {
            continue;
        }
        original_usr = "";
        if(p101_strncmp(env, note->name, wrapper_of_role, sizeof(wrapper_of_role) - 1U) == 0)
        {
            original_usr = note->name + sizeof(wrapper_of_role) - 1U;
            if(original_usr[0] == '\0')
            {
                P101_ERROR_RAISE_USER(err, "A wrapper-of semantic role lacks a callee identity.", 1);
                added = false;
                break;
            }
        }
        resolved = false;
        for(size_t function_index = first_fact; function_index < model->fact_count; function_index++)
        {
            const struct p101_wrapper_fact *function;

            function = &model->facts[function_index];
            if(function->kind == P101_C_ANALYSIS_FUNCTION && function->usr[0] != '\0' && p101_strcmp(env, function->usr, note->caller_usr) == 0)
            {
                added    = add_inventory_mapping(env, err, model, "", original_usr, function->name, function->usr);
                resolved = true;
                break;
            }
        }
        if(!resolved)
        {
            P101_ERROR_RAISE_USER(err, "A wrapper semantic role does not resolve to an admitted function declaration.", 1);
            added = false;
        }
    }
    return added;
}

bool p101_wrapper_model_load_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments, const char *program_path)
{
    char   libraries[P101_WRAPPER_PATH_SIZE];
    size_t index;
    bool   loaded;

    P101_TRACE_SCOPE(env);
    loaded = true;
    if(find_workspace_libraries(env, program_path, libraries, sizeof(libraries)) && !load_manifests(env, err, model, libraries))
    {
        loaded = false;
    }
    for(index = 0U; loaded && index < arguments->header_root_count; index++)
    {
        struct p101_c_analysis_options options;
        const char                    *path;
        size_t                         first_fact;

        path       = arguments->header_roots[index];
        first_fact = model->fact_count;
        p101_memset(env, &options, 0, sizeof(options));
        options.paths                                = &path;
        options.path_count                           = 1U;
        options.extra_arguments                      = arguments->extra_arguments;
        options.extra_argument_count                 = arguments->extra_argument_count;
        options.detailed_preprocessing               = true;
        options.include_headers_as_translation_units = true;
        options.keep_going                           = true;
        if(!p101_c_analysis_scan(env, err, &options, p101_wrapper_analysis_observer, model))
        {
            loaded = false;
            break;
        }
        if(!add_annotated_inventory(env, err, model, first_fact))
        {
            loaded = false;
        }
        model->fact_count = first_fact;
    }
    if(loaded && model->inventory_count == 0U)
    {
        P101_ERROR_RAISE_USER(err, "No p101 wrapper inventory could be discovered.", 1);
        loaded = false;
    }
    return loaded;
}
