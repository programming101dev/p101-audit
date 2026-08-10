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
#include <p101_record/record.h>
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
static bool   manifest_line_is_over_long(const struct p101_env *env, const char *line, size_t size);
static size_t split_manifest_fields(const struct p101_env *env, char *line, char *fields[], size_t capacity);
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
    void                          *p101_call_result_1;
    size_t                         capacity;
    struct p101_wrapper_inventory *inventory;
    bool                           grown;

    P101_TRACE_SCOPE(env);
    grown              = false;
    capacity           = model->inventory_capacity == 0U ? INITIAL_CAPACITY : model->inventory_capacity * 2U;
    p101_call_result_1 = p101_realloc(env, err, model->inventory, capacity * sizeof(*inventory));
    inventory          = (struct p101_wrapper_inventory *)p101_call_result_1;
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
    int    p101_expression_result_17;
    int    p101_expression_result_18;
    int    p101_call_result_19;
    int    p101_expression_result_20;
    bool   p101_call_result_21;
    size_t index;
    bool   added;

    P101_TRACE_SCOPE(env);
    added = true;
    for(index = 0U; index < model->inventory_count; index++)
    {
        p101_expression_result_18 = 0;
        if(wrapper_usr != NULL)
        {
            if(wrapper_usr[0] != '\0')
            {
                p101_expression_result_18 = 1;
            }
        }
        p101_expression_result_17 = 0;
        if(p101_expression_result_18)
        {
            p101_call_result_19 = p101_strcmp(env, model->inventory[index].wrapper_usr, wrapper_usr);
            if(p101_call_result_19 == 0)
            {
                p101_expression_result_17 = 1;
            }
        }
        if(p101_expression_result_17)
        {
            if(original_usr != NULL && original_usr[0] != '\0' && model->inventory[index].original_usr[0] == '\0')
            {
                copy_field(env, model->inventory[index].original, sizeof(model->inventory[index].original), original);
                copy_field(env, model->inventory[index].original_usr, sizeof(model->inventory[index].original_usr), original_usr);
            }
            goto done;
        }
    }
    p101_expression_result_20 = 0;
    if(model->inventory_count == model->inventory_capacity)
    {
        p101_call_result_21 = grow_inventory(env, err, model);
        if(!p101_call_result_21)
        {
            p101_expression_result_20 = 1;
        }
    }
    if(p101_expression_result_20)
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

/*
 * A row that filled the buffer without a newline was truncated by p101_fgets;
 * the remainder would otherwise resynchronise as a bogus extra row.
 */
static bool manifest_line_is_over_long(const struct p101_env *env, const char *line, size_t size)
{
    size_t length;
    bool   over_long;

    P101_TRACE_SCOPE(env);
    over_long = false;
    length    = p101_strlen(env, line);
    if(length == size - 1U)
    {
        const char *p101_call_result_47;

        p101_call_result_47 = p101_strchr(env, line, '\n');
        if(p101_call_result_47 == NULL)
        {
            over_long = true;
        }
    }
    return over_long;
}

static size_t split_manifest_fields(const struct p101_env *env, char *line, char *fields[], size_t capacity)
{
    size_t count;
    size_t length;
    char  *cursor;

    P101_TRACE_SCOPE(env);
    length = p101_strlen(env, line);
    while(length > 0U && (line[length - 1U] == '\n' || line[length - 1U] == '\r'))
    {
        length--;
        line[length] = '\0';
    }
    count  = 0U;
    cursor = line;
    while(count < capacity && cursor != NULL)
    {
        fields[count] = p101_record_split(&cursor);
        p101_record_unescape_field(fields[count]);
        count++;
    }
    return count;
}

