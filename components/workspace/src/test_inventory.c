#include "workspace_audit.h"
#include "workspace_json.h"
#include <dirent.h>
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/sys/p101_stat.h>

enum
{
    INVENTORY_PATH_LIMIT = 512U,
    INVENTORY_NAME_SIZE  = 1024U,
    INVENTORY_LINE_SIZE  = 8192U
};

struct inventory_paths
{
    char   values[INVENTORY_PATH_LIMIT][INVENTORY_NAME_SIZE];
    size_t count;
};

static bool token_copy(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t token, char *output, size_t output_size);
static bool require_string(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, char *output, size_t output_size, const char *path, struct p101_workspace_audit_result *result);
static bool path_status(const struct p101_env *env, struct p101_error *err, const char *path, bool executable, bool directory);
static bool path_list_contains(const struct p101_env *env, const struct inventory_paths *paths, const char *value);
static bool path_list_add(const struct p101_env *env, struct p101_error *err, struct inventory_paths *paths, const char *value);
static bool collect_graph_entries(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *graph, struct inventory_paths *governed);
static bool validate_entry_contracts(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *inventory, const char *scripts_root, const char *inventory_path, struct inventory_paths *entry_names,
                                     struct p101_workspace_audit_result *result);
static bool collect_exclusions(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *inventory, const char *scripts_root, const char *inventory_path, struct inventory_paths *exclusions,
                               struct p101_workspace_audit_result *result);
static bool collect_scripts(const struct p101_env *env, struct p101_error *err, const char *root, const char *relative, bool recursive, struct inventory_paths *discovered);
static bool verification_name(const struct p101_env *env, const char *name);
static bool validate_repository_entries(const struct p101_env *env, struct p101_error *err, const char *manifest_path, const char *scripts_root, struct p101_workspace_audit_result *result);

bool p101_workspace_audit_run_test_inventory(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_audit_options *options, struct p101_workspace_audit_result *result)
{
    struct p101_workspace_json inventory;
    struct p101_workspace_json graph;
    struct inventory_paths     governed;
    struct inventory_paths     exclusions;
    struct inventory_paths     discovered;
    struct inventory_paths     entry_names;
    char                       inventory_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       graph_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                       manifest_name[INVENTORY_NAME_SIZE];
    char                       manifest_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    size_t                     schema;
    size_t                     index;
    bool                       joined;
    bool                       loaded;
    bool                       found;
    bool                       valid;
    bool                       added;
    bool                       success;

    P101_TRACE_SCOPE(env);
    p101_workspace_json_init(&inventory);
    p101_workspace_json_init(&graph);
    p101_memset(env, &governed, 0, sizeof(governed));
    p101_memset(env, &exclusions, 0, sizeof(exclusions));
    p101_memset(env, &discovered, 0, sizeof(discovered));
    p101_memset(env, &entry_names, 0, sizeof(entry_names));
    success = false;
    joined  = p101_workspace_audit_join(env, err, inventory_path, sizeof(inventory_path), options->scripts_root, "contracts/p101-test-inventory.json");
    if(joined)
    {
        joined = p101_workspace_audit_join(env, err, graph_path, sizeof(graph_path), options->scripts_root, "contracts/p101-check-graph.json");
    }
    if(!joined)
    {
        goto done;
    }
    loaded = p101_workspace_json_load(env, err, inventory_path, &inventory);
    if(loaded)
    {
        loaded = p101_workspace_json_load(env, err, graph_path, &graph);
    }
    if(!loaded)
    {
        goto done;
    }
    found = p101_workspace_json_object_get(env, &inventory, 0U, "schema", &schema);
    valid = ((found && p101_workspace_json_token_equals(env, &inventory, schema, "p101-test-inventory-v1")) != 0);
    if(!valid)
    {
        p101_workspace_audit_add(env, err, result, inventory_path, "unexpected test-inventory schema");
        success = p101_error_has_no_error(err);
        goto done;
    }
    valid = require_string(env, err, &inventory, 0U, "does_not_prove", manifest_name, sizeof(manifest_name), inventory_path, result);
    valid = ((require_string(env, err, &inventory, 0U, "repository_manifest", manifest_name, sizeof(manifest_name), inventory_path, result) && valid) != 0);
    if(!valid)
    {
        success = p101_error_has_no_error(err);
        goto done;
    }
    joined = p101_workspace_audit_join(env, err, manifest_path, sizeof(manifest_path), options->scripts_root, manifest_name);
    if(!joined)
    {
        goto done;
    }
    collect_graph_entries(env, err, &graph, &governed);
    added = path_list_add(env, err, &governed, "checks/p101-check-graph.py");
    if(!added)
    {
        goto done;
    }
    valid = validate_entry_contracts(env, err, &inventory, options->scripts_root, inventory_path, &entry_names, result);
    if(valid)
    {
        valid = collect_exclusions(env, err, &inventory, options->scripts_root, inventory_path, &exclusions, result);
    }
    if(valid)
    {
        valid = collect_scripts(env, err, options->scripts_root, "", false, &discovered);
    }
    if(valid)
    {
        valid = collect_scripts(env, err, options->scripts_root, "checks", true, &discovered);
    }
    if(valid)
    {
        valid = collect_scripts(env, err, options->scripts_root, "workspace", true, &discovered);
    }
    if(valid)
    {
        valid = collect_scripts(env, err, options->scripts_root, "tests", true, &discovered);
    }
    if(valid)
    {
        valid = validate_repository_entries(env, err, manifest_path, options->scripts_root, result);
    }
    if(!valid)
    {
        goto done;
    }
    for(index = 0U; index < governed.count; index++)
    {
        char path[P101_WORKSPACE_AUDIT_PATH_SIZE];

        joined = p101_workspace_audit_join(env, err, path, sizeof(path), options->scripts_root, governed.values[index]);
        if(!joined)
        {
            goto done;
        }
        if(!path_status(env, P101_ERROR_OPTIONAL, path, true, false))
        {
            p101_workspace_audit_add(env, err, result, path, "governed scripts entry point is missing or not executable");
        }
    }
    for(index = 0U; index < exclusions.count; index++)
    {
        if(!path_list_contains(env, &discovered, exclusions.values[index]))
        {
            p101_workspace_audit_add(env, err, result, inventory_path, "verification exclusion is not a discovered verification entry point");
        }
    }
    for(index = 0U; index < discovered.count; index++)
    {
        if(!path_list_contains(env, &governed, discovered.values[index]) && !path_list_contains(env, &exclusions, discovered.values[index]))
        {
            char path[P101_WORKSPACE_AUDIT_PATH_SIZE];

            joined = p101_workspace_audit_join(env, err, path, sizeof(path), options->scripts_root, discovered.values[index]);
            if(!joined)
            {
                goto done;
            }
            p101_workspace_audit_add(env, err, result, path, "ungoverned scripts verification entry point");
        }
    }
    result->checks += governed.count + exclusions.count + discovered.count;
    success = p101_error_has_no_error(err);

done:
    p101_workspace_json_destroy(env, &graph);
    p101_workspace_json_destroy(env, &inventory);
    return success;
}

static bool token_copy(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t token, char *output, size_t output_size)
{
    size_t length;
    bool   copied;

    copied = false;
    if(token >= document->token_count || document->tokens[token].kind != P101_WORKSPACE_JSON_STRING)
    {
        goto done;
    }
    length = document->tokens[token].end - document->tokens[token].start;
    if(length >= output_size)
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    p101_memcpy(env, output, document->text + document->tokens[token].start, length);
    output[length] = '\0';
    copied         = true;

done:
    return copied;
}

static bool require_string(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *document, size_t object, const char *key, char *output, size_t output_size, const char *path, struct p101_workspace_audit_result *result)
{
    size_t value;
    bool   found;
    bool   copied;

    found  = p101_workspace_json_object_get(env, document, object, key, &value);
    copied = ((found && token_copy(env, err, document, value, output, output_size)) != 0);
    if(copied)
    {
        copied = output[0] != '\0';
    }
    if(!copied && p101_error_has_no_error(err))
    {
        p101_workspace_audit_add(env, err, result, path, "required inventory text field is absent or empty");
    }
    return copied;
}

static bool path_status(const struct p101_env *env, struct p101_error *err, const char *path, bool executable, bool directory)
{
    struct stat status_buffer;
    int         status;
    bool        matches;

    status  = p101_stat(env, err, path, &status_buffer);
    matches = status == 0;
    if(matches && directory)
    {
        matches = S_ISDIR(status_buffer.st_mode) != 0;
    }
    else if(matches)
    {
        matches = S_ISREG(status_buffer.st_mode) != 0;
    }
    if(matches && executable)
    {
        matches = (status_buffer.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
    }
    return matches;
}

static bool path_list_contains(const struct p101_env *env, const struct inventory_paths *paths, const char *value)
{
    size_t index;
    bool   present;

    present = false;
    for(index = 0U; index < paths->count; index++)
    {
        int comparison;

        comparison = p101_strcmp(env, paths->values[index], value);
        if(comparison == 0)
        {
            present = true;
            break;
        }
    }
    return present;
}

static bool path_list_add(const struct p101_env *env, struct p101_error *err, struct inventory_paths *paths, const char *value)
{
    size_t length;
    bool   added;

    added  = false;
    length = p101_strlen(env, value);
    if(paths->count >= INVENTORY_PATH_LIMIT || length >= sizeof(paths->values[0]))
    {
        P101_ERROR_RAISE_ERRNO(err, EOVERFLOW);
        goto done;
    }
    if(!path_list_contains(env, paths, value))
    {
        p101_memcpy(env, paths->values[paths->count], value, length + 1U);
        paths->count++;
    }
    added = true;

done:
    return added;
}

static bool collect_graph_entries(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *graph, struct inventory_paths *governed)
{
    char   command[INVENTORY_NAME_SIZE];
    size_t nodes;
    size_t node;
    size_t command_array;
    size_t executable;
    size_t index;
    bool   found;
    bool   copied;
    bool   added;
    bool   success;

    command_array = 0U;
    executable    = 0U;
    success       = p101_workspace_json_object_get(env, graph, 0U, "nodes", &nodes);
    if(!success)
    {
        P101_ERROR_RAISE_ERRNO(err, EINVAL);
        goto done;
    }
    for(index = 0U; index < graph->tokens[nodes].child_count; index++)
    {
        found = p101_workspace_json_array_get(graph, nodes, index, &node);
        found = ((found && p101_workspace_json_object_get(env, graph, node, "command", &command_array)) != 0);
        found = ((found && p101_workspace_json_array_get(graph, command_array, 0U, &executable)) != 0);
        if(!found)
        {
            continue;
        }
        copied = token_copy(env, err, graph, executable, command, sizeof(command));
        if(!copied)
        {
            goto done;
        }
        if(command[0] == '.' && command[1] == '/')
        {
            added = path_list_add(env, err, governed, command + 2);
            if(!added)
            {
                goto done;
            }
        }
    }
    success = true;

done:
    return success;
}

static bool validate_entry_contracts(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *inventory, const char *scripts_root, const char *inventory_path, struct inventory_paths *entry_names,
                                     struct p101_workspace_audit_result *result)
{
    char   entry_name[INVENTORY_NAME_SIZE];
    char   value[INVENTORY_NAME_SIZE];
    char   runner_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    size_t entry_points;
    size_t index;
    size_t child;
    size_t pair_index;
    bool   found;
    bool   copied;
    bool   added;
    bool   joined;
    bool   success;

    success = p101_workspace_json_object_get(env, inventory, 0U, "entry_points", &entry_points);
    if(!success || inventory->tokens[entry_points].kind != P101_WORKSPACE_JSON_OBJECT || inventory->tokens[entry_points].child_count == 0U)
    {
        p101_workspace_audit_add(env, err, result, inventory_path, "inventory has no repository entry-point contracts");
        success = p101_error_has_no_error(err);
        goto done;
    }
    child      = 0U;
    pair_index = 0U;
    for(index = entry_points + 1U; index < inventory->token_count && inventory->tokens[index].start < inventory->tokens[entry_points].end; index++)
    {
        if(inventory->tokens[index].parent != entry_points)
        {
            continue;
        }
        if((child % 2U) == 0U)
        {
            copied = token_copy(env, err, inventory, index, entry_name, sizeof(entry_name));
            if(!copied)
            {
                success = false;
                goto done;
            }
            pair_index = index + 1U;
            added      = path_list_add(env, err, entry_names, entry_name);
            if(!added)
            {
                success = false;
                goto done;
            }
        }
        else
        {
            found = inventory->tokens[pair_index].kind == P101_WORKSPACE_JSON_OBJECT;
            if(!found)
            {
                p101_workspace_audit_add(env, err, result, inventory_path, "repository entry-point contract is not an object");
            }
            else
            {
                require_string(env, err, inventory, pair_index, "owner", value, sizeof(value), inventory_path, result);
                require_string(env, err, inventory, pair_index, "oracle", value, sizeof(value), inventory_path, result);
                found = require_string(env, err, inventory, pair_index, "runner", value, sizeof(value), inventory_path, result);
                if(found)
                {
                    joined = p101_workspace_audit_join(env, err, runner_path, sizeof(runner_path), scripts_root, value);
                    if(joined && !path_status(env, P101_ERROR_OPTIONAL, runner_path, false, false))
                    {
                        p101_workspace_audit_add(env, err, result, runner_path, "repository entry-point contract has a missing runner");
                    }
                }
            }
        }
        child++;
    }
    success = p101_error_has_no_error(err);

done:
    return success;
}

static bool collect_exclusions(const struct p101_env *env, struct p101_error *err, const struct p101_workspace_json *inventory, const char *scripts_root, const char *inventory_path, struct inventory_paths *exclusions,
                               struct p101_workspace_audit_result *result)
{
    char   value[INVENTORY_NAME_SIZE];
    char   path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    size_t array;
    size_t row;
    size_t index;
    bool   found;
    bool   copied;
    bool   joined;
    bool   success;

    success = p101_workspace_json_object_get(env, inventory, 0U, "standalone_verification_exclusions", &array);
    if(!success)
    {
        p101_workspace_audit_add(env, err, result, inventory_path, "inventory has no exclusion list");
        goto done;
    }
    for(index = 0U; index < inventory->tokens[array].child_count; index++)
    {
        found  = p101_workspace_json_array_get(inventory, array, index, &row);
        copied = ((found && require_string(env, err, inventory, row, "path", value, sizeof(value), inventory_path, result)) != 0);
        if(!copied)
        {
            continue;
        }
        if(path_list_contains(env, exclusions, value))
        {
            p101_workspace_audit_add(env, err, result, inventory_path, "duplicate verification exclusion");
            continue;
        }
        path_list_add(env, err, exclusions, value);
        joined = p101_workspace_audit_join(env, err, path, sizeof(path), scripts_root, value);
        if(joined && !path_status(env, P101_ERROR_OPTIONAL, path, false, false))
        {
            p101_workspace_audit_add(env, err, result, path, "stale verification exclusion");
        }
        require_string(env, err, inventory, row, "owner", path, sizeof(path), inventory_path, result);
        require_string(env, err, inventory, row, "oracle", path, sizeof(path), inventory_path, result);
        require_string(env, err, inventory, row, "reason", path, sizeof(path), inventory_path, result);
    }
    success = p101_error_has_no_error(err);

done:
    return success;
}

static bool collect_scripts(const struct p101_env *env, struct p101_error *err, const char *root, const char *relative, bool recursive, struct inventory_paths *discovered)    // NOLINT(misc-no-recursion): bounded workspace tree walk.
{
    char           directory_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char           relative_path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    DIR           *directory;
    struct dirent *entry;
    int            comparison;
    int            close_status;
    int            written;
    bool           joined;
    bool           is_directory;
    bool           success;

    success = false;
    if(relative[0] == '\0')
    {
        written = p101_snprintf(env, err, directory_path, sizeof(directory_path), "%s", root);
        joined  = (bool)(written >= 0);
    }
    else
    {
        joined = p101_workspace_audit_join(env, err, directory_path, sizeof(directory_path), root, relative);
    }
    if(!joined)
    {
        goto done;
    }
    directory = p101_opendir(env, err, directory_path);
    if(directory == NULL)
    {
        goto done;
    }
    success = true;
    while(success)
    {
        entry = p101_readdir(env, err, directory);
        if(entry == NULL)
        {
            break;
        }
        comparison = p101_strcmp(env, entry->d_name, ".");
        if(comparison == 0)
        {
            continue;
        }
        comparison = p101_strcmp(env, entry->d_name, "..");
        if(comparison == 0)
        {
            continue;
        }
        if(relative[0] == '\0')
        {
            p101_snprintf(env, err, relative_path, sizeof(relative_path), "%s", entry->d_name);
        }
        else
        {
            p101_snprintf(env, err, relative_path, sizeof(relative_path), "%s/%s", relative, entry->d_name);
        }
        comparison = p101_strncmp(env, relative_path, "target/", sizeof("target/") - 1U);
        if(comparison == 0 || p101_strcmp(env, relative_path, "target") == 0 || p101_strncmp(env, relative_path, ".git/", sizeof(".git/") - 1U) == 0 || p101_strcmp(env, relative_path, ".git") == 0)
        {
            continue;
        }
        joined = p101_workspace_audit_join(env, err, directory_path, sizeof(directory_path), root, relative_path);
        if(!joined)
        {
            success = false;
            break;
        }
        is_directory = path_status(env, P101_ERROR_OPTIONAL, directory_path, false, true);
        if(is_directory && recursive)
        {
            success = collect_scripts(env, err, root, relative_path, true, discovered);
        }
        else if(verification_name(env, entry->d_name))
        {
            success = path_list_add(env, err, discovered, relative_path);
        }
    }
    close_status = p101_closedir(env, P101_ERROR_OPTIONAL, directory);
    if(close_status != 0)
    {
        success = false;
    }

done:
    return success;
}

static bool verification_name(const struct p101_env *env, const char *name)
{
    size_t length;
    int    prefix_check;
    bool   matches;

    length       = p101_strlen(env, name);
    prefix_check = p101_strncmp(env, name, "check-", sizeof("check-") - 1U);
    if(prefix_check != 0)
    {
        prefix_check = p101_strncmp(env, name, "test-", sizeof("test-") - 1U);
    }
    matches = ((prefix_check == 0 && length > 3U) != 0);
    if(matches)
    {
        int suffix_check;

        suffix_check = p101_strcmp(env, name + length - 3U, ".sh");
        if(suffix_check != 0)
        {
            suffix_check = p101_strcmp(env, name + length - 3U, ".py");
        }
        matches = suffix_check == 0;
    }
    return matches;
}

static bool validate_repository_entries(const struct p101_env *env, struct p101_error *err, const char *manifest_path, const char *scripts_root, struct p101_workspace_audit_result *result)
{
    char                   line[INVENTORY_LINE_SIZE];
    char                   repository[P101_WORKSPACE_AUDIT_PATH_SIZE];
    char                   path[P101_WORKSPACE_AUDIT_PATH_SIZE];
    struct inventory_paths repositories;
    FILE                  *stream;
    const char            *read_result;
    const char            *first;
    const char            *second;
    char                  *language;
    size_t                 second_offset;
    int                    close_status;
    bool                   joined;
    bool                   success;

    p101_memset(env, &repositories, 0, sizeof(repositories));
    stream = p101_fopen(env, err, manifest_path, "r");
    if(stream == NULL)
    {
        success = false;
        goto done;
    }
    while(true)
    {
        read_result = p101_fgets(env, err, line, sizeof(line), stream);
        if(read_result == NULL)
        {
            break;
        }
        if(line[0] == '#' || line[0] == '\n' || line[0] == '\0')
        {
            continue;
        }
        first  = p101_strchr(env, line, '|');
        second = first == NULL ? NULL : p101_strchr(env, first + 1, '|');
        if(first == NULL || second == NULL)
        {
            p101_workspace_audit_add(env, err, result, manifest_path, "malformed repository row");
            continue;
        }
        second_offset                                 = (size_t)(second - line);
        line[second_offset]                           = '\0';
        language                                      = line + second_offset + 1U;
        language[p101_strcspn(env, language, "\r\n")] = '\0';
        if(p101_strcmp(env, language, "c") != 0 && p101_strcmp(env, language, "cxx") != 0 && p101_strcmp(env, language, "c-reference") != 0 && p101_strcmp(env, language, "python") != 0 && p101_strcmp(env, language, "c-bootstrap") != 0)
        {
            p101_workspace_audit_add(env, err, result, manifest_path, "unsupported repository language");
        }
        joined = p101_workspace_audit_join(env, err, repository, sizeof(repository), scripts_root, first + 1);
        if(!joined)
        {
            break;
        }
        if(!path_status(env, P101_ERROR_OPTIONAL, repository, false, true))
        {
            p101_workspace_audit_add(env, err, result, repository, "missing repository");
            continue;
        }
        if(path_list_contains(env, &repositories, repository))
        {
            p101_workspace_audit_add(env, err, result, repository, "duplicate repository path");
            continue;
        }
        path_list_add(env, err, &repositories, repository);
        if(p101_strcmp(env, language, "c") == 0 || p101_strcmp(env, language, "cxx") == 0)
        {
            bool has_build_model;

            p101_workspace_audit_join(env, err, path, sizeof(path), repository, "CMakeLists.txt");
            has_build_model = p101_workspace_audit_file_exists(env, P101_ERROR_OPTIONAL, path);
            if(!has_build_model)
            {
                p101_workspace_audit_join(env, err, path, sizeof(path), repository, "Makefile");
                has_build_model = p101_workspace_audit_file_exists(env, P101_ERROR_OPTIONAL, path);
            }
            if(!has_build_model)
            {
                p101_workspace_audit_add(env, err, result, repository, "C/C++ repository has no CMake or Make build model");
            }
        }
        result->checks++;
    }
    close_status = p101_fclose(env, P101_ERROR_OPTIONAL, stream);
    success      = ((close_status == 0 && p101_error_has_no_error(err)) != 0);

done:
    return success;
}