static bool load_manifest_file(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const char *path)
{
    int         p101_call_result_16;
    int         p101_call_result_15;
    int         p101_call_result_13;
    const char *p101_call_result_2;
    int         p101_call_result_3;
    int         p101_call_result_4;
    int         p101_call_result_5;
    bool        p101_call_result_6;
    FILE       *stream;
    char        line[MANIFEST_LINE_SIZE];
    size_t      wrapper_column;
    size_t      wrapper_usr_column;
    size_t      original_column;
    size_t      original_usr_column;
    bool        loaded;

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
    p101_call_result_2 = p101_fgets(env, err, line, sizeof(line), stream);
    if(p101_call_result_2 != NULL)
    {
        char  *fields[MANIFEST_FIELD_LIMIT];
        size_t field_count;
        bool   p101_call_result_48;

        p101_call_result_48 = manifest_line_is_over_long(env, line, sizeof(line));
        if(p101_call_result_48)
        {
            P101_ERROR_RAISE_USER(err, "An API manifest contains an over-long row.", 1);
            goto close_stream;
        }
        field_count = split_manifest_fields(env, line, fields, sizeof(fields) / sizeof(fields[0]));
        for(size_t index = 0U; index < field_count; index++)
        {
            p101_call_result_3 = p101_strcmp(env, fields[index], "function");
            if(p101_call_result_3 == 0)
            {
                wrapper_column = index;
            }
            else
            {
                p101_call_result_13 = p101_strcmp(env, fields[index], "function_usr");
                if(p101_call_result_13 == 0)
                {
                    wrapper_usr_column = index;
                }
                else
                {
                    p101_call_result_15 = p101_strcmp(env, fields[index], "native_function");
                    if(p101_call_result_15 == 0)
                    {
                        original_column = index;
                    }
                    else
                    {
                        p101_call_result_16 = p101_strcmp(env, fields[index], "native_function_usr");
                        if(p101_call_result_16 == 0)
                        {
                            original_usr_column = index;
                        }
                    }
                }
            }
        }
    }
    if(wrapper_column == SIZE_MAX || wrapper_usr_column == SIZE_MAX || original_column == SIZE_MAX || original_usr_column == SIZE_MAX)
    {
        P101_ERROR_RAISE_USER(err, "An API manifest lacks semantic wrapper/native identity columns.", 1);
        goto close_stream;
    }
    for(;;)
    {
        char       *fields[MANIFEST_FIELD_LIMIT];
        const char *original;
        const char *original_usr;
        size_t      field_count;
        size_t      maximum_column;
        bool        p101_call_result_49;

        p101_call_result_2 = p101_fgets(env, err, line, sizeof(line), stream);
        if(p101_call_result_2 == NULL)
        {
            break;
        }
        p101_call_result_49 = manifest_line_is_over_long(env, line, sizeof(line));
        if(p101_call_result_49)
        {
            P101_ERROR_RAISE_USER(err, "An API manifest contains an over-long row.", 1);
            break;
        }
        field_count    = split_manifest_fields(env, line, fields, sizeof(fields) / sizeof(fields[0]));
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
        original           = fields[original_column];
        original_usr       = fields[original_usr_column];
        p101_call_result_4 = p101_strcmp(env, original, "-");
        if(p101_call_result_4 == 0)
        {
            original = "";
        }
        p101_call_result_5 = p101_strcmp(env, original_usr, "-");
        if(p101_call_result_5 == 0)
        {
            original_usr = "";
        }
        p101_call_result_6 = add_inventory_mapping(env, err, model, original, original_usr, fields[wrapper_column], fields[wrapper_usr_column]);
        if(!p101_call_result_6)
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
    int            p101_expression_result_22;
    int            p101_expression_result_23;
    int            p101_expression_result_24;
    int            p101_expression_result_25;
    int            p101_call_result_26;
    int            p101_call_result_27;
    int            p101_call_result_28;
    int            p101_call_result_29;
    int            p101_call_result_30;
    int            p101_expression_result_31;
    int            p101_call_result_32;
    int            p101_call_result_7;
    bool           p101_call_result_8;
    bool           p101_call_result_9;
    bool           no_error;
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
    for(;;)
    {
        char        path[P101_WRAPPER_PATH_SIZE];
        struct stat status;

        entry = p101_readdir(env, err, stream);
        if(entry == NULL)
        {
            break;
        }
        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
        p101_call_result_26 = p101_strcmp(env, entry->d_name, ".");
        if(p101_call_result_26 == 0)
        {
            p101_expression_result_25 = 1;
        }
        else
        {
            p101_call_result_27 = p101_strcmp(env, entry->d_name, "..");
            if(p101_call_result_27 == 0)
            {
                p101_expression_result_25 = 1;
            }
            else
            {
                p101_expression_result_25 = 0;
            }
        }
        if(p101_expression_result_25)
        {
            p101_expression_result_24 = 1;
        }
        else
        {
            p101_call_result_28 = p101_strcmp(env, entry->d_name, ".git");
            if(p101_call_result_28 == 0)
            {
                p101_expression_result_24 = 1;
            }
            else
            {
                p101_expression_result_24 = 0;
            }
        }
        if(p101_expression_result_24)
        {
            p101_expression_result_23 = 1;
        }
        else
        {
            p101_call_result_29 = p101_strcmp(env, entry->d_name, "build");
            if(p101_call_result_29 == 0)
            {
                p101_expression_result_23 = 1;
            }
            else
            {
                p101_expression_result_23 = 0;
            }
        }
        if(p101_expression_result_23)
        {
            p101_expression_result_22 = 1;
        }
        else
        {
            p101_call_result_30 = p101_strncmp(env, entry->d_name, "build-", sizeof("build-") - 1U);
            if(p101_call_result_30 == 0)
            {
                p101_expression_result_22 = 1;
            }
            else
            {
                p101_expression_result_22 = 0;
            }
        }
        if(p101_expression_result_22)
        {
            continue;
        }
        p101_snprintf(env, err, path, sizeof(path), "%s/%s", directory, entry->d_name);
        p101_call_result_7 = p101_lstat(env, err, path, &status);
        if(p101_call_result_7 != 0)
        {
            break;
        }
        p101_expression_result_31 = 0;
        if(S_ISREG(status.st_mode))
        {
            p101_call_result_32 = p101_strcmp(env, entry->d_name, "api-manifest.tsv");
            if(p101_call_result_32 == 0)
            {
                p101_expression_result_31 = 1;
            }
        }
        if(S_ISDIR(status.st_mode))
        {
            p101_call_result_8 = load_manifests(env, err, model, path);
            if(!p101_call_result_8)
            {
                break;
            }
        }
        else if(p101_expression_result_31)
        {
            p101_call_result_9 = load_manifest_file(env, err, model, path);
            if(!p101_call_result_9)
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

static bool find_libraries_from_directory(const struct p101_env *env, char *current, char *path, size_t size)
{
    size_t attempt;
    bool   found;

    P101_TRACE_SCOPE(env);
    found = false;
    for(attempt = 0U; attempt < WORKSPACE_PARENT_LIMIT; attempt++)
    {
        struct stat status;
        const char *slash;
        int         stat_result;
        bool        is_directory;

        /* P101_ERROR_OPTIONAL rationale: an empty path rejects this discovery candidate. */
        p101_snprintf(env, P101_ERROR_OPTIONAL, path, size, "%s/libraries", current);
        /* P101_ERROR_OPTIONAL rationale: discovery probes candidate roots. */
        stat_result  = p101_stat(env, P101_ERROR_OPTIONAL, path, &status);
        is_directory = false;
        if(stat_result == 0)
        {
            if(S_ISDIR(status.st_mode))
            {
                is_directory = true;
            }
        }
        if(is_directory)
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

    return found;
}

static bool find_workspace_libraries(const struct p101_env *env, const char *program_path, char *path, size_t size)
{
    const char *p101_call_result_34;
    char        current[P101_WRAPPER_PATH_SIZE];
    bool        found;

    P101_TRACE_SCOPE(env);
    found = false;
    /* P101_ERROR_OPTIONAL rationale: executable ancestry is a convenience probe. */
    p101_call_result_34 = NULL;
    if(program_path != NULL)
    {
        p101_call_result_34 = p101_realpath(env, P101_ERROR_OPTIONAL, program_path, current);
    }
    if(p101_call_result_34 != NULL)
    {
        const char *slash;

        slash = p101_strrchr(env, current, '/');
        if(slash != NULL)
        {
            current[(size_t)(slash - current)] = '\0';
            found                              = find_libraries_from_directory(env, current, path, size);
        }
    }
    if(!found)
    {
        const char *current_directory;

        /* P101_ERROR_OPTIONAL rationale: caller ancestry is the fallback probe. */
        current_directory = p101_getcwd(env, P101_ERROR_OPTIONAL, current, sizeof(current));
        if(current_directory != NULL)
        {
            found = find_libraries_from_directory(env, current, path, size);
        }
    }
    if(!found)
    {
        path[0] = '\0';
    }
    return found;
}

static bool add_annotated_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, size_t first_fact)
{
    int               p101_expression_result_37;
    int               p101_expression_result_38;
    int               p101_call_result_39;
    int               p101_call_result_40;
    int               p101_expression_result_41;
    int               p101_expression_result_42;
    int               p101_call_result_43;
    int               p101_call_result_10;
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
        if(note->kind != P101_C_ANALYSIS_NOTE)
        {
            p101_expression_result_37 = 1;
        }
        else
        {
            p101_call_result_39       = p101_strcmp(env, note->name, wrapper_role);
            p101_expression_result_38 = 0;
            if(p101_call_result_39 != 0)
            {
                p101_call_result_40 = p101_strncmp(env, note->name, wrapper_of_role, sizeof(wrapper_of_role) - 1U);
                if(p101_call_result_40 != 0)
                {
                    p101_expression_result_38 = 1;
                }
            }
            if(p101_expression_result_38)
            {
                p101_expression_result_37 = 1;
            }
            else
            {
                p101_expression_result_37 = 0;
            }
        }
        if(p101_expression_result_37)
        {
            continue;
        }
        original_usr        = "";
        p101_call_result_10 = p101_strncmp(env, note->name, wrapper_of_role, sizeof(wrapper_of_role) - 1U);
        if(p101_call_result_10 == 0)
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

            function                  = &model->facts[function_index];
            p101_expression_result_42 = 0;
            if(function->kind == P101_C_ANALYSIS_FUNCTION)
            {
                if(function->usr[0] != '\0')
                {
                    p101_expression_result_42 = 1;
                }
            }
            p101_expression_result_41 = 0;
            if(p101_expression_result_42)
            {
                p101_call_result_43 = p101_strcmp(env, function->usr, note->caller_usr);
                if(p101_call_result_43 == 0)
                {
                    p101_expression_result_41 = 1;
                }
            }
            if(p101_expression_result_41)
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

static bool path_is_within_header_root(const struct p101_env *env, const char *path, const char *root)
{
    char        canonical_root[P101_WRAPPER_PATH_SIZE];
    const char *resolved;
    size_t      root_length;
    size_t      path_length;
    int         comparison;
    bool        within;

    P101_TRACE_SCOPE(env);
    within   = false;
    resolved = p101_realpath(env, P101_ERROR_OPTIONAL, root, canonical_root);
    if(resolved == NULL)
    {
        goto done;
    }
    comparison = p101_strcmp(env, path, canonical_root);
    if(comparison == 0)
    {
        within = true;
        goto done;
    }
    root_length = p101_strlen(env, canonical_root);
    path_length = p101_strlen(env, path);
    if(path_length <= root_length || path[root_length] != '/')
    {
        goto done;
    }
    comparison = p101_strncmp(env, path, canonical_root, root_length);
    if(comparison == 0)
    {
        within = true;
    }

done:
    return within;
}

static bool add_public_header_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, size_t first_fact, const char *root)
{
    bool added;

    P101_TRACE_SCOPE(env);
    added = true;
    for(size_t index = first_fact; added && index < model->fact_count; index++)
    {
        const struct p101_wrapper_fact *function;
        bool                            admitted;

        function = &model->facts[index];
        if(function->kind != P101_C_ANALYSIS_FUNCTION || function->is_definition || !function->is_public || function->usr[0] == '\0')
        {
            continue;
        }
        admitted = path_is_within_header_root(env, function->path, root);
        if(!admitted)
        {
            continue;
        }
        added = add_inventory_mapping(env, err, model, "", "", function->name, function->usr);
    }
    return added;
}

bool p101_wrapper_model_load_inventory(const struct p101_env *env, struct p101_error *err, struct p101_wrapper_model *model, const struct p101_wrapper_arguments *arguments, const char *program_path)
{
    int    p101_expression_result_44;
    bool   p101_call_result_45;
    char   libraries[P101_WRAPPER_PATH_SIZE];
    size_t index;
    bool   loaded;

    P101_TRACE_SCOPE(env);
    loaded                    = true;
    p101_call_result_45       = find_workspace_libraries(env, program_path, libraries, sizeof(libraries));
    p101_expression_result_44 = 0;
    if(p101_call_result_45)
    {
        bool p101_call_result_46;

        p101_call_result_46 = load_manifests(env, err, model, libraries);
        if(!p101_call_result_46)
        {
            p101_expression_result_44 = 1;
        }
    }
    if(p101_expression_result_44)
    {
        loaded = false;
    }
    for(index = 0U; loaded && index < arguments->header_root_count; index++)
    {
        struct p101_c_analysis_options options;
        const char                    *path;
        size_t                         first_fact;
        bool                           p101_call_result_11;
        bool                           p101_call_result_12;

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
        p101_call_result_11                          = p101_c_analysis_scan(env, err, &options, p101_wrapper_analysis_observer, model);
        if(!p101_call_result_11)
        {
            loaded = false;
            break;
        }
        p101_call_result_12 = add_annotated_inventory(env, err, model, first_fact);
        if(!p101_call_result_12)
        {
            loaded = false;
        }
        if(loaded)
        {
            p101_call_result_12 = add_public_header_inventory(env, err, model, first_fact, path);
            if(!p101_call_result_12)
            {
                loaded = false;
            }
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
